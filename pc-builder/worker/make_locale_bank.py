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
    import hashlib,shutil
    from extended_locales import collect,encode
    from bps_patch import apply
    menus,races,characters,audit=collect(ROOT)
    source=encode(menus,races)
    assets=Path(__file__).resolve().parent/'locales'
    metadata=json.loads((assets/'pack.json').read_text(encoding='utf-8'))
    if hashlib.sha256(source).hexdigest()!=metadata['base_sha256']:
        raise ValueError('Los archivos de idioma no coinciden con el disco y el pack compatibles.')
    required=set(metadata['required_codepoints'])
    if not required.issubset(characters):
        raise ValueError('Las fuentes instaladas no contienen todos los caracteres de los idiomas.')
    patch=(assets/'extra-languages.bps').read_bytes()
    if hashlib.sha256(patch).hexdigest()!=metadata['patch_sha256']:
        raise ValueError('El parche de idiomas está dañado.')
    result=apply(source,patch)
    if hashlib.sha256(result).hexdigest()!=metadata['target_sha256']:
        raise ValueError('La verificación del banco de idiomas ha fallado.')
    destination=ROOT/'preview/GameData/Install/RetroRewind/pc_locale';destination.mkdir(parents=True,exist_ok=True)
    (destination/'texts.bin').write_bytes(result)
    shutil.copy2(assets/'menu.json',destination/'menu.json')
    (ROOT/'locale-bank-audit.json').write_text(json.dumps({'bytes':len(result),'unique_ids':struct.unpack_from('<I',result,8)[0],
        'languages':metadata['languages'],'font_coverage':'verified','base_languages':audit,
        'machine_translated_game_text':metadata['machine_translated_game_text'],'transliteration':metadata['transliteration']},indent=2))
    print(json.dumps({'bytes':len(result),'ids':struct.unpack_from('<I',result,8)[0],'languages':metadata['languages'],'font_coverage':'verified'}))
if __name__=='__main__':main()
