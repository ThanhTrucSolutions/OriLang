# ============================================================================
#  ori — the Ori toolchain CLI (Flutter-style). Native: C VM + Ori compiler.
#
#    ori create <name> [-Platform windows|web|android]
#    ori run    [path] [-Hot]      compile + run (–Hot = hot reload)
#    ori dev    [path]             = run -Hot
#    ori build  [path] [-Release]  -> build/app.orb  (or encrypted build/app.orx)
#    ori doctor                    build/install what the toolchain needs
#    ori version
#
#  Core VM: core/orivm.c (C). Compiler: tooling/oric.ori, shipped self-hosted
#  as tooling/oric.orb. Web: orivm compiled to WebAssembly (tooling/web).
# ============================================================================
param(
    [Parameter(Position = 0)] [string] $Command = "help",
    [Parameter(Position = 1)] [string] $Arg = "",
    [string] $Platform = "",
    [switch] $Hot,
    [switch] $Release
)
$ErrorActionPreference = "Stop"
$ORI_HOME = $PSScriptRoot
$VM       = Join-Path $ORI_HOME "core\orivm.exe"
$COMPILER = Join-Path $ORI_HOME "tooling\oric.orb"
$WEBRT    = Join-Path $ORI_HOME "tooling\web"

function Info($m){ Write-Host $m -ForegroundColor Cyan }
function Ok($m){ Write-Host $m -ForegroundColor Green }
function Warn($m){ Write-Host $m -ForegroundColor Yellow }

function Build-VM {
    Info "ori: building the native VM core (core/orivm.c)..."
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "Need a C compiler (Visual Studio C++ tools)." }
    $vsPath = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) { throw "No MSVC C++ toolset found ('Desktop development with C++')." }
    $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
    $core = Join-Path $ORI_HOME "core"
    cmd /c "call `"$vcvars`" >nul 2>&1 && cd /d `"$core`" && cl /nologo /O2 /Fe:orivm.exe orivm.c >nul"
    if (-not (Test-Path $VM)) { throw "VM build failed." }
    Ok "ori: built core/orivm.exe"
}

function Doctor {
    Info "ori doctor"
    if (Test-Path $VM) { Ok "  [ok] native VM      core/orivm.exe" } else { Build-VM }
    if (Test-Path $COMPILER) { Ok "  [ok] Ori compiler  tooling/oric.orb (self-hosted)" }
    else { throw "Missing tooling/oric.orb." }
    if (Test-Path (Join-Path $WEBRT "orivm.wasm")) { Ok "  [ok] web runtime    tooling/web/orivm.wasm" }
    else { Warn "  [..] web runtime    not built (only needed for -Platform web)" }
    Ok "  Toolchain ready."
}

function Read-Meta($projDir) {
    $metaPath = Join-Path $projDir "meta"
    if (-not (Test-Path $metaPath)) { throw "no 'meta' file in $projDir (not an Ori project?)" }
    $meta = @{ name = "app"; entry = "ori/main.ori"; version = "0.0.0"; platform = "windows" }
    foreach ($line in Get-Content $metaPath) {
        $t = $line.Trim()
        if ($t -eq "" -or $t.StartsWith("#")) { continue }
        $i = $t.IndexOf(":"); if ($i -lt 0) { continue }
        $k = $t.Substring(0,$i).Trim(); $v = $t.Substring($i+1).Trim()
        if ($v -ne "") { $meta[$k] = $v }
    }
    return $meta
}

function Compile-Orb($projDir, $meta) {
    $entry = Join-Path $projDir $meta.entry
    if (-not (Test-Path $entry)) { throw "entry not found: $($meta.entry)" }
    $build = Join-Path $projDir "build"; New-Item -ItemType Directory -Force -Path $build | Out-Null
    $orb = Join-Path $build "app.orb"
    & $VM $COMPILER $entry $orb | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "compile failed." }
    return $orb
}

function Resolve-Proj($p) { if ($p -eq "") { $p = "." }; return (Resolve-Path $p).Path }

function Build-Android($proj, $meta) {
    $orb = Compile-Orb $proj $meta
    $outDir = Join-Path $proj "build\android"; New-Item -ItemType Directory -Force -Path $outDir | Out-Null
    Copy-Item $orb (Join-Path $outDir "app.orb") -Force
    $ndkRoot = Join-Path $env:LOCALAPPDATA "Android\Sdk\ndk"
    $cc = $null
    if (Test-Path $ndkRoot) {
        $cc = Get-ChildItem $ndkRoot -Recurse -Filter "aarch64-linux-android21-clang.cmd" -ErrorAction SilentlyContinue | Select-Object -First 1
    }
    if ($cc) {
        Info "ori: cross-compiling the VM for Android (arm64) via NDK..."
        & $cc.FullName (Join-Path $ORI_HOME "core\orivm.c") -O2 -lm -o (Join-Path $outDir "orivm-arm64")
        Ok "ori: android build -> $outDir  (orivm-arm64 + app.orb)"
        Write-Host "  Run on a device:  adb push build/android/* /data/local/tmp/ && adb shell 'cd /data/local/tmp && ./orivm-arm64 app.orb'"
    } else {
        Warn "ori: Android NDK not found; wrote app.orb only. Install the NDK to cross-compile the VM."
    }
}

