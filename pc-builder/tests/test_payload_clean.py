import importlib.util
from pathlib import Path
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'prepare_payload.py'
SPEC = importlib.util.spec_from_file_location('prepare_payload', SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class PayloadSafetyTests(unittest.TestCase):
    def test_existing_files_are_not_packaged_or_deleted(self):
        with tempfile.TemporaryDirectory() as directory:
            marker = Path(directory) / 'unrelated.txt'
            marker.write_text('keep', encoding='utf-8')
            with self.assertRaises(ValueError):
                MODULE.prepare(directory)
            self.assertEqual(marker.read_text(encoding='utf-8'), 'keep')


if __name__ == '__main__':
    unittest.main()
