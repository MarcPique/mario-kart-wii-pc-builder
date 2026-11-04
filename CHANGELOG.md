# Changelog

## 1.1.0-preview.2 — 2026-11-04

- Add Exit to desktop to the native main menu. Reuse the original full-screen confirmation, default to Cancel, support B cancellation, and exit successfully through the existing clean window-close path.
- Add a live aspect-ratio page with sixteen common automatic, standard, ultrawide and portrait formats. Preserve legacy profile behavior; fixed formats adapt the EGG camera canvas and presentation together with proportion-preserving bars.
- Generalize Aurora's fixed presentation aspect instead of stretching a 16:9 framebuffer. Rebuild the matching window object in the backed-up Aurora core archive.
- Connect Aurora's Dolphin PNG/DDS replacement loader to native enable/reload/open-folder controls. Index PAL and region-independent folders with clear priority, preserve legacy folders, and keep original textures when a replacement is absent, corrupt or unsupported.
- Validate live PNG/DDS registration, corrupt-image fallback and removal without restarting. Add aspect migration/geometry checks and translated PC labels for all fifteen locales.

## 1.1.0-preview.1 — 2026-11-04

- Expand immediate language switching from five to fifteen locales: Spanish, English, French, German, Italian, Dutch, Czech, Finnish, Russian, Turkish, Latin American Spanish, Catalan, Galician, Basque and Andaluh (EPA).
- Reuse existing game fonts only, validating every required visible codepoint during generation and installation. Japanese and Korean are excluded because those fonts lack part of their message-bank coverage.
- Use bundled Retro Rewind translations where available. Catalan/Galician/Basque game text is experimental automatic translation, identified in the native language menu; additional locales include automatic supplements. Andaluh is generated from Spanish with Andalugeeks' EPA transcriber. Artwork and audio are unchanged.
- Ship a hash-checked BPS localization patch, PC-label catalog and maintainer generation sources; reconstruct the complete message bank locally from the user's disc and downloaded mod. Add dynamic bank descriptors while accepting the previous five-language format.
- Preserve BMG button/variable commands, validate offsets/checksums/terminators, and test language-list agreement across the installer, runtime and patch metadata.
- Distinguish Joy-Con, Switch Pro and other Switch family device names in the native assignment selector. Controller behavior and online limits from 1.0.2 remain applicable.

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
