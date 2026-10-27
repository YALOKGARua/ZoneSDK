[CmdletBinding()]
param(
    [string]$Out
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'layout.ps1')
$sdk = Get-FakehostRoot -ScriptRoot $PSScriptRoot
Import-Module (Join-Path $sdk 'build\zsdk.psm1') -Force
Reset-ZsdkLog ('fake host build ' + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))
Import-ZsdkToolchain
$work = Get-FakehostWorkRoot -ScriptRoot $PSScriptRoot -Out $Out
$bin = Join-Path $work 'bin'
$obj = Join-Path $work 'obj'
New-Item -ItemType Directory -Force -Path $bin, $obj | Out-Null
$exe = Join-Path $bin 'Stalker2-Win64-Shipping.exe'
if (Test-Path -LiteralPath ($exe + '.stamp')) { Remove-Item -LiteralPath ($exe + '.stamp') -Force }
$line = 'cl /nologo /std:c++20 /EHsc /W4 /permissive- /utf-8 /Zc:__cplusplus /MT /O2 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /external:W0 /external:I"{0}\extern\imgui" /external:I"{0}\extern\reshade" "{1}\fakehost.cpp" /Fo"{2}\\" /Fe"{3}" /link user32.lib' -f $sdk, $PSScriptRoot, $obj, $exe
$code = Invoke-ZsdkTool $line 'fake host'
Save-ZsdkLog (Join-Path $work 'last-build.log')
if ($code -ne 0) { throw ('fake host build failed: ' + $code) }
[System.IO.File]::WriteAllText(($exe + '.stamp'), (Get-FakehostStamp -ScriptRoot $PSScriptRoot), (New-Object System.Text.UTF8Encoding($false)))
Write-Host ('built {0}, warnings {1}' -f $exe, (Get-ZsdkWarnings))
Write-Output $exe
