"""Rebuild matching Aurora objects in the bundled, backed-up static archive."""
from pathlib import Path
import subprocess,shutil,sys
r=Path(sys.argv[1]).resolve();w=r/'Install/BuildWorkspace';d=w/'Dependencies';a=w/'aurora-main';tool=r/'Install/Toolkit/llvm-mingw/bin'
out=r/'aurora-patch';out.mkdir(exist_ok=True)
flags=['-std=c++20','-O2','-DNDEBUG','-DAURORA','-DAURORA_ENABLE_GX','-DWEBGPU_DAWN','-DTARGET_PC=1','-DNOMINMAX','-DIMGUI_ENABLE_FREETYPE','-DIMGUI_USER_CONFIG="aurora/imgui_config.h"']
for p in [a/'include',a/'lib',d/'abseil-cpp',d/'fmt/include',d/'SDL/include',d/'native_prebuilt/include/generated01',d/'imgui',d/'dawn_prebuilt/include',d/'xxhash',d/'tracy/public',d/'sqlite3']:
    flags+=['-I',str(p)]
objects=[]
for name in ['encoding','tex_copy_conv','texture']:
    obj=out/(name+'.cpp.obj');objects.append(obj)
    subprocess.run([str(tool/'x86_64-w64-mingw32-clang++.exe'),*flags,'-c',str(a/f'lib/gfx/{name}.cpp'),'-o',str(obj)],check=True)
archive=d/'native_prebuilt/lib/libaurora_gx.a';backup=out/'libaurora_gx.original.a'
if not backup.exists():shutil.copy2(archive,backup)
subprocess.run([str(tool/'llvm-ar.exe'),'r',str(archive),*map(str,objects)],check=True)
core_obj=out/'gpu.cpp.obj'
subprocess.run([str(tool/'x86_64-w64-mingw32-clang++.exe'),*flags,'-c',str(a/'lib/webgpu/gpu.cpp'),'-o',str(core_obj)],check=True)
core_archive=d/'native_prebuilt/lib/libaurora_core.a';core_backup=out/'libaurora_core.original.a'
if not core_backup.exists():shutil.copy2(core_archive,core_backup)
subprocess.run([str(tool/'llvm-ar.exe'),'r',str(core_archive),str(core_obj)],check=True)
print('Rebuilt Aurora graphics objects; original archive preserved',flush=True)
