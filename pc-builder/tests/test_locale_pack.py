from pathlib import Path
import hashlib,json,re,unittest

ROOT=Path(__file__).resolve().parents[2]
PACK=ROOT/'pc-builder/worker/locales'


class LocalePackTests(unittest.TestCase):
    def test_public_pack_has_completed_review_status(self):
        metadata=json.loads((PACK/'pack.json').read_text(encoding='utf-8'))
        self.assertEqual(metadata['localization_status'],'reviewed')
        self.assertEqual(metadata['reviewed_languages'],['ca','gl','eu'])
        self.assertGreaterEqual(metadata['review_revision'],2)
        for code in metadata['reviewed_languages']:self.assertGreater(metadata['reviewed_game_spans'][code],0)

    def test_installer_runtime_and_patch_offer_the_same_languages(self):
        metadata=json.loads((PACK/'pack.json').read_text(encoding='utf-8'))
        header=(ROOT/'runtime/include/pc_locale_catalog.h').read_text(encoding='utf-8')
        native=re.findall(r'Language\{"([^"]+)"',header)
        installer=(ROOT/'pc-builder/Installer/Program.cs').read_text(encoding='utf-8')
        declared=re.search(r'LanguageCodes\s*=\s*\[([^\]]+)\]',installer).group(1)
        self.assertEqual(native,metadata['languages'])
        self.assertEqual(re.findall(r'"([^"]+)"',declared),native)
        self.assertEqual(hashlib.sha256((PACK/'extra-languages.bps').read_bytes()).hexdigest(),metadata['patch_sha256'])

    def test_every_native_pc_message_has_a_translation(self):
        pattern=re.compile(r'T\(u"((?:[^"\\]|\\.)*)"\s*,u"((?:[^"\\]|\\.)*)"\s*,u"((?:[^"\\]|\\.)*)"\s*,u"((?:[^"\\]|\\.)*)"\s*,u"((?:[^"\\]|\\.)*)"\s*\)')
        expected=set()
        for directory in [ROOT/'runtime/include',ROOT/'runtime/src']:
            for path in directory.rglob('*'):
                if path.suffix in ('.h','.cpp','.inl'):
                    expected.update(json.loads('"'+item[1]+'"') for item in pattern.findall(path.read_text(encoding='utf-8')))
        document=json.loads((PACK/'menu.json').read_text(encoding='utf-8'))
        metadata=json.loads((PACK/'pack.json').read_text(encoding='utf-8'))
        allowed=set(metadata['required_codepoints'])
        self.assertEqual(set(document['languages']),set(metadata['languages']))
        for code,entries in document['languages'].items():
            with self.subTest(language=code):
                self.assertEqual(set(entries),expected)
                for text in entries.values():
                    self.assertIsInstance(text,str)
                    self.assertNotIn('\0',text)
                    self.assertNotIn('\x1a',text)
                    self.assertTrue({ord(c) for c in text if ord(c)>32}.issubset(allowed))


if __name__=='__main__':unittest.main()
