"""Preparación verificable y publicación transaccional; nunca escribe UserData."""
from pathlib import Path, PurePosixPath
import hashlib,json,os,re,shutil,stat,uuid
from contextlib import contextmanager

CANDIDATE='.mkwii-update'
RECEIPT='ready.json'
JOURNAL='mkwii-pc-update-transaction.json'
ROOT_FILES=frozenset({'Mario Kart Wii.exe','portable.txt','LICENSE',
    'THIRD-PARTY-NOTICES.md','LEEME.txt','mkwii-pc-install.json'})
REQUIRED=frozenset({'Mario Kart Wii.exe','portable.txt','mkwii-pc-install.json',
    'GameData/portable-data.txt','GameData/Install/RetroRewind/RetroRewind.exe'})
MAX_BYTES=32*1024**3
class WorkspaceBusyError(ValueError):pass
class InstallationBusyError(ValueError):pass

@contextmanager
def build_lock(work,timeout=0,name='build.lock'):
    """Impide que dos builders modifiquen el mismo workspace a la vez."""
    import time
    work=Path(work);work.mkdir(parents=True,exist_ok=True);path=plain(work/name,work)
    deadline=time.monotonic()+timeout
    if os.name=='nt':
        import ctypes
        kernel=ctypes.WinDLL('kernel32',use_last_error=True)
        kernel.CreateFileW.argtypes=[ctypes.c_wchar_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p]
        kernel.CreateFileW.restype=ctypes.c_void_p;kernel.CloseHandle.argtypes=[ctypes.c_void_p]
        while True:
            handle=kernel.CreateFileW(str(path),0xC0000000,0,None,4,0x80,None)
            if handle!=ctypes.c_void_p(-1).value:break
            if time.monotonic()>=deadline:raise WorkspaceBusyError('Otra sesión está usando esta instalación')
            time.sleep(.1)
        try:yield
        finally:kernel.CloseHandle(handle)
    else:
        import fcntl
        with path.open('a+b') as stream:
            while True:
                try:fcntl.flock(stream,fcntl.LOCK_EX|fcntl.LOCK_NB);break
                except BlockingIOError:
                    if time.monotonic()>=deadline:raise WorkspaceBusyError('Otra sesión está usando esta instalación')
                    time.sleep(.1)
            try:yield
            finally:fcntl.flock(stream,fcntl.LOCK_UN)

def digest(path):
    value=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):value.update(block)
    return value.hexdigest()

def plain(path,root):
    """Rechaza enlaces en el destino y en todos sus padres dentro de la raíz."""
    root=Path(root);path=Path(path)
    if path!=root and root not in path.parents:raise ValueError('Ruta fuera de la instalación')
    current=path
    while True:
        try:
            info=current.lstat()
            if stat.S_ISLNK(info.st_mode) or getattr(info,'st_file_attributes',0)&0x400:
                raise ValueError('Enlace o punto de análisis en la actualización')
        except FileNotFoundError:pass
        if current==root:break
        current=current.parent
    return path

def relative(value):
    if not isinstance(value,str) or not value or '\\' in value or ':' in value:
        raise ValueError('Ruta de paquete inválida')
    path=PurePosixPath(value)
    if path.is_absolute() or path.as_posix()!=value:raise ValueError('Ruta de paquete no canónica')
    for part in path.parts:
        device=part.split('.')[0].lower()
        if part in ('.','..') or part.endswith(('.', ' ')) or any(ord(c)<32 or c in '<>"|?*' for c in part):raise ValueError('Componente de paquete inválido')
        if device in ('con','prn','aux','nul') or re.fullmatch(r'(com|lpt)[0-9]',device):raise ValueError('Nombre de dispositivo en paquete')
    if value not in ROOT_FILES and not value.startswith('GameData/'):raise ValueError('El paquete intenta modificar datos ajenos al juego')
    return value

def inventory(folder):
    folder=Path(folder);plain(folder,folder);result={};total=0;seen=set()
    for parent,dirs,files in os.walk(folder,followlinks=False):
        for name in dirs:plain(Path(parent)/name,folder)
        for name in files:
            path=plain(Path(parent)/name,folder);name=path.relative_to(folder).as_posix()
            if name==RECEIPT:continue
            name=relative(name)
            if name.casefold() in seen:raise ValueError('Rutas duplicadas en el paquete')
            seen.add(name.casefold())
            if not path.is_file():raise ValueError('Entrada de paquete no regular')
            size=path.stat().st_size;total+=size
            if total>MAX_BYTES or len(result)>=100000:raise ValueError('El paquete supera el límite de tamaño')
            result[name]={'bytes':size,'sha256':digest(path)}
    if not REQUIRED<=result.keys():raise ValueError('Faltan archivos esenciales en la actualización')
    return result

