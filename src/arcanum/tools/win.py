# Drive the test game through the DLL's shared memory (never the foreground window).
#   python tools/win.py start [args]       copy build/dinput.dll into the game, launch -window with ARCJP_TEST=1
#   python tools/win.py shot NAME           game back-buffer -> capture/NAME.png
#   python tools/win.py key K [K..]         tap keys (DIK names: enter esc space up down ... a..z 0..9 f1..)
#   python tools/win.py click X Y [right]   move the game cursor to X,Y (800x600) and click
#   python tools/win.py mouse X Y
#   python tools/win.py wait MS
#   python tools/win.py type TEXT           text via WM_CHAR to the window titled "Arcanum" of our pid
#   python tools/win.py stop                kill the game, remove the DLL from the game folder
import sys, os, time, mmap, struct, subprocess, shutil, ctypes
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.environ.get('ARCJP_GAME') or os.path.abspath(os.path.join(ROOT, '..', 'Arcanum'))   # ARCJP_GAME=uap/game for the UAP copy
PIDF = os.path.join(ROOT, 'work', 'pid.txt')
SHM_SIZE = 4 + 8 + 256 * 16 + 8 + 260 + 8
O_WR, O_RD, O_CMD = 4, 8, 12
O_REQ = 12 + 4096; O_DONE = O_REQ + 4; O_PATH = O_DONE + 4; O_FRAME = O_PATH + 260; O_BUSY = O_FRAME + 4

DIK = {'esc': 1, 'enter': 0x1c, 'space': 0x39, 'tab': 0x0f, 'back': 0x0e, 'up': 0xc8, 'down': 0xd0,
       'left': 0xcb, 'right': 0xcd, 'lshift': 0x2a, 'lctrl': 0x1d, 'lalt': 0x38, 'pgup': 0xc9, 'pgdn': 0xd1,
       'home': 0xc7, 'end': 0xcf, 'del': 0xd3, 'minus': 0x0c, 'equals': 0x0d}
for i, c in enumerate('1234567890'): DIK[c] = 2 + i
for row, start in (('qwertyuiop', 0x10), ('asdfghjkl', 0x1e), ('zxcvbnm', 0x2c)):
    for i, c in enumerate(row): DIK[c] = start + i
for i in range(10): DIK['f%d' % (i + 1)] = 0x3b + i
DIK['f11'] = 0x57; DIK['f12'] = 0x58

def shm():
    m = mmap.mmap(-1, SHM_SIZE, tagname='ARCJP_TESTIO')
    if struct.unpack_from('<I', m, 0)[0] != 0x54504a41:
        raise SystemExit('game (test DLL) not running')
    return m

def push(m, *cmds):
    for c in cmds:
        while True:
            wr, rd = struct.unpack_from('<ii', m, O_WR)
            if wr - rd < 250: break
            time.sleep(0.02)
        c = list(c) + [0] * (4 - len(c))
        struct.pack_into('<4i', m, O_CMD + (wr & 255) * 16, *c)
        struct.pack_into('<i', m, O_WR, wr + 1)

def settle(m, timeout=60):
    t = time.time()
    while time.time() - t < timeout:
        wr, rd = struct.unpack_from('<ii', m, O_WR)
        busy = struct.unpack_from('<i', m, O_BUSY)[0]
        if wr == rd and not busy: return
        time.sleep(0.03)
    print('settle timeout')

def tap(m, k, hold=60, gap=120):
    code = DIK[k] if k in DIK else int(k, 0)
    push(m, (1, code), (6, hold), (2, code), (6, gap))

def shot(name):
    """PrintWindow on the game's own window (found by pid + class), so overlapping windows do not matter"""
    import ctypes.wintypes as W
    from PIL import Image
    m = shm(); settle(m)
    time.sleep(0.15)
    u = ctypes.windll.user32; g = ctypes.windll.gdi32
    h = game_hwnd()
    r = W.RECT(); u.GetClientRect(h, ctypes.byref(r))
    w, hh = r.right, r.bottom
    hdc = u.GetDC(h); mdc = g.CreateCompatibleDC(hdc); bmp = g.CreateCompatibleBitmap(hdc, w, hh)
    g.SelectObject(mdc, bmp)
    u.PrintWindow(h, mdc, 3)
    buf = ctypes.create_string_buffer(w * hh * 4)
    bi = struct.pack('<IiiHHIIiiII', 40, w, -hh, 1, 32, 0, 0, 0, 0, 0, 0)
    bib = ctypes.create_string_buffer(bi, len(bi))
    g.GetDIBits(mdc, bmp, 0, hh, buf, bib, 0)
    g.DeleteObject(bmp); g.DeleteDC(mdc); u.ReleaseDC(h, hdc)
    im = Image.frombuffer('RGB', (w, hh), buf, 'raw', 'BGRX', 0, 1)
    os.makedirs(os.path.join(ROOT, 'capture'), exist_ok=True)
    out = os.path.join(ROOT, 'capture', name + '.png')
    im.save(out)
    print(out, im.size)

