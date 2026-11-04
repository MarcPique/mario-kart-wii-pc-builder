# Dolphin texture packs

The PC runtime uses Aurora's Dolphin-compatible texture replacement loader. Unpacked **PNG and DDS** files named `tex1_WIDTHxHEIGHT[_m]_HASH[_PALETTEHASH]_FORMAT` are indexed recursively; palette hashes, wildcard hashes, DDS internal mipmaps and PNG/DDS `_mipN` sidecars are handled by that loader. Keep the filenames supplied by the pack author.

Copy a PAL or region-independent Mario Kart Wii pack into:

```text
Your installation/UserData/Load/Textures/RMCP01/
```

Subfolders are allowed. `UserData/Load/Textures/RMC/` also works for region-independent packs; PAL entries have higher priority when both supply the same texture. The older `UserData/texture_replacements/` directory remains supported at lower priority. ZIP and 7z files can be placed directly in those folders, including Dolphin resource packs with a `textures/RMCP01/` or `textures/RMC/` tree and a logo. Startup and Reload import them in a background worker. No system 7-Zip installation is needed: the builder downloads and verifies the standalone x64 7-Zip Extra 26.05 tool, installed with its license in the generated game. RAR archives must still be unpacked manually. A pack made only for another region may not match localized PAL images; use an appropriate PAL or all-regions pack.

The native **Advanced Options → Texture Packs** page enables/disables replacements, rescans the folders and opens the PAL folder. Startup also scans the folders automatically. Enabling, disabling and reloading do not require restarting the game. The count reports indexed base-texture registrations, not files already uploaded to the GPU. Streaming may briefly show the original before the replacement is ready.

Missing, corrupt or unsupported replacement images keep the original texture. Other game textures, including Retro Rewind custom content absent from a vanilla pack, continue using their originals. Installing a partial pack does not require filling every slot. If a pack looks wrong, disable it from the native page or remove it and select Reload.

Packs are optional and are not bundled, downloaded or uploaded automatically. Their licenses and credits remain their authors'. Original disc textures are never overwritten. Texture dump output is a private development aid and must not be published with this project.

References: [Dolphin texture matching](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/VideoCommon/HiresTextures.cpp), [Dolphin resource-pack specification](https://github.com/dolphin-emu/dolphin/blob/master/docs/ResourcePacks.md).

Archive imports use a SHA-256 cache under `UserData/Cache/TexturePacks`. Only completed imports are registered. Loose PNG/DDS entries have higher priority than archived packs; archive failures leave original textures available and do not prevent other valid packs from loading. The native page refreshes its import status and texture count when background work finishes. Decompression is not repeated for an unchanged completed pack, but its archive content hash is checked.

Encrypted archives, archive links, traversal/device paths, duplicate Windows paths and excessively large imports are rejected. Limits: 16 GiB compressed per archive, 512 MiB per extracted image, 32 GiB total images and 100,000 entries. Partial imports remain inactive. Keep the archive in the texture folder for future sessions; deleting it and selecting Reload removes its registrations. Do not rename the `tex1_` files inside a pack. The game renders originals while an imported replacement is unavailable or still streaming.
