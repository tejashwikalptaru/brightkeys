<#
.SYNOPSIS
  Scans a folder with Microsoft Defender (fresh signatures) and fails if anything is detected.

.DESCRIPTION
  Used by CI before publishing a release. Also works locally if Defender is your active antivirus
  (when another antivirus is installed, Windows turns Defender off and this script reports that).
  Writes a one-line summary to $env:GITHUB_OUTPUT as `summary=...` in GitHub Actions.

  MpCmdRun's exit code is ambiguous (2 means both "threat found" and "scan failed"), so the result is
  taken from its output text instead.

.EXAMPLE
  .\scripts\scan-defender.ps1 -Path dist
#>
param([Parameter(Mandatory)][string]$Path)

$ErrorActionPreference = 'Stop'
$mp = Join-Path $env:ProgramFiles 'Windows Defender\MpCmdRun.exe'
if (-not (Test-Path $mp)) { throw "Microsoft Defender command-line scanner not found at $mp" }
$target = (Resolve-Path $Path).Path

$status = Get-MpComputerStatus -ErrorAction SilentlyContinue
if ($status -and -not $status.AMServiceEnabled) {
    throw "Microsoft Defender is not running on this machine (mode: $($status.AMRunningMode)); another antivirus is probably active."
}

Write-Host 'Updating Defender signatures...'
& $mp -SignatureUpdate | Out-Host
if ($LASTEXITCODE) { Write-Warning "Signature update returned $LASTEXITCODE; scanning with current signatures." }

Write-Host "Scanning $target ..."
$output = (& $mp -Scan -ScanType 3 -File $target -DisableRemediation 2>&1 | Out-String)
Write-Host $output

if ($output -match 'found no threats') {
    $status = Get-MpComputerStatus -ErrorAction SilentlyContinue
    $summary = 'No threats found by Microsoft Defender'
    if ($status) { $summary += " (engine $($status.AMEngineVersion), signatures $($status.AntivirusSignatureVersion))" }
    Write-Host $summary
    if ($env:GITHUB_OUTPUT) { "summary=$summary" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8 }
} elseif ($output -match 'found \d+ threat') {
    throw "Microsoft Defender detected a threat in $target. Release blocked."
} else {
    throw "Microsoft Defender scan did not complete (MpCmdRun exit code $LASTEXITCODE)."
}
