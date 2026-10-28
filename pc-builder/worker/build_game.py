"""Local-only disc extraction and native build. Never uploads user files."""
from pathlib import Path,PurePosixPath
import argparse,hashlib,json,os,shutil,struct,subprocess,sys,time,urllib.request,zipfile
sys.stdout.reconfigure(encoding='utf-8',errors='replace')
sys.stderr.reconfigure(encoding='utf-8',errors='replace')
os.environ['PYTHONUTF8']='1';os.environ['PYTHONIOENCODING']='utf-8'
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
PINS=json.loads((HERE/'pins.json').read_text())
CREATE_NO_WINDOW=0x08000000 if os.name=='nt' else 0

def emit(stage,message,percent=None,**extra):
    value={'stage':stage,'message':message,**extra}
    if percent is not None:value['percent']=percent
    print(json.dumps(value,ensure_ascii=False),flush=True)

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as file:
        for chunk in iter(lambda:file.read(4*1024*1024),b''):h.update(chunk)
    return h.hexdigest()

def download(pin,cache,offline=False):
    target=cache/pin['name'];cache.mkdir(parents=True,exist_ok=True)
    if target.is_file() and sha(target)==pin['sha256']:return target
    if offline:raise ValueError('Falta una descarga verificada en la caché: '+pin['name'])
    part=target.with_suffix(target.suffix+'.part')
    for attempt in range(3):
        try:
            offset=part.stat().st_size if part.exists() else 0
            headers={'User-Agent':'MKWii-PC-Builder/1.0','Accept-Encoding':'identity'}
            if offset:headers['Range']=f'bytes={offset}-'
            with urllib.request.urlopen(urllib.request.Request(pin['url'],headers=headers),timeout=60) as response:
                append=offset>0 and response.status==206
                if append and not response.headers.get('Content-Range','').startswith(f'bytes {offset}-'):raise ValueError('Respuesta parcial inválida')
                if not append:offset=0
                maximum=pin.get('bytes',1024*1024*1024);last=-1
                with part.open('ab' if append else 'wb') as file:
                    while chunk:=response.read(1024*1024):
                        offset+=len(chunk)
                        if offset>maximum:raise ValueError('La descarga supera el tamaño fijado')
                        file.write(chunk);percent=int(offset*100/maximum)
                        if percent!=last:last=percent;emit('download',f"{pin['name']}: {percent}%")
            if pin.get('bytes') and part.stat().st_size!=pin['bytes']:raise ValueError('Descarga incompleta')
            if sha(part)!=pin['sha256']:
                part.unlink();raise ValueError('SHA256 no coincide: '+pin['name'])
            os.replace(part,target);return target
        except Exception:
            if attempt==2:raise
            time.sleep(attempt+1)

def safe_extract(archive,destination,prefix=None):
    destination=Path(destination).resolve();destination.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(archive) as z:
        entries=z.infolist()
        if len(entries)>100000 or sum(i.file_size for i in entries)>16*1024**3:raise ValueError('Archivo ZIP excesivo')
        for item in entries:
            name=item.filename.replace('\\','/');parts=PurePosixPath(name)
            if parts.is_absolute() or '..' in parts.parts or ':' in name or (item.external_attr>>16)&0xf000==0xa000:raise ValueError('Ruta ZIP insegura')
            if prefix and not name.startswith(prefix):continue
            relative=name[len(prefix):] if prefix else name
            if not relative or item.is_dir():continue
            target=destination/relative
            if not target.resolve().is_relative_to(destination):raise ValueError('Ruta fuera del destino')
            target.parent.mkdir(parents=True,exist_ok=True)
            with z.open(item) as src,target.open('wb') as dst:shutil.copyfileobj(src,dst,1024*1024)

def remove_owned(path,work):
    path=Path(path);work=Path(work).resolve()
    if not (work/'.mkwii-builder-workspace').is_file() or path.resolve()==work or not path.resolve().is_relative_to(work):raise ValueError('Limpieza fuera del espacio de trabajo')
    if not path.exists():return
    for p in [path,*path.rglob('*')]:
        if p.is_symlink() or (hasattr(p,'is_junction') and p.is_junction()):raise ValueError('Enlace inesperado en el espacio de trabajo')
    if path.is_dir():shutil.rmtree(path)
    else:path.unlink()

