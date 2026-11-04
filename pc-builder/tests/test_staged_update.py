import hashlib,json,os,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'worker'))
import staged_update as update

class StagedUpdateTests(unittest.TestCase):
    def setUp(self):
        self.folder=tempfile.TemporaryDirectory();self.addCleanup(self.folder.cleanup)
        self.root=Path(self.folder.name)/'game';self.root.mkdir()
        self.source=self.root/'.mkwii-builder/preview'
        self.write(self.root/'GameData/old.dat',b'old game')
        self.write(self.root/'GameData.previous/older.dat',b'older game')
        self.write(self.root/'Mario Kart Wii.exe',b'MZ-old-test-launcher')
        self.write(self.root/'portable.txt',b'old marker')
        self.write(self.root/'mkwii-pc-install.json',json.dumps({'game_id':'RMCP01','builder_version':'0.0.0'}).encode())
        self.write(self.root/'UserData/Config.toml',b'preserved settings')
        self.write(self.root/'UserData/Saves/own.dat',b'preserved save')
        self.before={p.relative_to(self.root).as_posix():p.read_bytes() for p in self.root.rglob('*') if p.is_file()}
        native=b'MZ-new-test-native';self.write(self.source/'GameData/Install/RetroRewind/RetroRewind.exe',native)
        self.write(self.source/'GameData/portable-data.txt',b'new game marker')
        self.write(self.source/'GameData/asset.bin',b'new asset')
        self.write(self.source/'Mario Kart Wii.exe',b'MZ-new-test-launcher')
        self.write(self.source/'portable.txt',b'new marker')
        self.write(self.source/'mkwii-pc-install.json',json.dumps({'game_id':'RMCP01','builder_version':'1.3.0','native_sha256':hashlib.sha256(native).hexdigest()}).encode())

    def write(self,path,data):path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
    def unchanged(self):
        for name,data in self.before.items():self.assertEqual((self.root/name).read_bytes(),data,name)
    def staged(self):return update.prepare(self.source,self.root)

    def test_prepare_does_not_modify_the_running_installation_or_userdata(self):
        candidate=self.staged();self.unchanged()
        self.assertFalse(self.source.exists());self.assertTrue((candidate/update.RECEIPT).is_file())
        self.assertEqual(update.verify(candidate)['builder_version'],'1.3.0')

    def test_workspace_lock_excludes_a_second_builder_and_releases_after_error(self):
        work=self.root/'.mkwii-builder'
        with update.build_lock(work):
            with self.assertRaises(ValueError):
                with update.build_lock(work):pass
            with update.build_lock(self.root/'other-workspace'):pass
        with self.assertRaises(RuntimeError):
            with update.build_lock(work):raise RuntimeError('simulated worker failure')
        with update.build_lock(work):pass

    def test_publishes_full_candidate_preserves_user_and_archives_older_backup(self):
        self.staged();update.apply(self.root)
        self.assertEqual((self.root/'GameData/asset.bin').read_bytes(),b'new asset')
        self.assertEqual((self.root/'Mario Kart Wii.exe').read_bytes(),b'MZ-new-test-launcher')
        self.assertEqual((self.root/'GameData.previous/old.dat').read_bytes(),b'old game')
        self.assertEqual(list((self.root/'Backups').glob('update-*/older-game-data/older.dat'))[0].read_bytes(),b'older game')
        self.assertEqual((self.root/'UserData/Saves/own.dat').read_bytes(),b'preserved save')
        self.assertEqual((self.root/'UserData/Config.toml').read_bytes(),b'preserved settings')
        self.assertFalse((self.root/update.CANDIDATE).exists());self.assertFalse((self.root/update.JOURNAL).exists())

    def test_failed_root_file_publication_restores_game_metadata_and_previous_backup(self):
        self.staged();original=update.replace_file;failed=False
        def fail_once(source,target):
            nonlocal failed
            if target.name=='mkwii-pc-install.json' and not failed:failed=True;raise OSError('simulated full disk')
            return original(source,target)
        with patch.object(update,'replace_file',side_effect=fail_once),self.assertRaises(OSError):update.apply(self.root)
        self.unchanged();self.assertTrue((self.root/update.CANDIDATE/'GameData/asset.bin').is_file())
        update.apply(self.root);self.assertTrue((self.root/'GameData/asset.bin').is_file())

    def test_process_interruption_after_each_game_rename_can_be_recovered(self):
        for target in ['old','new']:
            with self.subTest(target=target):
                if not (self.root/update.CANDIDATE).exists():self.staged()
                original=Path.rename;fired=False
                def interrupt(path,to):
                    nonlocal fired
                    result=original(path,to)
                    trigger=self.root/'GameData' if target=='old' else self.root/update.CANDIDATE/'GameData'
                    if path==trigger and not fired:fired=True;raise KeyboardInterrupt('simulated terminated worker')
                    return result
                with patch.object(Path,'rename',interrupt),self.assertRaises(KeyboardInterrupt):update.apply(self.root)
                self.assertTrue((self.root/update.JOURNAL).is_file())
                self.assertTrue(update.recover(self.root));self.unchanged()

    def test_committed_update_is_not_rolled_back_after_cleanup_interruption(self):
        self.staged();original=Path.rename
        def interrupt(path,to):
            if path==self.root/update.CANDIDATE:raise KeyboardInterrupt('simulated interrupted finalization')
            return original(path,to)
        with patch.object(Path,'rename',interrupt),self.assertRaises(KeyboardInterrupt):update.apply(self.root)
        self.assertTrue(update.recover(self.root))
        self.assertEqual((self.root/'GameData/asset.bin').read_bytes(),b'new asset')
        self.assertEqual((self.root/'GameData.previous/old.dat').read_bytes(),b'old game')

    def test_refuses_changed_missing_or_extra_payload_before_moving_existing_game(self):
        candidate=self.staged();asset=candidate/'GameData/asset.bin'
        for alteration in ['change','remove','extra']:
            with self.subTest(alteration=alteration):
                asset.write_bytes(b'new asset')
                extra=candidate/'GameData/unlisted.bin';extra.unlink(missing_ok=True)
                if alteration=='change':asset.write_bytes(b'corrupt')
                elif alteration=='remove':asset.unlink()
                else:extra.write_bytes(b'unlisted')
                with self.assertRaises(ValueError):update.apply(self.root)
                self.unchanged();self.assertFalse((self.root/update.JOURNAL).exists())

    def test_rejects_manifest_traversal_and_userdata_paths(self):
        candidate=self.staged();receipt=candidate/update.RECEIPT;original=receipt.read_bytes()
        for name in ['../outside','/absolute','GameData/../../outside','GameData\\..\\outside','UserData/Saves/own.dat','GameData/CON','GameData/a:stream','GameData/a/../b']:
            with self.subTest(name=name):
                value=json.loads(original);value['files'][name]={'bytes':1,'sha256':'0'*64};receipt.write_text(json.dumps(value))
                with self.assertRaises(ValueError):update.apply(self.root)
                self.unchanged()

    def test_rejects_user_data_files_in_candidate(self):
        self.write(self.source/'UserData/Config.toml',b'must never be published')
        with self.assertRaises(ValueError):self.staged()
        self.unchanged()

    def test_refuses_replacing_an_already_prepared_candidate(self):
        candidate=self.staged();saved=(candidate/update.RECEIPT).read_bytes();self.source.mkdir(parents=True)
        with self.assertRaises(ValueError):update.prepare(self.source,self.root)
        self.assertEqual((candidate/update.RECEIPT).read_bytes(),saved);self.unchanged()

    def test_failed_package_is_preserved_outside_the_next_startup_path(self):
        candidate=self.staged();(candidate/'GameData/asset.bin').write_bytes(b'corrupt')
        with self.assertRaises(ValueError):update.apply(self.root)
        update.quarantine_failed(self.root)
        self.unchanged();self.assertFalse(candidate.exists())
        self.assertEqual(list((self.root/'Backups').glob('rejected-update-*/GameData/asset.bin'))[0].read_bytes(),b'corrupt')

    def test_rejects_duplicate_manifest_keys(self):
        candidate=self.staged();receipt=candidate/update.RECEIPT
        receipt.write_text('{"format":1,"format":1,"game_id":"RMCP01"}')
        with self.assertRaises(ValueError):update.apply(self.root)
        self.unchanged()

    def test_rejects_reparse_destination_before_mutating_external_files(self):
        candidate=self.staged();outside=self.root.parent/'outside';outside.mkdir()
        (outside/'keep').write_bytes(b'outside data')
        link=self.root/'Backups'
        try:os.symlink(outside,link,target_is_directory=True)
        except OSError:self.skipTest('Symbolic links unavailable on this host')
        try:
            with self.assertRaises(ValueError):update.apply(self.root)
            self.assertEqual((outside/'keep').read_bytes(),b'outside data');self.unchanged()
        finally:link.unlink()

    def test_rejects_corrupted_recovery_backup_without_changing_live_files(self):
        self.staged();original=Path.rename
        def interrupt(path,to):
            result=original(path,to)
            if path==self.root/'GameData':raise KeyboardInterrupt()
            return result
        with patch.object(Path,'rename',interrupt),self.assertRaises(KeyboardInterrupt):update.apply(self.root)
        state=json.loads((self.root/update.JOURNAL).read_text());backup=self.root/state['backup']/'root/Mario Kart Wii.exe'
        backup.write_bytes(b'damaged backup')
        with self.assertRaises(ValueError):update.recover(self.root)
        self.assertTrue((self.root/'GameData.previous/old.dat').is_file())
        self.assertEqual((self.root/'UserData/Saves/own.dat').read_bytes(),b'preserved save')

if __name__=='__main__':unittest.main()
