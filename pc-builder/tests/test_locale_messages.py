import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'worker'))
from locale_messages import message_parts, transform_message, font_characters


class LocaleMessageTests(unittest.TestCase):
    def test_preserve_embedded_player_and_button_commands(self):
        command = b'\0\x1a\x08\x01\0\0\0\x09'
        source = 'Hola '.encode('utf-16-be') + command + ', vuelve'.encode('utf-16-be') + b'\0\0'
        translated = transform_message(source, lambda s: {'Hola ': 'Kaixo ', ', vuelve': ', itzuli'}[s])
        self.assertEqual(message_parts(translated), ['Kaixo ', command, ', itzuli'])

    def test_reject_damaged_or_injected_commands(self):
        for message in [b'\0H', b'\0\x1a', b'\0\x1a\x07\x01\0\0', b'\0\0\0A', b'A']:
            with self.subTest(message=message), self.assertRaises(ValueError):
                message_parts(message)
        with self.assertRaises(ValueError):
            transform_message(b'\0H\0\0', lambda s: '\x1a')

    def test_reject_invalid_font_header(self):
        with self.assertRaises(ValueError):
            font_characters(b'not a font')


if __name__ == '__main__':
    unittest.main()
