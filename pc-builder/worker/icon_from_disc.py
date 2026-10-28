"""Create the game icon locally from the user's Title_E archive, stdlib only."""
import struct,zlib
from pathlib import Path
from prepare_original_ui import read
from extract_ui import u8
def png(width,height,rgba):
    def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
    rows=b''.join(b'\0'+rgba[y*width*4:(y+1)*width*4] for y in range(height))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(rows,9))+chunk(b'IEND',b'')
def decode565(data):
    assert data[:4]==bytes.fromhex('0020af30')
    table=struct.unpack_from('>I',data,8)[0];header=struct.unpack_from('>I',data,table)[0]
    height,width,fmt,offset=struct.unpack_from('>HHII',data,header)
    if fmt!=4 or width*height>4_000_000:raise ValueError('Unsupported local title texture')
    pixels=bytearray(width*height*4);p=offset
    for ty in range(0,height,4):
        for tx in range(0,width,4):
            for y in range(4):
                for x in range(4):
                    value=struct.unpack_from('>H',data,p)[0];p+=2
                    if tx+x>=width or ty+y>=height:continue
                    red=(value>>11)&31;green=(value>>5)&63;blue=value&31
                    dest=((ty+y)*width+tx+x)*4
                    pixels[dest:dest+4]=bytes(((red<<3)|(red>>2),(green<<2)|(green>>4),(blue<<3)|(blue>>2),255))
    return width,height,pixels

def lz10(data):
    if data[:4]==b'IMD5':data=data[32:]
    if data[:4]==b'LZ77':data=data[4:]
    if len(data)<4 or data[0]!=16:raise ValueError('Invalid banner compression')
    size=int.from_bytes(data[1:4],'little')
    if not 0<size<=16*1024*1024:raise ValueError('Invalid banner size')
    out=bytearray();p=4
    while len(out)<size:
        flags=data[p];p+=1
        for bit in range(7,-1,-1):
            if len(out)>=size:break
            if flags&(1<<bit):
                a,c=data[p:p+2];p+=2;count=(a>>4)+3;distance=((a&15)<<8|c)+1
                if distance>len(out):raise ValueError('Invalid banner back reference')
                for _ in range(min(count,size-len(out))):out.append(out[-distance])
            else:out.append(data[p]);p+=1
    return bytes(out)

def decode_cmpr(data):
    table=struct.unpack_from('>I',data,8)[0];header=struct.unpack_from('>I',data,table)[0]
    height,width,fmt,p=struct.unpack_from('>HHII',data,header)
    if fmt!=14 or width*height>4_000_000:raise ValueError('Unsupported banner texture')
    def rgb(value):
        r=(value>>11)&31;g=(value>>5)&63;b=value&31
        return ((r<<3)|(r>>2),(g<<2)|(g>>4),(b<<3)|(b>>2),255)
    pixels=bytearray(width*height*4)
    for ty in range(0,height,8):
        for tx in range(0,width,8):
            for sy,sx in [(0,0),(0,4),(4,0),(4,4)]:
                a,b,bits=struct.unpack_from('>HHI',data,p);p+=8;colors=[rgb(a),rgb(b)]
                if a>b:colors += [tuple((2*colors[0][i]+colors[1][i])//3 for i in range(4)),tuple((colors[0][i]+2*colors[1][i])//3 for i in range(4))]
                else:colors += [tuple((colors[0][i]+colors[1][i])//2 for i in range(4)),(0,0,0,0)]
                for y in range(4):
                    for x in range(4):
                        if ty+sy+y>=height or tx+sx+x>=width:continue
                        at=((ty+sy+y)*width+tx+sx+x)*4;pixels[at:at+4]=bytes(colors[(bits>>(30-2*(y*4+x)))&3])
    return width,height,pixels

def banner_textures(disc):
    data=(Path(disc)/'files/opening.bnr').read_bytes();offset=data.find(bytes.fromhex('55aa382d'))
    if offset<0 or offset>4096:raise ValueError('Invalid local banner archive')
    outer=dict(u8(data[offset:]));return dict(u8(lz10(outer['meta/banner.bin'])))
def create(disc,out):
    archive=banner_textures(disc)
    width,height,pixels=decode_cmpr(archive['arc/timg/tk_logoENG_00.tpl'])
    # Matte the disc's transparent logo onto white, matching the local game icon.
    for i in range(0,len(pixels),4):
        alpha=pixels[i+3]
        for channel in range(3):pixels[i+channel]=(pixels[i+channel]*alpha+255*(255-alpha)+127)//255
        pixels[i+3]=255
    images=[]
    for size in [16,24,32,48,64,128,256]:
        canvas=bytearray(b'\xff'*(size*size*4));scale=min(size/width,size/height);w=max(1,int(width*scale));h=max(1,int(height*scale));left=(size-w)//2;top=(size-h)//2
        for y in range(h):
            for x in range(w):
                fx=max(0,min(width-1,(x+.5)/scale-.5));fy=max(0,min(height-1,(y+.5)/scale-.5));x0=int(fx);y0=int(fy);x1=min(width-1,x0+1);y1=min(height-1,y0+1);dx=fx-x0;dy=fy-y0
                dst=((top+y)*size+left+x)*4
                for channel in range(4):canvas[dst+channel]=round(pixels[(y0*width+x0)*4+channel]*(1-dx)*(1-dy)+pixels[(y0*width+x1)*4+channel]*dx*(1-dy)+pixels[(y1*width+x0)*4+channel]*(1-dx)*dy+pixels[(y1*width+x1)*4+channel]*dx*dy)
        images.append((size,png(size,size,canvas)))
    result=bytearray(struct.pack('<HHH',0,1,len(images)));offset=6+16*len(images)
    for size,data in images:result.extend(struct.pack('<BBBBHHII',size%256,size%256,0,0,1,32,len(data),offset));offset+=len(data)
    for size,data in images:result.extend(data)
    out=Path(out);out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(result)
    return out
if __name__=='__main__':
    import sys
    create(sys.argv[1],sys.argv[2])
