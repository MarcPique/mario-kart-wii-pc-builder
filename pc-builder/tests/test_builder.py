import unittest,tempfile,zipfile,sys,struct,argparse,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'worker'))
import build_game
from prepare_original_ui import pack,read

class BuilderTests(unittest.TestCase):
    def test_worker_imports_in_isolated_runtime(self):
        result=subprocess.run([sys.executable,'-I',str(ROOT/'worker/build_game.py'),'--help'],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn('--stage-update',result.stdout)

    def test_rejects_linked_workspace_before_writing_a_marker_outside_the_install(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);image=root/'own.wbfs';image.write_bytes(b'fixture')
            dest=root/'game';dest.mkdir();outside=root/'outside';outside.mkdir();link=dest/'.mkwii-builder'
            try:os.symlink(outside,link,target_is_directory=True)
            except OSError:self.skipTest('Symbolic links unavailable on this host')
            try:
                with self.assertRaises(ValueError):build_game.main(argparse.Namespace(iso=str(image),install_dir=str(dest)))
                self.assertEqual(list(outside.iterdir()),[])
            finally:link.unlink()

    def test_retired_multisession_sources_removed_without_touching_user_data(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);work=root/'work';payload=root/'payload';(payload/'patches').mkdir(parents=True)
            workspace=work/'Install/BuildWorkspace';old=workspace/'runtime/src/pc_local_sessions.cpp';old.parent.mkdir(parents=True);old.write_text('retired')
            keep=workspace/'runtime/src/hle.cpp';keep.write_text('upstream')
            save=work/'UserData/Saves/slot.dat';save.parent.mkdir(parents=True);save.write_bytes(b'own save')
            build_game.apply_sources(work,payload)
            self.assertFalse(old.exists());self.assertEqual(keep.read_text(),'upstream');self.assertEqual(save.read_bytes(),b'own save')

    def test_source_update_invalidates_newer_cached_object_only_on_content_change(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);payload=root/'payload';source=payload/'patches/runtime/src/example.cpp';source.parent.mkdir(parents=True)
            work=root/'work';target=work/'Install/BuildWorkspace/runtime/src/example.cpp';target.parent.mkdir(parents=True)
            source.write_text('replacement');target.write_text('old source')
            os.utime(source,(1000,1000));os.utime(target,(2000,2000))
            build_game.apply_sources(work,payload)
            self.assertEqual(target.read_text(),'replacement')
            self.assertGreater(target.stat().st_mtime,2000)
            timestamp=target.stat().st_mtime_ns
            build_game.apply_sources(work,payload)
            self.assertEqual(target.stat().st_mtime_ns,timestamp)

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
