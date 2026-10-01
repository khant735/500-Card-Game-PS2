#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <sbv_patches.h>
#include <libpad.h>
#include <audsrv.h>
#include <gsKit.h>
#include <gsTexture.h>
#include <dmaKit.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

extern unsigned char audsrv_irx[];
extern unsigned int size_audsrv_irx;
extern unsigned char sio2man_irx[];
extern unsigned int size_sio2man_irx;
extern unsigned char padman_irx[];
extern unsigned int size_padman_irx;

#define LOGICAL_W 960
#define LOGICAL_H 540
#define VIDEO_W 960
#define VIDEO_H 540
#define SAFE_LEFT 18
#define SAFE_RIGHT 942
#define FOOTER_TOP 512
#define MAX_HAND 16
#define MAX_DECK 54
#define WIDOW_SIZE 6
#define MAX_LOG 5
#define GS_VRAM_TOTAL_BYTES (4u * 1024u * 1024u)

#define PLAYER 0
#define CPU 1

#define PHASE_IDLE 0
#define PHASE_BIDDING 1
#define PHASE_PLAY 2
#define PHASE_RESULT 3
#define PHASE_DONE 4

#define SUIT_SPADES 0
#define SUIT_HEARTS 1
#define SUIT_DIAMONDS 2
#define SUIT_CLUBS 3
#define SUIT_NT 4
#define SUIT_JOKER 5

#define COL_BLACK       GS_SETREG_RGBAQ(10, 12, 12, 0x80, 0)
#define COL_PANEL       GS_SETREG_RGBAQ(12, 14, 16, 0x80, 0)
#define COL_PANEL2      GS_SETREG_RGBAQ(28, 23, 14, 0x80, 0)
#define COL_FELT        GS_SETREG_RGBAQ(12, 82, 52, 0x80, 0)
#define COL_FELT_DARK   GS_SETREG_RGBAQ(5, 36, 23, 0x80, 0)
#define COL_GOLD        GS_SETREG_RGBAQ(214, 181, 92, 0x80, 0)
#define COL_GOLD_DIM    GS_SETREG_RGBAQ(128, 90, 34, 0x80, 0)
#define COL_WHITE       GS_SETREG_RGBAQ(248, 245, 233, 0x80, 0)
#define COL_GRAY        GS_SETREG_RGBAQ(174, 170, 158, 0x80, 0)
#define COL_BLUE        GS_SETREG_RGBAQ(42, 145, 244, 0x80, 0)
#define COL_BLUE_DARK   GS_SETREG_RGBAQ(11, 58, 127, 0x80, 0)
#define COL_RED         GS_SETREG_RGBAQ(214, 58, 60, 0x80, 0)
#define COL_RED_DARK    GS_SETREG_RGBAQ(118, 20, 24, 0x80, 0)
#define COL_CARD        GS_SETREG_RGBAQ(246, 242, 232, 0x80, 0)
#define COL_CARD_EDGE   GS_SETREG_RGBAQ(186, 170, 138, 0x80, 0)
#define COL_TEXT_RED    GS_SETREG_RGBAQ(195, 32, 38, 0x80, 0)
#define COL_GREEN       GS_SETREG_RGBAQ(88, 232, 138, 0x80, 0)
#define COL_YELLOW      GS_SETREG_RGBAQ(249, 220, 122, 0x80, 0)

static GSGLOBAL *g_gs;
static char pad_buf[256] __attribute__((aligned(64)));
static int g_pad_open = 0;
static int g_pad_connected = 0;
static int g_pad_retry_frames = 0;
static int g_pad_state = PAD_STATE_DISCONN;
static u32 g_old_pad = 0;
static int g_audio_ok = 0;
static unsigned int g_audio_drops = 0;
static int g_pad_module_ok = 0;
static int g_sound_on = 1;
static int g_master_volume=100, g_sfx_volume=100, g_ui_volume=100, g_music_volume=100;
static int g_master_mute=0, g_sfx_mute=0, g_ui_mute=0, g_music_mute=0;
static int g_main_menu=1, g_main_cursor=0;
static int g_options_open=0, g_options_page=0, g_options_cursor=0, g_options_from_game=0;
static int g_paused=0, g_reset_confirm=0, g_return_confirm=0;
enum { RULE_STANDARD=0, RULE_ONE_JOKER, RULE_TWO_JOKERS, RULE_ORIGINAL, RULE_CUSTOM, RULE_COUNT };
static int g_ruleset=RULE_STANDARD, g_custom_jokers=0;
static int g_custom_low_rank=0;       /* 0=2 ... 8=10 */
static int g_custom_hand_size=10;
static int g_custom_kitty_size=2;
static int g_custom_min_bid=7;
static int g_custom_max_bid=10;
static int g_custom_target=500;
static int g_custom_start_chips=100;
static const char *g_rule_names[RULE_COUNT]={"STANDARD","ONE JOKER","TWO JOKERS","ORIGINAL REDUCED","CUSTOM"};
static s16 g_tone[4096 * 2] __attribute__((aligned(64)));
static int g_screen_h = VIDEO_H;
static float g_xscale = (float)VIDEO_W / (float)LOGICAL_W;
static float g_yscale = (float)VIDEO_H / (float)LOGICAL_H;


/* GS-resident artwork. Large/background surfaces use PSMT8
   (256-colour CLUT). The card face itself is now a plain GS cream fill
   with a tiny 32x32 grain overlay, while the card backs/chips stay as
   lighter-weight direct 16-bit textures. */
static GSTEXTURE g_tex_felt;
static GSTEXTURE g_tex_wood;
static GSTEXTURE g_tex_leather;
static GSTEXTURE g_tex_cardgrain;
static GSTEXTURE g_tex_cardback;
static GSTEXTURE g_tex_chips;
static int g_art_tex_ok = 0;

/* Large source images are stored zlib-compressed in the ELF / EE address
   space.  They are expanded into one reusable EE scratch buffer only when
   uploading to GS VRAM. */
extern unsigned char asset_felt_z[], asset_felt_z_end[], asset_felt_pal[];
extern unsigned char asset_wood_z[], asset_wood_z_end[], asset_wood_pal[];
extern unsigned char asset_leather_z[], asset_leather_z_end[], asset_leather_pal[];
extern unsigned char asset_cardgrain_z[], asset_cardgrain_z_end[];
extern unsigned char asset_cardback_z[], asset_cardback_z_end[];
extern unsigned char asset_chips_z[], asset_chips_z_end[];

#define SPECIAL_ASSET_COUNT 14
#define SPECIAL_CACHE_SLOTS SPECIAL_ASSET_COUNT
#define ASSET_SCRATCH_BYTES (256*384*2)

/* 0..11 = J/Q/K x four suits, 12 = greyscale Joker, 13 = colour Joker. */
extern unsigned char asset_special_00_z[], asset_special_00_z_end[];
extern unsigned char asset_special_01_z[], asset_special_01_z_end[];
extern unsigned char asset_special_02_z[], asset_special_02_z_end[];
extern unsigned char asset_special_03_z[], asset_special_03_z_end[];
extern unsigned char asset_special_04_z[], asset_special_04_z_end[];
extern unsigned char asset_special_05_z[], asset_special_05_z_end[];
extern unsigned char asset_special_06_z[], asset_special_06_z_end[];
extern unsigned char asset_special_07_z[], asset_special_07_z_end[];
extern unsigned char asset_special_08_z[], asset_special_08_z_end[];
extern unsigned char asset_special_09_z[], asset_special_09_z_end[];
extern unsigned char asset_special_10_z[], asset_special_10_z_end[];
extern unsigned char asset_special_11_z[], asset_special_11_z_end[];
extern unsigned char asset_special_12_z[], asset_special_12_z_end[];
extern unsigned char asset_special_13_z[], asset_special_13_z_end[];

typedef struct {
    GSTEXTURE tex;
    int asset_id;
    unsigned int stamp;
    u32 slot_vram;
} SpecialCacheSlot;

static SpecialCacheSlot g_special_cache[SPECIAL_CACHE_SLOTS];
static unsigned int g_special_stamp = 1;
static unsigned char g_asset_scratch[ASSET_SCRATCH_BYTES] __attribute__((aligned(64)));

