# 500 Card Game — PlayStation 2

Native PlayStation 2 implementation of the card game **500 (Five Hundred)**, built with PS2DEV/PS2SDK and gsKit. This repository contains the restored source for the current **501B — HIRES CARD GLYPHS** build together with the reproducible PS2 ELF.

**Target Renders:**
<img width="810" height="540" alt="501B_In_Play" src="https://github.com/user-attachments/assets/13296653-e9f0-4572-ac86-dd0936afaa72" />
<img width="810" height="540" alt="501B_Card_Presentation" src="https://github.com/user-attachments/assets/c557b448-56d7-47b5-bfe2-50093eaa031f" />
<img width="810" height="540" alt="501B_Main_Table" src="https://github.com/user-attachments/assets/3a728343-669f-4fb8-9550-d85392e3be27" />

**Current game build screenshots:**
<img width="2048" height="976" alt="501B_actual_gameplay_01" src="https://github.com/user-attachments/assets/01b40dea-ce47-4f5f-aa58-724dd26977cf" />
<img width="2048" height="870" alt="501B_actual_gameplay_03" src="https://github.com/user-attachments/assets/54148a12-2d83-434e-8e93-9ac94a6f8d9d" />
<img width="2048" height="932" alt="501B_actual_gameplay_02" src="https://github.com/user-attachments/assets/c47ffb29-c808-4152-8e61-3d1d70671a09" />

## Current verified build

- Build: **501B — HIRES CARD GLYPHS**
- Executable: `500_Card_Game_PS2_54CARD_960x540_X2.ELF`
- ELF size: **2,032,028 bytes**
- SHA-256: `dd5fe6326ce6144d816183bd3792949cc314bf3582f7b058f211a57bbce084b3`
- Target: PlayStation 2 / EE R5900
- Video mode: **DTV 1080i**, interlaced
- Logical/framebuffer resolution: **960×540**
- GS pixel format: **CT16**
- Z-buffer: disabled
- Dithering: enabled
- Rendering: gsKit + dmaKit / PS2 Graphics Synthesizer

The repository contains the current 501B PS2 source code and its verified reproducible ELF build.

## Downloads

The current verified build and complete restored source archive are stored in the repository:

- [501B PS2 ELF](release/500_Card_Game_PS2_501B.ELF) — 2,032,028 bytes — SHA-256 `dd5fe6326ce6144d816183bd3792949cc314bf3582f7b058f211a57bbce084b3`
- [501B complete source archive](release/500-Card-Game-PS2-501B-Source.zip) — 3,037,506 bytes — SHA-256 `7727892efd07862eb286d64b35f3fd55dae34d18e0470aba3e32cea33c9bc4cc`

The main C/assembly source is also browsable under `src/`. The complete source ZIP is the authoritative package for the full binary texture/compressed-asset set used by the 501B build.

## Game and card presentation

The game supports standard 500 play against a CPU opponent, bidding, tricks, a kitty/widow, scoring and match progression. Ruleset choices include:

- **Standard** — 52 cards, no Jokers.
- **One Joker** — 53 cards.
- **Two Jokers** — full 54-card deck.
- **Original Reduced** — 24-card reduced quick-play ruleset.
- **Custom** — selectable lowest rank, 0–2 Jokers, cards per hand, kitty size, minimum/maximum bid, target score and starting chips.

Cards combine procedural rendering with compressed artwork. Number cards use conventional pip layouts. Aces use a large centred suit. Jacks, Queens and Kings use full-card court artwork with procedural corner indices. Jokers use dedicated artwork and vertical corner labelling. The 501B renderer adds higher-resolution rank and suit glyphs while retaining the established court-card/Joker presentation.

Four 64×64 suit masks are embedded for the high-resolution suit renderer. Court, Joker, card-back, felt, leather, wood and chip artwork is stored in the source asset set, with several runtime assets kept in compressed form to control memory use.

## Rendering and VRAM

