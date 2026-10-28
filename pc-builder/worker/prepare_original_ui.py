from pathlib import Path
import struct, json, re
from extract_ui import u8
ROOT=Path(__file__).resolve().parent
def read(path):return {name.removeprefix('./'):data for name,data in u8(path.read_bytes())}
def pack(files):
    tree={}
    for path,data in files.items():
        node=tree
        parts=path.split('/')
        for part in parts[:-1]: node=node.setdefault(part,{})
        node[parts[-1]]=data
    entries=[];names=bytearray(b'\0')
    def visit(name,node,parent):
        idx=len(entries);name_offset=len(names) if name else 0
        if name:names.extend(name.encode()+b'\0')
        entries.append([bool(isinstance(node,dict)),name_offset,parent,0,node])
        if isinstance(node,dict):
            for child,content in sorted(node.items()):visit(child,content,idx)
            entries[idx][3]=len(entries)
    visit('',tree,0)
    start=0x20;header_size=len(entries)*12+len(names);offset=(start+header_size+31)&~31
    result=bytearray(struct.pack('>4I',0x55aa382d,start,header_size,offset)+b'\0'*16)
    records=bytearray();payload=bytearray()
    for directory,name,parent,end,node in entries:
        if directory:records.extend(struct.pack('>III',0x1000000|name,parent,end))
        else:
            while (offset+len(payload))%32:payload.append(0)
            records.extend(struct.pack('>III',name,offset+len(payload),len(node)));payload.extend(node)
    result.extend(records);result.extend(names);result.extend(b'\0'*(offset-len(result)));result.extend(payload)
    # A valid Yaz0 stream using literal blocks. No lossy image or layout conversion.
    raw=bytes(result);encoded=b'Yaz0'+struct.pack('>I',len(raw))+b'\0'*8+b''.join(b'\xff'+raw[i:i+8] for i in range(0,len(raw),8))
    assert dict(u8(encoded))==files
    return encoded
def main():
    disc=ROOT/'Install/GameAssets/DATA/files/Scene/UI'
    preview=ROOT/'preview/GameData'
    native=read(disc/'MenuSingle.szs')
    required={name for name in native if '/ctrl/VSSetting' in name}
    required.update(name for name in native if name.startswith('bg/'))
    required.add('button/ctrl/Back.brctr')
    pending=list(required)
    while pending:
        name=pending.pop();folder=name.split('/')[0]
        for raw in re.findall(rb'[A-Za-z_][A-Za-z0-9_.-]{3,}',native[name]):
            value=raw.decode('ascii')
            candidates=[f'{folder}/blyt/{value}.brlyt',f'{folder}/blyt/{value}',f'{folder}/anim/{value}',f'{folder}/timg/{value}']
            for candidate in candidates:
                if candidate in native and candidate not in required:required.add(candidate);pending.append(candidate)
    controls={name:native[name] for name in sorted(required)}
    race_controls=controls.copy()
    # Retro Rewind also precreates its shared friend/settings pages in races.
    # Make their non-message resources available in the race archive as well.
    for name,data in read(ROOT/'RetroRewind6/Assets/UIAssets.szs').items():
        if not name.startswith('message/'):race_controls.setdefault(name,data)
    print('Original graphics dependencies',len(controls),sum(map(len,controls.values())),flush=True)
    log=[]
    for target in [preview/'Install/GameAssets/DATA/files/Scene/UI/MenuOther.szs',preview/'Install/GameAssets/DATA/files/Scene/UI/Race.szs',preview/'RetroRewind6/UI/Race.szs',preview/'RetroRewind6/UI/MenuSingle.szs',preview/'RetroRewind6/UI/Channel.szs',preview/'RetroRewind6/Assets/RaceAssets.szs']:
        source=disc/target.name if 'GameAssets' in target.parts else ROOT/'RetroRewind6'/target.parent.name/target.name
        existing=read(source)
        # Preserve Retro Rewind's additional native controls, importing stock controls.
        for name,data in (race_controls if target.name in ('Race.szs','RaceAssets.szs') else controls).items():existing.setdefault(name,data)
        target.write_bytes(pack(existing));log.append({'path':str(target),'entries':len(existing)})
    for source,target in [('Title.szs','Title.szs'),('Title_E.szs','Title_U.szs')]:
        path=preview/'RetroRewind6/UI'/target
        existing=read(ROOT/'RetroRewind6/UI'/target);existing.update(read(disc/source))
        if target=='Title.szs':
            for name,data in controls.items():existing.setdefault(name,data)
        path.write_bytes(pack(existing));log.append({'path':str(path),'entries':len(existing),'original':source})
    (ROOT/'original-ui-assets.json').write_text(json.dumps(log,indent=2))
    print(json.dumps(log,indent=2))
if __name__=='__main__':main()
