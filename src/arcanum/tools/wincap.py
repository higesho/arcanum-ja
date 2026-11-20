# python wincap.py OUT.png [TITLE] : capture the window whose title is exactly TITLE (default "Arcanum")
# with PrintWindow, so other windows on top do not matter.
import sys, ctypes, ctypes.wintypes as W
from PIL import Image
u = ctypes.windll.user32; g = ctypes.windll.gdi32
title = sys.argv[2] if len(sys.argv) > 2 else 'Arcanum'
found = []
@ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
def cb(h, l):
    b = ctypes.create_unicode_buffer(256); u.GetWindowTextW(h, b, 256)
    if b.value == title and u.IsWindowVisible(h): found.append(h)
    return True
u.EnumWindows(cb, 0)
if not found: raise SystemExit('no window titled %r' % title)
h = found[0]
r = W.RECT(); u.GetClientRect(h, ctypes.byref(r))
w, hh = r.right, r.bottom
hdc = u.GetDC(h); mdc = g.CreateCompatibleDC(hdc); bmp = g.CreateCompatibleBitmap(hdc, w, hh)
g.SelectObject(mdc, bmp)
u.PrintWindow(h, mdc, 3)
buf = ctypes.create_string_buffer(w * hh * 4)
class BIH(ctypes.Structure):
    _fields_ = [('biSize', W.DWORD), ('biWidth', W.LONG), ('biHeight', W.LONG), ('biPlanes', W.WORD), ('biBitCount', W.WORD),
                ('biCompression', W.DWORD), ('biSizeImage', W.DWORD), ('a', W.LONG), ('b', W.LONG), ('c', W.DWORD), ('d', W.DWORD)]
bi = BIH(ctypes.sizeof(BIH), w, -hh, 1, 32, 0, 0, 0, 0, 0, 0)
g.GetDIBits(mdc, bmp, 0, hh, buf, ctypes.byref(bi), 0)
Image.frombuffer('RGB', (w, hh), buf, 'raw', 'BGRX', 0, 1).save(sys.argv[1])
print(sys.argv[1], w, hh)
