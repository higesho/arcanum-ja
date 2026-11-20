# Collect the player-visible English text -> text/src/units.json + work/fullmap.json
#   units:   [{"id", "en", "kind", "file", ...}]  (unique translatable segments)
#   fullmap: {full English string: [prefix, seg, sep, seg, ...]} to rebuild whole strings at merge time
# Only mes\*.mes (minus data/command files) and dlg\*.dlg, exactly the files the DLL translates.
import os, re, json, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
EX = os.path.join(ROOT, 'work', 'ex')
# later DATs override earlier ones (patch / module data win)
DAT_ORDER = ['arcanum1', 'arcanum2', 'arcanum3', 'Arcanum4', 'Arcanum']   # Vormantown = MP demo module, skipped

def fields(data):
    out = []; i = 0
    while True:
        a = data.find('{', i)
        if a < 0: return out
        b = data.find('}', a + 1)
        if b < 0: return out
        out.append(data[a + 1:b]); i = b + 1

CODE = re.compile(r'^\s*[a-zA-Z]:')
SKIP_FILES = {'mes/language.mes', 'mes/urls.mes', 'mes/userlevelname.mes'}
WRITTEN_PREFIX = {'mes/gamebook.mes', 'mes/gameplaque.mes', 'mes/gamenewspaper.mes'}
KIND = {
    'mes/description.mes': 'name', 'mes/gamedesc.mes': 'name', 'mes/gameitemeffect.mes': 'effect',
    'mes/item_effect.mes': 'effect', 'mes/gamebook.mes': 'book', 'mes/gamenote.mes': 'note',
    'mes/gameplaque.mes': 'book', 'mes/gamenewspaper.mes': 'news', 'mes/gametelegram.mes': 'note',
    'mes/quotes.mes': 'note', 'mes/gamequestlog.mes': 'quest', 'mes/gamequestlogdumb.mes': 'questdumb',
    'mes/gamereplog.mes': 'quest', 'mes/storystate.mes': 'quest', 'mes/gameback.mes': 'quest',
    'mes/gamearea.mes': 'area', 'mes/text_filter_sleep.mes': 'note',
}

def has_letters(s): return re.search(r'[A-Za-z]{2,}', s) is not None

def latest_files():
    files = {}
    for dat in DAT_ORDER:
        base = os.path.join(EX, dat)
        for dp, dn, fn in os.walk(base):
            for f in fn:
                p = os.path.join(dp, f)
                rel = os.path.relpath(p, base).replace('\\', '/').lower()
                if rel.endswith('.dlg') or (rel.startswith('mes/') and rel.endswith('.mes')):
                    files[rel] = p
    return files

def main():
    files = latest_files()
    units, by_en, fullmap = [], {}, {}
    def unit(en, kind, rel, extra=None):
        if en in by_en:
            u = by_en[en]
            if len(u['refs']) < 3: u['refs'].append(rel)
            return
        u = {'id': len(units) + 1, 'en': en, 'kind': kind, 'file': rel, 'refs': []}
        if extra: u.update(extra)
        units.append(u); by_en[en] = u
    for rel in sorted(files):
        if rel in SKIP_FILES: continue
        data = open(files[rel], 'rb').read().decode('latin-1')
        fl = fields(data)
        if rel.endswith('.mes'):
            name = rel.split('/')[-1][:-4]
            kind = KIND.get(rel)
            if not kind:
                if name.startswith(('gd_', 'game_rd', 'game_rp')): kind = 'gd'
                else: kind = 'ui'
            for k in range(0, len(fl) - 1, 2):
                num, s = fl[k].strip(), fl[k + 1]
                try: n = int(num)
                except ValueError: continue
                if rel == 'mes/broadcast.mes' and n < 1000: continue    # typed follower commands
                if rel == 'mes/mainmenu.mes' and len(s.strip()) == 1: continue  # hotkey letters
                if not has_letters(s): continue
                if re.match(r'^(MI|Tech): ', s): continue              # internal spell names
                if rel == 'mes/critter.mes' and n <= 10: continue      # class identifiers
                extra = None
                if kind == 'gd':
                    g = re.search(r'_([mfpc2]+)\.mes$', rel)
                    extra = {'who': g.group(1) if g else ''}
                    if 'dumb' in rel: extra['dumb'] = 1
                if rel == 'mes/gamearea.mes':
                    m = re.match(r'^([^/]*/)([^/]*)(/?)([^/]*)(/?.*)$', s, re.S)
                    if not m: continue
                    pre, nm, sl, desc, rest = m.groups()
                    parts = [pre, nm, sl, desc, rest]
                    if has_letters(nm): unit(nm, 'place', rel)
                    if has_letters(desc): unit(desc, 'area', rel)
                    fullmap[s] = ['seg', pre, nm, sl, desc, rest]
                    continue
                if rel in WRITTEN_PREFIX:
                    m = re.match(r'^(\d+[A-Za-z]?[ \t]?)(.*)$', s, re.S)
                    if m and has_letters(m.group(2)):
                        unit(m.group(2), kind, rel)
                        fullmap[s] = ['pre', m.group(1), m.group(2)]
                        continue
                unit(s, kind, rel, extra)
        else:
            for k in range(0, len(fl) - 6, 7):
                num, s, g, iq, cond, resp, act = fl[k:k + 7]
                iqs = iq.strip()
                pc = iqs != ''
                if not CODE.match(s) and has_letters(s):
                    ex = {'line': num.strip()}
                    kind = 'npc'
                    if pc:
                        try: v = int(iqs.split(',')[0])
                        except ValueError: v = 1
                        kind = 'pcdumb' if v < 0 else 'pc'
                    else:
                        if g.strip() and g != s: ex['male_pc'] = 1
                    unit(s, kind, rel, ex)
                if not pc and g.strip() and g != s and has_letters(g) and not CODE.match(g):
                    unit(g, 'npc', rel, {'line': num.strip(), 'female_pc': 1})
    os.makedirs(os.path.join(ROOT, 'text', 'src'), exist_ok=True)
    json.dump(units, open(os.path.join(ROOT, 'text', 'src', 'units.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    json.dump(fullmap, open(os.path.join(ROOT, 'work', 'fullmap.json'), 'w', encoding='utf-8'), ensure_ascii=False)
    from collections import Counter
    c, cc = Counter(), Counter()
    for u in units: c[u['kind']] += 1; cc[u['kind']] += len(u['en'])
    print(len(units), 'units', sum(cc.values()), 'chars')
    for k in c: print('  %-10s %6d %8d' % (k, c[k], cc[k]))

if __name__ == '__main__':
    main()
