import sys,struct,unittest,zlib
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'worker'))
from bps_patch import apply


def number(value):
    output=bytearray()
    while True:
        byte=value&127;value>>=7
        if value==0:output.append(byte|128);return bytes(output)
        output.append(byte);value-=1


def fixture(source,target,instructions):
    patch=b'BPS1'+number(len(source))+number(len(target))+number(0)+instructions
    patch+=struct.pack('<II',zlib.crc32(source),zlib.crc32(target))
    return patch+struct.pack('<I',zlib.crc32(patch))


class BpsTests(unittest.TestCase):
    def test_source_literal_and_overlapping_target_copy(self):
        source=b'ABC';target=b'ABCCC!'
        operations=number((3-1)*4)+number((2-1)*4+3)+number(2*2)+number(1)+b'!'
        self.assertEqual(apply(source,fixture(source,target,operations)),target)

    def test_source_copy_with_relative_offset(self):
        source=b'123ABCDE';target=b'ABCDE'
        patch=fixture(source,target,number((5-1)*4+2)+number(3*2))
        self.assertEqual(apply(source,patch),target)

    def test_reject_wrong_source_damaged_crc_and_oversized_output(self):
        patch=fixture(b'old',b'new',number((3-1)*4+1)+b'new')
        for source,data,maximum in [(b'bad',patch,100),(b'old',patch[:-1]+b'X',100),(b'old',patch,2)]:
            with self.assertRaises(ValueError):apply(source,data,max_output=maximum)

    def test_reject_copy_outside_existing_output(self):
        patch=fixture(b'input',b'A',number(3)+number(0))
        with self.assertRaises(ValueError):apply(b'input',patch)


if __name__=='__main__':unittest.main()
