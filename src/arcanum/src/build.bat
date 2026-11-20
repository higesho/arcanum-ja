@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0"
if not exist ..\build mkdir ..\build
rc /nologo /fo ..\build\arcjp.res arcjp.rc || exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /LD /D_CRT_SECURE_NO_WARNINGS /Fo..\build\ proxy.c main.c font.c text.c testio.c ..\build\arcjp.res /Fe..\build\dinput.dll /link /DEF:dinput.def kernel32.lib user32.lib gdi32.lib || exit /b 1
