@echo off
rem 引数なし: 配布版 ..\build\dinput.dll ／ build.bat test: 試験版 ..\build\test\dinput.dll（配布しない）
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0"
set O=..\build\release
set OUT=..\build\dinput.dll
set DEF=
set EXTRA=
if "%1"=="test" (
  set O=..\build\test
  set OUT=..\build\test\dinput.dll
  set DEF=/DARCJP_TEST
  set EXTRA=testio.c
)
if not exist %O% mkdir %O%
rc /nologo /fo %O%\arcjp.res arcjp.rc || exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /LD /D_CRT_SECURE_NO_WARNINGS %DEF% /Fo%O%\ proxy.c main.c font.c text.c %EXTRA% %O%\arcjp.res /Fe%OUT% /link /DEF:dinput.def kernel32.lib user32.lib gdi32.lib || exit /b 1
