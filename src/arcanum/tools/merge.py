# text/ja/*.json (+ text/fixes.json) -> build/tr.json  {full English string: Japanese}
#   unit translations are keyed by unit id; split strings (gamearea, written prefix) are rebuilt
#   from work/fullmap.json. text/fixes.json = {English: Japanese} manual overrides (win last).
import os, json, glob, re
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

def main():
    units = json.load(open(os.path.join(ROOT, 'text', 'src', 'units.json'), encoding='utf-8'))
    by_id = {u['id']: u for u in units}
    seg = {}
    for p in sorted(glob.glob(os.path.join(ROOT, 'text', 'ja', 'c*.json'))):
        try: ja = json.load(open(p, encoding='utf-8'))
        except Exception as e:
            print('BAD', p, e); continue
        if isinstance(ja, list): ja = {str(u.get('id')): u.get('ja') for u in ja if isinstance(u, dict)}
        for k, v in ja.items():
            try: u = by_id[int(k)]
            except (ValueError, KeyError): continue
            if isinstance(v, str) and v.strip(): seg[u['en']] = v
    fx = os.path.join(ROOT, 'text', 'fixes.json')
    if os.path.exists(fx): seg.update(json.load(open(fx, encoding='utf-8')))
    tr = dict(seg)
    fullmap = json.load(open(os.path.join(ROOT, 'work', 'fullmap.json'), encoding='utf-8'))
    for full, parts in fullmap.items():
        if parts[0] == 'pre':
            pre, body = parts[1], parts[2]
            if body in seg: tr[full] = pre + seg[body]
        elif parts[0] == 'seg':
            pre, nm, sl, desc, rest = parts[1:]
            if nm in seg or desc in seg:
                tr[full] = pre + seg.get(nm, nm) + sl + seg.get(desc, desc) + rest
    # the game's Latin fonts draw [ ] < > taller than the Japanese glyphs: use full-width forms
    FW = str.maketrans({'[': '［', ']': '］', '<': '＜', '>': '＞'})
    for k in tr:
        if re.search(r'[぀-ヿ一-鿿]', tr[k]): tr[k] = tr[k].translate(FW)
    json.dump(tr, open(os.path.join(ROOT, 'build', 'tr.json'), 'w', encoding='utf-8'), ensure_ascii=False)
    print('units translated %d/%d, table %d' % (len({u['en'] for u in units} & set(seg)), len(units), len(tr)))

if __name__ == '__main__':
    main()
