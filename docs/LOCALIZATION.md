# Languages and existing fonts

Version 1.1.0 supports `es`, `en`, `fr`, `de`, `it`, `nl`, `cs`, `fi`, `ru`, `tr`, `es-419`, `ca`, `gl`, `eu` and `and_ES`. Language changes update loaded messages in the same game process. They apply to original game messages, Retro Rewind messages and the native PC options. Course/character artwork, embedded images, voice recordings and untranslated names are not replaced.

Live announcements supplied by the Retro WFC server are displayed as received. They may be in English even when the local menus use another language; they are not part of the installed translation bank.

The original five PAL languages come from the user's disc plus Retro Rewind overrides. Dutch, Czech, Finnish, Russian, Turkish and Latin American Spanish retain the localized text bundled with Retro Rewind 6.13.4. Invariant names and technical labels missing from those banks are preserved rather than machine-translated. Catalan, Galician and Basque game text started as machine translation and has completed the contextual review described below. Version 1.1.0 promotes these languages to reviewed status and removes the beta notice and asterisks in both the installer and native menus. These are not the existing community translation projects or professionally verified translations. Andaluh is an EPA transliteration of the Spanish bank, generated with Andalugeeks' library; it is not a separately translated Spanish dialect recording.

## Completed contextual review in 1.1.0

- Review all **82 current PC messages in all 15 languages** (1,230 entries). These project-owned labels live in `pc-builder/maintainer/reviewed_pc.json`. The runtime consults this catalog for the original five languages as well; compiled fallback strings do not hide corrections. The previous previews had 84 and then 83 labels; obsolete room-plural and beta-notice entries are now removed.
- Inspect **1,543 distinct game-text spans per language** for Catalan, Galician and Basque, including 214 newly joined multiline contexts. Correct race/course/cup/drift terminology, elimination and team counts, ghost challenges, menus, confirmations, error-code prompts and messages that lost their final instruction. The private review records both replacements and accepted text.
- Treat BMG line breaks as layout. Translate whole prose spans and reflow afterwards, retaining embedded button/variable commands and C-style arguments such as `%ls` and `%04d` exactly. Preserve invariant names/credits and use canonical vehicle/track names where a reviewed localized name is unavailable.
- Protect product names, file formats, engine classes and controls during Andaluh transcription. `Dolphin`, `ZIP/7z`, `Retro WFC`, `200cc` and button letters remain recognizable.
- Replace Retro Rewind's obsolete restart-language instruction in all fifteen banks with the immediate-change help. Correct the inherited English automatic-registration typo and duplicated Spanish friend-invitation wording.

The structural audit checks **90,720 records** (3,024 IDs × 15 languages × menu/race contexts), font coverage and command/format preservation. Live switching and screenshots are checked separately. These checks and the editorial pass improve meaning and consistency; they do **not** certify native-speaker accuracy of every inherited Retro Rewind translation, name or layout. The completed review status records the editorial acceptance of every required generated span, not professional or independent native-speaker certification.

The final review additionally corrects stunts terminology, invitation and multiplayer phrasing, plural forms, elimination templates and canonical arena names. Release generation with `--reviewed-release` requires an explicit, nonempty accepted review for all 1,543 required spans in each of Catalan, Galician and Basque. A public installer refuses a pack without `localization_status: reviewed`; the generator still validates font coverage, command bytes and format arguments before producing it. Metadata preserves `machine_translated_game_text` as origin information alongside `reviewed_languages` and `review_revision`, rather than rewriting the provenance.

The preview-5 pass also ensures Spanish repairs run **before** Andaluh transcription, and replaces the fixed plural room suffix with a translated label and count (`Rooms: 1`). This avoids incorrect singular/plural combinations without changing room discovery or matchmaking.

Only the fonts already present in the compatible installation are used. The maintainer audit verifies every required visible codepoint against `kart_kanji_font.brfnt` in the active Retro Rewind `UI/Font.szs`; the installer repeats that check before applying the patch. Japanese and Korean are excluded: the available fonts do not cover their complete message banks. Supported typography is normalized (for example a long dash becomes a hyphen); font images and glyphs are not generated or downloaded.

## Public artifacts and local installation

`pc-builder/worker/locales/extra-languages.bps` contains source-checked localization changes. `pack.json` records source, result and patch SHA-256 values, required codepoints and translation provenance. `menu.json` contains translations of this fork's own PC labels. The builder first reconstructs the base from the user's compatible disc and the downloaded mod, checks its hash and font coverage, applies the BPS patch with CRC validation, then checks the result hash. No complete extracted message bank, disc or model is included in the installer.

The runtime reads the bounds-checked `MKWLOC04` bank with dynamic language descriptors; it also accepts the previous five-language `MKWLOC03` format. BMG button, variable and formatting command bytes are retained during translation.

## Maintainer generation

End users need no translation model, GPU or Python installation. To reproduce the reviewed public patch, maintainers need their own compatible builder workspace, a **private** translation cache and the matching **private reviewed game-text file**. Its SHA-256 is recorded in `pack.json`; the game corpus and reviewed game text are distributed only as a BPS delta. The tooling lives in `pc-builder/maintainer`; `--audit-dir` includes raw game messages and must stay outside any release or commit.

```powershell
python pc-builder/maintainer/generate_localizations.py --workspace "F:\PrivateInstall\.mkwii-builder" --cache "F:\Private\translations.json" --review "F:\Private\reviewed-game.json" --reviewed-release --output "F:\Private\public-locales" --audit-dir "F:\Private\audit"
```

Install `andaluh==0.3.1` for assembly. To fill a missing cache locally, add `--model-dir` pointing at an independently obtained, license-reviewed model directory, then inspect the results before release: automatic output can omit instructions or choose the wrong sense of a word. Generation was tested with PyTorch 2.6.0+cu124, Transformers 5.15.1 and SentencePiece, using `facebook/nllb-200-distilled-600M` revision `f8d333a098d19b4fd9a8b18f94170487ad3f821d` (CC-BY-NC-4.0 model). All inference is local, using complete text spans, beam size 4 and a 512-token input limit. No disc text is sent to a translation service. Model weights, cache and extracted corpus are not redistributed. See the [third-party notices](../pc-builder/THIRD-PARTY-NOTICES.md) and upstream model/library terms before regenerating or reusing translations.

Tests cover control-code preservation, font parsing, BPS bounds/checksums, bank descriptors and bounds, and agreement between installer/runtime/patch language lists. Visual checks cover live switching and local gameplay; they do not certify linguistic accuracy or every message layout.
