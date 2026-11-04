"""Apply a BPS translation patch to locally generated data, with all three CRCs checked."""
import struct
import zlib


def apply(source,patch,max_output=16*1024*1024):
    if len(patch)<16 or patch[:4]!=b'BPS1':raise ValueError('Invalid translation patch')
    source_crc,target_crc,patch_crc=struct.unpack_from('<III',patch,len(patch)-12)
    if zlib.crc32(patch[:-4])!=patch_crc:raise ValueError('Damaged translation patch')
    if zlib.crc32(source)!=source_crc:raise ValueError('Translation patch needs the original pinned language data')
    cursor=4;limit=len(patch)-12
    def number():
        nonlocal cursor
        value=0;shift=1
        for _ in range(10):
            if cursor>=limit:raise ValueError('Truncated translation patch')
            byte=patch[cursor];cursor+=1;value+=(byte&127)*shift
            if byte&128:return value
            shift<<=7;value+=shift
        raise ValueError('Translation patch integer is too large')
    source_size=number();target_size=number();metadata_size=number()
    if source_size!=len(source) or target_size>max_output:raise ValueError('Invalid translation patch size')
    if metadata_size>limit-cursor:raise ValueError('Truncated translation patch metadata')
    cursor+=metadata_size;output=bytearray();source_relative=target_relative=0
    while len(output)<target_size:
        instruction=number();length=(instruction>>2)+1;action=instruction&3
        if length>target_size-len(output):raise ValueError('Translation patch writes beyond its output')
        if action==0:
            start=len(output)
            if start+length>len(source):raise ValueError('Translation patch reads beyond its source')
            output.extend(source[start:start+length])
        elif action==1:
            if length>limit-cursor:raise ValueError('Truncated translation patch literal')
            output.extend(patch[cursor:cursor+length]);cursor+=length
        else:
            encoded=number();delta=-(encoded>>1) if encoded&1 else encoded>>1
            if action==2:
                source_relative+=delta
                if source_relative<0 or source_relative+length>len(source):raise ValueError('Invalid translation source copy')
                output.extend(source[source_relative:source_relative+length]);source_relative+=length
            else:
                target_relative+=delta
                if target_relative<0 or target_relative>=len(output):raise ValueError('Invalid translation target copy')
                for _ in range(length):output.append(output[target_relative]);target_relative+=1
    if cursor!=limit or zlib.crc32(output)!=target_crc:raise ValueError('Translation patch output verification failed')
    return bytes(output)
