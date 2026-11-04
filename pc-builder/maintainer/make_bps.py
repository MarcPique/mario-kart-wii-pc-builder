"""Create a BPS patch; publish changes rather than the generated game message bank."""
from pathlib import Path
import argparse,json,struct,zlib


def number(value):
    result=bytearray()
    while True:
        byte=value&127;value>>=7
        if not value:result.append(byte|128);return result
        result.append(byte);value-=1


def create(source,target,metadata=b''):
    if max(len(source),len(target))>16*1024*1024:raise ValueError('Language bank exceeds limit')
    output=bytearray(b'BPS1')+number(len(source))+number(len(target))+number(len(metadata))+metadata
    positions={}
    for p in range(max(0,len(source)-15)):positions.setdefault(source[p:p+16],p)
    cursor=0;source_relative=0;literal=bytearray()
    def flush():
        if literal:output.extend(number(((len(literal)-1)<<2)|1));output.extend(literal);literal.clear()
    while cursor<len(target):
        start=cursor if source[cursor:cursor+16]==target[cursor:cursor+16] and cursor+16<=len(source) else positions.get(target[cursor:cursor+16])
        if start is None or cursor+16>len(target):literal.append(target[cursor]);cursor+=1;continue
        length=16;maximum=min(len(source)-start,len(target)-cursor)
        while length+64<=maximum and source[start+length:start+length+64]==target[cursor+length:cursor+length+64]:length+=64
        while length<maximum and source[start+length]==target[cursor+length]:length+=1
        flush()
        if start==cursor:output.extend(number((length-1)<<2))
        else:
            output.extend(number(((length-1)<<2)|2));distance=start-source_relative
            output.extend(number((abs(distance)<<1)|(distance<0)));source_relative=start+length
        cursor+=length
    flush();output.extend(struct.pack('<II',zlib.crc32(source),zlib.crc32(target)))
    output.extend(struct.pack('<I',zlib.crc32(output)))
    return bytes(output)


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('source',type=Path);parser.add_argument('target',type=Path);parser.add_argument('patch',type=Path)
    options=parser.parse_args();options.patch.write_bytes(create(options.source.read_bytes(),options.target.read_bytes()))
    print(json.dumps({'patch_bytes':options.patch.stat().st_size}))
