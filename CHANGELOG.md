# Changelog

## 1.0.2 — 2026-11-04

- Automatically place connected gamepads on players 1–4 at startup and on connection changes, ahead of the keyboard. Preserve their relative player order and compact holes on removal. Manual assignments apply until the next roster change or launch; the keyboard controls player 1 when no gamepad is connected.
- Assign controllers by SDL device identity. Disconnecting another pad no longer shifts the selected device; disappeared devices are rejected. Four identical virtual controllers, removal and replacement were tested without restarting. A real DualSense Edge over Bluetooth was verified by the user.
- Show DualSense, DualShock, Switch Pro or Xbox in the native device selector, with complete SDL model names in the wide detail panel instead of clipping them inside narrow original controls.
- Verify the bundled Mario (Fire) character in offline Grand Prix, beyond its selection-screen preview. Included variants are selected with L/R and are available in local races.
- Include the native four-player assignment page, public-room selector and live MSAA/anisotropy from 1.0.1, with updated instructions.
- Include experimental My Stuff filesystem preparation behind test flags. No public My Stuff selector, live save switching or live code updates are enabled.
- Online remains at most two local players per PC. Four-player local split screen is separate; the existing Retro WFC SELECT and race packets only describe two local players per connection.

## 1.0.1 — 2026-11-01

- Fix the progress parser error at the end of installation.
- Native controller assignments for four local players and Retro Rewind version checks.
- Public-room selector with automatic refresh and stable positions while the list is open. Entry to Retro and CT rooms verified; CT 200cc has no available route.
- Apply MSAA and anisotropic filtering without restarting; tested on one GPU including the race pause menu.
- Enable all track categories in newly created settings while preserving existing choices.

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
