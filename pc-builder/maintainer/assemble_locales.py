"""Create source-checked BPS localization changes from a private translation cache."""
from pathlib import Path
import sys,json,hashlib,unicodedata,random
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'pc-builder/worker'))
from localization_helpers import corpus,translate_plain,TARGETS
from locale_messages import transform_message,message_parts,visible_characters
from extended_locales import encode,typography
from locale_overrides import TEXT,GAME,OTHER
from make_bps import create
from bps_patch import apply

def assemble(workspace,cache_file,output,audit_dir):
    import andaluh
    audit_dir.mkdir(parents=True,exist_ok=True)
    random.seed(0)
    menus,races,characters,audit,pc,jobs=corpus(workspace)
    assert set(pc)<=set(TEXT),set(pc)-set(TEXT)
    cache=json.loads(cache_file.read_text(encoding='utf-8'))
    for code in ('ca','gl','eu'):
        index=('ca','gl','eu').index(code)
        for source,values in {**TEXT,**GAME}.items():cache[code][source.strip()]=values[index].strip()
    for code,entries in OTHER.items():cache[code].update(entries)
    source=encode(menus,races)
    missing=[]
    def translated_record(record,code):
        font,data=record
        transformed=transform_message(data,lambda text:translate_plain(text,cache[code],characters))
        if transformed!=data:font=1
        return font,transformed

    for code in TARGETS:
        for context in [menus,races]:
            target=context.setdefault(code,{})
            for ident,record in context['en'].items():
                if code in ('ca','gl','eu') or ident not in target:target[ident]=translated_record(record,code)

    def epa(text):
        # BMG commands never enter the transcriber; retain manual line breaks.
        return typography(unicodedata.normalize('NFC',andaluh.epa(text)),characters)

    def epa_pc(text):
        import re
        corrections={'Resolucion':'Resolución','Vibracion':'Vibración','grafica':'gráfica',
                     'anisotropico':'anisotrópico','nitida':'nítida','publica':'pública',
                     'publicas':'públicas','opcion':'opción','version':'versión',
                     'automatico':'automático','asignalo':'asígnalo','Segun':'Según'}
        if text=='Si':text='Sí'
        for source,target in corrections.items():text=re.sub(r'\b'+source+r'\b',target,text)
        return epa(text)

    for context in [menus,races]:
        context['and_ES']={}
        for ident,(font,data) in context['es'].items():
            translated=transform_message(data,epa)
            context['and_ES'][ident]=(1 if translated!=data else font,translated)

    menu_output={}
    for code in menus:
        menu_output[code]={}
        for english,values in pc.items():
            if code in values:translated=values[code]
            elif code=='es-419':translated=values['es']
            elif code=='and_ES':translated=epa_pc(values['es'])
            elif code in ('ca','gl','eu'):translated=TEXT[english][('ca','gl','eu').index(code)]
            elif english.strip() in OTHER.get(code,{}):
                translated=translate_plain(english,cache[code],characters)
            else:translated=translate_plain(english,cache[code],characters)
            translated=typography(unicodedata.normalize('NFC',translated),characters)
            absent={ord(c) for c in translated if ord(c)>32}-characters
            if absent:missing.append({'code':code,'pc':english,'missing':[hex(c) for c in absent],'text':translated})
            menu_output[code][english]=translated

    for context_name,context in [('menu',menus),('race',races)]:
        for code,entries in context.items():
            for ident,(font,data) in entries.items():
                if code in ('es','en','fr','de','it'):continue
                absent=visible_characters(data)-characters
                if absent:missing.append({'code':code,'context':context_name,'id':ident,'missing':[hex(c) for c in absent],'text':str(message_parts(data))})
                # Every embedded button/variable/format command is retained.
                original=context['es' if code=='and_ES' else 'en'].get(ident)
                if code in ('ca','gl','eu','and_ES') and original:
                    commands=lambda b:[p for p in message_parts(b) if isinstance(p,bytes)]
                    assert commands(data)==commands(original[1]),(code,ident,'command mismatch')

    (audit_dir/'font-problems.json').write_text(json.dumps(missing,ensure_ascii=False,indent=2),encoding='utf-8')
    if missing:
        print('FONT PROBLEMS',len(missing),json.dumps(missing[:12],ensure_ascii=True));raise SystemExit(1)
    target=encode(menus,races)
    metadata={'schema':1,'languages':list(menus),'machine_translated_game_text':['ca','gl','eu'],
              'machine_supplemented':['nl','cs','fi','ru','tr','es-419'],
              'transliteration':['and_ES'],'model':'facebook/nllb-200-distilled-600M',
              'model_revision':'f8d333a098d19b4fd9a8b18f94170487ad3f821d',
              'andaluh':'0.3.1','base_sha256':hashlib.sha256(source).hexdigest(),
              'target_sha256':hashlib.sha256(target).hexdigest()}
    required=set()
    for context in [menus,races]:
        for entries in context.values():
            for font,message in entries.values():required.update(visible_characters(message))
    for texts in menu_output.values():
        for text in texts.values():required.update(ord(c) for c in text if ord(c)>32)
    assert required.issubset(characters)
    metadata['required_codepoints']=sorted(required)
    patch=create(source,target,json.dumps(metadata,separators=(',',':')).encode())
    assert apply(source,patch)==target
    metadata['patch_sha256']=hashlib.sha256(patch).hexdigest()
    destination=output;destination.mkdir(exist_ok=True,parents=True)
    (destination/'extra-languages.bps').write_bytes(patch)
    (destination/'pack.json').write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf-8')
    (destination/'menu.json').write_text(json.dumps({'schema':1,'languages':menu_output},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    (audit_dir/'target.bin').write_bytes(target)
    print(json.dumps({'languages':list(menus),'bytes':len(target),'patch_bytes':len(patch),'menu_keys':len(pc),'unsupported_characters':len(missing),'target_sha256':metadata['target_sha256']},indent=2))