function Run-Native($proj, $meta) {
    $orb = Compile-Orb $proj $meta
    Info "ori: running on the native C VM"
    Write-Host "----------------------------------------"
    & $VM $orb
}

function Run-Hot($proj, $meta) {
    $entry = Join-Path $proj $meta.entry
    Info "ori dev: hot reload watching $($meta.entry)  (Ctrl+C to stop)"
    $last = ""
    while ($true) {
        $stamp = (Get-Item $entry).LastWriteTimeUtc.Ticks.ToString()
        if ($stamp -ne $last) {
            $last = $stamp
            Write-Host "`n========== ori dev: reload @ $(Get-Date -Format HH:mm:ss) =========="
            try {
                $orb = Compile-Orb $proj $meta
                Write-Host "----------------------------------------"
                & $VM $orb
            } catch { Warn $_.Exception.Message }
        }
        Start-Sleep -Milliseconds 400
    }
}

function Run-Web($proj, $meta) {
    if (-not (Test-Path (Join-Path $WEBRT "orivm.wasm"))) { throw "web runtime missing. See docs/TOOLCHAIN.md to build it (emcc)." }
    $node = (Get-Command node -ErrorAction SilentlyContinue).Source
    if (-not $node) { throw "Node.js is required to serve the web app." }
    $webBuild = Join-Path $proj "build\web"; New-Item -ItemType Directory -Force -Path $webBuild | Out-Null
    Copy-Item (Join-Path $WEBRT "orivm.js") $webBuild -Force
    Copy-Item (Join-Path $WEBRT "orivm.wasm") $webBuild -Force
    Copy-Item (Join-Path $ORI_HOME "tooling\templates\web\index.html") $webBuild -Force
    $server = Join-Path $ORI_HOME "tooling\templates\web\server.mjs"
    Info "ori: serving web app with hot reload (edit ori/main.ori, the page reloads)"
    & $node $server $webBuild $proj $VM $COMPILER 5151
}

switch ($Command) {
    "doctor" { Doctor }
    "version" { Write-Host "Ori toolchain 0.4  (C core VM + self-hosted Ori compiler; native, no .NET)" }
    "create" {
        if ($Arg -eq "") { throw "usage: ori create <name> [-Platform windows|web|android]" }
        $name = $Arg
        if ($Platform -eq "") { $Platform = "windows" }
        New-Item -ItemType Directory -Force -Path (Join-Path $name "ori") | Out-Null
        $main = @'
// main.ori — your Ori program. Run it with:  ori run    (or  ori dev  for hot reload)
fold fib(n) {
    when n < 2 { give n }
    give fib(n - 1) + fib(n - 2)
}

hold i = 0
loop i < 12 {
    say("fib(" + str(i) + ") = " + str(fib(i)))
    i = i + 1
}
say("Hello from Ori!")
'@
        Set-Content -Path (Join-Path $name "ori\main.ori") -Value $main -Encoding UTF8
        $meta = "name: $name`nversion: 1.0.0`nentry: ori/main.ori`nplatform: $Platform`n`n# Declare dependencies here; `ori run` auto-installs the toolchain.`ndependencies:`n"
        Set-Content -Path (Join-Path $name "meta") -Value $meta -Encoding UTF8
        Ok "Created project '$name' (platform: $Platform)  -> ori/ + meta"
        Write-Host "  cd $name; ori run        # or: ori dev   (hot reload)"
    }
    "build" {
        Doctor | Out-Null
        $proj = Resolve-Proj $Arg
        $meta = Read-Meta $proj
        $plat = if ($Platform -ne "") { $Platform } else { $meta.platform }
        if ($plat -eq "android") {
            Build-Android $proj $meta
        } elseif ($Release) {
            $orb = Compile-Orb $proj $meta
            $orx = Join-Path $proj "build\app.orx"
            & $VM pack $orb $orx | Out-Host
            Ok "ori: release build -> $orx (encrypted, per-build opcode map)"
        } else {
            $orb = Compile-Orb $proj $meta
            Ok "ori: built $orb"
        }
    }
    "run" {
        Doctor | Out-Null
        $proj = Resolve-Proj $Arg
        $meta = Read-Meta $proj
        if ($meta.platform -eq "web") { Run-Web $proj $meta }
        elseif ($Hot) { Run-Hot $proj $meta }
        else { Run-Native $proj $meta }
    }
    "dev" {
        Doctor | Out-Null
        $proj = Resolve-Proj $Arg
        $meta = Read-Meta $proj
        if ($meta.platform -eq "web") { Run-Web $proj $meta } else { Run-Hot $proj $meta }
    }
    default {
        Write-Host @"
Ori toolchain (Flutter-style) — native C VM + self-hosted Ori compiler
  ori create <name> [-Platform windows|web|android]
  ori run [path] [-Hot]      compile and run (-Hot = hot reload)
  ori dev [path]             hot reload (web projects: live dev server)
  ori build [path] [-Release]  -> build/app.orb  (-Release: encrypted .orx)
  ori doctor / ori version
"@
    }
}
