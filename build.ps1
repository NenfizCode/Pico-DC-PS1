# Build PicoGamepadConverter on Windows using the VS Code Pico extension's toolchain.
# Usage:  .\build.ps1            (defaults to PICO_BOARD=pico)
#         .\build.ps1 -Board pico2
#         .\build.ps1 -Clean
param(
    [string]$Board = "pico",
    [switch]$Clean
)
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$ps   = Join-Path $env:USERPROFILE ".pico-sdk"

function Newest($dir) {
    $d = Get-ChildItem -Path $dir -Directory -ErrorAction SilentlyContinue |
         Sort-Object Name -Descending | Select-Object -First 1
    if (-not $d) { throw "Not found: $dir  (install the VS Code Raspberry Pi Pico extension)" }
    return $d.FullName
}

$toolchain = Newest (Join-Path $ps "toolchain")
$cmakeDir  = Get-ChildItem (Join-Path $ps "cmake") -Directory | Where-Object Name -like "v*" |
             Sort-Object Name -Descending | Select-Object -First 1
$ninjaDir  = Newest (Join-Path $ps "ninja")
$cmake     = Join-Path $cmakeDir.FullName "bin\cmake.exe"

# The Pico SDK builds pioasm and picotool as *host* tools, so a native compiler is required.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "MSVC C++ build tools not found. Install 'Desktop development with C++' in the Visual Studio Installer." }
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"

$build = Join-Path $root "build"
if ($Clean) {
    Remove-Item -Recurse -Force $build, (Join-Path $root "picotool") -ErrorAction SilentlyContinue
}
New-Item -ItemType Directory -Force -Path $build | Out-Null

$bat = Join-Path $env:TEMP "pgc_build.bat"
@"
@echo off
call "$vcvars" >nul || exit /b 1
set "PICO_SDK_PATH=$($root.Replace([char]92,[char]47))/pico-sdk"
set "PICO_TOOLCHAIN_PATH=$($toolchain.Replace([char]92,[char]47))"
set "PATH=$toolchain\bin;$ninjaDir;%PATH%"
cd /d "$build" || exit /b 1
"$cmake" ../src -G Ninja -DPICO_BOARD=$Board -DCMAKE_BUILD_TYPE=Release || exit /b 1
"$cmake" --build . || exit /b 1
"@ | Set-Content -Path $bat -Encoding ascii

& cmd.exe /c $bat
if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)" }

$uf2 = Join-Path $build "PicoGamepadConverter.uf2"
Write-Host ""
Write-Host "Build OK -> $uf2" -ForegroundColor Green
Write-Host "Flash: hold BOOTSEL, plug in the Pico, then copy that .uf2 onto the RPI-RP2 drive."
