# ============================================================================
#  ori — the Ori toolchain CLI (Flutter-style).
#
#    ori create <name>   scaffold a new project (just  ori/  +  meta)
#    ori run [path]      compile the project's Ori and run it on the C VM
#    ori build [path]    compile to build/app.orb
#    ori doctor          install/build everything the toolchain needs
#    ori version
#
#  The core VM is C (core/orivm.c) and the compiler is Ori (tooling/oric.ori,
#  shipped as tooling/oric.orx). `ori` ties them together and auto-installs the
#  native VM on first use.
# ============================================================================
param(
    [Parameter(Position = 0)] [string] $Command = "help",
    [Parameter(Position = 1)] [string] $Path = "."
)
$ErrorActionPreference = "Stop"
$ORI_HOME = $PSScriptRoot
$VM       = Join-Path $ORI_HOME "core\orivm.exe"
$COMPILER = Join-Path $ORI_HOME "tooling\oric.orx"   # bootstrap compiler image

function Info($m){ Write-Host $m -ForegroundColor Cyan }
function Ok($m){ Write-Host $m -ForegroundColor Green }
function Err($m){ Write-Host $m -ForegroundColor Red }

function Build-VM {
    Info "ori: building the native VM core (core/orivm.c)..."
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "Need a C compiler. Install Visual Studio (C++ tools) or run from a dev shell." }
    $vsPath = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) { throw "No MSVC C++ toolset found. Install the 'Desktop development with C++' workload." }
    $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
    $core = Join-Path $ORI_HOME "core"
    cmd /c "call `"$vcvars`" >nul 2>&1 && cd /d `"$core`" && cl /nologo /O2 /Fe:orivm.exe orivm.c >nul"
    if (-not (Test-Path $VM)) { throw "VM build failed." }
    Ok "ori: built core/orivm.exe"
}

function Doctor {
    Info "ori doctor"
    if (Test-Path $VM) { Ok "  [ok] native VM      core/orivm.exe" } else { Build-VM }
    if (Test-Path $COMPILER) { Ok "  [ok] Ori compiler  tooling/oric.orx" }
    else { throw "Missing tooling/oric.orx (the bootstrap compiler image)." }
    Ok "  Toolchain ready."
}

function Read-Meta($projDir) {
    $metaPath = Join-Path $projDir "meta"
    if (-not (Test-Path $metaPath)) { throw "no 'meta' file found in $projDir (is this an Ori project?)" }
    $meta = @{ name = "app"; entry = "ori/main.ori"; version = "0.0.0" }
    foreach ($line in Get-Content $metaPath) {
        $t = $line.Trim()
        if ($t -eq "" -or $t.StartsWith("#")) { continue }
        $i = $t.IndexOf(":")
        if ($i -lt 0) { continue }
        $k = $t.Substring(0, $i).Trim()
        $v = $t.Substring($i + 1).Trim()
        if ($v -ne "") { $meta[$k] = $v }
    }
    return $meta
}

function Compile-Project($projDir) {
    Doctor
    $proj = (Resolve-Path $projDir).Path
    $meta = Read-Meta $proj
    $entry = Join-Path $proj $meta.entry
    if (-not (Test-Path $entry)) { throw "entry not found: $($meta.entry)" }
    $build = Join-Path $proj "build"
    New-Item -ItemType Directory -Force -Path $build | Out-Null
    $orb = Join-Path $build "app.orb"
    Info "ori: compiling $($meta.name) ($($meta.entry)) with the Ori compiler on the C VM..."
    & $VM $COMPILER $entry $orb | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "compile failed." }
    return $orb
}

switch ($Command) {
    "doctor" { Doctor }
    "version" { Write-Host "Ori toolchain 0.3  (C core VM + Ori-written compiler)" }
    "create" {
        $name = $Path
        if ($name -eq ".") { throw "usage: ori create <name>" }
        New-Item -ItemType Directory -Force -Path (Join-Path $name "ori") | Out-Null
        $main = @'
// main.ori — your Ori program. Run it with:  ori run
fold fib(n) {
    when n < 2 { give n }
    give fib(n - 1) + fib(n - 2)
}

hold i = 0
loop i < 10 {
    say("fib(" + str(i) + ") = " + str(fib(i)))
    i = i + 1
}
say("Hello from Ori!")
'@
        Set-Content -Path (Join-Path $name "ori\main.ori") -Value $main -Encoding UTF8
        $meta = @"
name: $name
version: 1.0.0
entry: ori/main.ori

# Everything this project needs is declared here.
# `ori run` auto-installs the toolchain and builds the native VM on first use.
dependencies:
"@
        Set-Content -Path (Join-Path $name "meta") -Value $meta -Encoding UTF8
        Ok "Created project '$name'  (ori/ + meta)"
        Write-Host "  cd $name; ori run"
    }
    "build" {
        $orb = Compile-Project $Path
        Ok "ori: built $orb"
    }
    "run" {
        $orb = Compile-Project $Path
        Info "ori: running on the C VM"
        Write-Host "----------------------------------------"
        & $VM $orb
    }
    default {
        Write-Host @"
Ori toolchain (Flutter-style)
  ori create <name>   scaffold a new project (just  ori/  +  meta)
  ori run [path]      compile and run on the native C VM
  ori build [path]    compile to build/app.orb
  ori doctor          install/build what the toolchain needs
  ori version
"@
    }
}
