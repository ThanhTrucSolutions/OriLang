@echo off
REM Bootstrap the Ori toolchain on Windows.
REM Builds: core\orivm.exe, tools\ori.orb (Ori CLI), ori.exe (thin C bootstrap)
REM Requires: Visual Studio C++ tools (MSVC).
REM After this:  ori create myapp  &  ori run myapp
set "VCV="
for %%E in (Community Professional Enterprise BuildTools) do (
  if exist "%ProgramFiles%\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" set "VCV=%ProgramFiles%\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat"
)
if "%VCV%"=="" ( echo [ori] MSVC C++ tools not found. Install "Desktop development with C++". & exit /b 1 )
call "%VCV%" >nul

echo [ori] building core\orivm.exe ...
cl /nologo /O2 /Fe:"%~dp0core\orivm.exe" "%~dp0core\orivm.c" >nul || exit /b 1

echo [ori] compiling tools\ori.ori -> tools\ori.orb ...
set "ORI_HOME=%~dp0"
set "ORI_HOME=%ORI_HOME:~0,-1%"
set "ORI_WIN=1"
"%~dp0core\orivm.exe" "%~dp0tools\oric.orb" "%~dp0tools\ori.ori" "%~dp0tools\ori.orb" || exit /b 1

echo [ori] building ori.exe (thin C bootstrap) ...
cl /nologo /O2 /Fe:"%~dp0ori.exe" "%~dp0tools\ori.c" >nul || exit /b 1

echo [ori] building platforms\win\oriwin.exe (Win32 GUI host) ...
cl /nologo /O2 "%~dp0platforms\win\oriwin.c" /Fe:"%~dp0platforms\win\oriwin.exe" /link user32.lib gdi32.lib /SUBSYSTEM:WINDOWS >nul || exit /b 1

echo [ori] done.
echo        ori create myapp   ^&   ori run myapp