The renderer is written for the PS2 GS through **gsKit** and **dmaKit**. The current build uses a 960×540 CT16 framebuffer while outputting through the GS DTV 1080i mode. The horizontal framebuffer is half of 1920 pixels, allowing GS display magnification to provide the 1080i output width while reducing framebuffer VRAM pressure.

The game includes an on-screen **VRAM meter**. It measures current GS memory use against the PS2's **4 MiB VRAM** and displays both KiB used and percentage. This is the preferred runtime indicator when tuning textures and caches.

Texture handling includes GS VRAM allocation, compressed asset inflation, cached special/court textures, and explicit GIF submission before overwriting cache slots that may still be referenced by queued drawing commands.

## Menus and options

The main menu contains **Start Game** and **Options**. Options can also be opened during a match with **Select**.

The options hierarchy contains:

- **Ruleset** — Standard, One Joker, Two Jokers, Original Reduced and Custom.
- **Audio** — Master, Sound Effects, Music and UI/Menu levels, each with independent mute state.
- **Custom Rules** — lowest card, Jokers, cards per hand, kitty size, minimum bid, maximum bid, target score and starting chips.
- **Return to Main Menu** when options are opened from gameplay.

The HUD/footer also reports the active ruleset, controller state (`PAD 1 READY` / `PAD 1 WAITING`), audio state (`AUDIO OK` / `AUDIO BYPASS`) and VRAM usage.

## Controls

### General

- **D-pad Up/Down** — menu navigation.
- **Cross (X)** — select/confirm.
- **Circle** — back/cancel where applicable.
- **Start** — pause/unpause during gameplay.
- **Select** — open options during gameplay; return from options to the game.
- **Square** — deal/start the next hand when allowed.
- **Triangle** — open the reset confirmation.

### Bidding

- **D-pad Up/Down** — increase/decrease bid level.
- **D-pad Left/Right** — change bid suit / no-trumps selection.
- **Cross (X)** — submit bid.
- **Circle** — pass.

### Playing cards

- **D-pad Left/Right** — move through the player's hand.
- **Cross (X)** — play the selected legal card.

### Audio/custom-rule menus

- **D-pad Left/Right** — change a level/value.
- **Cross (X)** — mute/unmute an audio category or select/configure a ruleset.
- **Circle** — return to the previous menu.

## Audio and known sound limitations

Audio uses PS2SDK **audsrv**, with `LIBSD` plus the embedded `audsrv.irx`. The current cue generator produces short 16-bit, 22.05 kHz stereo tones for menu/game feedback.

A deliberate non-blocking audio path is used. Earlier behaviour could stall the EE/game loop when `audsrv_wait_audio()` waited indefinitely after an emulated IOP/SPU2 stream stopped consuming data. The current build submits a short cue once and drops it if audsrv cannot accept it immediately. After repeated submission failures the game disables the audio path and reports **AUDIO BYPASS** instead of allowing sound to hang gameplay.

Because of this safeguard, short cues can occasionally be dropped when the audio backend is unhealthy. The separate Music and UI/Menu controls are present in the menu structure, although this build's implemented sound generation is primarily short generated cues rather than a complete streamed music system.

## Controller implementation

The build embeds the free/new `sio2man` and `padman` modules and uses the PS2SDK `libpadx` EE client. Gameplay only requires digital input. The pad reader tolerates late controller connection and periodically retries opening the port.

## Building

### Requirements

A working PS2DEV environment containing:

- EE toolchain (`mips64r5900el-ps2-elf-gcc`)
- PS2SDK
- gsKit
- dmaKit
- zlib/PS2SDK ports required by the Makefile

The recovered build used GCC/GIMPLE **15.2.0**. For a byte-identical 501B reproduction, compiler/toolchain version, optimisation behaviour, source layout and build path matter.

Example environment:

```bash
export PS2DEV=/path/to/ps2dev
export PS2SDK=$PS2DEV/ps2sdk
export GSKIT=$PS2DEV/gsKit
export PATH=$PS2DEV/bin:$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2DEV/dvp/bin:$PS2SDK/bin:$PATH
```

