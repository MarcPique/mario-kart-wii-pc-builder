"""Build the public installer. Python 3 + .NET SDK 10 are maintainer-only tools."""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys,zipfile
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'worker'))
from icon_from_disc import png
from prepare_payload import prepare
from build_game import download

def icon():
    images=[]
    for size in [16,32,48,64,128,256]:
        rgba=bytearray()
        for y in range(size):
            for x in range(size):
                # Original, code-drawn checkered flag on a blue background.
                if size//5<x<size*4//5 and size//5<y<size*3//5:
                    c=245 if ((x*10//size+y*10//size)%2)==0 else 28;rgba.extend([c,c,c,255])
                elif size//5-1<=x<=size//5+max(1,size//30) and size//5<y<size*4//5:rgba.extend([245,245,245,255])
                else:rgba.extend([0,105+int(60*y/size),175+int(35*y/size),255])
        images.append((size,png(size,size,rgba)))
    import struct
    result=bytearray(struct.pack('<HHH',0,1,len(images)));offset=6+16*len(images)
    for size,data in images:result.extend(struct.pack('<BBBBHHII',size%256,size%256,0,0,1,32,len(data),offset));offset+=len(data)
    for size,data in images:result.extend(data)
    (ROOT/'Installer/builder.ico').write_bytes(result)

def main():
    cache=ROOT/'.cache';build=ROOT/'.build';out=ROOT/'dist';build.mkdir(exist_ok=True);out.mkdir(exist_ok=True)
    pins=json.loads((ROOT/'worker/pins.json').read_text());python_zip=download(pins['python'],cache)
    payload=build/'payload';prepare(payload);archive=build/'BuilderPayload.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for p in sorted(payload.rglob('*')):
            if p.is_file():z.write(p,p.relative_to(payload).as_posix())
    icon()
    subprocess.run(['dotnet','publish',str(ROOT/'Installer/Installer.csproj'),'-c','Release','-o',str(out),f'-p:PayloadZip={archive}',f'-p:PythonZip={python_zip}'],check=True)
    exe=out/'MKWii-PC-Builder.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();(out/'SHA256SUMS.txt').write_text(f'{digest}  {exe.name}\n')
    print(json.dumps({'exe':str(exe),'bytes':exe.stat().st_size,'sha256':digest}))
if __name__=='__main__':main()
