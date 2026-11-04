"""Extract translation fragments locally; callers must keep the corpus out of published artifacts."""
from pathlib import Path
import sys,re,json,unicodedata,textwrap

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'pc-builder/worker'))
from extended_locales import collect,encode,typography
from locale_messages import message_parts,transform_message

TARGETS={'nl':'nld_Latn','cs':'ces_Latn','fi':'fin_Latn','ru':'rus_Cyrl','tr':'tur_Latn',
         'es-419':'spa_Latn','ca':'cat_Latn','gl':'glg_Latn','eu':'eus_Latn'}
PRESERVED={'Mario','Luigi','Wario','Waluigi','Peach','Daisy','Rosalina','Bowser','Yoshi','Toad','Toadette',
           'Funky Kong','Donkey Kong','Diddy Kong','Wii','Nintendo','Mii','Retro Rewind','Retro WFC',
           'CTGP','CT','RR','VR','BR','FPS','MSAA','V-Sync','Wiimmfi','Grand Prix','BGM','SD','NAND',
           'OTT','KO','VS','OK','HD','USB','Bluetooth','CPU','GPU','A','B','X','Y','L','R','Z'}
PRINTF = r'%(?:\d+\$)?[-+#0 ]*(?:\d+|\*)?(?:\.(?:\d+|\*))?(?:hh|ll|[hljztL])?[diuoxXfFeEgGaAcspn%]'
# A line break in a BMG message is layout, not a new sentence. In particular,
# never translate a possessive suffix or half of a warning as its own sentence.
SPLIT=re.compile(r'('+PRINTF+r'|\b(?:A|B|X|Y|L|R|Z|L1|R1|L2|R2):\s*)')


def units(text):
    for i,part in enumerate(SPLIT.split(text)):
        if not part:continue
        if i%2:
            yield False,part
            continue
        # Whole prose spans retain context. Their size is checked by the offline
        # tokenizer; silently splitting sentences here caused nonsensical text.
        yield True,part


def key(text):
    value=' '.join(text.split())
    if not value or value in PRESERVED or not any(c.isalpha() for c in value):return None
    if re.fullmatch(r'\d+\s*(?:cc|FPS|Hz|p|x)',value):return None
    if '://' in value or re.fullmatch(r'[\w.]+\.(?:com|net|org|dll|exe|szs|brres)',value):return None
    return value


def shared_record(contexts,ident):
    """Names/credits and invariant technical labels must not be translated."""
    source=contexts['en'][ident][1]
    return all(contexts[code].get(ident,(0,b''))[1]==source for code in ('es','fr','de','it'))


def reflow(source,translation):
    """Retain a message's layout width without translating its lines separately."""
    if '\n' not in source and '\r' not in source:return translation
    lines=source.splitlines()
    width=max(12,max((len(line.strip()) for line in lines),default=36))
    return textwrap.fill(translation,width=width,break_long_words=False,break_on_hyphens=False)


def transcribe_prose(text,transcribe,names=()):
    """Transcribe prose while keeping identities, format strings and units exact."""
    literals=PRESERVED|set(names)|{'Mario Kart Wii','Mario Kart','Dolphin','WiiConnect24','GameCube',
                                  'Miis','ZIP','7z','PNG','DDS'}
    literals={name for name in literals if len(name)>=2}
    pattern=re.compile(r'(?<!\w)(?:'+'|'.join(re.escape(n) for n in sorted(literals,key=len,reverse=True))+
                       r')(?=\W|$)|'+PRINTF+r'|\b\d+(?:[.,]\d+)?(?:cc|fps|Hz|p|x)\b|\b[ABXYLRZ](?=:)')
    output=[];start=0
    for match in pattern.finditer(text):
        output.extend([transcribe(text[start:match.start()]),match.group()]);start=match.end()
    output.append(transcribe(text[start:]))
    return ''.join(output)


def spans(message):
    for part in message_parts(message):
        if isinstance(part,str):
            for active,text in units(part):
                value=key(text) if active else None
                if value:yield value


def pc_strings():
    result={}
    pattern=re.compile(r'T\(u"((?:[^"\\]|\\.)*)"\s*,u"((?:[^"\\]|\\.)*)"\s*,u"((?:[^"\\]|\\.)*)"\s*,u"((?:[^"\\]|\\.)*)"\s*,u"((?:[^"\\]|\\.)*)"\s*\)')
    for directory in [ROOT/'runtime/include',ROOT/'runtime/src']:
        for path in directory.rglob('*'):
            if path.suffix not in ('.h','.cpp','.inl'):continue
            for es,en,fr,de,it in pattern.findall(path.read_text(encoding='utf-8')):
                values=[json.loads('"'+text+'"') for text in [es,en,fr,de,it]]
                result[values[1]]=dict(zip(['es','en','fr','de','it'],values))
    return result


def corpus(workspace):
    menus,races,characters,audit=collect(Path(workspace))
    jobs={code:set() for code in TARGETS}
    for code in TARGETS:
        for context in [menus,races]:
            source=context['en'];destination=context.get(code,{})
            for ident,(font,message) in source.items():
                if shared_record(context,ident):continue
                if code in ('ca','gl','eu') or ident not in destination:
                    jobs[code].update(spans(message))
    pc=pc_strings()
    return menus,races,characters,audit,pc,{code:sorted(values) for code,values in jobs.items()}


def translate_plain(text,cache,characters):
    output=[]
    for active,part in units(text):
        lookup=key(part) if active else None
        if lookup is None:output.append(part);continue
        translated=cache[lookup].strip()
        if not translated:raise ValueError('Empty translation')
        leading=part[:len(part)-len(part.lstrip())];trailing=part[len(part.rstrip()):]
        translated=typography(unicodedata.normalize('NFC',translated),characters)
        output.append(leading+reflow(part,translated)+trailing)
    return ''.join(output)
