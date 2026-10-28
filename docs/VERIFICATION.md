# Release verification — 1.0.0

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
