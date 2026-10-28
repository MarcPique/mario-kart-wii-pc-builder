from pathlib import Path
import struct,json
from prepare_original_ui import read
ROOT=Path(__file__).resolve().parent
def bmg(data):
    assert data[:8]==b'MESGbmg1'
    sections={};p=32
    for _ in range(struct.unpack_from('>I',data,12)[0]):
        kind,size=struct.unpack_from('>4sI',data,p);assert size>=8 and p+size<=len(data)
        sections[kind]=data[p:p+size];p+=size
    info,mid,pool=sections[b'INF1'],sections[b'MID1'],sections[b'DAT1'][8:]
    count,step=struct.unpack_from('>HH',info,8);assert count==struct.unpack_from('>H',mid,8)[0] and step>=8
    offsets=[struct.unpack_from('>I',info,16+i*step)[0]&0xfffffffe for i in range(count)]
    ends=sorted(set(offsets+[len(pool)]));following={a:b for a,b in zip(ends,ends[1:])}
    result={}
    for i,start in enumerate(offsets):
        if start>=len(pool):continue
        ident=struct.unpack_from('>I',mid,16+4*i)[0];font=info[16+i*step+4]
        text=pool[start:following[start]]
        assert len(text)%2==0 and len(text)<65536
        if not text.endswith(b'\0\0'):text+=b'\0\0'
        result[ident]=(font,text)
    return result
def main():
    ui=ROOT/'Install/GameAssets/DATA/files/Scene/UI';rr=ROOT/'RetroRewind6';banks=[];race_banks=[];audit=[]
    for code,suffix,folder in [('es','S','SPA(EU)'),('en','E',None),('fr','F','FRA'),('de','G','GER'),('it','I','ITA')]:
        entries={};stock={};sources=[];conflicts=[]
        paths=sorted(ui.glob(f'*_{suffix}.szs'))
        localized=rr/'Language'/folder/'Assets' if folder else rr/'Assets'
        paths += [localized/'RaceAssets.szs',localized/'UIAssets.szs']
        for path in paths:
            for name,data in read(path).items():
                if not name.endswith('.bmg'):continue
                parsed=bmg(data)
                for ident,value in parsed.items():
                    if ident in entries and entries[ident]!=value:conflicts.append(ident)
                entries.update(parsed);sources.append([path.name,name,len(parsed)])
                if path.parent==ui:stock.update(parsed)
        # Keep the original offline names. The online screens retain Retro WFC.
        for ident in [0x7de,0x7e2,0x7ea,0x80a,0x80c,0x810,0x819,0x838,0x1004,0x106f,0x1070,0x1388,0x1390,0x1a3e,0x212a]:
            if ident in stock:entries[ident]=stock[ident]
        race_entries=entries.copy()
        for name,data in read(localized/'RaceAssets.szs').items():
            if name.endswith('.bmg'):race_entries.update(bmg(data))
        banks.append(entries);race_banks.append(race_entries);audit.append({'language':code,'entries':len(entries),'overrides':len(set(conflicts)),'sources':sources})
    banks+=race_banks
    # ID 1 is a context-specific fallback, rather than a translatable UI message.
    ids=sorted(set().union(*(b.keys() for b in banks))-{1});payload=bytearray(b'\0\0');dedup={};tables=[]
    for bank in banks:
        records=[]
        for ident in ids:
            font,text=bank.get(ident,banks[1].get(ident,(1,b'\0\0')))
            key=(font,text)
            if key not in dedup:dedup[key]=len(payload);payload.extend(text)
            records.append((dedup[key],font))
        tables.append(records)
    result=bytearray(b'MKWLOC03'+struct.pack('<II',len(ids),len(payload)))
    result.extend(struct.pack(f'<{len(ids)}I',*ids))
    for table in tables:
        for offset,font in table:result.extend(struct.pack('<II',offset,font))
    result.extend(payload)
    path=ROOT/'preview/GameData/Install/RetroRewind/pc_locale/texts.bin';path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(result)
    (ROOT/'locale-bank-audit.json').write_text(json.dumps({'bytes':len(result),'unique_ids':len(ids),'languages':audit},indent=2))
    print(json.dumps({'bytes':len(result),'ids':len(ids),'languages':[{k:v for k,v in a.items() if k!='sources'} for a in audit]}))
if __name__=='__main__':main()
