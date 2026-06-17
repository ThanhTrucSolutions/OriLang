@echo off
REM Build the native Ori VM (orivm.exe) with MSVC.
set "VCV="
for %%E in (Community Professional Enterprise BuildTools) do (
  if exist "%ProgramFiles%\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" set "VCV=%ProgramFiles%\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat"
)
if "%VCV%"=="" ( echo [ori] MSVC C++ tools not found & exit /b 1 )
call "%VCV%" >nul
cl /nologo /O2 /Fe:"%~dp0orivm.exe" "%~dp0orivm.c" >nul