def native_workspace(actual):
    """Use an ASCII alias for CMake's cmd/batch steps, as upstream LocalBuild does."""
    actual=Path(actual).resolve()
    if all(32<=ord(c)<127 and c not in "&!%[];" for c in str(actual)):return actual
    import ctypes
    short=ctypes.create_unicode_buffer(32768)
    if ctypes.windll.kernel32.GetShortPathNameW(str(actual),short,len(short)):
        candidate=Path(short.value)
        if str(candidate).isascii() and candidate.resolve()==actual:return candidate
    base=Path(os.environ.get('ProgramData',r'C:\ProgramData'))/'MKWiiPCBuilder'
    if not str(base).isascii():raise ValueError('Selecciona una carpeta con una ruta de compilación ASCII.')
    base.mkdir(parents=True,exist_ok=True)
    alias=base/('work-'+hashlib.sha256(str(actual).lower().encode()).hexdigest()[:24])
    if alias.exists():
        if alias.resolve()!=actual:raise ValueError('El alias de compilación existente apunta a otra carpeta.')
        return alias
    target=str(actual).replace("'","''");link=str(alias).replace("'","''")
    command=f"$ErrorActionPreference='Stop';New-Item -ItemType Junction -Path '{link}' -Target '{target}' | Out-Null"
    executable=Path(os.environ['SystemRoot'])/'System32/WindowsPowerShell/v1.0/powershell.exe'
    result=subprocess.run([str(executable),'-NoLogo','-NoProfile','-Command',command],capture_output=True,creationflags=CREATE_NO_WINDOW)
    if result.returncode or alias.resolve()!=actual:raise ValueError('No se pudo crear un alias ASCII para compilar. Selecciona una carpeta con letras A-Z y números.')
    return alias

def run(args,cwd,log,stage):
    emit(stage,' '.join(map(str,args[:2])))
    with subprocess.Popen(list(map(str,args)),cwd=cwd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding='utf-8',errors='replace',creationflags=CREATE_NO_WINDOW) as process:
        for line in process.stdout:
            log.write(line);log.flush()
            if line.strip():emit(stage,line.strip()[-700:])
        code=process.wait()
    if code:raise RuntimeError(f'{stage} terminó con código {code}. Consulta builder.log.')

def prepare_setup(setup,work):
    marker=work/'toolkit-ready.json'
    if marker.exists() and json.loads(marker.read_text()).get('sha256')==PINS['setup']['sha256']:return
    with setup.open('rb') as f:
        f.seek(-24,2);magic,offset,length=struct.unpack('<8sqq',f.read(24))
        if magic!=b'MKWCPAY1' or offset<0 or length<=0 or offset+length!=setup.stat().st_size-24:raise ValueError('Contenedor del toolkit inválido')
    emit('toolkit','Extrayendo el toolkit verificado...',12)
    safe_extract(setup,work/'Install/Toolkit','Toolkit/')
    safe_extract(setup,work/'Install/BuildWorkspace','BuildWorkspace/')
    for p in (work/'Install/Toolkit/Redist').glob('*.dll'):shutil.copy2(p,work/'Install/Toolkit'/p.name)
    marker.write_text(json.dumps({'sha256':PINS['setup']['sha256']}))

def verify_disc(data):
    if (data/'sys/boot.bin').read_bytes()[:6]!=b'RMCP01':raise ValueError('Esta versión necesita un disco PAL RMCP01.')
    for file,digest in [(data/'sys/main.dol',PINS['dol_sha256']),(data/'files/rel/StaticR.rel',PINS['rel_sha256'])]:
        if sha(file)!=digest:raise ValueError('El disco tiene código modificado o una revisión incompatible: '+file.name)

def apply_sources(work,payload):
    workspace=work/'Install/BuildWorkspace'
    for p in (payload/'patches').rglob('*'):
        if p.is_file():
            target=workspace/p.relative_to(payload/'patches');target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target)

