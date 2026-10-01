500 Card Game PS2 - 54 Card Stream Cache build

Purpose
-------
This build keeps the current GS pixel formats and 1920x540 CT16 framebuffer,
but changes source storage and special-card residency.

1. EE/ELF compression
- Texture source payloads are zlib-compressed in the ELF / EE address space.
- A single 192 KB aligned EE scratch buffer is reused for decompression.
- Permanent textures are decompressed once at startup, uploaded to GS VRAM,
  then the scratch buffer is reused.
- Court/Joker sources remain compressed and are decompressed on cache misses.

2. GS VRAM streaming cache
- Previous HQ build permanently allocated:
    court atlas 512x512 CT16 = 512 KB
    two 128x256 CT16 Jokers = 128 KB
    total = 640 KB
- This build allocates four fixed 128x256 CT16 cache slots:
    4 x 64 KB = 256 KB
- Expected GS VRAM saving = about 384 KB.
- Court images are stored individually as the exact 96x160 visible regions,
  not as padded 512x512 atlas space.
- A cache miss inflates the required card into EE scratch RAM and uploads it
  into the least-recently-used slot. If a slot must be overwritten, queued GS
  draws are submitted first so the old texture is consumed before replacement.

Compression figures for the embedded working pixel payload
----------------------------------------------------------
Previous HQ raw linked texture payload represented about 1,105,920 bytes for
these sources (including the padded 512x512 court atlas). The new individually
packed/zlib source payload is about 539,583 bytes, before palettes/linker
alignment. This does not itself save GS VRAM; it saves ELF/EE storage. The GS
VRAM reduction comes from the four-slot streaming cache above.

Build
-----
Requires PS2DEV/PS2SDK/gsKit plus the PS2SDK ports zlib library.
The Makefile links -lz and adds $(PS2SDK)/ports/include and /ports/lib.


ART REALLOC PASS:
- card face is now GS-drawn cream with 32x32 grain overlay
- card back reduced to 128x192 CT16 plain design
- chips reduced to 128x64 CT16 plain design
- court portraits enlarged to 128x224 streamed art
- joker art refreshed; greyscale joker derived from colour design
- suit pips and rank glyphs redrawn at higher bitmap resolution