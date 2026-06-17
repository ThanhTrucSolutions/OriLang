# Build the whole Ori toolchain (requires .NET SDK 9+)
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

Write-Host "==> Building OriLang (core)..." -ForegroundColor Cyan
dotnet build "$root\src\OriLang\OriLang.csproj" -c Release -v quiet

Write-Host "==> Building oric (CLI)..." -ForegroundColor Cyan
dotnet build "$root\src\oric\oric.csproj" -c Release -v quiet

Write-Host "==> Building OriDemo (Windows GUI)..." -ForegroundColor Cyan
dotnet build "$root\src\OriDemo\OriDemo.csproj" -c Release -v quiet

Write-Host ""
Write-Host "Build complete!" -ForegroundColor Green
Write-Host "  CLI : src\oric\bin\Release\net9.0\oric.exe"
Write-Host "  GUI : src\OriDemo\bin\Release\net9.0-windows\OriDemo.exe"