def read_json(path,limit=32*1024*1024):
    if path.stat().st_size>limit:raise ValueError('Metadatos demasiado grandes')
    def unique(items):
        result={}
        for key,value in items:
            if key in result:raise ValueError('Clave JSON duplicada')
            result[key]=value
        return result
    return json.loads(path.read_text(encoding='utf-8'),object_pairs_hook=unique)

def atomic_json(path,value):
    temporary=path.with_name(path.name+'.'+uuid.uuid4().hex+'.tmp')
    plain(temporary,path.parent)
    with temporary.open('x',encoding='utf-8',newline='\n') as stream:
        json.dump(value,stream,ensure_ascii=False,sort_keys=True,indent=2)
        stream.flush();os.fsync(stream.fileno())
    os.replace(temporary,path)

def owned(root):
    file=plain(root/'mkwii-pc-install.json',root)
    if not file.is_file() or read_json(file).get('game_id')!='RMCP01':raise ValueError('La instalación no pertenece a este builder')

def verify(candidate):
    candidate=Path(candidate);plain(candidate,candidate)
    receipt=read_json(plain(candidate/RECEIPT,candidate))
    if not isinstance(receipt,dict) or receipt.get('format')!=1 or receipt.get('game_id')!='RMCP01':raise ValueError('Formato de actualización incompatible')
    files=receipt.get('files')
    if not isinstance(files,dict) or not REQUIRED<=files.keys():raise ValueError('Manifiesto de actualización incompleto')
    for name,entry in files.items():
        relative(name)
        if not isinstance(entry,dict) or type(entry.get('bytes')) is not int or not 0<=entry['bytes']<=MAX_BYTES or not isinstance(entry.get('sha256'),str) or not re.fullmatch('[0-9a-f]{64}',entry['sha256']):raise ValueError('Huella de actualización inválida')
    if inventory(candidate)!=files:raise ValueError('La actualización ha cambiado o está incompleta')
    report=read_json(candidate/'mkwii-pc-install.json')
    if report.get('game_id')!='RMCP01' or report.get('native_sha256')!=files['GameData/Install/RetroRewind/RetroRewind.exe']['sha256'] or report.get('builder_version')!=receipt.get('builder_version'):raise ValueError('La identidad del juego no coincide con el paquete')
    return receipt

def prepare(source,root,allow_new=False):
    root=Path(root).resolve();source=Path(source).resolve()
    if source==root or root not in source.parents:raise ValueError('Preparación fuera de la instalación')
    plain(source,root)
    if not allow_new:owned(root)
    candidate=plain(root/CANDIDATE,root)
    if candidate.exists():raise ValueError('Ya hay una actualización preparada; aplícala antes de preparar otra')
    if (root/JOURNAL).exists():raise ValueError('Hay una actualización pendiente de recuperación')
    files=inventory(source);report=read_json(source/'mkwii-pc-install.json')
    if report.get('game_id')!='RMCP01' or report.get('native_sha256')!=files['GameData/Install/RetroRewind/RetroRewind.exe']['sha256'] or not isinstance(report.get('builder_version'),str) or not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+',report['builder_version']):raise ValueError('Identidad del juego preparado inválida')
    receipt={'format':1,'game_id':'RMCP01','builder_version':report.get('builder_version'),'files':files}
    atomic_json(source/RECEIPT,receipt)
    source.rename(candidate)
    return candidate

def replace_file(source,target):
    temporary=target.with_name(target.name+'.'+uuid.uuid4().hex+'.update-tmp')
    plain(temporary,target.parent)
    with Path(source).open('rb') as incoming,temporary.open('xb') as outgoing:
        shutil.copyfileobj(incoming,outgoing,1024*1024);outgoing.flush();os.fsync(outgoing.fileno())
    os.replace(temporary,target)

def transaction(root,value):
    if not isinstance(value,dict) or value.get('format')!=1 or value.get('phase') not in ('prepared','committed'):raise ValueError('Diario de actualización inválido')
    backup=value.get('backup','')
    if not isinstance(backup,str) or not re.fullmatch(r'Backups/update-[0-9a-f]{32}',backup):raise ValueError('Copia de seguridad fuera de la instalación')
    if type(value.get('had_game')) is not bool or type(value.get('had_previous')) is not bool:raise ValueError('Estado de recuperación inválido')
    originals=value.get('originals')
    if not isinstance(originals,dict) or not originals.keys()<=ROOT_FILES:raise ValueError('Archivos de recuperación inválidos')
    directory=plain(root/backup,root)
    for name,expected in originals.items():
        file=plain(directory/'root'/name,root)
        if not isinstance(expected,str) or not re.fullmatch('[0-9a-f]{64}',expected) or digest(file)!=expected:raise ValueError('Copia de seguridad dañada')
    return directory

