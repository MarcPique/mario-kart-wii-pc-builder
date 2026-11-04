# Release verification

## 1.2.0

The published installer was run through its embedded Python runtime in offline
mode against the user's locally held PAL RMCP01 WBFS and an existing lab
installation. It completed translation, native compilation, graphics-object
rebuilding, menu and fifteen-language generation, verified texture-archiver
installation and publication of the portable game.

All **63 pre-existing configuration, save and other persistent files checked
under UserData stayed byte-identical** during that update. Logs, caches, texture
packs and the room-status cache were excluded from this comparison. The install
report advanced to 1.2.0 and the previous game data was retained as
`GameData.previous`. The already running user installation was not controlled or
modified by the lab test.

Native-menu testing with isolated SDL virtual gamepads verified Circle accepts,
Cross returns, Triangle produces guest X and Square produces guest Y. The
original two-player Online controller-confirmation page appeared with exactly
one native client. Cancelling returned to the original menu, and all four
controllers then registered for local play in that same process. This test does
not claim that a new online race was completed or that four physical gamepads
were tested.

The desktop installation and public release were verified separately. The
source and installer CI jobs passed for the 1.2.0 release commit. Downloading the
uploaded installer again reproduced its published SHA-256. See
[compatibility boundaries](RETRO-COMPATIBILITY.md) for CT 200cc and upstream-code
updates; neither is advertised as implemented.

An additional lab-only build requested Dawn's low-power adapter and verified
**AMD Radeon(TM) Graphics**, D3D12 driver **32.0.13066.12**, in the runtime log.
The original title, license selection and native Options page rendered, and
the menu accepted ordinary PAD input. The capture was visually inspected.
This covers startup and menu rendering, not race performance on that GPU.
The probe's source, static archives and executable were restored byte-for-byte
to the public build afterward; the distributed GPU preference and the user's
running game were not changed. Existing RTX 4090 race tests remain separate.

## Original 1.0.0 verification

The Windows installer was tested locally with a user-provided PAL RMCP01 disc. The disc and all generated products remain outside this repository.

- Extracted and validated a real ISO; checked the pinned `main.dol` and `StaticR.rel` SHA-256 values.
- Completed translation and native compilation from a fresh workspace, including the modified runtime and Aurora objects.
- Ran the public single-file EXE through its integrated Python environment and completed a builder-created update, preserving the data folder and retaining the previous game files.
- Tested a Windows installation path containing an accent and spaces; the toolkit uses its verified ASCII workspace alias.
- Created an actual desktop EXE, preserved an existing file with the same name, and launched the generated game through that EXE.
- Verified the visible window title **Mario Kart Wii** and both 32 px and 256 px window icons against the complete icon generated from the local disc banner.
- Verified hidden startup preparation and the original title presentation on a fresh profile.
- Tested immediate switching among five menu languages, native Options and pause entry points, pause resume, MSAA 4x in a local race, Retro WFC login and creation/closure of a private friends room during runtime integration testing.
- Verified room queries at startup and subsequent refreshes.
- Passed the installer embedded-runtime smoke test and six Python tests covering ZIP traversal, symlinks, source archive round trips, desktop links, existing installation protection, cleanup boundaries and invalid offline download hashes.
- Passed the C++ room parser tests for valid, empty, malformed, incorrectly typed and oversized responses.

The publication lint scanned the tracked source export against the extracted disc: **0 failures**. Seven warnings were reviewed: they refer to pre-existing symbolic address labels in the upstream translator parser and its synthetic test fixtures, not newly generated game functions. No ROM, extracted disc asset, generated translation, profile, private key or local build cache is published.

The installer interface was rendered and checked at the development display's high-DPI setting. Documentation screenshots show the tested game UI; no friend codes are included. Local private logs and the generated game are deliberately excluded from the repository and release.

Tested host: Windows x64, Ryzen 7 7800X3D, RTX 4090. This does not establish compatibility with every supported Windows/GPU configuration. The public CI builds and tests the installer without using a disc image.
