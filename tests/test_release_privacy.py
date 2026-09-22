import ctypes
import io
import os
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from package_release import private_scan


class ReleasePrivacy(unittest.TestCase):
    def test_account_and_machine_names_rejected_in_both_encodings(self):
        with patch.dict(os.environ, {'USERNAME': 'FixtureAccount42', 'COMPUTERNAME': 'FixtureMachine42'}):
            for value in ('FixtureAccount42', 'fixturemachine42'):
                for encoding in ('utf-8', 'utf-16-le'):
                    with self.subTest(value=value, encoding=encoding):
                        with self.assertRaisesRegex(ValueError, 'Personal data'):
                            private_scan('example.txt', value.encode(encoding))
            private_scan('example.txt', b'Public mod source and instructions.')

    def test_nested_archive_content_is_checked(self):
        data = io.BytesIO()
        with zipfile.ZipFile(data, 'w') as archive:
            archive.writestr('example.txt', str(Path.home()))
        with self.assertRaisesRegex(ValueError, 'Personal data'):
            private_scan('runtime.zip', data.getvalue())

    def test_local_short_account_path_is_rejected_without_hardcoding_it(self):
        buffer = ctypes.create_unicode_buffer(32768)
        length = ctypes.windll.kernel32.GetShortPathNameW(str(Path.home()), buffer, len(buffer))
        if not 0 < length < len(buffer):
            self.skipTest('No Windows short path available')
        short_name = Path(buffer.value).name
        with self.assertRaisesRegex(ValueError, 'Personal data'):
            private_scan('example.txt', short_name.encode('utf-16-le'))


if __name__ == '__main__':
    unittest.main()
