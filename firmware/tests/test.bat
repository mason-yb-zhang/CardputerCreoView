@echo off
setlocal
if not defined VSDIR set "VSDIR=D:\Program Files\Microsoft Visual Studio\18\Community"
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
if errorlevel 1 exit /b 1
cd /d "%~dp0"
if not exist bin mkdir bin
cl /nologo /std:c++17 /O2 /W3 /EHsc /I"..\src" /Fo"bin\\" tests.cpp /Fe"bin\firmware_protocol_tests.exe"
if errorlevel 1 exit /b 1
"bin\firmware_protocol_tests.exe"
exit /b %errorlevel%
