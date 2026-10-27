@echo off
setlocal
if not defined VSDIR set "VSDIR=D:\Program Files\Microsoft Visual Studio\18\Community"
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
if errorlevel 1 exit /b 1
cd /d "%~dp0"
if not exist bin mkdir bin
cl /nologo /std:c++17 /O2 /W3 /EHsc /DNOMINMAX /I"..\src" /Fo"bin\\" tests.cpp "..\src\view_math.cpp" "..\src\motion_codec.cpp" /Fe"bin\ccv_tests.exe"
if errorlevel 1 exit /b 1
"bin\ccv_tests.exe"
exit /b %errorlevel%