def translate(work,log,jobs):
    workspace=work/'Install/BuildWorkspace';tool=work/'Install/Toolkit/Translator/Translator.Cli.exe';project=workspace/'projects/mkwii/recomp.yml';generated=workspace/'generated';metadata=generated/'base_translation_output.json';functions=generated/'functions';manifest=workspace/'build/base/mkwii_base_manifest.json';retro=work/'RetroRewind6';mod=workspace/'build/mods/retro_rewind_full_cpp'
    awareness=workspace/'PulsarPacks/completed/RetroRewind/RetroRewind6/Binaries';awareness.mkdir(parents=True,exist_ok=True);shutil.copy2(retro/'Binaries/Code.pul',awareness/'Code.pul')
    run([tool,'translate-recursive','0x800060A4','--project',project,'--outdir',functions,'--output-metadata',metadata,'--production-source-bundle',generated/'base_translation_sources.bin','--no-function-files','--prune-stale','--threads',jobs],workspace,log,'translate-base')
    run([tool,'emit-base-manifest','--project',project,'--out',manifest.parent,'--functions-dir',functions,'--translation-output-metadata',metadata,'--region','P'],workspace,log,'manifest')
    run([tool,'translate-mod','--project',project,'--profile','retro-rewind','--base-manifest',manifest,'--base-translation-output-metadata',metadata,'--code-pul',retro/'Binaries/Code.pul','--mod-root',retro,'--mod-name','Retro Rewind','--region','P','--out',mod,'--prefer-cached-inputs','--emit-cpp','--threads',jobs,'--retro-wfc-payload',work/'RetroWfc/binary/payload.RMCPD00.bin'],workspace,log,'translate-retro')
    run([tool,'generate-data-init','--project',project],workspace,log,'data-init')
    run([tool,'emit-build-shards','--project',project,'--base-metadata',metadata,'--base-functions-dir',functions,'--native-source-dir',workspace/'runtime/src','--out',generated/'build_shards','--resolved-profile',mod/'resolved_dispatch_profile.json','--retro-cpp-dir',mod/'cpp'],workspace,log,'shards')

def create_launcher(work,stage,log):
    workspace=work/'Install/BuildWorkspace';bin=work/'Install/Toolkit/llvm-mingw/bin';resource=work/'launcher.rc'
    icon=(workspace/'runtime/resources/MarioKartWii.ico').as_posix()
    resource.write_text(f'1 ICON "{icon}"\n',encoding='utf-8')
    run([bin/'x86_64-w64-mingw32-windres.exe',resource,work/'launcher-res.o'],work,log,'launcher-icon')
    run([bin/'x86_64-w64-mingw32-clang++.exe','-std=c++20','-O2','-static','-municode','-mwindows',HERE/'portable_launcher.cpp',work/'launcher-res.o','-lole32','-lshell32','-o',stage/'Mario Kart Wii.exe'],work,log,'launcher')

def compile_native(work,log,jobs):
    w=work/'Install/BuildWorkspace';t=work/'Install/Toolkit';bin=t/'llvm-mingw/bin';deps=w/'Dependencies';cmake=t/'CMake/bin/cmake.exe';ninja=t/'Ninja/ninja.exe';build=w/'native-build'
    os.environ['PATH']=os.pathsep.join([str(bin),str(t),os.path.join(os.environ['SystemRoot'],'System32'),os.environ['SystemRoot']])
    cache=build/'CMakeCache.txt'
    if cache.exists():
        home=next((s.split('=',1)[1] for s in cache.read_text(errors='replace').splitlines() if s.startswith('CMAKE_HOME_DIRECTORY:INTERNAL=')),None)
        if home and home.replace('\\','/').lower()!=(w/'runtime').as_posix().lower():remove_owned(build,work)
    args=[cmake,'-S',w/'runtime','-B',build,'-G','Ninja',f'-DCMAKE_MAKE_PROGRAM={ninja.as_posix()}',f'-DCMAKE_C_COMPILER={(bin/"x86_64-w64-mingw32-clang.exe").as_posix()}',f'-DCMAKE_CXX_COMPILER={(bin/"x86_64-w64-mingw32-clang++.exe").as_posix()}',f'-DCMAKE_RC_COMPILER={(bin/"x86_64-w64-mingw32-windres.exe").as_posix()}','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_SYSTEM_PROCESSOR=x86_64','-DAURORA_DAWN_PROVIDER=package','-DAURORA_SDL3_PROVIDER=vendor','-DCMAKE_POLICY_DEFAULT_CMP0168=NEW','-DFETCHCONTENT_FULLY_DISCONNECTED=ON','-DAWK:FILEPATH=',f'-DMKW_CPPWINRT_INCLUDE_DIR={(deps/"cppwinrt").as_posix()}',f'-DMKW_NATIVE_PREBUILT_DIR={(deps/"native_prebuilt").as_posix()}','-DMKW_USE_PREBUILT_RUNTIME=OFF',f'-DMKW_TRANSLATED_COMPILE_JOBS={jobs}','-DMKW_X86_CPU_PROFILE=v3']
    for p in sorted(deps.iterdir()):
        if p.is_dir() and p.name!='native_prebuilt':args.append(f'-DFETCHCONTENT_SOURCE_DIR_{p.name.upper()}={p.as_posix()}')
    run(args,w,log,'configure');run([ninja,'-C',build,'RetroRewind.exe','-j',jobs],w,log,'compile')

