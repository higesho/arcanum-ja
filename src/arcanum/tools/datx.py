# Arcanum DAT reader: python datx.py list DAT | python datx.py extract DAT OUTDIR [ext,ext]
import sys, os, struct, zlib
def read_dat(path):
    f = open(path, 'rb'); data_size = os.path.getsize(path)
    f.seek(-12, 2); fid, nts = struct.unpack('<4sI', f.read(8))
    if fid == b'1TAD':
        pass
    f.seek(-12 + 8, 2)
    eto, = struct.unpack('<I', f.read(4))
    f.seek(-4 - eto, 2)
    ets, = struct.unpack('<I', f.read(4))
    base = data_size - ets - eto
    cnt, = struct.unpack('<I', f.read(4))
    ents = []
    for i in range(cnt):
        ns, = struct.unpack('<I', f.read(4)); name = f.read(ns).rstrip(b'\0').decode('latin-1')
        _, flags, size, csize, off = struct.unpack('<5I', f.read(20))
        ents.append((name.replace(chr(92), '/').lower(), flags, size, csize, off + base))
    return f, ents
def get(f, e):
    name, flags, size, csize, off = e
    f.seek(off)
    if flags & 2: return zlib.decompress(f.read(csize))
    return f.read(size)
if __name__ == '__main__':
    f, ents = read_dat(sys.argv[2])
    if sys.argv[1] == 'list':
        for e in ents: print(e[1], e[2], e[0])
    else:
        exts = sys.argv[4].split(',') if len(sys.argv) > 4 else None
        n = 0
        for e in ents:
            if e[1] & 0x400: continue
            if exts and e[0].rsplit('.', 1)[-1] not in exts: continue
            out = os.path.join(sys.argv[3], e[0]); os.makedirs(os.path.dirname(out), exist_ok=True)
            open(out, 'wb').write(get(f, e)); n += 1
        print(n, 'files')
