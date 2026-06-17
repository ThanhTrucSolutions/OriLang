@echo off
REM Bootstrap the Ori toolchain on Windows: builds the native VM and the CLI.
REM Requires Visual Studio C++ tools (MSVC). After this, use:  ori run <project>
set "VCV="
for %%E in (Community Professional Enterprise BuildTools) do (
  if exist "%ProgramFiles%\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" set "VCV=%ProgramFiles%\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat"
)
if "%VCV%"=="" ( echo [ori] MSVC C++ tools not found. Install "Desktop development with C++". & exit /b 1 )
call "%VCV%" >nul
echo [ori] building core\orivm.exe ...
cl /nologo /O2 /Fe:"%~dp0core\orivm.exe" "%~dp0core\orivm.c" >nul || exit /b 1
echo [ori] building ori.exe ...
cl /nologo /O2 /Fe:"%~dp0ori.exe" "%~dp0tooling\ori.c" >nul || exit /b 1
echo [ori] building platforms\win\oriwin.exe (GUI host) ...
cl /nologo /O2 "%~dp0platforms\win\oriwin.c" /Fe:"%~dp0platforms\win\oriwin.exe" /link user32.lib gdi32.lib /SUBSYSTEM:WINDOWS >nul || exit /b 1
echo [ori] done. Try:  ori create myapp  ^&  ori run myapp
