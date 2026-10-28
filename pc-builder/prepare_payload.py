"""Package source-only patches and the Python worker; no game or profile files."""
from pathlib import Path
import json,shutil,zipfile,hashlib,sys
ROOT=Path(__file__).resolve().parent;REPO=ROOT.parent
def prepare(destination):
    destination=Path(destination);destination.mkdir(parents=True,exist_ok=True)
    shutil.copytree(ROOT/'worker',destination/'worker',dirs_exist_ok=True,ignore=shutil.ignore_patterns('__pycache__','*.pyc'))
    for relative in json.loads((ROOT/'source-changes.json').read_text()):
        if relative.endswith('pc_options_menu.inl'):continue
        p=REPO/relative;target=destination/'patches'/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target)
    shutil.copytree(ROOT/'patches/aurora-main',destination/'patches/aurora-main',dirs_exist_ok=True)
    shutil.copy2(REPO/'LICENSE',destination/'LICENSE')
    if (ROOT/'licenses').is_dir():shutil.copytree(ROOT/'licenses',destination/'licenses',dirs_exist_ok=True)
    for name in ['THIRD-PARTY-NOTICES.md','GAME-README.txt']:
        if (ROOT/name).exists():shutil.copy2(ROOT/name,destination/name)
    # Complete allowlist manifest used by the installer to verify extracted payload files.
    files={p.relative_to(destination).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in destination.rglob('*') if p.is_file() and p.name!='payload-files.json'}
    (destination/'payload-files.json').write_text(json.dumps(files,sort_keys=True,indent=2))
    return files
if __name__=='__main__':
    files=prepare(sys.argv[1]);print(json.dumps({'payload_files':len(files)}))