def desktop_exe(launcher,directory,root):
    directory.mkdir(parents=True,exist_ok=True)
    for n in range(100):
        path=directory/('Mario Kart Wii.exe' if n==0 else f'Mario Kart Wii PC {n}.exe')
        if not path.exists():break
    else:raise ValueError('No se encontró un nombre libre en el escritorio')
    link=str(root.resolve()).encode('utf-8')
    with launcher.open('rb') as src,path.open('xb') as dst:
        shutil.copyfileobj(src,dst);dst.write(link);dst.write(struct.pack('<8sQ',b'MKWLINK1',len(link)))
    return path

def main(options):
    iso=Path(options.iso).resolve(strict=True);dest=Path(os.path.expandvars(options.install_dir)).resolve()
    if iso.suffix.lower() not in ['.iso','.wbfs','.rvz']:raise ValueError('Selecciona una imagen ISO, WBFS o RVZ.')
    if (dest/'GameData').exists() and not (dest/'mkwii-pc-install.json').exists():raise ValueError('El destino contiene otra instalación. Selecciona una carpeta nueva.')
    dest.mkdir(parents=True,exist_ok=True);actual_work=dest/'.mkwii-builder';actual_work.mkdir(exist_ok=True);(actual_work/'.mkwii-builder-workspace').touch();work=native_workspace(actual_work)
    cache=Path(options.cache_dir).resolve() if options.cache_dir else work/'downloads';log_path=work/'builder.log'
    jobs=max(1,min(options.jobs,os.cpu_count() or 2));payload=HERE.parent
    if shutil.disk_usage(dest).free<20*1024**3:raise ValueError('Se necesitan al menos 20 GiB libres para la compilación local.')
    with log_path.open('w',encoding='utf-8',buffering=1) as log:
        setup=download(PINS['setup'],cache,options.offline);prepare_setup(setup,work)
        nod=work/'Install/Toolkit/nodtool.exe';data=work/'Install/GameAssets/DATA'
        signature={'path':str(iso),'bytes':iso.stat().st_size,'mtime':iso.stat().st_mtime_ns};disc_marker=work/'disc-ready.json'
        if not disc_marker.exists() or json.loads(disc_marker.read_text())!=signature:
            emit('disc','Validando y extrayendo tu imagen del juego...',20)
            run([nod,'info',iso],work,log,'disc-info')
            temporary=data.with_name('DATA.incomplete');remove_owned(temporary,work)
            run([nod,'extract',iso,temporary,'-q'],work,log,'extract-disc');verify_disc(temporary)
            remove_owned(data,work);temporary.rename(data);disc_marker.write_text(json.dumps(signature))
        verify_disc(data)
        retro_zip=download(PINS['retro'],cache,options.offline);retro=work/'RetroRewind6';rr_marker=work/'retro-ready.json'
        if not rr_marker.exists():
            emit('retro','Preparando Retro Rewind 6.13.4...',30);remove_owned(retro,work);safe_extract(retro_zip,work)
            if not (retro/'Binaries/Code.pul').is_file():raise ValueError('Estructura Retro Rewind inesperada')
            rr_marker.write_text(json.dumps({'sha256':PINS['retro']['sha256']}))
        wfc=download(PINS['wfc'],cache,options.offline);(work/'RetroWfc/binary').mkdir(parents=True,exist_ok=True);shutil.copy2(wfc,work/'RetroWfc/binary/payload.RMCPD00.bin')
        apply_sources(work,payload)
        assets=work/'Install/BuildWorkspace/Assets';assets.mkdir(exist_ok=True)
        shutil.copy2(data/'sys/main.dol',assets/'main.dol');shutil.copy2(data/'files/rel/StaticR.rel',assets/'StaticR.rel')
        from icon_from_disc import create
        create(data,work/'Install/BuildWorkspace/runtime/resources/MarioKartWii.ico')
        run([sys.executable,HERE/'build_aurora_patch.py',work],work,log,'graphics')
        emit('translate','Traduciendo localmente el código de tu disco...',40);translate(work,log,jobs)
        emit('compile','Compilando el juego para Windows...',55)
        compile_native(work,log,jobs)
        emit('assets','Preparando menús e idiomas a partir de tu disco...',85)
        stage=work/'preview';remove_owned(stage,work);stage.mkdir();game=stage/'GameData'
        shutil.copytree(work/'Install/GameAssets',game/'Install/GameAssets');shutil.copytree(retro,game/'RetroRewind6')
        native=game/'Install/RetroRewind';native.mkdir(parents=True)
        build=work/'Install/BuildWorkspace/native-build'
        for p in build.iterdir():
            if p.is_file() and (p.suffix.lower()=='.dll' or p.name in ['RetroRewind.exe','dsp_coef.bin','initial_pipeline_cache.db']):shutil.copy2(p,native/p.name)
        for p in (work/'Install/Toolkit/Redist').glob('*.dll'):shutil.copy2(p,native/p.name)
        shutil.copytree(build/'wii_bootstrap',native/'wii_bootstrap')
        import prepare_original_ui,make_locale_bank
        prepare_original_ui.ROOT=work;make_locale_bank.ROOT=work
        prepare_original_ui.main();make_locale_bank.main()
        (game/'portable-data.txt').write_text('Mario Kart Wii PC portable data\n')
        create_launcher(work,stage,log)
        # Publish the new immutable data in one directory rename; preserve UserData.
        emit('install','Publicando el juego y creando el EXE del escritorio...',95)
        old=dest/'GameData';previous=dest/'GameData.previous'
        if not game.resolve().is_relative_to(actual_work.resolve()) or old.resolve().parent!=dest:raise ValueError('Publicación fuera del destino elegido')
        if old.is_symlink() or (hasattr(old,'is_junction') and old.is_junction()):raise ValueError('GameData no puede ser un enlace')
        if previous.exists():raise ValueError('Existe GameData.previous de una actualización anterior. Consérvala o muévela antes de actualizar.')
        if old.exists():old.rename(previous)
        try:shutil.move(game,old)
        except Exception:
            if previous.exists():previous.rename(old)
            raise
        shutil.copy2(stage/'Mario Kart Wii.exe',dest/'Mario Kart Wii.exe')
        user=dest/'UserData';user.mkdir(exist_ok=True);config=user/'Config.toml'
        if not config.exists():config.write_text(f'[paths]\ndvd_root = "../GameData/Install/GameAssets/DATA"\nretro_rewind_root = "../GameData/RetroRewind6"\n\n[video]\nwindow_width = 1280\nwindow_height = 720\ndisplay_mode = "windowed"\nresolution_multiplier = 2\nvsync = true\nshow_fps = false\n\n[system]\nlanguage = "{options.language}"\n\n[network]\nenabled = true\n',encoding='utf-8')
        (dest/'portable.txt').write_text('Keep Mario Kart Wii.exe, GameData and UserData together.\n')
        for name in ['LICENSE','THIRD-PARTY-NOTICES.md','GAME-README.txt']:
            if (payload/name).exists():shutil.copy2(payload/name,dest/('LEEME.txt' if name=='GAME-README.txt' else name))
        report={'builder_version':PINS['version'],'game_id':'RMCP01','retro_rewind':'6.13.4','native_sha256':sha(old/'Install/RetroRewind/RetroRewind.exe'),'game_exe':str(dest/'Mario Kart Wii.exe'),'language':options.language,'created_at':time.time(),'game_files_uploaded':False}
        (dest/'mkwii-pc-install.json').write_text(json.dumps(report,indent=2))
        desktop=None
        if options.desktop_dir:desktop=desktop_exe(dest/'Mario Kart Wii.exe',Path(options.desktop_dir),dest)
        emit('complete','Instalación terminada. Tu juego se ha compilado localmente.',100,game_exe=report['game_exe'],desktop_exe=str(desktop) if desktop else None,log=str(log_path))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--iso',required=True);parser.add_argument('--install-dir',required=True);parser.add_argument('--desktop-dir');parser.add_argument('--cache-dir');parser.add_argument('--jobs',type=int,default=4);parser.add_argument('--language',choices=['es','en','fr','de','it'],default='es');parser.add_argument('--offline',action='store_true')
    try:main(parser.parse_args())
    except Exception as e:
        import traceback
        traceback.print_exc(file=sys.stderr);emit('error',str(e));sys.exit(1)
