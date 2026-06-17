@echo off
REM Build a signed Android APK that runs an Ori program on the native C VM.
REM Usage:  build-apk.cmd <app.orb> <out.apk>
REM Tools: Android SDK build-tools + NDK, a JDK (JAVA_HOME), debug keystore.
setlocal enabledelayedexpansion
set "AORB=%~1"
set "OUTAPK=%~2"
set "AND=%~dp0"
set "SDK=%LOCALAPPDATA%\Android\Sdk"
set "BT=%SDK%\build-tools\35.0.0"
set "AJAR=%SDK%\platforms\android-34\android.jar"
set "KS=%USERPROFILE%\.android\debug.keystore"
set "NDK="
for /d %%d in ("%SDK%\ndk\*") do set "NDK=%%d\toolchains\llvm\prebuilt\windows-x86_64\bin"
set "JB=%JAVA_HOME%\bin"
set "B=%AND%build"

rmdir /s /q "%B%" 2>nul
mkdir "%B%\assets" "%B%\lib\x86_64" "%B%\classes" 2>nul
copy /y "%AORB%" "%B%\assets\app.orb" >nul

echo [apk] native libori.so (x86_64)
call "%NDK%\x86_64-linux-android24-clang.cmd" -shared -fPIC -O2 "%AND%oriandroid.c" -o "%B%\lib\x86_64\libori.so" -lm || exit /b 1

echo [apk] javac
"%JB%\javac" -source 8 -target 8 -classpath "%AJAR%" -d "%B%\classes" "%AND%java\ori\app\OriBridge.java" "%AND%java\ori\app\MainActivity.java" || exit /b 1

echo [apk] dex
set "CLS="
for %%f in ("%B%\classes\ori\app\*.class") do set "CLS=!CLS! "%%f""
call "%BT%\d8.bat" --min-api 24 --output "%B%" !CLS! || exit /b 1

echo [apk] aapt2 link
"%BT%\aapt2.exe" link --manifest "%AND%AndroidManifest.xml" -I "%AJAR%" -o "%B%\base.apk" --min-sdk-version 24 --target-sdk-version 34 || exit /b 1

echo [apk] package (dex + lib + assets)
pushd "%B%"
"%JB%\jar" uf base.apk classes.dex
"%JB%\jar" uf base.apk lib/x86_64/libori.so
"%JB%\jar" uf base.apk assets/app.orb
popd

echo [apk] zipalign + sign
"%BT%\zipalign.exe" -p -f 4 "%B%\base.apk" "%OUTAPK%" || exit /b 1
call "%BT%\apksigner.bat" sign --ks "%KS%" --ks-pass pass:android --ks-key-alias androiddebugkey --key-pass pass:android "%OUTAPK%" || exit /b 1
echo [apk] done: %OUTAPK%
