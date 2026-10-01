500 Card Game PS2 - 960x540 CT16 + GS 2x Horizontal Magnification

Based on the VRAM OPT2 build.

Changes:
- Framebuffer width reduced from 1920 to 960.
- Framebuffer height remains 540.
- GS mode remains DTV 1080i.
- gsKit automatically derives MAGH = (1920 / 960) - 1 = 1 for 2x horizontal display magnification.
- Existing 540-line framebuffer remains vertically magnified for 1080i timing.
- CT16, single buffering, no Z-buffer retained.
- All OPT2 texture/cache settings retained.

Expected framebuffer VRAM:
- 1920x540 CT16: ~2025 KiB
- 960x540 CT16: ~1013 KiB
- Approx saving: ~1012 KiB before allocator/page effects.

Use the in-game VRAM meter as the authoritative runtime reading.