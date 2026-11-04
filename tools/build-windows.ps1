# Local Windows build of the desktop client (MSI + portable zip), same as CI.
#   powershell -ExecutionPolicy Bypass -File tools\build-windows.ps1 [-Clean]
# Needs: Visual Studio 2022 Build Tools (C++ workload), Qt 6.12 msvc2022_64 in
# C:\Qt (aqtinstall), 7-Zip, optional jom in C:\Qt\Tools\jom. See docs/building.md.
param(
    [string]$QtDir = "C:\Qt\6.12.0\msvc2022_64",
    [switch]$Clean
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$desktop = Join-Path $repo 'client-desktop'
$out = Join-Path $repo 'out\windows'

if (-not (Test-Path "$QtDir\bin\qmake.exe")) { throw "Qt not found at $QtDir (see docs/building.md)" }
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere) -or -not (& $vswhere -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)) {
    throw "Visual Studio C++ build tools not found"
}

# Tools on PATH: Qt, 7-Zip, jom
$env:Path = "$QtDir\bin;C:\Program Files\7-Zip;C:\Qt\Tools\jom;" + $env:Path

# Prebuilt FFmpeg/SDL/OpenSSL/... from moonlight-qt-deps, fetched once
if (-not (Test-Path "$desktop\libs\windows\lib\x64")) {
    Write-Host "Fetching prebuilt dependencies..." -ForegroundColor Cyan
    & powershell -ExecutionPolicy Bypass -File "$desktop\setup-deps.ps1"
    if ($LASTEXITCODE -ne 0) { throw "setup-deps.ps1 failed" }
}

if ($Clean) { Remove-Item -Recurse -Force "$desktop\build" -ErrorAction SilentlyContinue }

$version = (Get-Content "$desktop\app\version.txt" -Raw).Trim()
$sha = (git -C $repo rev-parse --short=7 HEAD).Trim()
$env:CI_VERSION = "$version-dev.$sha"   # only names the output files

Push-Location $desktop
try {
    cmd /c "scripts\build-arch.bat Release x64"
    if ($LASTEXITCODE -ne 0) { throw "build-arch.bat failed ($LASTEXITCODE)" }
} finally { Pop-Location }

New-Item -ItemType Directory -Force $out | Out-Null
$msi = Get-ChildItem "$desktop\build" -Recurse -Include *.msi | Sort-Object LastWriteTime -Descending | Select-Object -First 1
$zip = Get-ChildItem "$desktop\build" -Recurse -Filter "MoonlightPortable-x64-$env:CI_VERSION.zip" | Select-Object -First 1
Copy-Item $msi.FullName "$out\CloudDeckClient-$env:CI_VERSION-windows-x64.msi" -Force
Copy-Item $zip.FullName "$out\CloudDeckClient-$env:CI_VERSION-windows-x64-portable.zip" -Force
Get-ChildItem $out | Format-Table Name, Length
