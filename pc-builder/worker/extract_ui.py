import struct
from pathlib import Path

ROOT=Path(__file__).resolve().parent
def yaz0(b):
    if b[:4]!=b'Yaz0': return b
    size=struct.unpack_from('>I',b,4)[0]; out=bytearray(); p=16
    while len(out)<size:
        flags=b[p]; p+=1
        for bit in range(7,-1,-1):
            if len(out)>=size: break
            if flags&(1<<bit): out.append(b[p]); p+=1
            else:
                a,c=b[p:p+2]; p+=2; distance=((a&15)<<8|c)+1; n=a>>4
                if not n: n=b[p]+18; p+=1
                else: n+=2
                for _ in range(n): out.append(out[-distance])
    return bytes(out)
def u8(b):
    b=yaz0(b); assert b[:4]==b'U\xaa8-'
    start=struct.unpack_from('>I',b,4)[0]; count=struct.unpack_from('>I',b,start+8)[0]; names=start+12*count; dirs=[('',count)]
    for i in range(1,count):
        kind,offset,size=struct.unpack_from('>III',b,start+12*i)
        while i>=dirs[-1][1]: dirs.pop()
        name_start=names+(kind&0xffffff); name=b[name_start:b.index(0,name_start)].decode()
        path=dirs[-1][0]+name
        if kind>>24: dirs.append((path+'/',size))
        else: yield path,b[offset:offset+size]
def main():
    ui=ROOT/'Install/GameAssets/DATA/files/Scene/UI'
    for archive in ('Font.szs','Option.szs','MenuSingle.szs'):
        if not (ui/archive).exists(): continue
        folder=ROOT/'ui-reference'/Path(archive).stem; folder.mkdir(parents=True,exist_ok=True)
        files=list(u8((ui/archive).read_bytes()))
        print(archive,[(name,len(data)) for name,data in files if name.endswith(('.brfnt','.tpl'))][:65],flush=True)
        for name,data in files:
            path=folder/name; path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(data)
            if name.endswith('.brfnt'):
                print(name,data[:16].hex())
                p=struct.unpack_from('>H',data,12)[0]
                while p<len(data):
                    kind,size=struct.unpack_from('>4sI',data,p); print(kind,p,size,data[p+8:p+40].hex()); p+=size
if __name__=='__main__': main()