/* 5x7 bitmap font: row bits use low 5 bits, left to right. */
typedef struct { char c; unsigned char row[7]; } Glyph;
static const Glyph font5x7[] = {
 {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
 {'2',{14,17,1,2,4,8,31}}, {'3',{30,1,1,14,1,1,30}},
 {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,16,30,1,1,30}},
 {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
 {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,1,14}},
 {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
 {'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}},
 {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
 {'G',{14,17,16,23,17,17,15}}, {'H',{17,17,17,31,17,17,17}},
 {'I',{14,4,4,4,4,4,14}}, {'J',{7,2,2,2,2,18,12}},
 {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
 {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}},
 {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
 {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
 {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}},
 {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
 {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
 {'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
 {'-',{0,0,0,31,0,0,0}}, {'+',{0,4,4,31,4,4,0}},
 {':',{0,4,4,0,4,4,0}}, {'/',{1,2,2,4,8,8,16}},
 {'.',{0,0,0,0,0,12,12}}, {'!',{4,4,4,4,4,0,4}},
 {'?',{14,17,1,2,4,0,4}}, {'=',{0,31,0,31,0,0,0}},
 {' ',{0,0,0,0,0,0,0}}
};

static const char *suit_names[5] = {"SPADES", "HEARTS", "DIAMONDS", "CLUBS", "NO TRUMPS"};
static const char *rank_names[13] = {"2","3","4","5","6","7","8","9","10","J","Q","K","A"};

typedef struct {
    unsigned char rank; /* 0..12, 255 Joker */
    unsigned char suit; /* 0..3, 5 Joker */
    unsigned char joker;
} Card;

typedef struct {
    int player;
    Card card;
} PlayedCard;

static Card deck[MAX_DECK];
static int deck_count = 0;
static Card human[MAX_HAND];
static int human_count = 0;
static Card cpu[MAX_HAND];
static int cpu_count = 0;
static Card widow[WIDOW_SIZE];
static int widow_count = 0;
static PlayedCard trick[2];
static int trick_count = 0;
static int tricks_won[2] = {0,0};
static int scores[2] = {0,0};
static int chip_bank[2] = {100,100};
static int phase = PHASE_IDLE;
static int current_player = PLAYER;
static int selected_card = 0;
static int bid_level = 6;
static int bid_suit = SUIT_SPADES;
static int contract_level = 0;
static int contract_suit = SUIT_SPADES;
static int contract_by = PLAYER;
static int trump = SUIT_SPADES;
static int trick_stake = 0;
static int hand_no = 1;
static int result_timer = 0;
static int result_winner = PLAYER;
static int result_transfer = 0;
static int pending_leader = PLAYER;
static int match_winner = -1;
static char logs[MAX_LOG][64];
static int log_count = 0;
static const char *card_rank_label(const Card *c);

static inline float X(float x) { return x * g_xscale; }
static inline float Y(float y) { return y * g_yscale; }


static int inflate_asset(const unsigned char *src,const unsigned char *end,void *dst,u32 bytes) {
    uLongf out_len=(uLongf)bytes;
    uLong src_len=(uLong)(end-src);
    int zr=uncompress((Bytef *)dst,&out_len,(const Bytef *)src,src_len);
    return zr==Z_OK && out_len==(uLongf)bytes;
}

static int init_texture_t8(GSTEXTURE *t,int w,int h,void *pix,void *pal,int filter) {
    memset(t,0,sizeof(*t));
    t->Width=w; t->Height=h; t->PSM=GS_PSM_T8;
    t->ClutPSM=GS_PSM_CT32;
    t->ClutStorageMode=GS_CLUT_STORAGE_CSM1;
    t->Filter=filter;
    t->Mem=pix; t->Clut=pal;
    t->Vram=gsKit_vram_alloc(g_gs,gsKit_texture_size(w,h,GS_PSM_T8),GSKIT_ALLOC_USERBUFFER);
    t->VramClut=gsKit_vram_alloc(g_gs,gsKit_texture_size(16,16,GS_PSM_CT32),GSKIT_ALLOC_USERBUFFER);
    if(t->Vram==GSKIT_ALLOC_ERROR || t->VramClut==GSKIT_ALLOC_ERROR) return 0;
    gsKit_texture_upload(g_gs,t);
    dmaKit_wait_fast();
    return 1;
}

static int init_texture_ct16(GSTEXTURE *t,int w,int h,void *pix,int filter) {
    memset(t,0,sizeof(*t));
    t->Width=w; t->Height=h; t->PSM=GS_PSM_CT16;
    t->Filter=filter;
    t->Mem=pix;
    t->Vram=gsKit_vram_alloc(g_gs,gsKit_texture_size(w,h,GS_PSM_CT16),GSKIT_ALLOC_USERBUFFER);
    if(t->Vram==GSKIT_ALLOC_ERROR) return 0;
    gsKit_texture_upload(g_gs,t);
    dmaKit_wait_fast();
    return 1;
}

static const unsigned char *g_special_start[SPECIAL_ASSET_COUNT]={
    asset_special_00_z,asset_special_01_z,asset_special_02_z,asset_special_03_z,
    asset_special_04_z,asset_special_05_z,asset_special_06_z,asset_special_07_z,
    asset_special_08_z,asset_special_09_z,asset_special_10_z,asset_special_11_z,
    asset_special_12_z,asset_special_13_z
};
static const unsigned char *g_special_end[SPECIAL_ASSET_COUNT]={
    asset_special_00_z_end,asset_special_01_z_end,asset_special_02_z_end,asset_special_03_z_end,
    asset_special_04_z_end,asset_special_05_z_end,asset_special_06_z_end,asset_special_07_z_end,
    asset_special_08_z_end,asset_special_09_z_end,asset_special_10_z_end,asset_special_11_z_end,
    asset_special_12_z_end,asset_special_13_z_end
};
static const unsigned short g_special_w[SPECIAL_ASSET_COUNT]={
    128,128,128,128,128,128,128,128,128,128,128,128,128,128
};
static const unsigned short g_special_h[SPECIAL_ASSET_COUNT]={
    224,224,224,224,224,224,224,224,224,224,224,224,256,256
};

static int init_special_cache(void) {
    int i;
    u32 slot_bytes=gsKit_texture_size(128,256,GS_PSM_CT16);
    for(i=0;i<SPECIAL_CACHE_SLOTS;i++) {
        memset(&g_special_cache[i],0,sizeof(g_special_cache[i]));
        g_special_cache[i].asset_id=-1;
        g_special_cache[i].slot_vram=gsKit_vram_alloc(g_gs,slot_bytes,GSKIT_ALLOC_USERBUFFER);
        if(g_special_cache[i].slot_vram==GSKIT_ALLOC_ERROR) return 0;
    }
    return 1;
}

static GSTEXTURE *special_texture_get(int asset_id) {
    int i,best=-1;
    unsigned int oldest=0xFFFFFFFFu;
    SpecialCacheSlot *slot;
    u32 bytes;
    if(asset_id<0 || asset_id>=SPECIAL_ASSET_COUNT) return NULL;

    for(i=0;i<SPECIAL_CACHE_SLOTS;i++) {
        if(g_special_cache[i].asset_id==asset_id) {
            g_special_cache[i].stamp=++g_special_stamp;
            return &g_special_cache[i].tex;
        }
        if(g_special_cache[i].asset_id<0) best=i;
    }
    if(best<0) {
        for(i=0;i<SPECIAL_CACHE_SLOTS;i++) {
            if(g_special_cache[i].stamp<oldest) { oldest=g_special_cache[i].stamp; best=i; }
        }
        /* The victim may already be referenced by commands in the current
           GIF queue. Submit those commands before overwriting its VRAM slot.
           GIF ordering guarantees the upload follows the earlier draws. */
        gsKit_queue_exec(g_gs);
        dmaKit_wait_fast();
    }

    slot=&g_special_cache[best];
    bytes=(u32)g_special_w[asset_id]*(u32)g_special_h[asset_id]*2u;
    if(bytes>ASSET_SCRATCH_BYTES) return NULL;
    if(!inflate_asset(g_special_start[asset_id],g_special_end[asset_id],g_asset_scratch,bytes)) return NULL;

    memset(&slot->tex,0,sizeof(slot->tex));
    slot->tex.Width=g_special_w[asset_id];
    slot->tex.Height=g_special_h[asset_id];
    slot->tex.PSM=GS_PSM_CT16;
    slot->tex.Filter=GS_FILTER_LINEAR;
    slot->tex.Mem=(u32 *)g_asset_scratch;
    slot->tex.Vram=slot->slot_vram;
    gsKit_texture_upload(g_gs,&slot->tex);
    dmaKit_wait_fast();
    slot->asset_id=asset_id;
    slot->stamp=++g_special_stamp;
    return &slot->tex;
}

static int init_art_textures(void) {
    int ok=1;

    if(!inflate_asset(asset_felt_z,asset_felt_z_end,g_asset_scratch,256u*128u)) ok=0;
    else ok &= init_texture_t8(&g_tex_felt,256,128,g_asset_scratch,asset_felt_pal,GS_FILTER_LINEAR);

    if(!inflate_asset(asset_wood_z,asset_wood_z_end,g_asset_scratch,128u*32u)) ok=0;
    else ok &= init_texture_t8(&g_tex_wood,128,32,g_asset_scratch,asset_wood_pal,GS_FILTER_LINEAR);

    if(!inflate_asset(asset_leather_z,asset_leather_z_end,g_asset_scratch,128u*32u)) ok=0;
    else ok &= init_texture_t8(&g_tex_leather,128,32,g_asset_scratch,asset_leather_pal,GS_FILTER_LINEAR);

    if(!inflate_asset(asset_cardgrain_z,asset_cardgrain_z_end,g_asset_scratch,32u*32u*2u)) ok=0;
    else ok &= init_texture_ct16(&g_tex_cardgrain,32,32,g_asset_scratch,GS_FILTER_LINEAR);

    if(!inflate_asset(asset_cardback_z,asset_cardback_z_end,g_asset_scratch,64u*96u*2u)) ok=0;
    else ok &= init_texture_ct16(&g_tex_cardback,64,96,g_asset_scratch,GS_FILTER_LINEAR);

    if(!inflate_asset(asset_chips_z,asset_chips_z_end,g_asset_scratch,64u*32u*2u)) ok=0;
    else ok &= init_texture_ct16(&g_tex_chips,64,32,g_asset_scratch,GS_FILTER_LINEAR);

    ok &= init_special_cache();

    /* Benchmark C: the baseline only had four special-card slots for fourteen
       court/Joker assets.  Cache eviction forces queue submission, DMA waits,
       inflate and VRAM upload while drawing a frame.  The baseline assets are
       small enough to keep all fourteen resident, so pay that cost once here. */
    if (ok) {
        int special_id;
        for (special_id=0; special_id<SPECIAL_ASSET_COUNT; ++special_id) {
            if (!special_texture_get(special_id)) { ok=0; break; }
        }
    }

    g_art_tex_ok=ok;
    gsKit_queue_exec(g_gs);
    dmaKit_wait_fast();
    return ok;
}

static void tex_sprite_uv(const GSTEXTURE *t,float x,float y,float w,float h,
                          float u1,float v1,float u2,float v2,int z) {
    if(!g_art_tex_ok) return;
    gsKit_prim_sprite_texture_3d(g_gs,t,X(x),Y(y),z,u1,v1,X(x+w),Y(y+h),z,u2,v2,
                                 GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0));
}

static void tex_sprite(const GSTEXTURE *t,float x,float y,float w,float h,int z) {
    tex_sprite_uv(t,x,y,w,h,0,0,(float)t->Width,(float)t->Height,z);
}

static void tile_texture(const GSTEXTURE *t,float x,float y,float w,float h,float tw,float th,int z) {
    float yy,xx;
    if(!g_art_tex_ok) return;
    for(yy=y;yy<y+h;yy+=th) for(xx=x;xx<x+w;xx+=tw) {
        float rw=(xx+tw>x+w)?(x+w-xx):tw;
        float rh=(yy+th>y+h)?(y+h-yy):th;
        gsKit_prim_sprite_texture_3d(g_gs,t,X(xx),Y(yy),z,0,0,X(xx+rw),Y(yy+rh),z,
                                    (float)t->Width*(rw/tw),(float)t->Height*(rh/th),
                                    GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0));
    }
}

static const unsigned char *glyph_rows(char c) {
    unsigned int i;
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    for (i = 0; i < sizeof(font5x7)/sizeof(font5x7[0]); ++i)
        if (font5x7[i].c == c) return font5x7[i].row;
    return font5x7[sizeof(font5x7)/sizeof(font5x7[0]) - 1].row;
}

static void rect(float x1,float y1,float x2,float y2,int z,u64 color) {
    gsKit_prim_sprite(g_gs,X(x1),Y(y1),X(x2),Y(y2),z,color);
}

static void frame(float x,float y,float w,float h,float t,u64 color) {
    rect(x,y,x+w,y+t,2,color);
    rect(x,y+h-t,x+w,y+h,2,color);
    rect(x,y,x+t,y+h,2,color);
    rect(x+w-t,y,x+w,y+h,2,color);
}

static inline u64 rgba8(int r,int g,int b,int a) { return GS_SETREG_RGBAQ(r,g,b,a,0); }

static void fill_vgrad(float x,float y,float w,float h,int r1,int g1,int b1,int r2,int g2,int b2,int steps) {
    int i;
    if (steps < 1) steps = 1;
    for (i=0;i<steps;i++) {
        float yy1 = y + h * (float)i / (float)steps;
        float yy2 = y + h * (float)(i+1) / (float)steps;
        int r = r1 + (r2 - r1) * i / (steps > 1 ? (steps - 1) : 1);
        int g = g1 + (g2 - g1) * i / (steps > 1 ? (steps - 1) : 1);
        int b = b1 + (b2 - b1) * i / (steps > 1 ? (steps - 1) : 1);
        rect(x,yy1,x+w,yy2,2,rgba8(r,g,b,0x80));
    }
}

static void fill_hgrad(float x,float y,float w,float h,int r1,int g1,int b1,int r2,int g2,int b2,int steps) {
    int i;
    if (steps < 1) steps = 1;
    for (i=0;i<steps;i++) {
        float xx1 = x + w * (float)i / (float)steps;
        float xx2 = x + w * (float)(i+1) / (float)steps;
        int r = r1 + (r2 - r1) * i / (steps > 1 ? (steps - 1) : 1);
        int g = g1 + (g2 - g1) * i / (steps > 1 ? (steps - 1) : 1);
        int b = b1 + (b2 - b1) * i / (steps > 1 ? (steps - 1) : 1);
        rect(xx1,y,xx2,y+h,2,rgba8(r,g,b,0x80));
    }
}

static void draw_text(float x,float y,const char *s,float scale,u64 color);
static float text_width(const char *s,float scale);
static void draw_text_center(float cx,float y,const char *s,float scale,u64 color);

static void luxe_panel(float x,float y,float w,float h,const char *title) {
    fill_vgrad(x,y,w,h,8,10,12,16,19,24,12);
    frame(x,y,w,h,1,rgba8(235,184,74,0x80));
    frame(x+1,y+1,w-2,h-2,1,rgba8(82,55,18,0x80));
    if (title) {
        fill_vgrad(x+2,y+2,w-4,22,22,18,14,10,10,8,10);
        rect(x+9,y+20,x+w-9,y+21,3,rgba8(104,79,31,0x80));
        draw_text(x+12,y+7,title,1.15f,rgba8(242,204,96,0x80));
    }
}

static void draw_char(float x,float y,char c,float scale,u64 color) {
    const unsigned char *rows = glyph_rows(c);
    int r, col;
    float px = scale;

    /* Merge adjacent lit pixels into one horizontal GS sprite.  This keeps the
       exact 5x7 glyph appearance while cutting the font command stream by
       roughly half compared with one sprite per pixel. */
    for (r=0;r<7;r++) {
        col=0;
        while (col<5) {
            int start;
            while (col<5 && !(rows[r] & (1 << (4-col)))) col++;
            if (col>=5) break;
            start=col;
            while (col<5 && (rows[r] & (1 << (4-col)))) col++;
            rect(x+start*px,y+r*px,x+col*px,y+(r+1)*px,5,color);
        }
    }
}

static void draw_text(float x,float y,const char *s,float scale,u64 color) {
    while (*s) {
        draw_char(x,y,*s,scale,color);
        x += scale * 6.0f;
        s++;
    }
}

static void draw_char_180(float x,float y,char c,float scale,u64 color) {
    const unsigned char *rows = glyph_rows(c);
    int r, col;
    float px = scale;
    for (r=0;r<7;r++) {
        int span=-1;
        for (col=0;col<5;col++) {
            int sr=6-r;
            int sc=4-col;
            int on=(rows[sr] >> (4-sc)) & 1;
            if (on && span<0) span=col;
            if ((!on || col==4) && span>=0) {
                int end=(on && col==4)?5:col;
                rect(x+span*px,y+r*px,x+end*px,y+(r+1)*px,4,color);
                span=-1;
            }
        }
    }
}

/* Rotate the complete rank string 180 degrees, including character order.
   This makes the lower-right index a true inverted corner index. */
static void draw_text_180(float x,float y,const char *s,float scale,u64 color) {
    int len=(int)strlen(s), i;
    float cx=x;
    for (i=len-1;i>=0;i--) {
        draw_char_180(cx,y,s[i],scale,color);
        cx += 6.0f*scale;
    }
}


static float text_width(const char *s,float scale) {
    return (float)strlen(s) * scale * 6.0f;
}

static void draw_text_vertical(float x,float y,const char *s,float scale,u64 color) {
    int i;
    float cy=y;
    for(i=0;s[i];++i) {
        draw_char(x,cy,s[i],scale,color);
        cy += scale * 8.0f;
    }
}

/* Bottom-right Joker index. When the card is rotated 180 degrees this becomes
   the same top-left J-O-K-E-R sequence as the upright index. */
static void draw_text_vertical_180(float x,float y,const char *s,float scale,u64 color) {
    int len=(int)strlen(s), i;
    float cy=y;
    for(i=len-1;i>=0;--i) {
        draw_char_180(x,cy,s[i],scale,color);
        cy += scale * 8.0f;
    }
}

static void draw_joker_indices(float x,float y,float w,float h,u64 color) {
    const char *label="JOKER";
    float scale=0.50f;
    float total_h=(float)strlen(label)*scale*8.0f;
    /* Small vertical indices, like a conventional printed Joker. */
    draw_text_vertical(x+5.0f,y+5.0f,label,scale,color);
    draw_text_vertical_180(x+w-8.0f,y+h-5.0f-total_h,label,scale,color);
}

static void draw_text_center(float cx,float y,const char *s,float scale,u64 color) {
    draw_text(cx - text_width(s,scale)/2.0f,y,s,scale,color);
}

static void panel(float x,float y,float w,float h,const char *title) {
    rect(x,y,x+w,y+h,1,COL_PANEL);
    frame(x,y,w,h,2,COL_GOLD_DIM);
    if (title) {
        rect(x+2,y+2,x+w-2,y+18,2,COL_PANEL2);
        draw_text(x+7,y+6,title,1.2f,COL_GOLD);
    }
}

static void log_line(const char *s) {
    int i;
    for (i=MAX_LOG-1;i>0;i--) strcpy(logs[i],logs[i-1]);
    strncpy(logs[0],s,sizeof(logs[0])-1);
    logs[0][sizeof(logs[0])-1] = '\0';
    if (log_count < MAX_LOG) log_count++;
}


static void sound_beep(int freq,int ms,int amplitude) {
    int frames, i, period, half, v, sent;
    if (!g_audio_ok || !g_sound_on || g_master_mute || g_sfx_mute || g_master_volume<=0 || g_sfx_volume<=0) return;
    amplitude = amplitude * g_master_volume / 100;
    amplitude = amplitude * g_sfx_volume / 100;
    if (ms < 10) ms = 10;
    if (ms > 100) ms = 100;
    frames = 22050 * ms / 1000;
    if (frames > 4096) frames = 4096;
    if (freq < 80) freq = 80;
    period = 22050 / freq;
    if (period < 2) period = 2;
    half = period / 2;
    for (i=0;i<frames;i++) {
        v = ((i % period) < half) ? amplitude : -amplitude;
        g_tone[i*2] = (s16)v;
        g_tone[i*2+1] = (s16)v;
    }

    /* Never block the EE/game loop for a UI sound.  audsrv_wait_audio() can
       wait forever if the emulated IOP/SPU2 stream stops consuming data.
       For short button/trick cues it is safer to submit once and drop the
       cue if audsrv cannot accept it immediately. */
    sent = audsrv_play_audio((char*)g_tone, frames * 4);
    if (sent < 0) {
        g_audio_drops++;
        if (g_audio_drops > 8) g_audio_ok = 0;
    } else {
        g_audio_drops = 0;
    }
}

static void audio_apply_master(void) {
    int v;
    if(!g_audio_ok) return;
    if(!g_sound_on || g_master_mute || g_master_volume<=0) { audsrv_set_volume(0); return; }
    v=(MAX_VOLUME*g_master_volume)/100;
    if(v<0)v=0; if(v>MAX_VOLUME)v=MAX_VOLUME;
    audsrv_set_volume(v);
}

static void init_audio(void) {
    int ret;
    struct audsrv_fmt_t fmt;
    ret = SifLoadModule("rom0:LIBSD",0,NULL);
    (void)ret;
    ret = SifExecModuleBuffer(audsrv_irx,size_audsrv_irx,0,NULL,NULL);
    if (ret < 0) return;
    if (audsrv_init() != 0) return;
    fmt.bits = 16;
    fmt.freq = 22050;
    fmt.channels = 2;
    if (audsrv_set_format(&fmt) != 0) return;
    g_audio_ok = 1;
    audio_apply_master();
}

static int try_open_pad(void) {
    if (g_pad_open) return 1;
    memset(pad_buf,0,sizeof(pad_buf));
    if (!padPortOpen(0,0,pad_buf)) return 0;
    g_pad_open = 1;
    g_old_pad = 0;
    return 1;
}

static void init_pad(void) {
    int ret_sio2, ret_pad;

    /* These are the free/new PAD modules. They require the libpadx EE client. */
    ret_sio2 = SifExecModuleBuffer(sio2man_irx,size_sio2man_irx,0,NULL,NULL);
    ret_pad  = SifExecModuleBuffer(padman_irx,size_padman_irx,0,NULL,NULL);
    if (ret_sio2 < 0 || ret_pad < 0) {
        g_pad_module_ok = 0;
        return;
    }
    g_pad_module_ok = 1;

    if (!padInit(0)) {
        g_pad_module_ok = 0;
        return;
    }

    /* Opening the port does not require the emulated controller to have
       reached STABLE yet. The per-frame reader handles late connection. */
    try_open_pad();
}

static u32 read_pad_new(void) {
    struct padButtonStatus buttons;
    u32 data, pressed;

    if (!g_pad_module_ok) return 0;

    if (!g_pad_open) {
        if (++g_pad_retry_frames >= 30) {
            g_pad_retry_frames = 0;
            try_open_pad();
        }
        return 0;
    }

    g_pad_state = padGetState(0,0);
    if (g_pad_state == PAD_STATE_DISCONN) {
        g_pad_connected = 0;
        g_old_pad = 0;
        return 0;
    }
    if (g_pad_state != PAD_STATE_STABLE && g_pad_state != PAD_STATE_FINDCTP1) return 0;

    /* Digital mode is sufficient for all gameplay controls and avoids
       issuing padSetMainMode RPC commands while PCSX2 is running.  Initialize
       analog bytes to neutral before padRead so a digital-only report cannot
       look like a stuck stick. */
    memset(&buttons,0,sizeof(buttons));
    buttons.ljoy_h=128; buttons.ljoy_v=128;
    buttons.rjoy_h=128; buttons.rjoy_v=128;
    if (!padRead(0,0,&buttons)) return 0;

    if (!g_pad_connected) {
        g_pad_connected = 1;
        g_old_pad = 0;
    }

    data = 0xffffu ^ (u32)buttons.btns;

    pressed = data & ~g_old_pad;
    g_old_pad = data;
    return pressed;
}

static void init_graphics(void) {
    g_gs = gsKit_init_global();
    if (!g_gs) return;

    g_gs->Mode = GS_MODE_DTV_1080I;
    g_gs->Interlace = GS_INTERLACED;
    g_gs->Field = GS_FIELD;
    g_gs->Width = VIDEO_W;
    g_gs->Height = VIDEO_H;
    g_gs->PSM = GS_PSM_CT16;
    g_gs->PSMZ = GS_PSMZ_16S;
    g_gs->ZBuffering = GS_SETTING_OFF;
    g_gs->DoubleBuffering = GS_SETTING_ON;
    g_gs->Dithering = GS_SETTING_ON;
    g_gs->PrimAlphaEnable = GS_SETTING_OFF;
    g_gs->PrimAAEnable = GS_SETTING_OFF;

    dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF,D_CTRL_STS_UNSPEC,D_CTRL_STD_OFF,D_CTRL_RCYC_8,1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gsKit_vram_clear(g_gs);
    gsKit_init_screen(g_gs);
    gsKit_set_test(g_gs,GS_ATEST_OFF);
    gsKit_set_test(g_gs,GS_ZTEST_OFF);
    gsKit_set_clamp(g_gs,GS_CMODE_CLAMP);
    gsKit_mode_switch(g_gs,GS_ONESHOT);

    g_screen_h = VIDEO_H;
    g_xscale = (float)VIDEO_W / (float)LOGICAL_W;
    g_yscale = (float)VIDEO_H / (float)LOGICAL_H;
}


static int is_red_suit(int suit) { return suit == SUIT_HEARTS || suit == SUIT_DIAMONDS; }

static int rank_strength(const Card *c) {
    return c->rank == 255 ? 100 + (int)c->joker : (int)c->rank;
}

static void swap_card(Card *a,Card *b) { Card t=*a;*a=*b;*b=t; }

static int card_sort_cmp(const Card *a,const Card *b) {
    if (a->rank == 255 && b->rank != 255) return 1;
    if (b->rank == 255 && a->rank != 255) return -1;
    if (a->suit != b->suit) return (int)a->suit - (int)b->suit;
    return rank_strength(a)-rank_strength(b);
}

static void sort_human(void) {
    int i,j;
    for (i=0;i<human_count;i++)
        for (j=i+1;j<human_count;j++)
            if (card_sort_cmp(&human[i],&human[j]) > 0) swap_card(&human[i],&human[j]);
}

static int rules_jokers(void) { if(g_ruleset==RULE_ONE_JOKER)return 1; if(g_ruleset==RULE_TWO_JOKERS)return 2; if(g_ruleset==RULE_CUSTOM)return g_custom_jokers; return 0; }
static void make_deck(void) {
    int s,r,i,j,low=(g_ruleset==RULE_ORIGINAL)?7:(g_ruleset==RULE_CUSTOM?g_custom_low_rank:0),jokers=rules_jokers();
    deck_count=0;
    for(s=0;s<4;s++) for(r=low;r<13;r++) { deck[deck_count].rank=r; deck[deck_count].suit=s; deck[deck_count].joker=0; deck_count++; }
    for(i=0;i<jokers;i++){ deck[deck_count].rank=255; deck[deck_count].suit=SUIT_JOKER; deck[deck_count].joker=i+1; deck_count++; }
    for(i=deck_count-1;i>0;i--){ j=rand()%(i+1); swap_card(&deck[i],&deck[j]); }
}

static Card deck_pop(void) { return deck[--deck_count]; }

static int hand_has_suit(const Card *cards,int count,int suit) {
    int i;
    for (i=0;i<count;i++) if (cards[i].rank != 255 && cards[i].suit == suit) return 1;
    return 0;
}

static int human_card_legal(int index) {
    int lead;
    Card *c;
    if (index < 0 || index >= human_count) return 0;
    if (trick_count == 0) return 1;
    c=&human[index];
    lead=trick[0].card.suit;
    if (c->rank == 255 || c->suit == lead) return 1;
    return !hand_has_suit(human,human_count,lead);
}

static int card_strength(const Card *c,int lead) {
    int v;
    if (c->rank == 255) return 1000 + (int)c->joker * 20;
    v=(int)c->rank;
    if (trump != SUIT_NT && c->suit == trump) return 500+v+(c->rank==9?25:0); /* J index = 9 */
    if (c->suit == lead) return 200+v;
    return v;
}

static void remove_card(Card *cards,int *count,int index) {
    int i;
    for (i=index;i<(*count)-1;i++) cards[i]=cards[i+1];
    (*count)--;
}
static int discard_priority(const Card *c,int trump_suit) {
    if (c->rank == 255) return 10000 + c->joker * 100;
    return (c->suit == trump_suit ? 500 : 0) + (int)c->rank;
}

static void take_widow_and_auto_discard(Card *cards,int *count,int owner) {
    int i, j, best;
    char b[64];
    for (i=0; i<widow_count && *count < MAX_HAND; ++i) cards[(*count)++] = widow[i];
    snprintf(b,sizeof(b),"%s CLAIMS WIDOW %d", owner==PLAYER?"YOU":"CPU", widow_count);
    log_line(b);
    while (*count > 13) {
        best = 0;
        for (i=1; i<*count; ++i) {
            if (discard_priority(&cards[i], trump) < discard_priority(&cards[best], trump)) best = i;
        }
        snprintf(b,sizeof(b),"%s DISCARD %s %s", owner==PLAYER?"YOU":"CPU", card_rank_label(&cards[best]),
                 cards[best].rank==255?"JOKER":suit_names[cards[best].suit]);
        log_line(b);
        remove_card(cards,count,best);
    }
    widow_count = 0;
    if (owner==PLAYER) sort_human();
    else {
        for (i=0;i<*count;i++)
            for (j=i+1;j<*count;j++)
                if (card_sort_cmp(&cards[i],&cards[j]) > 0) swap_card(&cards[i],&cards[j]);
    }
}


static int settle_chips(int winner) {
    int loser = winner==PLAYER?CPU:PLAYER;
    int paid = trick_stake;
    if (paid > chip_bank[loser]) paid = chip_bank[loser];
    if (paid < 0) paid = 0;
    chip_bank[loser] -= paid;
    chip_bank[winner] += paid;
    return paid;
}

static void finish_hand(void);
static void cpu_play(void);

static void resolve_trick(void) {
    int lead,winner;
    char b[64];
    if (trick_count < 2) return;
    lead=trick[0].card.suit;
    winner = card_strength(&trick[0].card,lead) >= card_strength(&trick[1].card,lead)
           ? trick[0].player : trick[1].player;
    tricks_won[winner]++;
    result_winner=winner;
    result_transfer=settle_chips(winner);
    pending_leader=winner;
    snprintf(b,sizeof(b),"%s WINS TRICK +%d CHIPS",winner==PLAYER?"YOU":"CPU",result_transfer);
    log_line(b);
    sound_beep(winner==PLAYER?880:260,100,winner==PLAYER?7500:5500);
    trick_count=0;
    phase=PHASE_RESULT;
    result_timer=180; /* ~3 sec at 60Hz, ~3.6 sec PAL */
}

static void cpu_play(void) {
    int legal[MAX_HAND],n=0,i,lead,has,cidx;
    Card c;
    if (phase != PHASE_PLAY || current_player != CPU || cpu_count <= 0) return;
    lead = trick_count ? trick[0].card.suit : -1;
    has = (lead >= 0) ? hand_has_suit(cpu,cpu_count,lead) : 0;
    for (i=0;i<cpu_count;i++) {
        if (lead < 0 || cpu[i].rank==255 || cpu[i].suit==lead || !has) legal[n++]=i;
    }
    if (n <= 0) { current_player=PLAYER; return; }
    cidx=legal[rand()%n];
    c=cpu[cidx];
    remove_card(cpu,&cpu_count,cidx);
    trick[trick_count].player=CPU;
    trick[trick_count].card=c;
    trick_count++;
    sound_beep(420,35,3500);
    if (trick_count==2) resolve_trick();
    else current_player=PLAYER;
}

static void play_human_selected(void) {
    Card c;
    if (phase!=PHASE_PLAY || current_player!=PLAYER || human_count<=0) return;
    if (selected_card >= human_count) selected_card=human_count-1;
    if (!human_card_legal(selected_card)) { sound_beep(130,45,3500); return; }
    c=human[selected_card];
    remove_card(human,&human_count,selected_card);
    if (selected_card>=human_count && human_count>0) selected_card=human_count-1;
    trick[trick_count].player=PLAYER;
    trick[trick_count].card=c;
    trick_count++;
    sound_beep(560,35,3500);
    if (trick_count==2) resolve_trick();
    else { current_player=CPU; cpu_play(); }
}

static void submit_bid(int pass) {
    char b[64];
    if (phase != PHASE_BIDDING) return;
    if (pass) {
        contract_level=7;
        contract_suit=SUIT_SPADES;
        contract_by=CPU;
        trump=SUIT_SPADES;
        log_line("YOU PASS - CPU 7 SPADES");
        take_widow_and_auto_discard(cpu,&cpu_count,CPU);
        current_player=CPU;
    } else {
        contract_level=bid_level;
        contract_suit=bid_suit;
        contract_by=PLAYER;
        trump=bid_suit;
        snprintf(b,sizeof(b),"YOU BID %d %s",contract_level,suit_names[contract_suit]);
        log_line(b);
        take_widow_and_auto_discard(human,&human_count,PLAYER);
        current_player=PLAYER;
    }
    trick_stake=contract_level;
    snprintf(b,sizeof(b),"STAKE %d CHIPS PER TRICK",trick_stake);
    log_line(b);
    phase=PHASE_PLAY;
    sound_beep(700,70,5000);
}

static void deal_hand(void) {
    int i;
    char b[64];
    make_deck();
    human_count=cpu_count=0;
    widow_count=0;
    trick_count=0;
    tricks_won[0]=tricks_won[1]=0;
    selected_card=0;
    bid_level=(g_ruleset==RULE_CUSTOM)?g_custom_min_bid:7;
    bid_suit=SUIT_SPADES;
    contract_level=0;
    contract_by=PLAYER;
    trump=SUIT_SPADES;
    trick_stake=0;
    current_player=PLAYER;
    match_winner=-1;
    { int hc=(g_ruleset==RULE_ORIGINAL)?10:(g_ruleset==RULE_CUSTOM?g_custom_hand_size:13);
      int kc=(g_ruleset==RULE_CUSTOM)?g_custom_kitty_size:2;
      for (i=0;i<hc && deck_count>=2;i++) { human[human_count++]=deck_pop(); cpu[cpu_count++]=deck_pop(); }
      for (i=0;i<kc && deck_count>0;i++) widow[widow_count++]=deck_pop(); }
    sort_human();
    phase=PHASE_BIDDING;
    snprintf(b,sizeof(b),"HAND %d - %s",hand_no++,g_rule_names[g_ruleset]);
    log_line(b);
    snprintf(b,sizeof(b),"WIDOW %d CARD%s",widow_count, widow_count==1?"":"S");
    log_line(b);
    sound_beep(620,70,4500);
}

static void reset_match(void) {
    scores[0]=scores[1]=0;
    chip_bank[0]=chip_bank[1]=(g_ruleset==RULE_CUSTOM)?g_custom_start_chips:100;
    hand_no=1;
    log_count=0;
    memset(logs,0,sizeof(logs));
    phase=PHASE_IDLE;
    match_winner=-1;
    log_line("PRESS SQUARE TO DEAL");
}

static void finish_hand(void) {
    char b[64];
    if (contract_by==PLAYER) {
        if (tricks_won[PLAYER]>=contract_level) {
            scores[PLAYER]+=contract_level*10;
            snprintf(b,sizeof(b),"CONTRACT MADE +%d",contract_level*10);
        } else {
            scores[PLAYER]-=contract_level*10;
            snprintf(b,sizeof(b),"CONTRACT FAILED -%d",contract_level*10);
        }
    } else {
        if (tricks_won[CPU]>=contract_level) {
            scores[CPU]+=contract_level*10;
            snprintf(b,sizeof(b),"CPU CONTRACT +%d",contract_level*10);
        } else {
            scores[CPU]-=contract_level*10;
            snprintf(b,sizeof(b),"CPU CONTRACT FAILED -%d",contract_level*10);
        }
    }
    log_line(b);
    { int target=(g_ruleset==RULE_CUSTOM)?g_custom_target:500;
      if (scores[PLAYER]>=target) match_winner=PLAYER;
      else if (scores[CPU]>=target) match_winner=CPU; }
    phase=PHASE_DONE;
    sound_beep(match_winner==PLAYER?980:340,120,6500);
}

static const char *card_rank_label(const Card *c) {
    if (c->rank==255) return "JOKER";
    return rank_names[c->rank];
}

static const unsigned int suit_bits_spade[32] = {
    0x00000000u,0x00000000u,0x00000000u,0x00000000u,0x00010000u,0x00038000u,0x00038000u,0x0007C000u,
    0x000FE000u,0x001FE000u,0x001FF000u,0x003FF800u,0x007FFC00u,0x00FFFE00u,0x00FFFE00u,0x01FFFF00u,
    0x01FFFF00u,0x01FFFF00u,0x01FFFF00u,0x01FFFF00u,0x01FD7F00u,0x00F97E00u,0x00711C00u,0x00010000u,
    0x00038000u,0x00038000u,0x00000000u,0x00000000u,0x00000000u,0x00000000u,0x00000000u,0x00000000u
};

static const unsigned int suit_bits_heart[32] = {
    0x00000000u,0x00000000u,0x00000000u,0x00000000u,0x00F01E00u,0x03F87F00u,0x03FC7F80u,0x07FEFFC0u,
    0x07FEFFC0u,0x07FFFFC0u,0x07FFFFC0u,0x07FFFFC0u,0x07FFFFC0u,0x07FFFFC0u,0x03FFFF80u,0x01FFFF00u,
    0x01FFFF00u,0x00FFFE00u,0x007FFC00u,0x003FF800u,0x001FF000u,0x001FE000u,0x000FE000u,0x0007C000u,
    0x00038000u,0x00010000u,0x00000000u,0x00000000u,0x00000000u,0x00000000u,0x00000000u,0x00000000u
};

static const unsigned int suit_bits_diamond[32] = {
    0x00000000u,0x00000000u,0x00000000u,0x00000000u,0x00010000u,0x00038000u,0x00078000u,0x0007C000u,
    0x000FE000u,0x001FF000u,0x003FF800u,0x003FF800u,0x007FFC00u,0x00FFFE00u,0x01FFFF00u,0x01FFFF00u,
    0x00FFFE00u,0x007FFC00u,0x007FF800u,0x003FF800u,0x001FF000u,0x000FE000u,0x000FC000u,0x0007C000u,
    0x00038000u,0x00010000u,0x00000000u,0x00000000u,0x00000000u,0x00000000u,0x00000000u,0x00000000u
};

static const unsigned int suit_bits_club[32] = {
    0x00000000u,0x00000000u,0x00000000u,0x00000000u,0x0007C000u,0x000FE000u,0x001FF000u,0x001FF000u,
    0x001FF000u,0x001FF000u,0x001FF000u,0x001FF000u,0x000FE000u,0x007FF800u,0x01FFFF00u,0x03FFFF80u,
    0x03FFFF80u,0x07FFFFC0u,0x07FFFFC0u,0x07FFFFC0u,0x03FFFF80u,0x03FD7F80u,0x01F93E00u,0x00010000u,
    0x00038000u,0x00038000u,0x00000000u,0x00000000u,0x00000000u,0x00000000u,0x00000000u,0x00000000u
};

static const unsigned long long suit64_spade[64] = {
    0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
    0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
    0x0000000300000000ULL,0x0000000300000000ULL,0x0000000FC0000000ULL,0x0000000FC0000000ULL,
    0x0000000FC0000000ULL,0x0000000FC0000000ULL,0x0000003FF0000000ULL,0x0000003FF0000000ULL,
    0x000000FFFC000000ULL,0x000000FFFC000000ULL,0x000003FFFC000000ULL,0x000003FFFC000000ULL,
    0x000003FFFF000000ULL,0x000003FFFF000000ULL,0x00000FFFFFC00000ULL,0x00000FFFFFC00000ULL,
    0x00003FFFFFF00000ULL,0x00003FFFFFF00000ULL,0x0000FFFFFFFC0000ULL,0x0000FFFFFFFC0000ULL,
    0x0000FFFFFFFC0000ULL,0x0000FFFFFFFC0000ULL,0x0003FFFFFFFF0000ULL,0x0003FFFFFFFF0000ULL,
    0x0003FFFFFFFF0000ULL,0x0003FFFFFFFF0000ULL,0x0003FFFFFFFF0000ULL,0x0003FFFFFFFF0000ULL,
    0x0003FFFFFFFF0000ULL,0x0003FFFFFFFF0000ULL,0x0003FFFFFFFF0000ULL,0x0003FFFFFFFF0000ULL,
    0x0003FFF33FFF0000ULL,0x0003FFF33FFF0000ULL,0x0000FFC33FFC0000ULL,0x0000FFC33FFC0000ULL,
    0x00003F0303F00000ULL,0x00003F0303F00000ULL,0x0000000300000000ULL,0x0000000300000000ULL,
    0x0000000FC0000000ULL,0x0000000FC0000000ULL,0x0000000FC0000000ULL,0x0000000FC0000000ULL,
    0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
    0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
    0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
};


static const unsigned long long suit64_heart[64] = {
    0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
    0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
    0x0000FF0003FC0000ULL,0x0000FF0003FC0000ULL,0x000FFFC03FFF0000ULL,0x000FFFC03FFF0000ULL,
    0x000FFFF03FFFC000ULL,0x000FFFF03FFFC000ULL,0x003FFFFCFFFFF000ULL,0x003FFFFCFFFFF000ULL,
    0x003FFFFCFFFFF000ULL,0x003FFFFCFFFFF000ULL,0x003FFFFFFFFFF000ULL,0x003FFFFFFFFFF000ULL,
    0x003FFFFFFFFFF000ULL,0x003FFFFFFFFFF000ULL,0x003FFFFFFFFFF000ULL,0x003FFFFFFFFFF000ULL,
    0x003FFFFFFFFFF000ULL,0x003FFFFFFFFFF000ULL,0x003FFFFFFFFFF000ULL,0x003FFFFFFFFFF000ULL,
    0x000FFFFFFFFFC000ULL,0x000FFFFFFFFFC000ULL,0x0003FFFFFFFF0000ULL,0x0003FFFFFFFF0000ULL,
    0x0003FFFFFFFF0000ULL,0x0003FFFFFFFF0000ULL,0x0000FFFFFFFC0000ULL,0x0000FFFFFFFC0000ULL,
    0x00003FFFFFF00000ULL,0x00003FFFFFF00000ULL,0x00000FFFFFC00000ULL,0x00000FFFFFC00000ULL,
    0x000003FFFF000000ULL,0x000003FFFF000000ULL,0x000003FFFC000000ULL,0x000003FFFC000000ULL,
    0x000000FFFC000000ULL,0x000000FFFC000000ULL,0x0000003FF0000000ULL,0x0000003FF0000000ULL,
    0x0000000FC0000000ULL,0x0000000FC0000000ULL,0x0000000300000000ULL,0x0000000300000000ULL,
    0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
    0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
    0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
};


static const unsigned long long suit64_diamond[64] = {
    0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
    0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
    0x0000000300000000ULL,0x0000000300000000ULL,0x0000000FC0000000ULL,0x0000000FC0000000ULL,
    0x0000003FC0000000ULL,0x0000003FC0000000ULL,0x0000003FF0000000ULL,0x0000003FF0000000ULL,
    0x000000FFFC000000ULL,0x000000FFFC000000ULL,0x000003FFFF000000ULL,0x000003FFFF000000ULL,
    0x00000FFFFFC00000ULL,0x00000FFFFFC00000ULL,0x00000FFFFFC00000ULL,0x00000FFFFFC00000ULL,
    0x00003FFFFFF00000ULL,0x00003FFFFFF00000ULL,0x0000FFFFFFFC0000ULL,0x0000FFFFFFFC0000ULL,
    0x0003FFFFFFFF0000ULL,0x0003FFFFFFFF0000ULL,0x0003FFFFFFFF0000ULL,0x0003FFFFFFFF0000ULL,
    0x0000FFFFFFFC0000ULL,0x0000FFFFFFFC0000ULL,0x00003FFFFFF00000ULL,0x00003FFFFFF00000ULL,
    0x00003FFFFFC00000ULL,0x00003FFFFFC00000ULL,0x00000FFFFFC00000ULL,0x00000FFFFFC00000ULL,
    0x000003FFFF000000ULL,0x000003FFFF000000ULL,0x000000FFFC000000ULL,0x000000FFFC000000ULL,
    0x000000FFF0000000ULL,0x000000FFF0000000ULL,0x0000003FF0000000ULL,0x0000003FF0000000ULL,
    0x0000000FC0000000ULL,0x0000000FC0000000ULL,0x0000000300000000ULL,0x0000000300000000ULL,
    0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
    0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
    0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
};


static const unsigned long long suit64_club[64] = {
    0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
    0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
    0x0000003FF0000000ULL,0x0000003FF0000000ULL,0x000000FFFC000000ULL,0x000000FFFC000000ULL,
    0x000003FFFF000000ULL,0x000003FFFF000000ULL,0x000003FFFF000000ULL,0x000003FFFF000000ULL,
    0x000003FFFF000000ULL,0x000003FFFF000000ULL,0x000003FFFF000000ULL,0x000003FFFF000000ULL,
    0x000003FFFF000000ULL,0x000003FFFF000000ULL,0x000003FFFF000000ULL,0x000003FFFF000000ULL,
    0x000000FFFC000000ULL,0x000000FFFC000000ULL,0x00003FFFFFC00000ULL,0x00003FFFFFC00000ULL,
    0x0003FFFFFFFF0000ULL,0x0003FFFFFFFF0000ULL,0x000FFFFFFFFFC000ULL,0x000FFFFFFFFFC000ULL,
    0x000FFFFFFFFFC000ULL,0x000FFFFFFFFFC000ULL,0x003FFFFFFFFFF000ULL,0x003FFFFFFFFFF000ULL,
    0x003FFFFFFFFFF000ULL,0x003FFFFFFFFFF000ULL,0x003FFFFFFFFFF000ULL,0x003FFFFFFFFFF000ULL,
    0x000FFFFFFFFFC000ULL,0x000FFFFFFFFFC000ULL,0x000FFFF33FFFC000ULL,0x000FFFF33FFFC000ULL,
    0x0003FFC30FFC0000ULL,0x0003FFC30FFC0000ULL,0x0000000300000000ULL,0x0000000300000000ULL,
    0x0000000FC0000000ULL,0x0000000FC0000000ULL,0x0000000FC0000000ULL,0x0000000FC0000000ULL,
    0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
    0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
    0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
};


typedef struct { char c; unsigned short row[24]; } CardGlyph;
static const CardGlyph card_font16x24[] = {
 {'0',{0x0000,0x0000,0x0FE0,0x1CF0,0x3C78,0x787C,0x787C,0xF83C,0xF83E,0xF83E,0xF83E,0xF83E,0xF83E,0xF83E,0xF83C,0x787C,0x787C,0x3C78,0x1CF0,0x0FE0,0x0000,0x0000,0x0000,0x0000}},
 {'1',{0x0000,0x0000,0x07C0,0x0FC0,0x3FC0,0x37C0,0x07C0,0x07C0,0x07C0,0x07C0,0x07C0,0x07C0,0x07C0,0x07C0,0x07C0,0x07C0,0x07C0,0x07C0,0x07C0,0x3FFC,0x0000,0x0000,0x0000,0x0000}},
 {'2',{0x0000,0x0000,0x3FC0,0x71F0,0x60F8,0x60F8,0x00FC,0x00FC,0x00FC,0x00F8,0x00F8,0x01F0,0x03E0,0x0780,0x0F00,0x1C0C,0x780C,0xFFFC,0xFFFC,0xFFFC,0x0000,0x0000,0x0000,0x0000}},
 {'3',{0x0000,0x0000,0x3FE0,0x71F0,0x60F8,0x60FC,0x007C,0x00F8,0x00F8,0x01F0,0x07E0,0x01F8,0x00FC,0x007C,0x007C,0x007C,0xC07C,0xE07C,0xF1F8,0x3FE0,0x0000,0x0000,0x0000,0x0000}},
 {'4',{0x0000,0x0000,0x01F0,0x03F0,0x03F0,0x07F0,0x0FF0,0x0DF0,0x1DF0,0x39F0,0x31F0,0x71F0,0x61F0,0xE1F0,0xFFFE,0x01F0,0x01F0,0x01F0,0x01F0,0x0FFE,0x0000,0x0000,0x0000,0x0000}},
 {'5',{0x0000,0x0000,0x3FF8,0x3FF8,0x3FF8,0x2000,0x2000,0x2000,0x3FC0,0x39F0,0x20F8,0x007C,0x007C,0x007C,0x007C,0x007C,0x607C,0x60F8,0x71F0,0x3FE0,0x0000,0x0000,0x0000,0x0000}},
 {'6',{0x0000,0x0000,0x07F0,0x1E3C,0x3C0C,0x3C00,0x7800,0x7800,0x7FF0,0xFEF8,0xFC7C,0xFC7C,0xFC3C,0xFC3E,0x7C3E,0x7C3C,0x7C7C,0x3C7C,0x1E78,0x0FE0,0x0000,0x0000,0x0000,0x0000}},
 {'7',{0x0000,0x0000,0x7FFC,0x7FFC,0x7FFC,0x601C,0x6038,0x0038,0x0070,0x0070,0x00E0,0x00E0,0x01C0,0x01C0,0x0380,0x0380,0x0300,0x0700,0x0600,0x0E00,0x0000,0x0000,0x0000,0x0000}},
 {'8',{0x0000,0x0000,0x1FE0,0x3CF8,0x7C7C,0x7C7C,0x7C7C,0x7C7C,0x7C78,0x3CF8,0x1FE0,0x3CF8,0x787C,0xF87C,0xF83E,0xF83E,0xF87C,0x787C,0x3CF8,0x1FE0,0x0000,0x0000,0x0000,0x0000}},
 {'9',{0x0000,0x0000,0x1FC0,0x3CF0,0x7C78,0x7878,0xF87C,0xF87C,0xF87C,0xF87C,0xF87C,0x7C7C,0x3CFC,0x1FFC,0x003C,0x007C,0x0078,0x6070,0x71E0,0x3FC0,0x0000,0x0000,0x0000,0x0000}},
 {'A',{0x0000,0x0000,0x03C0,0x03C0,0x03E0,0x07E0,0x07F0,0x0FF0,0x0DF0,0x0CF8,0x18F8,0x18FC,0x187C,0x307C,0x3FFE,0x603E,0x603E,0x601F,0xC01F,0xF07F,0x0000,0x0000,0x0000,0x0000}},
 {'J',{0x03E0,0x03E0,0x03E0,0x03E0,0x03E0,0x03E0,0x03E0,0x03E0,0x03E0,0x03E0,0x03E0,0x03E0,0x03E0,0x03E0,0x03E0,0x03E0,0x03E0,0x03E0,0x03E0,0x63E0,0x73C0,0x3F80,0x0000,0x0000}},
 {'Q',{0x1FF0,0x7C78,0xF83E,0xF01E,0xF01F,0xF01F,0xE00F,0xE00F,0xE00F,0xE00F,0xE00F,0xE00F,0xF01F,0xF01F,0xF01F,0xF83E,0x7C7C,0x1FF0,0x01E0,0x00FC,0x007C,0x003C,0x0000,0x0000}},
 {'K',{0x0000,0x0000,0xFE3F,0xF80C,0xF838,0xF870,0xF8E0,0xF9C0,0xFB80,0xFF80,0xFFC0,0xFFE0,0xFBF0,0xF9F0,0xF8F8,0xF8FC,0xF87E,0xF83F,0xF81F,0xFE0F,0x0000,0x0000,0x0000,0x0000}},
};

static const unsigned int *suit_bitmap_for(int suit) {
    switch (suit) {
        case SUIT_HEARTS: return suit_bits_heart;
        case SUIT_DIAMONDS: return suit_bits_diamond;
        case SUIT_CLUBS: return suit_bits_club;
        case SUIT_SPADES: return suit_bits_spade;
        default: return NULL;
    }
}

static const unsigned long long *suit64_for(int suit) {
    switch (suit) {
        case SUIT_HEARTS: return suit64_heart;
        case SUIT_DIAMONDS: return suit64_diamond;
        case SUIT_CLUBS: return suit64_club;
        case SUIT_SPADES: return suit64_spade;
        default: return NULL;
    }
}

static void draw_suit64_oriented(float x,float y,int suit,float scale,u64 color,int rotated) {
    const unsigned long long *rows = suit64_for(suit);
    int r, c;
    float px = scale;
    if (!rows) return;
    for (r = 0; r < 64; ++r) {
        int span = -1;
        for (c = 0; c < 64; ++c) {
            int sr = rotated ? (63-r) : r;
            int sc = rotated ? (63-c) : c;
            int on = (int)((rows[sr] >> (63-sc)) & 1ULL);
            if (on && span < 0) span = c;
            if ((!on || c == 63) && span >= 0) {
                int end = (on && c == 63) ? 64 : c;
                rect(x + span*px, y + r*px, x + end*px, y + (r+1)*px, 4, color);
                span = -1;
            }
        }
    }
}

#define SUIT_BITMAP_W 32
#define SUIT_BITMAP_H 32

static void draw_suit_bitmap_oriented(float x,float y,int suit,float scale,u64 color,int rotated) {
    const unsigned int *rows = suit_bitmap_for(suit);
    int r, c;
    float px = scale;
    if (!rows) { draw_text_center(x + (SUIT_BITMAP_W*scale)*0.5f, y + 3*scale, "NT", 1.0f, color); return; }
    for (r = 0; r < SUIT_BITMAP_H; ++r) {
        int span = -1;
        for (c = 0; c < SUIT_BITMAP_W; ++c) {
            int sr = rotated ? (SUIT_BITMAP_H-1-r) : r;
            int sc = rotated ? (SUIT_BITMAP_W-1-c) : c;
            int on = (rows[sr] >> ((SUIT_BITMAP_W-1)-sc)) & 1u;
            if (on && span < 0) span = c;
            if ((!on || c == SUIT_BITMAP_W-1) && span >= 0) {
                int end = (on && c == SUIT_BITMAP_W-1) ? SUIT_BITMAP_W : c;
                rect(x + span*px, y + r*px, x + end*px, y + (r+1)*px, 4, color);
                span = -1;
            }
        }
    }
}

static void draw_suit_bitmap(float x,float y,int suit,float scale,u64 color) {
    draw_suit_bitmap_oriented(x,y,suit,scale,color,0);
}

static void draw_suit_icon(float cx,float cy,int suit,float scale,u64 color) {
    if (suit == SUIT_NT) {
        draw_text_center(cx, cy-4, "NT", 1.0f, color);
        return;
    }
    draw_suit_bitmap(cx, cy, suit, scale, color);
}

#define CARD_GLYPH_W 16
#define CARD_GLYPH_H 24

static const unsigned short *card_glyph_rows(char c) {
    unsigned int i;
    for(i=0;i<sizeof(card_font16x24)/sizeof(card_font16x24[0]);++i)
        if(card_font16x24[i].c==c) return card_font16x24[i].row;
    return card_font16x24[0].row;
}

static float card_rank_width_hi(const char *s,float scale) {
    int n=(int)strlen(s);
    return n>0 ? (float)(n*34-2)*scale : 0.0f;
}

/* 501B doubles the 16x24 source glyph in software to a 32x48 raster. */
static void draw_card_rank_text_hi(float x,float y,const char *s,float scale,u64 color,int rotated) {
    int n=(int)strlen(s),gi,r,c;
    for(gi=0;gi<n;++gi) {
        char ch=rotated?s[n-1-gi]:s[gi];
        const unsigned short *rows=card_glyph_rows(ch);
        for(r=0;r<48;++r) {
            int span=-1;
            for(c=0;c<32;++c) {
                int sr0=r>>1;
                int sr=(rotated & 1)?(23-sr0):sr0;
                int sc0=c>>1;
                int sc=rotated?(15-sc0):sc0;
                int on=(rows[sr]>>((CARD_GLYPH_W-1)-sc))&1;
                if(on && span<0) span=c;
                if((!on || c==31) && span>=0) {
                    int end=(on && c==31)?32:c;
                    rect(x+(gi*34+span)*scale,y+r*scale,
                         x+(gi*34+end)*scale,y+(r+1)*scale,5,color);
                    span=-1;
                }
            }
        }
    }
}

typedef struct {
    float x, y;
    unsigned char rotated;
} PipPos;

typedef struct {
    unsigned char count;
    PipPos p[10];
} PipLayout;

/* Traditional 2..10 centre-field pip layouts.

   IMPORTANT: the small suit marks beside the rank in the two corner indices
   are INDEX SYMBOLS only.  They do NOT count toward the card value.  The
   centre field therefore always draws exactly VALUE pips:

       2 -> 2 body pips  + two separate corner index suits
       3 -> 3 body pips  + two separate corner index suits
       ...
      10 -> 10 body pips + two separate corner index suits

   Pips in the upper half are upright. Pips in the lower half are rotated
   180 degrees, matching a conventional two-way playing-card layout. */
static const PipLayout pip_layouts[9] = {
    /* Positions measured against the supplied full-deck reference. */
    /* 2 */
    {2, {{.50f,.23f,0},{.50f,.77f,1}}},

    /* 3 */
    {3, {{.50f,.21f,0},{.50f,.50f,0},{.50f,.79f,1}}},

    /* 4 */
    {4, {{.31f,.23f,0},{.69f,.23f,0},
         {.31f,.77f,1},{.69f,.77f,1}}},

    /* 5 */
    {5, {{.31f,.22f,0},{.69f,.22f,0},{.50f,.50f,0},
         {.31f,.78f,1},{.69f,.78f,1}}},

    /* 6 */
    {6, {{.31f,.19f,0},{.69f,.19f,0},
         {.31f,.50f,0},{.69f,.50f,0},
         {.31f,.81f,1},{.69f,.81f,1}}},

    /* 7 = six-pip layout plus upper-centre pip */
    {7, {{.31f,.18f,0},{.69f,.18f,0},{.50f,.34f,0},
         {.31f,.50f,0},{.69f,.50f,0},
         {.31f,.82f,1},{.69f,.82f,1}}},

    /* 8 = seven plus matching lower-centre inverted pip */
    {8, {{.31f,.17f,0},{.69f,.17f,0},{.50f,.33f,0},
         {.31f,.50f,0},{.69f,.50f,0},{.50f,.67f,1},
         {.31f,.83f,1},{.69f,.83f,1}}},

    /* 9 = four side rows plus one upright centre pip */
    {9, {{.31f,.16f,0},{.69f,.16f,0},
         {.31f,.38f,0},{.69f,.38f,0},{.50f,.50f,0},
         {.31f,.62f,1},{.69f,.62f,1},
         {.31f,.84f,1},{.69f,.84f,1}}},

    /* 10 = four side rows plus an upright/inverted centre pair */
    {10,{{.31f,.15f,0},{.69f,.15f,0},{.50f,.29f,0},
         {.31f,.39f,0},{.69f,.39f,0},
         {.31f,.61f,1},{.69f,.61f,1},{.50f,.71f,1},
         {.31f,.85f,1},{.69f,.85f,1}}}
};

static void draw_small_pip(float cx,float cy,int suit,float scale,u64 color,int rotated) {
    draw_suit64_oriented(cx - scale*16.0f, cy - scale*16.0f, suit, scale*0.5f, color, rotated);
}

static void draw_number_pips(float x,float y,float w,float h,int rank,int suit,u64 color) {
    const PipLayout *layout;
    float pip_scale;
    int i;
    if (rank < 0 || rank > 8) return; /* rank indices 0..8 = 2..10 */
    layout=&pip_layouts[rank];

    /* Scale the centre pips with card width so played cards get large,
       readable symbols while the fanned hand still remains legible. */
    pip_scale = w / 106.6667f;
    if (pip_scale < 0.49f) pip_scale = 0.49f;
    if (pip_scale > 0.83f) pip_scale = 0.83f;

    for (i=0;i<(int)layout->count;i++)
        draw_small_pip(x+w*layout->p[i].x,y+h*layout->p[i].y,
                       suit,pip_scale,color,layout->p[i].rotated);
}

static int rank_needs_orientation_mark(const Card *c) {
    return c && (c->rank==4 || c->rank==7); /* rank indices: 6 and 9 */
}

static __attribute__((noinline)) void draw_corner_indices(float x,float y,float w,float h,const Card *c,u64 color) {
    const char *label=card_rank_label(c);
    const float rank_scale=0.166f;
    const float suit_scale=0.078f;
    const float rank_h=48.0f*rank_scale;
    const float suit_wh=64.0f*suit_scale;
    float rw=card_rank_width_hi(label,rank_scale);
    float bx=x+w-rw-5.0f;
    float by=y+h-rank_h-5.0f;

    draw_card_rank_text_hi(x+5.0f,y+3.0f,label,rank_scale,color,0);
    draw_suit64_oriented(x+5.0f,y+4.0f+rank_h+1.0f,c->suit,suit_scale,color,0);

    draw_card_rank_text_hi(bx,by,label,rank_scale,color,1);
    draw_suit64_oriented(x+w-5.0f-suit_wh,by-1.0f-suit_wh,c->suit,suit_scale,color,1);

    if (rank_needs_orientation_mark(c)) {
        float markw=rw>4.0f?rw:4.0f;
        rect(x+5.0f,y+3.0f+rank_h+0.5f,x+5.0f+markw,y+4.0f+rank_h,5,color);
        rect(bx,by-2.0f,bx+markw,by-1.0f,5,color);
    }
}

static void draw_court_portrait(float x,float y,float w,float h,int rank,int suit) {
    u64 red=rgba8(184,42,39,0x80), blue=rgba8(42,77,145,0x80);
    u64 gold=rgba8(214,171,68,0x80), skin=rgba8(232,190,137,0x80);
    u64 dark=rgba8(30,26,22,0x80);
    float cx=x+w*.50f, cy=y+h*.51f;
    /* old playing-card style mirrored portrait panel */
    fill_vgrad(x+w*.22f,y+h*.22f,w*.56f,h*.56f,238,221,176,204,166,88,8);
    frame(x+w*.22f,y+h*.22f,w*.56f,h*.56f,1,gold);
    rect(cx-w*.08f,cy-h*.20f,cx+w*.08f,cy-h*.08f,4,skin);
    rect(cx-w*.11f,cy-h*.24f,cx+w*.11f,cy-h*.19f,4,gold);
    rect(cx-w*.05f,cy-h*.14f,cx+w*.05f,cy-h*.12f,5,dark);
    rect(cx-w*.16f,cy-h*.07f,cx+w*.16f,cy+h*.12f,4,(suit==SUIT_HEARTS||suit==SUIT_DIAMONDS)?red:blue);
    rect(cx-w*.13f,cy+h*.12f,cx+w*.13f,cy+h*.22f,4,gold);
    rect(cx-w*.05f,cy+h*.04f,cx+w*.05f,cy+h*.18f,5,rgba8(244,238,220,0x80));
    if(rank==9) { /* jack */
        rect(cx+w*.11f,cy-h*.10f,cx+w*.14f,cy+h*.20f,5,dark);
        rect(cx+w*.08f,cy-h*.12f,cx+w*.18f,cy-h*.08f,5,gold);
    } else if(rank==10) { /* queen */
        rect(cx-w*.13f,cy-h*.28f,cx+w*.13f,cy-h*.23f,5,gold);
        rect(cx-w*.08f,cy-h*.32f,cx-w*.04f,cy-h*.27f,5,gold);
        rect(cx,cy-h*.34f,cx+w*.04f,cy-h*.27f,5,gold);
        rect(cx+w*.08f,cy-h*.32f,cx+w*.12f,cy-h*.27f,5,gold);
    } else { /* king */
        rect(cx-w*.15f,cy-h*.30f,cx+w*.15f,cy-h*.24f,5,gold);
        rect(cx-w*.12f,cy-h*.36f,cx-w*.06f,cy-h*.28f,5,gold);
        rect(cx-w*.03f,cy-h*.38f,cx+w*.03f,cy-h*.28f,5,gold);
        rect(cx+w*.06f,cy-h*.36f,cx+w*.12f,cy-h*.28f,5,gold);
    }
    draw_suit_icon(cx-w*.10f,cy-h*.08f,suit,0.4875f,rgba8(248,240,214,0x80));
}

static void draw_joker_portrait(float x,float y,float w,float h) {
    u64 red=rgba8(198,43,38,0x80), blue=rgba8(38,83,155,0x80), gold=rgba8(222,179,70,0x80);
    u64 skin=rgba8(236,193,141,0x80), dark=rgba8(28,26,24,0x80);
    float cx=x+w*.50f, cy=y+h*.53f;
    rect(cx-w*.09f,cy-h*.17f,cx+w*.09f,cy-h*.05f,5,skin);
    rect(cx-w*.13f,cy-h*.22f,cx-w*.01f,cy-h*.17f,5,red);
    rect(cx+w*.01f,cy-h*.22f,cx+w*.13f,cy-h*.17f,5,blue);
    rect(cx-w*.16f,cy-h*.27f,cx-w*.08f,cy-h*.21f,5,red);
    rect(cx+w*.08f,cy-h*.27f,cx+w*.16f,cy-h*.21f,5,blue);
    rect(cx-w*.18f,cy-h*.29f,cx-w*.14f,cy-h*.25f,6,gold);
    rect(cx+w*.14f,cy-h*.29f,cx+w*.18f,cy-h*.25f,6,gold);
    rect(cx-w*.04f,cy-h*.11f,cx-.01f,cy-h*.09f,6,dark);
    rect(cx+.01f,cy-h*.11f,cx+w*.04f,cy-h*.09f,6,dark);
    rect(cx-w*.03f,cy-h*.04f,cx+w*.04f,cy-h*.02f,6,red);
    rect(cx-w*.17f,cy,cx+w*.17f,cy+h*.20f,5,gold);
    rect(cx-w*.13f,cy+h*.03f,cx-w*.01f,cy+h*.18f,6,red);
    rect(cx+w*.01f,cy+h*.03f,cx+w*.13f,cy+h*.18f,6,blue);
    rect(cx-w*.03f,cy+h*.02f,cx+w*.03f,cy+h*.19f,7,rgba8(244,239,220,0x80));
}

static void draw_joker_bw_portrait(float x,float y,float w,float h) {
    u64 dark=rgba8(28,28,28,0x80), mid_dark=rgba8(70,70,70,0x80);
    u64 mid=rgba8(118,118,118,0x80), light=rgba8(188,188,188,0x80);
    u64 skin=rgba8(214,214,214,0x80), pale=rgba8(242,242,238,0x80);
    float cx=x+w*.50f, cy=y+h*.53f;
    rect(cx-w*.09f,cy-h*.17f,cx+w*.09f,cy-h*.05f,5,skin);
    rect(cx-w*.13f,cy-h*.22f,cx-w*.01f,cy-h*.17f,5,mid_dark);
    rect(cx+w*.01f,cy-h*.22f,cx+w*.13f,cy-h*.17f,5,light);
    rect(cx-w*.16f,cy-h*.27f,cx-w*.08f,cy-h*.21f,5,dark);
    rect(cx+w*.08f,cy-h*.27f,cx+w*.16f,cy-h*.21f,5,mid);
    rect(cx-w*.18f,cy-h*.29f,cx-w*.14f,cy-h*.25f,6,light);
    rect(cx+w*.14f,cy-h*.29f,cx+w*.18f,cy-h*.25f,6,light);
    rect(cx-w*.04f,cy-h*.11f,cx-.01f,cy-h*.09f,6,dark);
    rect(cx+.01f,cy-h*.11f,cx+w*.04f,cy-h*.09f,6,dark);
    rect(cx-w*.03f,cy-h*.04f,cx+w*.04f,cy-h*.02f,6,mid_dark);
    rect(cx-w*.17f,cy,cx+w*.17f,cy+h*.20f,5,light);
    rect(cx-w*.13f,cy+h*.03f,cx-w*.01f,cy+h*.18f,6,dark);
    rect(cx+w*.01f,cy+h*.03f,cx+w*.13f,cy+h*.18f,6,mid);
    rect(cx-w*.03f,cy+h*.02f,cx+w*.03f,cy+h*.19f,7,pale);
}

static void draw_back_ornament(float x,float y,float w,float h) {
    int i;
    u64 cream=rgba8(250,235,218,0x80), ruby=rgba8(150,23,28,0x80), ruby2=rgba8(101,13,19,0x80);
    fill_vgrad(x+5,y+5,w-10,h-10,183,45,43,111,14,20,14);
    frame(x+5,y+5,w-10,h-10,1,cream);
    frame(x+9,y+9,w-18,h-18,1,rgba8(232,202,180,0x80));
    frame(x+13,y+13,w-26,h-26,1,ruby2);
    for(i=0;i<7;i++) {
        float yy=y+16+i*(h-32)/6.0f;
        rect(x+16,yy,x+w-16,yy+1,4,rgba8(226,184,165,0x80));
    }
    for(i=0;i<5;i++) {
        float xx=x+18+i*(w-36)/4.0f;
        rect(xx,y+16,xx+1,y+h-16,4,rgba8(214,159,147,0x80));
    }
    /* centre rosette */
    rect(x+w*.34f,y+h*.34f,x+w*.66f,y+h*.66f,5,ruby2);
    frame(x+w*.34f,y+h*.34f,w*.32f,h*.32f,1,cream);
    rect(x+w*.41f,y+h*.41f,x+w*.59f,y+h*.59f,6,ruby);
    draw_suit_icon(x+w*.44f,y+h*.40f,SUIT_DIAMONDS,0.60f,cream);
    draw_text_center(x+w/2,y+h*.71f,"500",.78f,cream);
}

static void draw_card(float x,float y,float w,float h,const Card *c,int selected,int back) {
    u64 suit_color;
    rect(x+4,y+5,x+w+6,y+h+7,1,rgba8(16,14,12,0x80));
    if(selected) {
        frame(x-4,y-4,w+8,h+8,2,rgba8(250,221,116,0x80));
        frame(x-2,y-2,w+4,h+4,1,rgba8(134,94,26,0x80));
    }

    /* 500W+ court fast path, retained by original 501A/501B.
       The embedded J/Q/K assets are full-card composites: draw them over the
       normal card surface, then restore procedural corner indices. */
    if(!back && c->rank>=9 && c->rank<=11 && g_art_tex_ok) {
        int court_id=((int)c->rank-9)*4+(int)c->suit;
        GSTEXTURE *ct=special_texture_get(court_id);
        fill_vgrad(x,y,w,h,250,248,242,236,229,214,10);
        if(g_art_tex_ok) tile_texture(&g_tex_cardgrain,x,y,w,h,32,32,3);
        frame(x,y,w,h,1,rgba8(183,169,140,0x80));
        frame(x+1,y+1,w-2,h-2,1,rgba8(255,252,246,0x80));
        if(ct) tex_sprite(ct,x,y,w,h,4);
        suit_color=is_red_suit(c->suit)?rgba8(190,30,34,0x80):rgba8(15,16,17,0x80);
        draw_corner_indices(x,y,w,h,c,suit_color);
        return;
    }

    fill_vgrad(x,y,w,h,250,248,242,236,229,214,10);
    if(g_art_tex_ok) tile_texture(&g_tex_cardgrain,x,y,w,h,32,32,3);
    frame(x,y,w,h,1,rgba8(183,169,140,0x80));
    frame(x+1,y+1,w-2,h-2,1,rgba8(255,252,246,0x80));

    if(back) {
        if(g_art_tex_ok) tex_sprite(&g_tex_cardback,x+4,y+4,w-8,h-8,4);
        else draw_back_ornament(x,y,w,h);
        frame(x+4,y+4,w-8,h-8,1,rgba8(250,237,222,0x80));
        return;
    }

    if(c->rank==255) {
        GSTEXTURE *jt=NULL;
        if(c->joker==1) {
            if(g_art_tex_ok) jt=special_texture_get(12);
            if(jt) { tex_sprite(jt,x,y,w,h,4); return; }
            draw_joker_bw_portrait(x,y,w,h);
        } else {
            if(g_art_tex_ok) jt=special_texture_get(13);
            if(jt) { tex_sprite(jt,x,y,w,h,4); return; }
            draw_joker_portrait(x,y,w,h);
        }
        return;
    }

    suit_color=is_red_suit(c->suit)?rgba8(190,30,34,0x80):rgba8(15,16,17,0x80);

    draw_corner_indices(x,y,w,h,c,suit_color);

    if(c->rank<=8) { /* 2 through 10: centre pips equal value; corner suits are indices */
        draw_number_pips(x,y,w,h,(int)c->rank,c->suit,suit_color);
    } else if(c->rank>=9 && c->rank<=11) { /* Jack, Queen, King */
        GSTEXTURE *ct=NULL;
        if(g_art_tex_ok) {
            int court_id=((int)c->rank-9)*4+(int)c->suit;
            ct=special_texture_get(court_id);
        }
        if(ct) {
            tex_sprite(ct,x+w*.13f,y+h*.08f,w*.74f,h*.84f,4);
            frame(x+w*.13f,y+h*.08f,w*.74f,h*.84f,1,rgba8(178,150,90,0x80));
        } else draw_court_portrait(x,y,w,h,(int)c->rank,c->suit);
    } else { /* Ace: one clean, large central suit pip */
        float ace_scale=w/56.0f;
        if(ace_scale<0.86f) ace_scale=0.86f;
        if(ace_scale>1.31f) ace_scale=1.31f;
        draw_suit64_oriented(x+w*.50f-ace_scale*16.0f,
                             y+h*.50f-ace_scale*16.0f,
                             c->suit,ace_scale*0.5f,suit_color,0);
    }
}

static int chip_stack_amounts(int amount,int *out,int cap) {
    int n=0;
    while (amount>0 && n<cap) {
        out[n++]=amount>20?20:amount;
        amount-=20;
    }
    return n;
}

static void draw_chip_stack_visual(float sx,float stack_base,int count,u64 chip,u64 dark) {
    int j;
    float width=32.0f;
    float lip=13.0f;
    float top=stack_base-8.0f-(count>1?(count-1)*2.8f:0.0f);
    if(g_art_tex_ok) {
        float u1=(chip==COL_RED)?0.0f:32.0f;
        float u2=u1+32.0f;
        tex_sprite_uv(&g_tex_chips,sx,top+lip,width,stack_base-top-lip+2,u1,0,u2,32,4);
        tex_sprite_uv(&g_tex_chips,sx-1,top-2,width+2,lip+8,u1,0,u2,32,6);
    } else {
        rect(sx,top+lip,sx+width,stack_base+2,4,dark);
        rect(sx+2,top+lip+2,sx+width-2,stack_base,5,chip);
    }
    frame(sx,top+lip,width,stack_base-top-lip+2,1,dark);
    for(j=5;j<count;j+=5) {
        float yy=stack_base-j*2.8f;
        rect(sx+4,yy,sx+width-4,yy+1,7,rgba8(249,247,240,0x80));
    }
}

static void draw_chip_bank(float x,float base_y,const char *title,int amount,u64 chip,u64 dark) {
    int stacks[10],n,i;
    const int max_cols=5;
    const float col_step=36.0f;
    const float row_step=58.0f;
    char b[32];
    draw_text(x,base_y-112,title,.95f,rgba8(242,205,96,0x80));
    n=chip_stack_amounts(amount,stacks,10);
    for (i=0;i<n;i++) {
        int col=i%max_cols;
        int row=i/max_cols;
        float sx=x+col*col_step;
        float yb=base_y-row*row_step;
        draw_chip_stack_visual(sx,yb,stacks[i],chip,dark);
        snprintf(b,sizeof(b),"%d",stacks[i]);
        draw_text_center(sx+16,yb+10,b,.66f,COL_WHITE);
    }
    snprintf(b,sizeof(b),"TOTAL %d",amount);
    draw_text(x,base_y+22,b,.80f,COL_WHITE);
}

static void draw_oval_logo(void) {
    float x=295,y=155,w=370,h=176;
    frame(x,y,w,h,1,rgba8(202,161,73,0x80));
    frame(x+4,y+4,w-8,h-8,1,rgba8(155,117,46,0x80));
    draw_text_center(x+w/2+2,y+34,"500",5.5f,rgba8(101,76,28,0x80));
    draw_text_center(x+w/2,y+32,"500",5.5f,rgba8(231,194,105,0x80));
    draw_text_center(x+w/2,y+96,"FIVE HUNDRED",1.35f,rgba8(225,188,94,0x80));
    draw_suit_icon(x+w/2-94,y+112,SUIT_SPADES,1.05f,rgba8(211,177,90,0x80));
    draw_suit_icon(x+w/2-30,y+112,SUIT_HEARTS,1.05f,rgba8(190,45,40,0x80));
    draw_suit_icon(x+w/2+34,y+112,SUIT_DIAMONDS,1.05f,rgba8(198,48,40,0x80));
    draw_suit_icon(x+w/2+98,y+112,SUIT_CLUBS,1.05f,rgba8(211,177,90,0x80));
}

static void draw_played_trick(void) {
    int i;
    float w=82,h=118,step=90;
    float start=480.0f - ((trick_count-1)*step + w)/2.0f;
    for (i=0;i<trick_count;i++) {
        float x=start+i*step;
        float y=190.0f + i*10.0f;
        draw_card(x,y,w,h,&trick[i].card,0,0);
        draw_text_center(x+w/2,y+h+8,trick[i].player==PLAYER?"YOU":"CPU",.78f,COL_WHITE);
    }
}

static void draw_cpu_hand(void) {
    int i;
    float w=54.0f,h=80.0f;
    float step = cpu_count>1 ? ((760.0f - w) / (float)(cpu_count-1)) : w;
    if (step > 36.0f) step = 36.0f;
    float start=480.0f-(cpu_count-1)*step/2.0f-w/2.0f;
    for (i=0;i<cpu_count;i++) draw_card(start+i*step,62.0f,w,h,NULL,0,1);
}

static void draw_human_hand(void) {
    int i;
    float w=68.0f,h=104.0f;
    float step = human_count>1 ? ((760.0f - w) / (float)(human_count-1)) : w;
    if (step > 45.0f) step = 45.0f;
    float start=480.0f-(human_count-1)*step/2.0f-w/2.0f;
    for (i=0;i<human_count;i++) {
        int sel=(phase==PHASE_PLAY && current_player==PLAYER && i==selected_card);
        float yy=sel?386.0f:398.0f;
        draw_card(start+i*step,yy,w,h,&human[i],sel,0);
        if (sel && !human_card_legal(i))
            draw_text_center(start+i*step+w/2,375,"MUST FOLLOW",.64f,COL_RED);
    }
}

static void draw_score_and_log(void) {
    int i;
    char b[48];
    luxe_panel(18,16,206,102,"SCORE");
    { int target=(g_ruleset==RULE_CUSTOM)?g_custom_target:500;
      snprintf(b,sizeof(b),"YOU %d / %d",scores[PLAYER],target); draw_text(32,44,b,1.05f,rgba8(76,190,255,0x80));
      snprintf(b,sizeof(b),"CPU %d / %d",scores[CPU],target); draw_text(32,66,b,1.05f,rgba8(244,82,80,0x80)); }
    snprintf(b,sizeof(b),"HAND %d",hand_no>1?hand_no-1:1); draw_text(32,88,b,.84f,COL_WHITE);

    luxe_panel(18,130,272,140,"GAME LOG");
    for (i=0;i<log_count;i++) draw_text(28,157+i*18,logs[i],.72f,i==0?COL_WHITE:COL_GRAY);
}

static void draw_trump_panel(void) {
    char b[40];
    luxe_panel(762,16,180,102,"TRUMP");
    if (phase==PHASE_IDLE || contract_level==0) strcpy(b,"-");
    else strncpy(b,suit_names[trump],sizeof(b)-1), b[sizeof(b)-1]=0;
    draw_text_center(852,46,b,.88f,COL_WHITE);
    if (phase!=PHASE_IDLE && contract_level!=0) draw_suit_icon(838,58,trump,1.425f,rgba8(239,209,118,0x80));
}

static void draw_status(void) {
    char b[64];
    luxe_panel(320,12,320,34,NULL);
    if (phase==PHASE_IDLE) strcpy(b,"PRESS SQUARE TO DEAL");
    else if (phase==PHASE_BIDDING) strcpy(b,"YOU - CHOOSE YOUR BID");
    else if (phase==PHASE_PLAY) strcpy(b,current_player==PLAYER?"YOU - YOUR TURN":"CPU - THINKING");
    else if (phase==PHASE_RESULT) strcpy(b,result_winner==PLAYER?"YOU WIN THE TRICK":"CPU WINS THE TRICK");
    else if (match_winner>=0) strcpy(b,match_winner==PLAYER?"YOU WIN THE MATCH":"CPU WINS THE MATCH");
    else strcpy(b,"HAND COMPLETE - START NEXT HAND");
    draw_text_center(480,22,b,.96f,phase==PHASE_PLAY&&current_player==PLAYER?rgba8(92,192,255,0x80):COL_WHITE);
}

static void draw_bidding(void) {
    char b[64];
    int i;
    if (phase != PHASE_BIDDING) return;
    luxe_panel(322,312,316,78,"YOUR BID");
    snprintf(b,sizeof(b),"%d  %s",bid_level,suit_names[bid_suit]);
    draw_text_center(480,338,b,1.18f,rgba8(244,210,110,0x80));
    draw_text_center(480,356,"54 CARD DECK  /  13 TRICKS",.64f,COL_WHITE);
    draw_text_center(480,372,"DPAD CHANGE  X BID  O PASS",.68f,COL_WHITE);
    for (i=0;i<widow_count;i++) draw_card(440 + i*34, 230, 48, 70, NULL, 0, 1);
    if (widow_count>0) draw_text_center(480,218,"WIDOW",.70f,rgba8(244,210,110,0x80));
}

static void draw_result_overlay(void) {
    char b[64];
    if (phase != PHASE_RESULT) return;
    luxe_panel(305,150,350,146,NULL);
    draw_text_center(480,174,result_winner==PLAYER?"YOU WIN THE TRICK":"CPU WINS THE TRICK",1.2f,result_winner==PLAYER?COL_BLUE:COL_RED);
    draw_text_center(480,208,result_winner==PLAYER?"DOG!":"CAT!",2.8f,rgba8(247,214,112,0x80));
    snprintf(b,sizeof(b),"%s +%d CHIPS",result_winner==PLAYER?"YOU":"CPU",result_transfer);
    draw_text_center(480,252,b,1.20f,COL_WHITE);
}

static u32 current_vram_used(void) {
    u32 used;
    if (!g_gs) return 0;
    used = g_gs->CurrentPointer;
    if (used > GS_VRAM_TOTAL_BYTES) used = GS_VRAM_TOTAL_BYTES;
    return used;
}

static void draw_vram_meter(float x,float y,float w) {
    u32 used=current_vram_used();
    u32 used_kb=(used+1023u)/1024u;
    u32 pct10=(u32)(((unsigned long long)used*1000ull)/GS_VRAM_TOTAL_BYTES);
    float ratio=(float)used/(float)GS_VRAM_TOTAL_BYTES;
    float fill=(w-2.0f)*ratio;
    u64 bar;
    char b[64];

    if (pct10 < 750) bar=rgba8(72,210,116,0x80);
    else if (pct10 < 900) bar=rgba8(238,190,70,0x80);
    else bar=rgba8(232,70,62,0x80);

    snprintf(b,sizeof(b),"VRAM %u / 4096 KB  %u.%u%%",
             (unsigned)used_kb,(unsigned)(pct10/10u),(unsigned)(pct10%10u));
    draw_text(x,y,b,.50f,COL_WHITE);

    rect(x,y+8,x+w,y+14,3,rgba8(26,29,28,0x80));
    frame(x,y+8,w,6,1,rgba8(118,91,39,0x80));
    if (fill > 0.0f) rect(x+1,y+9,x+1+fill,y+13,4,bar);
}

static void draw_controls_footer(void) {
    rect(18,FOOTER_TOP,942,536,2,rgba8(10,12,14,0x80));
    rect(18,FOOTER_TOP,942,FOOTER_TOP+1,3,rgba8(136,95,33,0x80));

    draw_text(28,518,"START PAUSE  SELECT OPTIONS  SQUARE DEAL  TRI RESET",.47f,COL_GRAY);
    draw_text(28,528,"X PLAY / BID",.47f,COL_GRAY);

    draw_vram_meter(340,517,245);

    draw_text(604,518,g_rule_names[g_ruleset],.44f,rgba8(244,206,102,0x80));
    draw_text(780,518,g_pad_connected?"PAD 1 READY":"PAD 1 WAITING",.46f,
              g_pad_connected?COL_GREEN:COL_YELLOW);
    draw_text(780,528,g_audio_ok?"AUDIO OK":"AUDIO BYPASS",.42f,
              g_audio_ok?COL_GREEN:COL_YELLOW);
}

static void draw_menu_box(float x,float y,float w,float h,const char *title){ rect(x,y,x+w,y+h,30,COL_PANEL); frame(x,y,w,h,2,COL_GOLD); draw_text_center(x+w*.5f,y+18,title,1.35f,COL_GOLD); }
static void draw_main_menu(void){ int i; const char *m[2]={"START GAME","OPTIONS"}; draw_menu_box(280,135,400,250,"500 CARD GAME"); for(i=0;i<2;i++){ if(i==g_main_cursor) rect(330,220+i*55,630,258+i*55,31,COL_PANEL2); draw_text_center(480,230+i*55,m[i],1.05f,i==g_main_cursor?COL_GOLD:COL_WHITE); } draw_text_center(480,355,"X SELECT",.65f,COL_GRAY); }
static void draw_options(void){
    int i; char b[80];
    draw_menu_box(205,72,550,395,g_options_page==0?"OPTIONS":(g_options_page==1?"RULESET":(g_options_page==2?"AUDIO":"CUSTOM RULES")));

    if(g_options_page==0){
        const char *n[4]={"RULESET","AUDIO","RETURN TO MAIN MENU","BACK"};
        int count=3;
        for(i=0;i<count;i++){
            int src_i=i;
            if(!g_options_from_game && i==2) src_i=3;
            if(i==g_options_cursor) rect(250,145+i*48,710,180+i*48,31,COL_PANEL2);
            draw_text(275,155+i*48,n[src_i],.78f,i==g_options_cursor?COL_GOLD:COL_WHITE);
            if(src_i==0) draw_text(520,155+i*48,g_rule_names[g_ruleset],.64f,COL_GRAY);
        }
        if(g_options_from_game)
            draw_text_center(480,365,"X SELECT   SELECT RETURN TO GAME",.58f,COL_GRAY);
        else
            draw_text_center(480,365,"X SELECT   O BACK",.58f,COL_GRAY);
    } else if(g_options_page==1){
        for(i=0;i<RULE_COUNT;i++){
            if(i==g_options_cursor) rect(250,132+i*43,710,165+i*43,31,COL_PANEL2);
            draw_text(275,141+i*43,g_rule_names[i],.72f,i==g_options_cursor?COL_GOLD:COL_WHITE);
            if(i==g_ruleset) draw_text(620,141+i*43,"ACTIVE",.52f,COL_GREEN);
        }
        if(g_options_cursor==RULE_STANDARD) strcpy(b,"52 CARDS / NO JOKERS");
        else if(g_options_cursor==RULE_ONE_JOKER) strcpy(b,"53 CARDS / 1 JOKER");
        else if(g_options_cursor==RULE_TWO_JOKERS) strcpy(b,"54 CARDS / 2 JOKERS");
        else if(g_options_cursor==RULE_ORIGINAL) strcpy(b,"24 CARDS / REDUCED QUICK PLAY");
        else snprintf(b,sizeof(b),"CUSTOM - X TO CONFIGURE");
        draw_text_center(480,362,b,.57f,COL_WHITE);
        draw_text_center(480,390,"X SELECT RULESET   O BACK",.55f,COL_GRAY);
    } else if(g_options_page==2){
        const char *n[4]={"MASTER","SOUND EFFECTS","MUSIC","UI / MENU"};
        int *v[4]={&g_master_volume,&g_sfx_volume,&g_music_volume,&g_ui_volume};
        int *m[4]={&g_master_mute,&g_sfx_mute,&g_music_mute,&g_ui_mute};
        for(i=0;i<4;i++){
            if(i==g_options_cursor) rect(250,135+i*50,710,172+i*50,31,COL_PANEL2);
            draw_text(275,146+i*50,n[i],.72f,i==g_options_cursor?COL_GOLD:COL_WHITE);
            snprintf(b,sizeof(b),"%3d%%  %s",*v[i],*m[i]?"MUTED":"ON");
            draw_text(565,146+i*50,b,.65f,*m[i]?COL_RED:COL_GREEN);
        }
        draw_text_center(480,365,"LEFT/RIGHT LEVEL   X MUTE / UNMUTE",.55f,COL_GRAY);
        draw_text_center(480,390,"O BACK",.55f,COL_GRAY);

    } else {
        const char *n[8]={"LOWEST CARD","JOKERS","CARDS PER HAND","KITTY","MINIMUM BID","MAXIMUM BID","TARGET SCORE","STARTING CHIPS"};
        for(i=0;i<8;i++){
            if(i==g_options_cursor) rect(238,111+i*34,722,140+i*34,31,COL_PANEL2);
            draw_text(255,118+i*34,n[i],.58f,i==g_options_cursor?COL_GOLD:COL_WHITE);
            if(i==0) snprintf(b,sizeof(b),"%d",g_custom_low_rank+2);
            else if(i==1) snprintf(b,sizeof(b),"%d",g_custom_jokers);
            else if(i==2) snprintf(b,sizeof(b),"%d",g_custom_hand_size);
            else if(i==3) snprintf(b,sizeof(b),"%d",g_custom_kitty_size);
            else if(i==4) snprintf(b,sizeof(b),"%d",g_custom_min_bid);
            else if(i==5) snprintf(b,sizeof(b),"%d",g_custom_max_bid);
            else if(i==6) snprintf(b,sizeof(b),"%d",g_custom_target);
            else snprintf(b,sizeof(b),"%d",g_custom_start_chips);
            draw_text(640,118+i*34,b,.58f,COL_GREEN);
        }
        snprintf(b,sizeof(b),"%d-%s DECK + %d JOKER%s / %d-CARD HAND / %d KITTY",
                 g_custom_low_rank+2,"A",g_custom_jokers,g_custom_jokers==1?"":"S",
                 g_custom_hand_size,g_custom_kitty_size);
        draw_text_center(480,393,b,.46f,COL_GRAY);
        draw_text_center(480,414,"LEFT/RIGHT CHANGE   O BACK",.48f,COL_GRAY);
    }
}

static void draw_pause_overlay(void){ rect(320,225,640,315,40,rgba8(8,10,12,0x80)); frame(320,225,320,90,2,COL_GOLD); draw_text_center(480,254,"PAUSED!",1.7f,COL_GOLD); }
static void draw_confirm(const char *t){ rect(260,210,700,330,45,COL_PANEL); frame(260,210,440,120,2,COL_GOLD); draw_text_center(480,235,t,.80f,COL_WHITE); draw_text_center(480,285,"X YES     CIRCLE NO",.65f,COL_GRAY); }

static void draw_game(void) {
    gsKit_clear(g_gs,rgba8(28,12,6,0x80));
    if(g_art_tex_ok) {
        tile_texture(&g_tex_wood,0,0,960,540,256,64,0);
        tile_texture(&g_tex_leather,68,34,824,42,256,42,1);
        tile_texture(&g_tex_leather,68,456,824,42,256,42,1);
        tile_texture(&g_tex_leather,68,76,54,380,54,128,1);
        tile_texture(&g_tex_leather,838,76,54,380,54,128,1);
        tex_sprite(&g_tex_felt,120,50,720,432,1);
    } else {
        fill_hgrad(0,0,960,540,28,12,6,72,32,12,24);
        fill_vgrad(0,0,960,540,58,22,10,24,10,6,24);
        fill_vgrad(120,50,720,432,10,74,48,4,44,29,28);
    }
    frame(116,46,728,440,2,rgba8(61,35,16,0x80));
    frame(126,56,708,420,1,rgba8(218,176,84,0x80));
    frame(132,62,696,408,1,rgba8(112,78,26,0x80));

    draw_oval_logo();
    draw_score_and_log();
    draw_trump_panel();
    draw_status();
    draw_cpu_hand();

    draw_chip_bank(764,220,"CPU RED CHIPS",chip_bank[CPU],COL_RED,COL_RED_DARK);
    draw_chip_bank(28,440,"YOU BLUE CHIPS",chip_bank[PLAYER],COL_BLUE,COL_BLUE_DARK);

    draw_played_trick();
    draw_bidding();
    draw_human_hand();
    draw_result_overlay();
    draw_controls_footer();
    if(g_paused) draw_pause_overlay();
    if(g_main_menu && !g_options_open) draw_main_menu();
    if(g_options_open) draw_options();
    if(g_reset_confirm) draw_confirm("RESET CURRENT GAME?");
    if(g_return_confirm) draw_confirm("RETURN TO MAIN MENU?");

    gsKit_queue_exec(g_gs);
    gsKit_sync_flip(g_gs);
}


static void handle_result_tick(void) {
    if (phase != PHASE_RESULT) return;
    if (result_timer > 0) result_timer--;
    if (result_timer > 0) return;
    if (human_count == 0) { finish_hand(); return; }
    phase=PHASE_PLAY;
    current_player=pending_leader;
    if (current_player==CPU) cpu_play();
}

static void handle_input(u32 p) {
    if(g_reset_confirm){ if(p&PAD_CROSS){ reset_match(); g_reset_confirm=0; } else if(p&PAD_CIRCLE)g_reset_confirm=0; return; }
    if(g_return_confirm){ if(p&PAD_CROSS){ reset_match(); g_return_confirm=0; g_options_open=0; g_paused=0; g_main_menu=1; } else if(p&PAD_CIRCLE)g_return_confirm=0; return; }
    if(g_main_menu){ if(g_options_open){ /* handled below */ } else { if(p&PAD_UP||p&PAD_DOWN)g_main_cursor^=1; if(p&PAD_CROSS){ if(g_main_cursor==0){ reset_match(); g_main_menu=0; } else {g_options_open=1;g_options_page=0;g_options_cursor=0;g_options_from_game=0;} } return; } }
    if(g_options_open){
      if(g_options_page==0){
        int max=3;
        if(g_options_from_game && (p&PAD_SELECT)){
          g_options_open=0; g_options_page=0; g_options_cursor=0; return;
        }
        if(p&PAD_UP) g_options_cursor=(g_options_cursor+max-1)%max;
        if(p&PAD_DOWN) g_options_cursor=(g_options_cursor+1)%max;
        if(p&PAD_CROSS){
          if(g_options_cursor==0){ g_options_page=1; g_options_cursor=g_ruleset; return; }
          if(g_options_cursor==1){ g_options_page=2; g_options_cursor=0; return; }
          if(g_options_from_game){ g_return_confirm=1; return; }
          g_options_open=0; g_options_page=0; g_options_cursor=0; return;
        }
        if(!g_options_from_game && (p&PAD_CIRCLE)){
          g_options_open=0; g_options_page=0; g_options_cursor=0; return;
        }
      } else if(g_options_page==1){
        if(p&PAD_UP) g_options_cursor=(g_options_cursor+RULE_COUNT-1)%RULE_COUNT;
        if(p&PAD_DOWN) g_options_cursor=(g_options_cursor+1)%RULE_COUNT;
        if(p&PAD_CROSS){
          if(g_options_cursor==RULE_CUSTOM){ g_ruleset=RULE_CUSTOM; g_options_page=3; g_options_cursor=0; return; }
          g_ruleset=g_options_cursor;
        }
        if(p&PAD_CIRCLE){ g_options_page=0; g_options_cursor=0; return; }
      } else if(g_options_page==2){
        int *v[4]={&g_master_volume,&g_sfx_volume,&g_music_volume,&g_ui_volume};
        int *m[4]={&g_master_mute,&g_sfx_mute,&g_music_mute,&g_ui_mute};
        if(p&PAD_UP) g_options_cursor=(g_options_cursor+3)%4;
        if(p&PAD_DOWN) g_options_cursor=(g_options_cursor+1)%4;
        if(p&PAD_LEFT){ *v[g_options_cursor]-=5; if(*v[g_options_cursor]<0)*v[g_options_cursor]=0; }
        if(p&PAD_RIGHT){ *v[g_options_cursor]+=5; if(*v[g_options_cursor]>100)*v[g_options_cursor]=100; }
        if(p&PAD_CROSS) *m[g_options_cursor]=!*m[g_options_cursor];
        audio_apply_master();
        if(p&PAD_CIRCLE){ g_options_page=0; g_options_cursor=1; return; }
      } else {
        int dir=(p&PAD_RIGHT)?1:((p&PAD_LEFT)?-1:0);
        if(p&PAD_UP) g_options_cursor=(g_options_cursor+7)%8;
        if(p&PAD_DOWN) g_options_cursor=(g_options_cursor+1)%8;
        if(dir){
          if(g_options_cursor==0){ g_custom_low_rank+=dir; if(g_custom_low_rank<0)g_custom_low_rank=8; if(g_custom_low_rank>8)g_custom_low_rank=0; }
          else if(g_options_cursor==1){ g_custom_jokers+=dir; if(g_custom_jokers<0)g_custom_jokers=2; if(g_custom_jokers>2)g_custom_jokers=0; }
          else if(g_options_cursor==2){ g_custom_hand_size+=dir; if(g_custom_hand_size<5)g_custom_hand_size=13; if(g_custom_hand_size>13)g_custom_hand_size=5; }
          else if(g_options_cursor==3){ g_custom_kitty_size+=dir; if(g_custom_kitty_size<0)g_custom_kitty_size=6; if(g_custom_kitty_size>6)g_custom_kitty_size=0; }
          else if(g_options_cursor==4){ g_custom_min_bid+=dir; if(g_custom_min_bid<6)g_custom_min_bid=13; if(g_custom_min_bid>13)g_custom_min_bid=6; if(g_custom_max_bid<g_custom_min_bid)g_custom_max_bid=g_custom_min_bid; }
          else if(g_options_cursor==5){ g_custom_max_bid+=dir; if(g_custom_max_bid<g_custom_min_bid)g_custom_max_bid=13; if(g_custom_max_bid>13)g_custom_max_bid=g_custom_min_bid; }
          else if(g_options_cursor==6){ g_custom_target+=dir*250; if(g_custom_target<250)g_custom_target=1000; if(g_custom_target>1000)g_custom_target=250; }
          else { g_custom_start_chips+=dir*25; if(g_custom_start_chips<25)g_custom_start_chips=500; if(g_custom_start_chips>500)g_custom_start_chips=25; }
          { int avail=4*(13-g_custom_low_rank)+g_custom_jokers;
            while(2*g_custom_hand_size+g_custom_kitty_size>avail && g_custom_hand_size>5) g_custom_hand_size--;
            while(2*g_custom_hand_size+g_custom_kitty_size>avail && g_custom_kitty_size>0) g_custom_kitty_size--;
          }
        }
        if(p&PAD_CIRCLE){ g_options_page=1; g_options_cursor=RULE_CUSTOM; return; }
      }
      return;
    }
    if(p&PAD_START){ g_paused=!g_paused; return; }
    if(g_paused) return;
    if(p&PAD_SELECT){ g_options_open=1;g_options_page=0;g_options_cursor=0;g_options_from_game=1;return; }
    if(p&PAD_TRIANGLE){ g_reset_confirm=1; return; }
    if(p&PAD_SQUARE){ if(phase==PHASE_DONE && match_winner>=0)reset_match(); if(phase==PHASE_IDLE||phase==PHASE_DONE)deal_hand(); return; }
    if(phase==PHASE_BIDDING){ int minb=(g_ruleset==RULE_CUSTOM)?g_custom_min_bid:7, maxb=(g_ruleset==RULE_CUSTOM)?g_custom_max_bid:13; if(p&PAD_UP){if(bid_level<maxb)bid_level++;sound_beep(650,20,2500);} if(p&PAD_DOWN){if(bid_level>minb)bid_level--;sound_beep(560,20,2500);} if(p&PAD_RIGHT){bid_suit=(bid_suit+1)%5;sound_beep(600,20,2500);} if(p&PAD_LEFT){bid_suit=(bid_suit+4)%5;sound_beep(600,20,2500);} if(p&PAD_CROSS)submit_bid(0); if(p&PAD_CIRCLE)submit_bid(1); }
    else if(phase==PHASE_PLAY&&current_player==PLAYER){ if(p&PAD_RIGHT){if(human_count>0)selected_card=(selected_card+1)%human_count;} if(p&PAD_LEFT){if(human_count>0)selected_card=(selected_card+human_count-1)%human_count;} if(p&PAD_CROSS)play_human_selected(); }
}

static void reset_iop_for_homebrew(void) {
    /* Start from a clean IOP state so Run ELF behaves consistently in PCSX2
       and on real hardware/homebrew launchers. */
    sceSifInitRpc(0);
    while (!SifIopReset(NULL,0)) { }
    while (!SifIopSync()) { }
    sceSifInitRpc(0);

    /* Allow embedded IRX modules to be executed from EE memory. */
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();
}

int main(int argc,char **argv) {
    u32 p;
    (void)argc; (void)argv;
    reset_iop_for_homebrew();
    srand((unsigned int)clock() ^ 0x5002u);
    init_graphics();
    if (!g_gs) return 1;
    init_art_textures();
    init_pad();
    init_audio();
    reset_match();

    for (;;) {
        p=read_pad_new();
        if (p) handle_input(p);
        if(!g_paused && !g_options_open && !g_main_menu) handle_result_tick();
        draw_game();
    }
    return 0;
}