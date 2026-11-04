"""Extract translation fragments locally; callers must keep the corpus out of published artifacts."""
from pathlib import Path
import sys,re,json,unicodedata

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
SPLIT=re.compile(r'(\r\n|\r|\n|%[-+#0 .\d]*[sdifuxX]|\b(?:A|B|X|Y|L|R|Z|L1|R1|L2|R2):\s*)')


def units(text):
    for i,part in enumerate(SPLIT.split(text)):
        if not part:continue
        if i%2:
            yield False,part
            continue
        for sentence in re.split(r'(?<=[.!?])(?=\s)',part):
            while len(sentence)>300:
                cut=sentence.rfind(' ',0,300)
                if cut<80:cut=300
                yield True,sentence[:cut]
                sentence=sentence[cut:]
            if sentence:yield True,sentence


def key(text):
    value=text.strip()
    if not value or value in PRESERVED or not any(c.isalpha() for c in value):return None
    if re.fullmatch(r'\d+\s*(?:cc|FPS|Hz|p|x)',value):return None
    if '://' in value or re.fullmatch(r'[\w.]+\.(?:com|net|org|dll|exe|szs|brres)',value):return None
    return value


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
                if code in ('ca','gl','eu') or ident not in destination:
                    jobs[code].update(spans(message))
    pc=pc_strings()
    for code in TARGETS:
        for english in pc:
            for active,text in units(english):
                value=key(text) if active else None
                if value:jobs[code].add(value)
    return menus,races,characters,audit,pc,{code:sorted(values) for code,values in jobs.items()}


def translate_plain(text,cache,characters):
    output=[]
    for active,part in units(text):
        lookup=key(part) if active else None
        if lookup is None:output.append(part);continue
        translated=cache[lookup].strip()
        if not translated:raise ValueError('Empty translation')
        leading=part[:len(part)-len(part.lstrip())];trailing=part[len(part.rstrip()):]
        output.append(leading+typography(unicodedata.normalize('NFC',translated),characters)+trailing)
    return ''.join(output)
