# build/tr.json ({English: Japanese}) -> build/arcjp.bin (resource ARCJP/1)
#   "AJP1", count, count x {u64 fnv64(English latin-1 bytes), u32 offset, u32 0}, EUC-JP strings
import json, os, struct, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

def fnv64(b):
    h = 0xcbf29ce484222325
    for c in b:
        if c == 13: continue                  # CR is ignored (text-mode reads may drop it)
        h ^= c; h = (h * 0x100000001b3) & 0xffffffffffffffff
    return h

# characters euc_jp cannot hold -> nearest JIS X 0208 form
REPL = {'～': '〜', '－': '−', '・': '・', '“': '「', '”': '」', '‘': '\'', '’': '\'', '—': '―', '–': '−',
        '♪': '♪', '①': '(1)', '②': '(2)', '③': '(3)', 'é': 'e', 'è': 'e', 'ü': 'u', 'ö': 'o', 'ä': 'a',
        ' ': ' ', '…': '…', '〜': '〜', '♥': '♡', '❤': '♡', 'ⅰ': 'i', '╳': '×', '①': '(1)'}

def to_euc(s, bad):
    out = []
    for ch in s:
        ch = REPL.get(ch, ch)
        try:
            out.append(ch.encode('euc_jp'))
        except UnicodeEncodeError:
            bad[ch] = bad.get(ch, 0) + 1
            out.append(b'?')
    b = b''.join(out)
    if b'\x8e' in b or b'\x8f' in b:          # half-width kana / JIS X 0212: not supported by the renderer
        raise ValueError('half-width or 0212 char in %r' % s)
    return b

def main():
    tr = json.load(open(os.path.join(ROOT, 'build', 'tr.json'), encoding='utf-8'))
    bad = {}
    ents = {}
    for en, ja in tr.items():
        h = fnv64(en.encode('latin-1'))
        if h in ents: continue
        ents[h] = to_euc(ja.replace(chr(13), ''), bad)
    keys = sorted(ents)
    blob = bytearray(); offs = {}
    for k in keys:
        offs[k] = len(blob); blob += ents[k] + b'\0'
    out = bytearray(b'AJP1' + struct.pack('<I', len(keys)))
    for k in keys: out += struct.pack('<QII', k, offs[k], 0)
    out += blob
    open(os.path.join(ROOT, 'build', 'arcjp.bin'), 'wb').write(out)
    print(len(keys), 'entries', len(out), 'bytes')
    if bad: print('unencodable:', ''.join(sorted(bad)), bad)

if __name__ == '__main__':
    main()
