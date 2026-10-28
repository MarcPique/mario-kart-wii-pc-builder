# Credits and source licenses

This project is a GPL-3.0 fork of [WiiCompiled](https://github.com/patchzyy/Wiicompiled), pinned to v0.9.3, commit `f02d000ba8e67240a04d95676da89023f781222f`. Its copyright and license notices are retained in the repository. The installer retrieves its original toolkit from the upstream release and verifies SHA-256 before use.

- **Retro Rewind 6.13.4 / Retro WFC**: official downloads from `cdn.update.rwfc.net` and `rwfc.net`. The mod is downloaded directly during installation, rather than republished in this repository or installer. Its own licenses and credits remain applicable. Server rules: <https://rwfc.net/rules>.
- **Pulsar**, MelgMKW and contributors: public UI definitions informed the native menu integration. <https://github.com/MelgMKW/Pulsar>.
- **nlohmann/json 3.12.0**, Niels Lohmann, MIT. The complete MIT notice is preserved in `runtime/third_party/pc_json.hpp`.
- **CPython 3.13.17 embeddable distribution**, Python Software Foundation license. Its `LICENSE.txt` is retained in the embedded runtime. Source and license: <https://www.python.org/downloads/release/python-31317/> and <https://docs.python.org/3/license.html>.
- **.NET 10 / Windows Forms**, Microsoft and contributors, MIT. The installer is self-contained; no system .NET or Python installation is required. <https://github.com/dotnet/runtime>, <https://github.com/dotnet/winforms>.
- **Aurora, LLVM-MinGW, CMake, Ninja, Dawn, SDL and the other upstream toolkit dependencies** retain their upstream licenses and source references distributed by WiiCompiled.

The installer icon is an original checkered flag drawn by source code. The game icon and native language bank are generated locally from the user's disc; they are never embedded in the public installer. Documentation screenshots show the game running and are not reusable game assets. Mario Kart Wii, its characters, music and trademarks belong to Nintendo and their respective owners. This project is unofficial and is not endorsed by Nintendo, WiiCompiled, Retro Rewind or Retro WFC.

Development assistance: Codex (OpenAI, GPT-6), with local compilation, validation and in-game testing. No AI-generated art or audio is included.