def recover(root):
    root=Path(root).resolve();journal=plain(root/JOURNAL,root)
    if not journal.exists():return False
    value=read_json(journal);backup=transaction(root,value)
    candidate=plain(root/CANDIDATE,root);game=plain(root/'GameData',root);previous=plain(root/'GameData.previous',root)
    if value['phase']=='committed':
        if candidate.exists():candidate.rename(plain(backup/'package',root))
        journal.unlink();return True
    if not candidate.is_dir():raise ValueError('Falta el paquete necesario para recuperar la actualización')
    incoming=plain(candidate/'GameData',root)
    if not incoming.exists() and game.exists():game.rename(incoming)
    if value['had_game'] and not game.exists():
        if not previous.exists():raise ValueError('Falta la versión anterior del juego')
        previous.rename(game)
    for name in ROOT_FILES:
        target=plain(root/name,root)
        if name in value['originals']:
            if not target.is_file() or digest(target)!=value['originals'][name]:replace_file(backup/'root'/name,target)
        elif target.exists():target.unlink()
    older=plain(backup/'older-game-data',root)
    if value['had_previous'] and older.exists():older.rename(previous)
    journal.unlink();return True

def apply(root,allow_new=False):
    root=Path(root).resolve();assert_native_idle(root);recovered=recover(root)
    if not allow_new:owned(root)
    if recovered and not (root/CANDIDATE).exists():return {'already_applied':True}
    candidate=plain(root/CANDIDATE,root);receipt=verify(candidate);assert_native_idle(root)
    game=plain(root/'GameData',root);previous=plain(root/'GameData.previous',root)
    backup=plain(root/'Backups'/('update-'+uuid.uuid4().hex),root)
    for name in ROOT_FILES:plain(root/name,root)
    backup.mkdir(parents=True);(backup/'root').mkdir()
    originals={}
    for name in ROOT_FILES:
        source=root/name
        if source.exists():
            shutil.copy2(source,backup/'root'/name);originals[name]=digest(backup/'root'/name)
    state={'format':1,'phase':'prepared','backup':backup.relative_to(root).as_posix(),
           'had_game':game.exists(),'had_previous':previous.exists(),'originals':originals}
    atomic_json(root/JOURNAL,state)
    try:
        if previous.exists():previous.rename(backup/'older-game-data')
        if game.exists():game.rename(previous)
        (candidate/'GameData').rename(game)
        for name in ROOT_FILES:
            if (candidate/name).exists():replace_file(candidate/name,root/name)
        state['phase']='committed';atomic_json(root/JOURNAL,state)
    except Exception:
        recover(root);raise
    recover(root)
    return receipt

def assert_native_idle(root):
    # También protege instalaciones antiguas cuyo launcher todavía no tenía bloqueo.
    if os.name!='nt':return
    import ctypes
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.CreateFileW.argtypes=[ctypes.c_wchar_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p]
    kernel.CreateFileW.restype=ctypes.c_void_p;kernel.CloseHandle.argtypes=[ctypes.c_void_p]
    for directory in ('GameData','GameData.previous'):
        file=plain(root/directory/'Install/RetroRewind/RetroRewind.exe',root)
        if not file.is_file():continue
        handle=kernel.CreateFileW(str(file),0x40000000,7,None,3,0x80,None)
        if handle==ctypes.c_void_p(-1).value:raise InstallationBusyError('El juego está abierto o el ejecutable no permite escritura')
        kernel.CloseHandle(handle)

def quarantine_failed(root):
    """Conserva el paquete rechazado y permite volver a compilar sin un bucle de arranque."""
    root=Path(root).resolve()
    if plain(root/JOURNAL,root).exists():return False
    candidate=plain(root/CANDIDATE,root)
    if candidate.exists():
        target=plain(root/'Backups'/('rejected-update-'+uuid.uuid4().hex),root)
        target.parent.mkdir(parents=True,exist_ok=True);candidate.rename(target)
    try:
        owned(root);metadata=read_json(root/'mkwii-pc-install.json')
        native=plain(root/'GameData/Install/RetroRewind/RetroRewind.exe',root)
        return native.is_file() and digest(native)==metadata.get('native_sha256')
    except (OSError,ValueError,TypeError,AttributeError):return False

if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser();parser.add_argument('--install-dir',required=True)
    args=parser.parse_args();previous_usable=False
    try:
        with build_lock(Path(args.install_dir)/'.mkwii-builder',timeout=30):
            try:apply(Path(args.install_dir))
            except InstallationBusyError:raise
            except Exception:
                previous_usable=quarantine_failed(Path(args.install_dir))
                raise
        print(json.dumps({'stage':'complete','applied':True}),flush=True)
    except Exception as error:
        import traceback,sys
        traceback.print_exc(file=sys.stderr)
        print(json.dumps({'stage':'error','message':str(error),'previous_usable':previous_usable}),flush=True);sys.exit(1)