def gshot(name):
    """the game's own screenshot of its back buffer (for DDrawCompat windows PrintWindow cannot read)"""
    from PIL import Image
    m = shm(); settle(m)
    os.makedirs(os.path.join(ROOT, 'capture'), exist_ok=True)
    bmp = os.path.join(ROOT, 'work', 'gshot.bmp')
    path = bmp.encode('mbcs') + bytes(1)
    m[O_PATH:O_PATH + len(path)] = path
    req = struct.unpack_from('<i', m, O_REQ)[0] + 1
    struct.pack_into('<i', m, O_REQ, req)
    t = time.time()
    while struct.unpack_from('<i', m, O_DONE)[0] != req and time.time() - t < 15: time.sleep(0.05)
    out = os.path.join(ROOT, 'capture', name + '.png')
    if not os.path.exists(bmp):
        # modules write screenshots into their own folder (modules\<name>\screenNNNN.bmp)
        import glob
        c = [f for f in glob.glob(os.path.join(GAME, 'modules', '*', 'screen*.bmp')) if time.time() - os.path.getmtime(f) < 30]
        if c: shutil.move(max(c, key=os.path.getmtime), bmp)
    im = Image.open(bmp)
    import ctypes.wintypes as W
    u = ctypes.windll.user32; u.SetProcessDPIAware()
    h = game_hwnd(); r = W.RECT(); u.GetClientRect(h, ctypes.byref(r))
    if im.size != (r.right, r.bottom):
        # windowed High Resolution Patch: the game's screenshot is the whole desktop -> keep the game only
        pt = W.POINT(0, 0); u.ClientToScreen(h, ctypes.byref(pt))
        x0 = pt.x - u.GetSystemMetrics(76); y0 = pt.y - u.GetSystemMetrics(77)
        im = im.crop((x0, y0, x0 + r.right, y0 + r.bottom))
    im.save(out); os.remove(bmp)
    print(out, Image.open(out).size)

def start():
    exe = os.path.join(GAME, 'Arcanum.exe')
    shutil.copy(os.path.join(ROOT, 'build', 'dinput.dll'), os.path.join(GAME, 'dinput.dll'))
    env = dict(os.environ)
    env['ARCJP_TEST'] = '1'
    env['ARCJP_LOG'] = os.path.join(ROOT, 'work', 'arcjp.log')
    # the UAP High Resolution Patch breaks text with -window: use its own Windowed = 1 setting
    args = [] if os.path.isdir(os.path.join(GAME, 'HighRes')) else ['-window']
    for a in sys.argv[2:]:
        if '=' in a and not a.startswith('-'):
            k, v = a.split('=', 1); env[k] = v
        else: args.append(a)
    p = subprocess.Popen([exe] + args, cwd=GAME, env=env)
    open(PIDF, 'w').write(str(p.pid))
    print('pid', p.pid)

def stop():
    try:
        pid = int(open(PIDF).read())
        subprocess.run(['taskkill', '/PID', str(pid), '/F'], capture_output=True)
    except Exception as e:
        print(e)
    time.sleep(1.5)
    try: os.remove(os.path.join(GAME, 'dinput.dll'))
    except OSError as e: print(e)

def game_hwnd():
    import ctypes.wintypes as W
    u = ctypes.windll.user32
    pid = int(open(PIDF).read())
    hw = []
    @ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
    def cb(h, l):
        p = W.DWORD(); u.GetWindowThreadProcessId(h, ctypes.byref(p))
        b = ctypes.create_unicode_buffer(64); u.GetWindowTextW(h, b, 64)
        c = ctypes.create_unicode_buffer(64); u.GetClassNameW(h, c, 64)
        if p.value == pid and c.value == 'TIGClass': hw.append(h)
        return True
    u.EnumWindows(cb, 0)
    if not hw: raise SystemExit('game window not found')
    return hw[0]

def main():
    a = sys.argv[1:]
    if not a: print(__doc__); return
    c = a[0]
    if c == 'start': start()
    elif c == 'stop': stop()
    elif c == 'shot': shot(a[1])
    elif c == 'key':
        m = shm()
        for k in a[1:]: tap(m, k)
        settle(m)
    elif c == 'hold':
        m = shm(); code = DIK[a[1]]
        push(m, (1, code), (6, int(a[2])), (2, code), (6, 60)); settle(m)
    elif c == 'mouse':
        m = shm(); push(m, (3, int(a[1]), int(a[2])), (6, 100)); settle(m)
    elif c == 'click':
        m = shm(); b = 1 if (len(a) > 3 and a[3] == 'right') else 0
        push(m, (3, int(a[1]), int(a[2])), (6, 120), (4, b), (6, 100), (5, b), (6, 250)); settle(m)
    elif c == 'type':
        h = game_hwnd()
        for ch in ' '.join(a[1:]):
            ctypes.windll.user32.PostMessageW(h, 0x102, ord(ch), 1)
            time.sleep(0.08)
    elif c == 'written':    # written TYPE NUM  (0 book, 1 note, 2 newspaper, 3 telegram, 6 plaque)
        m = shm(); push(m, (7, int(a[1]), int(a[2])), (6, 300)); settle(m)
    elif c == 'wait':
        m = shm(); push(m, (6, int(a[1]))); settle(m)
    elif c == 'gshot':
        gshot(sys.argv[2])
    elif c == 'frame':
        print(struct.unpack_from('<i', shm(), O_FRAME)[0])
    else: print(__doc__)

if __name__ == '__main__':
    main()
