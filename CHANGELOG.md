# Changelog

## 1.0.0 — 2026-10-28

- Windows single-file installer: local PAL RMCP01 ISO, WBFS or RVZ to a native Windows game.
- Actual desktop EXE linked to the installation, with the complete Mario Kart Wii icon generated from the user's disc banner. Existing desktop files preserved.
- Original window name and icon in the window and taskbar; hidden preparation followed by the original presentation.
- Native Advanced Options in Options and local race pause menus; no floating settings bar or F10 requirement.
- Display modes, internal/output resolution, aspect ratio, V-Sync, bloom, FPS, MSAA, anisotropy, sharpness, interpolation, vibration, mute and coherent quality presets.
- Immediate Spanish, English, French, German and Italian menu-language switching, including loaded controls. No language restart.
- Retro Rewind 6.13.4 / Retro WFC login, friends and rooms in the same process.
- Asynchronous room checks at startup and every 40 seconds, with an unavailable state on failure.
- Aurora multisampled depth-copy support, tested with MSAA 4x in a local race.
- Pause layout, UI dependencies and overlapping controls corrected.
- Portable UserData paths, preserved saves on builder-created updates and retained previous game data.
- Pinned SHA-256 downloads, ZIP path checks, verified installer payload and ASCII workspace alias for Windows paths.

Based on WiiCompiled v0.9.3 (`f02d000ba8e67240a04d95676da89023f781222f`).
