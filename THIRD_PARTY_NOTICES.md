# Third-Party Notices

This is an attribution record, not a replacement for upstream licence texts.

## PS2SDK / PS2DEV
Upstream: https://github.com/ps2dev/ps2sdk

PS2SDK supplies SDK headers/libraries, IOP/EE facilities and build infrastructure used by this project. The Makefile includes `$(PS2SDK)/samples/Makefile.pref` and `$(PS2SDK)/samples/Makefile.eeglobal`. The game links PS2SDK components including audsrv, libpadx and libpatches.

The PS2SDK project states that PS2SDK is licensed under the Academic Free License version 2.0. Individual files/examples may carry more specific notices and those notices must be retained when copied.

## gsKit / dmaKit
Upstream: https://github.com/ps2dev/gsKit

gsKit provides the C interface to the PlayStation 2 Graphics Synthesizer; dmaKit provides DMAC support used by gsKit. Upstream identifies gsKit as Academic Free License version 2.0. Consult upstream LICENSE/AUTHORS for complete terms/credits.

## audsrv
audsrv is supplied through PS2SDK and is used for the game's audio path. This source package contains an embedded C representation of `audsrv.irx` for this build. PS2SDK audsrv examples were useful API references. Do not assume all examples have one identical licence: check/retain the header of any example actually copied. For example, upstream `playwav2.c` carries a GNU Library General Public License version 2 notice.

## sio2man / padman / libpadx
The build uses PS2SDK controller facilities and embeds `sio2man.irx` and `padman.irx` as C arrays. `libpadx` is linked on the EE side. These remain third-party PS2DEV/PS2SDK components; consult the corresponding upstream source/AUTHORS/licence notices for the exact version used.

## zlib
The build links zlib through PS2SDK ports for compressed assets. zlib remains subject to its upstream zlib licence.

## Borrowed snippets and example-derived code
PS2DEV/PS2SDK and gsKit documentation/examples were used as implementation references for IOP module loading, controller initialisation/polling, audsrv setup/submission, gsKit screen/primitive/texture handling and PS2SDK Makefile structure.

The present source history does not provide a trustworthy complete mapping of every small snippet to an original example file. Do not invent attribution. When an exact copied/adapted source is positively identified, record its file/URL, copyright holder and licence here and preserve any required notice.

## Trademarks
PlayStation and PlayStation 2 are trademarks of Sony Interactive Entertainment. This homebrew project is unaffiliated with and not endorsed by Sony.