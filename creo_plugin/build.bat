@echo off
setlocal enabledelayedexpansion
if not defined VSDIR set "VSDIR=D:\Program Files\Microsoft Visual Studio\18\Community"
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
if errorlevel 1 exit /b 1
if not defined PROTK set "PROTK=D:\PTC\Creo 4.0\M130\Common Files\protoolkit"
set "LIBS=%PROTK%\x86e_win64\obj"
cd /d "%~dp0"
if not exist bin mkdir bin
if not exist obj mkdir obj
del /q obj\*.obj 2>nul

for %%s in (main log motion_codec motion_receiver view_math view_controller ui_timer) do (
    echo [BUILD] %%s.cpp
    cl /nologo /std:c++17 /O2 /W3 /utf-8 /MT /EHsc /DNOMINMAX /DPRO_USE_VAR_ARGS /D_UNICODE /DUNICODE /D_CRT_SECURE_NO_WARNINGS /I"%PROTK%\includes" /I"src" /Fo"obj\%%s.obj" /c "src\%%s.cpp"
    if errorlevel 1 exit /b 1
)

cl /nologo /LD /Fe:bin\cardputer_creo_view.dll obj\main.obj obj\log.obj obj\motion_codec.obj obj\motion_receiver.obj obj\view_math.obj obj\view_controller.obj obj\ui_timer.obj /link /MAP:bin\cardputer_creo_view.map "%LIBS%\protk_dll_NU.lib" "%LIBS%\ucore.lib" "%LIBS%\udata.lib" ws2_32.lib shell32.lib kernel32.lib user32.lib gdi32.lib advapi32.lib ole32.lib oleaut32.lib mpr.lib netapi32.lib winspool.lib comdlg32.lib uuid.lib psapi.lib
if errorlevel 1 exit /b 1
echo [OK] %CD%\bin\cardputer_creo_view.dll
exit /b 0
