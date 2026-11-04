# Languages and existing fonts

The 1.1.0 preview supports `es`, `en`, `fr`, `de`, `it`, `nl`, `cs`, `fi`, `ru`, `tr`, `es-419`, `ca`, `gl`, `eu` and `and_ES`. Language changes update loaded messages in the same game process. They apply to original game messages, Retro Rewind messages and the native PC options. Course/character artwork, embedded images, voice recordings and untranslated names are not replaced.

The original five PAL languages come from the user's disc plus Retro Rewind overrides. Dutch, Czech, Finnish, Russian, Turkish and Latin American Spanish use the localized text bundled with Retro Rewind 6.13.4; messages absent from those banks are supplemented automatically. Catalan, Galician and Basque game text is experimental machine translation, with reviewed PC labels and selected terminology. Their menu entries have an asterisk and their language page identifies the automatic translation. These are not the existing community translation projects or professionally verified translations. Andaluh is an EPA transliteration of the Spanish bank, generated with Andalugeeks' library; it is not a separately translated Spanish dialect recording.

Only the fonts already present in the compatible installation are used. The maintainer audit verifies every required visible codepoint against `kart_kanji_font.brfnt` in the active Retro Rewind `UI/Font.szs`; the installer repeats that check before applying the patch. Japanese and Korean are excluded: the available fonts do not cover their complete message banks. Supported typography is normalized (for example a long dash becomes a hyphen); font images and glyphs are not generated or downloaded.

## Public artifacts and local installation

`pc-builder/worker/locales/extra-languages.bps` contains source-checked localization changes. `pack.json` records source, result and patch SHA-256 values, required codepoints and translation provenance. `menu.json` contains translations of this fork's own PC labels. The builder first reconstructs the base from the user's compatible disc and the downloaded mod, checks its hash and font coverage, applies the BPS patch with CRC validation, then checks the result hash. No complete extracted message bank, disc or model is included in the installer.

The runtime reads the bounds-checked `MKWLOC04` bank with dynamic language descriptors; it also accepts the previous five-language `MKWLOC03` format. BMG button, variable and formatting command bytes are retained during translation.

## Maintainer generation

End users need no translation model, GPU or Python installation. To regenerate the public patch, maintainers need their own compatible builder workspace and a **private** translation cache. The tooling lives in `pc-builder/maintainer`; `--audit-dir` includes raw game messages and must stay outside any release or commit.

```powershell
python pc-builder/maintainer/generate_localizations.py --workspace "F:\PrivateInstall\.mkwii-builder" --cache "F:\Private\translations.json" --output "F:\Private\public-locales" --audit-dir "F:\Private\audit"
```

Install `andaluh==0.3.1` for assembly. To fill a missing cache locally, add `--model-dir` pointing at an independently obtained, license-reviewed model directory. Generation was tested with PyTorch 2.6.0+cu124, Transformers 5.15.1 and SentencePiece, using `facebook/nllb-200-distilled-600M` revision `f8d333a098d19b4fd9a8b18f94170487ad3f821d` (CC-BY-NC-4.0 model). All inference is local, using sentence-sized fragments, beam size 4 and a 512-token input limit. No disc text is sent to a translation service. Model weights, cache and extracted corpus are not redistributed. See the [third-party notices](../pc-builder/THIRD-PARTY-NOTICES.md) and upstream model/library terms before regenerating or reusing translations.

Tests cover control-code preservation, font parsing, BPS bounds/checksums, bank descriptors and bounds, and agreement between installer/runtime/patch language lists. Visual checks cover live switching and local gameplay; they do not certify linguistic accuracy or every message layout.
