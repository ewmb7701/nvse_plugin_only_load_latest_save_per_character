[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [switch]$Test
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'Install Visual Studio with Desktop development with C++.' }
$msbuild = Join-Path $installation 'MSBuild\Current\Bin\MSBuild.exe'
& $msbuild (Join-Path $projectRoot 'source\nvse_plugin_only_load_latest_save_per_character.vcxproj') /m /t:Build "/p:Configuration=$Configuration" /p:Platform=Win32 /v:minimal
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if ($Test) {
    & $msbuild (Join-Path $projectRoot 'source\nvse_plugin_only_load_latest_save_per_character.vcxproj') /m /t:Build "/p:Configuration=$Configuration" /p:Platform=Win32 /p:GuardTests=true /v:minimal
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $dll = Join-Path $projectRoot "target\$Configuration\only_load_latest_save_per_character.dll"
    $testExe = Join-Path $projectRoot "target\$Configuration\tests\only_load_latest_save_per_character_tests.exe"
    Push-Location (Split-Path -Parent $testExe)
    try {
        foreach ($mode in @('vanilla', 'stewie', 'conflict', 'sound-conflict', 'appearance-conflict', 'refresh-conflict')) {
            for ($attempt = 0; $attempt -lt 32; $attempt++) {
                & $testExe $dll $mode
                if ($LASTEXITCODE -ne 77) { break }
            }
            if ($LASTEXITCODE -ne 0) { throw "Integration test failed: $mode" }
        }
    } finally { Pop-Location }
}

