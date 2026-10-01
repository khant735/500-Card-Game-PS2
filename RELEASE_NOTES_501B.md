# 501B — HIRES CARD GLYPHS

This is the current restored PlayStation 2 build of 500 Card Game.

## Highlights

- Higher-resolution procedural rank glyph rendering.
- Four 64×64 high-resolution suit masks.
- Existing numbered-card pip layouts retained.
- Large centred Ace suit presentation retained.
- Full-card Jack/Queen/King artwork retained with procedural corner indices.
- Existing Joker presentation retained.
- 960×540 CT16 GS framebuffer targeting DTV 1080i output.
- On-screen 4 MiB GS VRAM usage meter.
- Standard, 1-Joker, 2-Joker, reduced and configurable custom rulesets.
- Nested audio controls for Master, SFX, Music and UI/Menu levels/mutes.
- Non-blocking audsrv cue submission to avoid audio stalls locking the EE/game loop.
- Controller late-connect/retry handling using embedded sio2man/padman and libpadx.


## Known limitation

The audio path intentionally drops short generated cues rather than blocking when audsrv/IOP/SPU2 stops accepting data. Repeated failures place the game in AUDIO BYPASS mode. This protects gameplay from the earlier sound-related hang but means cues can be lost on an unhealthy audio backend.