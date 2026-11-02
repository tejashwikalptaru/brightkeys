<#
.SYNOPSIS
  Computes the next release version from git tags and commit messages.

.DESCRIPTION
  The version lives only in git tags (vX.Y.Z); nothing is ever committed back to the branch.
  The bump is chosen from the commits since the last tag (Conventional Commits style):
    - "BREAKING CHANGE:" in a body, or "type!:" in a subject  -> major
    - "feat:" / "feat(scope):"                                 -> minor
    - anything else                                            -> patch
  With no tags yet, the first release is 1.0.0.

  Writes version=X.Y.Z, tag=vX.Y.Z and release=true|false to $env:GITHUB_OUTPUT when running in
  GitHub Actions, and prints them either way. release=false means HEAD is already tagged.

.PARAMETER Bump
  auto (default), patch, minor or major.
#>
param([ValidateSet('auto', 'patch', 'minor', 'major')][string]$Bump = 'auto')

$ErrorActionPreference = 'Stop'

$last = git tag --list 'v*' --sort=-v:refname | Where-Object { $_ -match '^v\d+\.\d+\.\d+$' } | Select-Object -First 1
$release = $true

if (-not $last) {
    $version = '1.0.0'
} else {
    $range = "$last..HEAD"
    $count = [int](git rev-list --count $range)
    if ($count -eq 0) { $release = $false }

    if ($Bump -eq 'auto') {
        $log = (git log $range --format='%s%n%b') -join "`n"
        if ($log -match '(?m)^BREAKING[ -]CHANGE:' -or $log -match '(?m)^\w+(\([^)]*\))?!:') { $Bump = 'major' }
        elseif ($log -match '(?m)^feat(\([^)]*\))?:') { $Bump = 'minor' }
        else { $Bump = 'patch' }
    }

    $v = [version]$last.TrimStart('v')
    $version = switch ($Bump) {
        'major' { "$($v.Major + 1).0.0" }
        'minor' { "$($v.Major).$($v.Minor + 1).0" }
        default { "$($v.Major).$($v.Minor).$($v.Build + 1)" }
    }
}

$out = @("version=$version", "tag=v$version", "release=$($release.ToString().ToLower())")
$out | ForEach-Object { Write-Host $_ }
if ($env:GITHUB_OUTPUT) { $out | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8 }
