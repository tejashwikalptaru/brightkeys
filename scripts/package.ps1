<#
.SYNOPSIS
  Packages build\BrightKeys.exe into dist\: a portable zip, the installer, and SHA256SUMS.txt.

.EXAMPLE
  .\build.bat 1.2.3
  .\scripts\package.ps1 -Version 1.2.3
#>
param(
    [Parameter(Mandatory)][ValidatePattern('^\d+\.\d+\.\d+$')][string]$Version,
    [switch]$SkipInstaller
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$dist = Join-Path $root 'dist'
$exe = Join-Path $root 'build\BrightKeys.exe'

if (-not (Test-Path $exe)) { throw "Missing $exe - run build.bat first." }
if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory -Path $dist | Out-Null

# Portable zip
$stage = Join-Path $dist 'stage'
New-Item -ItemType Directory -Path $stage | Out-Null
Copy-Item $exe, (Join-Path $root 'LICENSE'), (Join-Path $root 'README.md') $stage
Compress-Archive -Path "$stage\*" -DestinationPath (Join-Path $dist "BrightKeys-$Version-portable-x64.zip")
Remove-Item $stage -Recurse -Force

# Installer (Inno Setup 6)
if (-not $SkipInstaller) {
    $iscc = @(
        (Get-Command iscc.exe -ErrorAction SilentlyContinue).Source,
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
        "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
    ) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
    if (-not $iscc) { throw 'Inno Setup 6 (ISCC.exe) not found. Install it or pass -SkipInstaller.' }
    & $iscc /Qp "/DAppVersion=$Version" (Join-Path $root 'installer\brightkeys.iss')
    if ($LASTEXITCODE) { throw "ISCC failed with exit code $LASTEXITCODE" }
}

# Checksums
Get-ChildItem $dist -File | ForEach-Object {
    '{0}  {1}' -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower(), $_.Name
} | Set-Content (Join-Path $dist 'SHA256SUMS.txt') -Encoding ascii

Get-ChildItem $dist | Format-Table Name, Length -AutoSize
