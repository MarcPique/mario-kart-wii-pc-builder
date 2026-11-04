"""Translation regressions using only this project's own/synthetic messages."""
from pathlib import Path
import json,re,sys,unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'pc-builder/maintainer'))
from localization_helpers import translate_plain,transcribe_prose,PRINTF,shared_record


class ContextualLocalizationTests(unittest.TestCase):
    def test_layout_breaks_do_not_split_sentence_context(self):
        source='A texture pack\ncould not be loaded.'
        cache={'A texture pack could not be loaded.':'No se pudo cargar el paquete de texturas.'}
        translated=translate_plain(source,cache,set(range(65536)))
        self.assertEqual(' '.join(translated.split()),cache['A texture pack could not be loaded.'])
        self.assertIn('\n',translated)

    def test_wide_and_padded_format_arguments_survive(self):
        source='Player %ls: %04d-%04d-%04d, %%'
        translated=translate_plain(source,{'Player':'Jugador'},set(range(65536)))
        self.assertEqual(re.findall(PRINTF,translated),re.findall(PRINTF,source))
        self.assertIn('%ls',translated)

    def test_transcription_keeps_products_units_and_controls(self):
        source='Dolphin ZIP/7z PNG/DDS, Mario Kart Wii, Retro WFC: 200cc 60fps %ls A: back'
        result=transcribe_prose(source,str.upper)
        self.assertEqual(result,'Dolphin ZIP/7z PNG/DDS, Mario Kart Wii, Retro WFC: 200cc 60fps %ls A: BACK')

    def test_shared_name_requires_all_base_languages(self):
        table={code:{7:(1,b'\x00N\x00\x00')} for code in ('en','es','fr','de','it')}
        self.assertTrue(shared_record(table,7))
        table['fr'].clear()
        self.assertFalse(shared_record(table,7))

    def test_reviewed_pc_catalog_is_complete_and_keeps_identity_tokens(self):
        document=json.loads((ROOT/'pc-builder/maintainer/reviewed_pc.json').read_text(encoding='utf-8'))['languages']
        reference=document['en']
        for code,entries in document.items():
            self.assertEqual(set(entries),set(reference),code)
            for key,value in entries.items():
                self.assertTrue(value.strip(),(code,key))
                for token in ('Dolphin','ZIP/7z','PNG/DDS','Retro Rewind','Mario Kart'):
                    if token in key:self.assertIn(token,value,(code,key,token))
        self.assertEqual(document['es']['On'],'Sí')
        self.assertEqual(document['nl']['Not checked'],'Niet gecontroleerd')
        self.assertEqual(document['ru']['Options'],'Настройки')


if __name__=='__main__':unittest.main()
