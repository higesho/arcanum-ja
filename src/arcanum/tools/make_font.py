# Noto Serif JP (OFL) -> SemiBold static instance, subset to the characters the patch can show
# -> build/arcjp_font.ttf (resource ARCJP/2, family "ArcJP Serif")
import json, os
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer
from fontTools import subset
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
SRC = r'C:\Windows\Fonts\NotoSerifJP-VF.ttf'

def euc_chars(blob):
    """decode the EUC bytes exactly like font.c does (EUC -> SJIS -> cp932)"""
    out = set(); i = 0
    while i < len(blob) - 1:
        a, b = blob[i], blob[i + 1]
        if 0xa1 <= a <= 0xfe and 0xa1 <= b <= 0xfe:
            j1, j2 = a - 0x80, b - 0x80
            s1 = ((j1 + 1) >> 1) + (0x70 if j1 <= 0x5e else 0xb0)
            s2 = j2 + ((0x20 if j2 >= 0x60 else 0x1f) if j1 & 1 else 0x7e)
            try: out.add(bytes([s1, s2]).decode('cp932'))
            except UnicodeDecodeError: pass
            i += 2
        else: i += 1
    return out

def main():
    blob = open(os.path.join(ROOT, 'build', 'arcjp.bin'), 'rb').read()
    chars = euc_chars(blob)
    extra = bytearray()
    for a in range(0xa1, 0xa9):            # JIS rows 1-8: symbols, kana, greek, cyrillic, box
        for b in range(0xa1, 0xff): extra += bytes([a, b])
    chars |= euc_chars(bytes(extra))
    chars |= {chr(c) for c in range(0x20, 0x7f)}   # Latin drawn by the patch in small fonts
    print('chars', len(chars))
    f = TTFont(SRC)
    f = instancer.instantiateVariableFont(f, {'wght': 600})
    opt = subset.Options()
    opt.name_IDs = ['*']; opt.name_languages = ['*']; opt.notdef_outline = True; opt.hinting = False
    opt.layout_features = []; opt.drop_tables += ['DSIG']
    s = subset.Subsetter(opt)
    s.populate(unicodes=[ord(c) for c in chars])
    s.subset(f)
    for rec in f['name'].names:
        if rec.nameID in (1, 4, 16):
            rec.string = 'ArcJP Serif'
        elif rec.nameID == 6:
            rec.string = 'ArcJPSerif'
    f.save(os.path.join(ROOT, 'build', 'arcjp_font.ttf'))
    print(os.path.getsize(os.path.join(ROOT, 'build', 'arcjp_font.ttf')), 'bytes')

if __name__ == '__main__':
    main()
