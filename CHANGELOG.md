# Changelog

## 1.2.0 — 2026-11-04

- Keep Online to one original client and one or two players per PC. Retire the coordinated-client prototype; four players remain available locally.
- Display **Online** throughout the localized menus. Preserve BMG formatting, variables, fonts and the original on-disk language bank.
- Use Nintendo face-button defaults: DualSense Circle accepts/accelerates, Cross returns/brakes, Triangle maps to X and Square to Y. Apply these defaults over old serialized PC mappings while respecting explicit custom bindings.
- Add native My Stuff controls with five modes, background immutable snapshots, safe cancellation and retained reads. Add confirmed save-bank switching back to the title without restarting the EXE; invalid/busy targets retain the current bank.
- Review the expanded PC catalog: 92 labels in each of fifteen languages. Preserve existing game text, save paths and user data.
- Remove retired coordinator source files from warm build caches. CT 200cc routing and applying newer upstream mod code remain unavailable; version checks continue to work.

## 1.1.0 — 2026-11-04

- Complete the contextual language review and promote Catalan, Galician and Basque to reviewed status. Remove beta notices and asterisks from native menus and the installer while retaining machine-assisted provenance.
- Refine stunts, multiplayer/invitation wording, plural forms, elimination templates and canonical track names. Review all 1,543 required game spans per added translation and 82 PC labels in each of fifteen languages.
- Require explicit nonempty acceptance of every generated game span for release generation; reject incomplete reviews and non-reviewed public packs. Retain font, BMG command and printf validation.
- Include the prior previews’ integrated online, controller assignment, live display formats, texture archives and desktop exit. Existing online and launcher integration limits remain documented.

## 1.1.0-preview.5 — 2026-11-04

- Repair inherited Spanish wording before deriving Andaluh, so the friend-invitation correction reaches both languages. Preserve names, buttons and format arguments.
- Display a translated room label followed by its count instead of appending a fixed plural suffix. Zero, one and multiple rooms use consistent wording in all fifteen languages.
- Remove the obsolete plural-only PC label and add a regression check for inherited corrections surviving transcription.

## 1.1.0-preview.4 — 2026-11-04

- Review all 1,260 PC catalog entries across fifteen languages: correct technical terminology, accents, button verbs, room/status messages and missing negation. Make the native runtime use the corrected catalog for the original five languages too, retaining compiled labels as fallback.
- Inspect 1,543 game-text spans each for Catalan, Galician and Basque, including 214 multiline contexts. Correct racing, cups, drift, team sizes, eliminations, ghost challenges, confirmations, error codes and omitted instructions. Retain the experimental-translation notice.
- Translate complete BMG prose spans before wrapping lines; keep shared names/credits and canonical unreviewed vehicle/track names intact. Preserve wide/padded printf arguments as well as embedded button and variable commands.
- Protect product names, file formats, units and controls in Andaluh EPA transcription. Replace the obsolete restart-language help in all fifteen languages and fix inherited friend-registration wording.
- Audit 90,720 bank records with no missing font characters or generated command/format mismatches. Add regression checks for context, `%ls`, identities and the reviewed PC catalog. See [localization provenance and scope](docs/LOCALIZATION.md).

## 1.1.0-preview.3 — 2026-11-04

- Put all sixteen aspect ratios directly in the Display page scroller. Label the three radio shortcuts Quick ratio; keep the complete internal-resolution selector on its own page.
- Import ZIP and 7z texture packs from the native texture folders in the background at startup or on Reload, without a restart or system 7-Zip installation. Cache completed imports by archive SHA-256; reuse them on subsequent scans.
- Download and verify the official standalone 7-Zip Extra 26.05 x64 tool during installation, keeping its license/readme with the generated game.
- Preflight archive paths, links, device names, encryption, duplicates and image-size quotas. Use a contained hidden child process and publish only completed cache directories. A failed archive retains original game textures; loose PNG/DDS files take priority over imported packs.
- Refresh the native texture count/import status automatically. Verify a ZIP resource pack including its logo, live 7z replacement, traversal rejection and original-texture fallback.

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
