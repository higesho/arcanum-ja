# release/Arcanum_JP.zip = dinput.dll + 読んでください.txt + OFL.txt (UTF-8 file names)
import os, zipfile, shutil
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
REL = os.path.join(ROOT, 'release')
dll = os.path.join(ROOT, 'build', 'dinput.dll')   # 配布版（build.bat 引数なし）。試験版 build/test は入れない
if b'ARCJP_TESTIO' in open(dll, 'rb').read(): raise SystemExit('試験版の DLL です。src/build.bat を引数なしで作り直してください')
shutil.copy(dll, os.path.join(REL, 'dinput.dll'))
out = os.path.join(REL, 'Arcanum_JP.zip')
if os.path.exists(out): os.remove(out)
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
    for f in ('dinput.dll', '読んでください.txt', 'OFL.txt'):
        z.write(os.path.join(REL, f), f)
with zipfile.ZipFile(out) as z:
    for i in z.infolist(): print(i.filename, i.file_size, hex(i.flag_bits))
print(os.path.getsize(out), 'bytes')
