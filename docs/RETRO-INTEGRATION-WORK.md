# Retro Rewind integration work

This document tracks the development branch. The public stable release remains
1.1.0; the work below is not yet included in its installer or desktop runtime.

The reference launcher is [Retro Rewind Team rr-launcher](https://github.com/Retro-Rewind-Team/rr-launcher/tree/d7bde0b7dd933e4c5cfc7b4c237c49b97e135baf),
especially `source/settings.c`. The compatible installed content remains Retro
Rewind 6.13.4 and the user's PAL RMCP01 disc.

| Area | Development state | Verification and remaining work |
|---|---|---|
| My Stuff | Native page with the five original choices, Apply and Folder; settings persist. Portable user content takes priority over the legacy SD-style folders. Preparation runs in the background and publishes on the game thread. Leaving these options cancels it. | Real DVD tests passed mode changes, stable entry IDs, hidden inactive files, retained reads after source replacement/deletion, late-added background music and cancelled publication. The native Apply button also passed. Arbitrary new paths that would change existing entry IDs are rejected; further general hot-reload work and final installer tests remain. |
| Separate savegame | Native page and confirmation, initially Cancel. Switches future save opens and reloads the licenses, returning to the title in the same process. | Real native-menu roundtrip passed. Both banks stayed byte-identical. An invalid target was rejected and the original bank restored, including an exact check of the loaded save core in guest memory. Final installer/update testing remains. |
| Session isolation | Named sessions keep their own configuration, NAND, saves, cache, textures, mappings and logs while sharing immutable game content. Invalid names fail closed. | Pure contracts and two concurrent native processes passed. Each process used a distinct mutable-data directory. |
| Four local players online | Investigating two compatible clients with separate profiles and a shared window, following the user's preference for public Retro WFC. | Private window-host prototype displayed two live title screens and restored their original windows afterwards. Controller routing, account setup, room coordination, four-player racing, display layout and performance are still unverified. |
| CT 200cc room routing | Pending compatibility investigation. | The current native room route table has no verified path for `vs_22`. Do not route it through a different mode or advertise it as available. |
| Launcher updates | Stable release checks official version information. | Download, staging, repair/application and coordination with active sessions still need implementation and tests. Replacing running native code is not implemented. |
| Hardware coverage | DualSense Edge Bluetooth was verified by the user in 1.1.0. Existing virtual-controller and aspect-ratio tests remain relevant. | More physical models, simultaneous controllers, the integrated GPU, and combined online-session performance still need validation. |

My Stuff files intended to survive updates belong under
`UserData/MyStuff/CTGP` or `UserData/MyStuff/RetroRewind`. The legacy directories
inside the virtual SD/game-data tree remain readable. Save banks are
`UserData/Saves/RetroWFC/RMCP` and `UserData/Saves/RetroWFC2/RMCP`.

Applied My Stuff files are copied to immutable SHA-256 versions under each
profile's `Cache/MyStuff`. Replacing or deleting a source file does not invalidate
an already open DVD handle. Reads retain their published catalogue while in
progress; cancellation does not change the active catalogue or setting. The
cache is not pruned while old handles may still refer to its contents. The five
standard background-music paths are reserved at boot, including absent files.

Save switching is available from Options. The pause menu does not permit a live
save change during a race. Busy saves or an invalid target retain the current
bank. Returning to the title is an in-process menu transition, not an EXE restart.

The 92 PC labels are reviewed in all fifteen languages. Andaluh labels use the
same protected EPA transcription as the existing catalog. The original/Retro
Rewind message-bank hash remains unchanged in this development milestone.

Only source, project-owned labels and the source-checked BPS patch are suitable
for publication. Disc files, generated native game binaries, raw message banks,
save/profile fixtures and private test captures remain local.
