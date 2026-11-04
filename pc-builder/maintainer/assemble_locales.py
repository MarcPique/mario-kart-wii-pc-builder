"""Create source-checked BPS localization changes from a private translation cache."""
from pathlib import Path
import sys,json,hashlib,unicodedata,random
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'pc-builder/worker'))
from localization_helpers import corpus,translate_plain,TARGETS,shared_record,PRINTF,reflow,transcribe_prose,correct_base_wording,validate_review_coverage
from locale_messages import transform_message,message_parts,visible_characters
from extended_locales import encode,typography
from locale_overrides import TEXT,GAME,OTHER
from make_bps import create
from bps_patch import apply

def assemble(workspace,cache_file,output,audit_dir,review_file=None,reviewed_release=False):
    import andaluh
    audit_dir.mkdir(parents=True,exist_ok=True)
    random.seed(0)
    menus,races,characters,audit,pc,jobs=corpus(workspace)
    assert set(pc)<=set(TEXT),set(pc)-set(TEXT)
    cache=json.loads(cache_file.read_text(encoding='utf-8'))
    reviewed_pc=json.loads((Path(__file__).with_name('reviewed_pc.json')).read_text(encoding='utf-8'))['languages']
    for code in ('ca','gl','eu'):
        index=('ca','gl','eu').index(code)
        for source,values in {**TEXT,**GAME}.items():cache[code][source.strip()]=values[index].strip()
    for code,entries in OTHER.items():cache[code].update(entries)
    reviewed={}
    if review_file:
        reviewed=json.loads(Path(review_file).read_text(encoding='utf-8'))
        for code,entries in reviewed.items():
            if code not in TARGETS:raise ValueError('Unknown reviewed language: '+code)
            for english,text in entries.items():
                if not isinstance(text,str) or not text.strip():raise ValueError('Empty reviewed text')
                cache[code][english]=text
    accepted_counts=validate_review_coverage(jobs,reviewed) if reviewed_release else {}
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
                if code in ('ca','gl','eu') or ident not in target:
                    target[ident]=record if shared_record(context,ident) else translated_record(record,code)

    import re
    names=set()
    for context in [menus,races]:
        for ident,(_,data) in context['en'].items():
            if shared_record(context,ident):
                parts=message_parts(data)
                if len(parts)==1 and isinstance(parts[0],str) and '\n' not in parts[0]:names.add(parts[0].strip())
        # Original Spanish proper names remain intact in the EPA transcription.
        for ident,(_,data) in context['es'].items():
            if 0x2328<=ident<=0x2473:
                parts=message_parts(data)
                if len(parts)==1 and isinstance(parts[0],str) and '\n' not in parts[0]:names.add(parts[0].strip())
    names.update(['Mario Kart Wii','Mario Kart','Retro Rewind','Retro WFC','Dolphin','WiiConnect24',
                  'Nintendo','GameCube','Mii','Miis','Wii','ZIP','7z','PNG','DDS','FPS','MSAA','V-Sync'])
    names={name for name in names if name and len(name)>=2}
    def epa(text):
        # BMG commands never enter the transcriber; retain manual line breaks.
        # Product names, credits, formats and numerical engine classes do not
        # become dialect words. Split by spans, not by replacement placeholders.
        return typography(unicodedata.normalize('NFC',transcribe_prose(text,andaluh.epa,names)),characters)

    # Keep the immutable source snapshot above unchanged, and let EPA inherit
    # repaired Spanish rather than the original duplicated invitation wording.
    for context in [menus,races]:correct_base_wording(context)

    for context in [menus,races]:
        context['and_ES']={}
        for ident,(font,data) in context['es'].items():
            translated=data if shared_record(context,ident) else transform_message(data,epa)
            context['and_ES'][ident]=(1 if translated!=data else font,translated)

    menu_output={}
    if set(reviewed_pc)!=set(menus):raise ValueError('Reviewed PC language list does not match the bank')
    for code in menus:
        if set(reviewed_pc[code])!=set(pc):raise ValueError('Reviewed PC catalog needs updating: '+code)
        menu_output[code]={}
        for english,values in pc.items():
            translated=reviewed_pc[code][english]
            translated=typography(unicodedata.normalize('NFC',translated),characters)
            absent={ord(c) for c in translated if ord(c)>32}-characters
            if absent:missing.append({'code':code,'pc':english,'missing':[hex(c) for c in absent],'text':translated})
            menu_output[code][english]=translated

    # Retro Rewind's old language-help message asks for a restart. This runtime
    # refreshes loaded messages immediately; say so in every offered language.
    for context in [menus,races]:
        for code,entries in context.items():
            if 0x295f in entries:
                previous=entries[0x295f][1]
                text=menu_output[code]['Language changes immediately. Saved automatically.']
                entries[0x295f]=(1,transform_message(previous,lambda part:reflow(part,text)))

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
                    formats=lambda b:re.findall(PRINTF,''.join(p for p in message_parts(b) if isinstance(p,str)))
                    assert formats(data)==formats(original[1]),(code,ident,'printf mismatch')

    (audit_dir/'font-problems.json').write_text(json.dumps(missing,ensure_ascii=False,indent=2),encoding='utf-8')
    if missing:
        print('FONT PROBLEMS',len(missing),json.dumps(missing[:12],ensure_ascii=True));raise SystemExit(1)
    target=encode(menus,races)
    metadata={'schema':1,'languages':list(menus),'machine_translated_game_text':['ca','gl','eu'],
              'machine_supplemented':[code for code in TARGETS if code not in ('ca','gl','eu') and jobs[code]],
              'transliteration':['and_ES'],'model':'facebook/nllb-200-distilled-600M',
              'model_revision':'f8d333a098d19b4fd9a8b18f94170487ad3f821d',
              'andaluh':'0.3.1','base_sha256':hashlib.sha256(source).hexdigest(),
              'target_sha256':hashlib.sha256(target).hexdigest(),
              'review_revision':2 if reviewed_release else 1 if reviewed else 0,
              'localization_status':'reviewed' if reviewed_release else 'development',
              'reviewed_languages':list(accepted_counts),
              'translation_context':'whole BMG text spans; reflow after translation',
              'private_review_sha256':hashlib.sha256(Path(review_file).read_bytes()).hexdigest() if review_file else None,
              'reviewed_pc_entries':sum(map(len,menu_output.values())),
              'reviewed_game_spans':{code:len(set(entries)&set(jobs[code])) for code,entries in reviewed.items()}}
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
