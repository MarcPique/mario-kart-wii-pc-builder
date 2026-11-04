"""Build language tables from the user's disc and the pinned Retro Rewind pack."""
from pathlib import Path
import struct
from prepare_original_ui import read
from make_locale_bank import bmg
from locale_messages import transform_message, visible_characters, font_characters

BASE = [('es','S','SPA(EU)'),('en','E',None),('fr','F','FRA'),('de','G','GER'),('it','I','ITA')]
EXTRA = [('nl','DUT'),('cs','CZE'),('fi','FIN'),('ru','RUS'),('tr','TUR'),('es-419','SPA(NTSC)')]
OFFLINE_IDS = [0x7de,0x7e2,0x7ea,0x80a,0x80c,0x810,0x819,0x838,0x1004,0x106f,0x1070,0x1388,0x1390,0x1a3e,0x212a]
TYPOGRAPHY = {'\u2013':'-','\u2014':'-','\u200b':'','\u200e':'','\u200f':'','\ufeff':'','\u0140':'l·','\u013f':'L·','\u2018':"'",'\u2019':"'",'\u202f':' ','\u00a0':' '}


def messages(path):
    result={}
    for name,data in read(path).items():
        if name.endswith('.bmg'):result.update(bmg(data))
    return result


def typography(text, characters):
    return ''.join(TYPOGRAPHY.get(c,c) if ord(c) not in characters else c for c in text)


def collect(root):
    root=Path(root);ui=root/'Install/GameAssets/DATA/files/Scene/UI';rr=root/'RetroRewind6'
    menus={};races={};audit=[]
    for code,suffix,folder in BASE:
        stock={};entries={};localized=rr/'Language'/folder/'Assets' if folder else rr/'Assets'
        for path in sorted(ui.glob(f'*_{suffix}.szs')):stock.update(messages(path))
        entries.update(stock)
        for name in ['RaceAssets.szs','UIAssets.szs']:entries.update(messages(localized/name))
        for ident in OFFLINE_IDS:
            if ident in stock:entries[ident]=stock[ident]
        race=entries.copy();race.update(messages(localized/'RaceAssets.szs'))
        menus[code]=entries;races[code]=race
    characters=font_characters(read(rr/'UI/Font.szs')['kart_kanji_font.brfnt'])
    for code,folder in EXTRA:
        localized=rr/'Language'/folder;stock={};entries={}
        for path in sorted((localized/'UI').glob('*.szs')):stock.update(messages(path))
        entries.update(stock)
        for name in ['RaceAssets.szs','UIAssets.szs']:entries.update(messages(localized/'Assets'/name))
        for ident in OFFLINE_IDS:
            if ident in stock:entries[ident]=stock[ident]
        race=entries.copy();race.update(messages(localized/'Assets/RaceAssets.szs'))
        for table in [entries,race]:
            for ident,(font,text) in list(table.items()):
                table[ident]=(font,transform_message(text,lambda value:typography(value,characters)))
        menus[code]=entries;races[code]=race
    for code,entries in menus.items():
        missing=set()
        for ident,(font,text) in entries.items():
            if menus['en'].get(ident)!=(font,text):missing.update(visible_characters(text)-characters)
        audit.append({'language':code,'menu_entries':len(entries),'race_entries':len(races[code]),
                      'missing_from_english':len(set(menus['en'])-set(entries)),
                      'unsupported_characters':sorted(missing)})
    return menus,races,characters,audit


def encode(menus,races):
    codes=list(menus)
    if not codes or 'en' not in codes or len(codes)>64:raise ValueError('Invalid languages')
    ids=sorted(set().union(*(b.keys() for b in [*menus.values(),*races.values()]))-{1})
    payload=bytearray(b'\0\0');dedup={};tables=[]
    for context in [menus,races]:
        for code in codes:
            records=[]
            for ident in ids:
                value=context[code].get(ident,context['en'].get(ident,(1,b'\0\0')))
                font,text=value
                if not 0<=font<=5 or len(text)%2 or not text.endswith(b'\0\0'):raise ValueError('Invalid message')
                if value not in dedup:dedup[value]=len(payload);payload.extend(text)
                records.append((dedup[value],font))
            tables.append(records)
    output=bytearray(b'MKWLOC04'+struct.pack('<IIII',len(ids),len(codes),len(payload),0))
    for code in codes:
        value=code.encode('ascii')
        if not 0<len(value)<8:raise ValueError('Invalid language code')
        output.extend(value.ljust(8,b'\0'))
    output.extend(struct.pack(f'<{len(ids)}I',*ids))
    for table in tables:
        for offset,font in table:output.extend(struct.pack('<II',offset,font))
    output.extend(payload)
    if len(output)>16*1024*1024:raise ValueError('Language bank too large')
    return bytes(output)