Build:

```bash
make clean
make
```

Output:

```text
500_Card_Game_PS2_54CARD_960x540_X2.ELF
```

The Makefile links against:

```text
-lgskit -ldmakit -laudsrv -lpadx -lpatches -lz -lm -lc
```

and uses:

```text
-Os -ffunction-sections -fdata-sections
-Wl,--gc-sections
```

### Reproducing the exact 501B ELF

The authoritative build recorded `/mnt/data/501B_hires_glyphs` as its compilation directory. DWARF/debug strings and source paths are bytes inside the ELF, so building from a different directory can produce a functionally identical executable with a different whole-file checksum.

For strict reproduction, use the documented PS2DEV toolchain and reproduce the original build path/source layout before comparing the complete ELF.

Verify with:

```bash
sha256sum 500_Card_Game_PS2_54CARD_960x540_X2.ELF
```

Expected 501B SHA-256:

```text
dd5fe6326ce6144d816183bd3792949cc314bf3582f7b058f211a57bbce084b3
```


## Third-party code, SDKs, examples and attribution

This project is built on the PS2DEV homebrew ecosystem. It uses PS2SDK interfaces/build infrastructure and gsKit/dmaKit for Graphics Synthesizer and DMA access. The Makefile includes the standard PS2SDK `samples/Makefile.pref` and `samples/Makefile.eeglobal` fragments.

The build links components including `audsrv`, `libpadx`, `libpatches`, zlib, gsKit and dmaKit. The source package also contains C arrays generated from the `audsrv.irx`, `sio2man.irx` and `padman.irx` modules used by this build. Those components remain subject to their upstream authors' licences and notices; inclusion here does not relicense them as original game code.

PS2SDK states that it is licensed under the **Academic Free License 2.0**. gsKit likewise identifies itself as **Academic Free License 2.0**. Individual PS2SDK files/examples can carry their own notices. For example, some audsrv sample code is marked **GNU Library General Public License v2**; therefore the header/licence of any upstream file or snippet actually copied must be retained and checked.

PS2DEV/PS2SDK and gsKit documentation/examples were also used as implementation references for controller input, IOP module loading, audio, GS rendering, texture upload and build configuration. Where an identifiable upstream snippet is copied/adapted rather than independently written against the API, its original copyright/licence notice must be retained or recorded in `THIRD_PARTY_NOTICES.md`.

Upstream:
- PS2SDK: https://github.com/ps2dev/ps2sdk
- gsKit/dmaKit: https://github.com/ps2dev/gsKit
- PS2DEV environment: https://github.com/ps2dev/ps2dev

## Artwork, texture references and image provenance

The card/table presentation was developed using procedural graphics, generated/edited project artwork, and visual reference material found through internet/search-engine image searches. Online images were used as visual samples/references while iterating on playing-card courts/Jokers and materials such as felt, wood, leather, card stock/grain and chips.

An image appearing in Google, Bing, Yahoo, Yandex or another search engine does **not** grant a redistribution licence. Search engines are discovery tools rather than the underlying copyright source. Internet references therefore must not be described as public-domain/free-to-use unless the original creator/source and licence have been identified.

The asset tree contains runtime assets and development/intermediate PNGs. Before public redistribution, any image containing recognisable third-party source pixels should have its creator, original URL and licence recorded. If that provenance cannot be established, omit/replace the affected image rather than inventing attribution.

See `ASSET_PROVENANCE.md` for the audit register.

## Project licensing status

There is currently **no blanket open-source licence for the project's original game code and original artwork**. Normal copyright restrictions therefore apply unless a project licence is deliberately added later.

Third-party components remain governed by their own upstream licences and copyright notices. See `THIRD_PARTY_NOTICES.md` and `ASSET_PROVENANCE.md`.

PlayStation and PlayStation 2 are trademarks of Sony Interactive Entertainment. This is a homebrew project and is not affiliated with or endorsed by Sony.
