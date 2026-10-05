[CmdletBinding()]
param([string]$Version = '0.5.1')
$ErrorActionPreference = 'Stop'
if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw 'Version must be major.minor.patch.' }
$projectRoot = Split-Path -Parent $PSScriptRoot
$dll = Join-Path $projectRoot 'target\Release\only_load_latest_save_per_character.dll'
if (!(Test-Path -LiteralPath $dll)) { throw 'Build Release before packaging.' }
$stage = Join-Path $projectRoot "dist\Only Load Latest Save Per Character-$Version"
$pluginDirectory = Join-Path $stage 'NVSE\Plugins'
New-Item -ItemType Directory -Path $pluginDirectory -Force | Out-Null
Copy-Item -LiteralPath $dll -Destination $pluginDirectory -Force
# Package only the DLL and the author's optional public README.
$packagePaths = @((Join-Path $stage 'NVSE'))
$readme = Join-Path $projectRoot 'README.md'
if (Test-Path -LiteralPath $readme) {
    Copy-Item -LiteralPath $readme -Destination $stage -Force
    $packagePaths += Join-Path $stage 'README.md'
}
$zip = Join-Path $projectRoot "dist\Only Load Latest Save Per Character-$Version.zip"
Compress-Archive -Path $packagePaths -DestinationPath $zip -Force
Write-Output $zip
