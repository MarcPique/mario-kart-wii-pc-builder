import unittest,tempfile,zipfile,sys,struct,argparse
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'worker'))
import build_game
from prepare_original_ui import pack,read

class BuilderTests(unittest.TestCase):
    def test_archive_round_trip(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'example.szs';files={'bg/ctrl/test.brctr':b'local mock resource','message/own.txt':'Español'.encode()}
            path.write_bytes(pack(files));self.assertEqual(read(path),files)

    def test_reject_zip_traversal_and_symlinks(self):
        for name in ['../outside','/absolute','C:/outside','safe/../../outside']:
            with self.subTest(name=name),tempfile.TemporaryDirectory() as folder:
                p=Path(folder);archive=p/'input.zip'
                with zipfile.ZipFile(archive,'w') as z:z.writestr(name,'test')
                with self.assertRaises(ValueError):build_game.safe_extract(archive,p/'output')
                self.assertFalse((p/'outside').exists())
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder);entry=zipfile.ZipInfo('link');entry.external_attr=0xa1ff<<16
            with zipfile.ZipFile(p/'input.zip','w') as z:z.writestr(entry,'../target')
            with self.assertRaises(ValueError):build_game.safe_extract(p/'input.zip',p/'output')

    def test_desktop_footer_and_existing_file(self):
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder);launcher=p/'launcher.exe';launcher.write_bytes(b'MZ-test-launcher');desktop=p/'Desktop';desktop.mkdir();existing=desktop/'Mario Kart Wii.exe';existing.write_bytes(b'keep existing')
            destination=p/'Instalación PC';new=build_game.desktop_exe(launcher,desktop,destination);data=new.read_bytes();magic,size=struct.unpack('<8sQ',data[-16:])
            self.assertEqual(existing.read_bytes(),b'keep existing');self.assertEqual(magic,b'MKWLINK1');self.assertEqual(data[-16-size:-16].decode(),str(destination.resolve()))

    def test_refuse_unowned_installation(self):
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder);iso=p/'own.iso';iso.write_bytes(b'test');dest=p/'game';(dest/'GameData').mkdir(parents=True);save=dest/'GameData/keep';save.write_bytes(b'keep')
            options=argparse.Namespace(iso=str(iso),install_dir=str(dest))
            with self.assertRaisesRegex(ValueError,'otra instalación'):build_game.main(options)
            self.assertEqual(save.read_bytes(),b'keep')

    def test_cleanup_requires_owned_workspace(self):
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder);target=p/'keep';target.mkdir()
            with self.assertRaises(ValueError):build_game.remove_owned(target,p)
            self.assertTrue(target.exists())
            (p/'.mkwii-builder-workspace').touch()
            with self.assertRaises(ValueError):build_game.remove_owned(p,p)
            build_game.remove_owned(target,p);self.assertFalse(target.exists())

    def test_download_offline_rejects_wrong_hash(self):
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder);(p/'test').write_bytes(b'bad')
            with self.assertRaises(ValueError):build_game.download({'name':'test','sha256':'0'*64},p,offline=True)

if __name__=='__main__':unittest.main()
