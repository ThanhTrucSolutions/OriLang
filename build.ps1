# Build the Ori native toolchain.
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

if ($IsWindows -or $env:OS -eq "Windows_NT") {
    & "$root\build.cmd"
} else {
    & sh "$root/build.sh"
}
