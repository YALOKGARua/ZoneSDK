[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Config = 'Release',
    [switch]$Tests,
    [switch]$Deterministic,
    [switch]$Verify,
    [int]$WaitMinutes = -1,
    [string]$Exe,
    [switch]$LiveExe,
    [switch]$RequireExe,
    [switch]$KeepSymbols,
    [switch]$Where,
    [switch]$Help
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if ($Help) {
    Write-Host 'usage: build.cmd [-Tests] [-Verify] [-Config Release|Debug] [-Exe <game exe> | -LiveExe] [-RequireExe] [-KeepSymbols] [-Deterministic] [-WaitMinutes <n>] [-Where]'
    Write-Host '  -Tests         run the add-on tests and the ZoneSDK generic suites after the build; test.cmd does the same'
    Write-Host '  -Verify        check the finished binary: a fake host run, the exports, the version resource and a scan for build paths'
    Write-Host '  -Config Debug  a Debug build into bin\Debug, which is never shipped'
    Write-Host '  -Exe <file>    let the guards suite compare the guard sets with this game exe, read in place; -LiveExe finds the installed game through Steam'
    Write-Host '  -RequireExe    with -Tests, fail when no exe is given or a declared guard set is not listed in App.sets'
    Write-Host '  -KeepSymbols   a tested Release build, kept with its pdb, lock, log and a source zip in dist\<version>\symbols'
    Write-Host '  -Where         print where the ZoneSDK release is found, without building'
    return
}
for ($walk = $root; $walk; $walk = Split-Path $walk -Parent) {
    if (Test-Path -LiteralPath (Join-Path $walk 'MANIFEST.sha256')) { throw ('this add-on folder lies inside the ZoneSDK release ' + $walk + ', and a release is never built in place: create your own add-on with new-addon.cmd and build that one') }
}
$lockFile = Join-Path $PSScriptRoot 'zsdk.lock'
$lock = @{}
foreach ($line in Get-Content -LiteralPath $lockFile -Encoding UTF8) { $pair = $line -split '=', 2; if ($pair.Count -eq 2) { $lock[$pair[0].Trim()] = $pair[1].Trim() } }
$version = $lock['version']
if (-not $version) { throw ('build\zsdk.lock names no ZoneSDK version: ' + $lockFile) }
if (-not $lock['manifest']) { throw 'build\zsdk.lock pins no release manifest: create the add-on with new-addon.cmd, which writes the lock, instead of copying a folder by hand' }
if ($lock['manifest'] -notmatch '^[0-9A-Fa-f]{64}$') { throw ('build\zsdk.lock: the manifest is not a sha256: ' + $lock['manifest']) }
$wanted = $lock['manifest'].ToUpperInvariant()
$places = New-Object System.Collections.Generic.List[object]
$tried = New-Object System.Collections.Generic.List[string]
function Add-ZsdkPlace([string]$folder, [string]$via) {
    $full = [System.IO.Path]::GetFullPath($folder).TrimEnd('\')
    $places.Add(@{ path = (Join-Path $full ('release\' + $version)); via = $via })
    if ((Split-Path $full -Leaf) -eq $version) { $places.Add(@{ path = $full; via = $via }) }
}
if ($env:ZSDK_ROOT) { Add-ZsdkPlace $env:ZSDK_ROOT 'ZSDK_ROOT' } else { $tried.Add('ZSDK_ROOT is not set') }
if ($lock['home']) {
    $homeFolder = if ([System.IO.Path]::IsPathRooted($lock['home'])) { $lock['home'] } else { Join-Path $root $lock['home'] }
    Add-ZsdkPlace $homeFolder 'home= in build\zsdk.lock'
}
else { $tried.Add('build\zsdk.lock has no home=') }
for ($walk = $root; $walk; $walk = Split-Path $walk -Parent) {
    $places.Add(@{ path = (Join-Path $walk ('_sdk\release\' + $version)); via = 'parent walk' })
    $places.Add(@{ path = (Join-Path $walk ('release\' + $version)); via = 'parent walk' })
}
$release = $null
$via = $null
foreach ($place in $places) {
    $manifest = Join-Path $place.path 'MANIFEST.sha256'
    if (-not (Test-Path -LiteralPath $manifest)) { $tried.Add($place.path + ' (' + $place.via + '): no release here'); continue }
    $hash = (Get-FileHash -LiteralPath $manifest -Algorithm SHA256).Hash
    if ($hash -ne $wanted) { $tried.Add($place.path + ' (' + $place.via + '): manifest ' + $hash.Substring(0, 8) + ' is another build of ' + $version); continue }
    $release = $place.path
    $via = $place.via
    break
}
if (-not $release) {
    $list = '  ' + ($tried -join ([Environment]::NewLine + '  '))
    throw ('ZoneSDK ' + $version + ' with manifest ' + $wanted.Substring(0, 8) + ' was not found for ' + $root + '. Set ZSDK_ROOT to the unpacked ZoneSDK folder that holds release\' + $version + ', or put home=<path relative to this add-on> into build\zsdk.lock. Tried:' + [Environment]::NewLine + $list)
}
$found = 'zsdk release {0} via {1}' -f $release, $via
if ($Where) {
    [pscustomobject]@{ Release = $release; Via = $via; Version = $version; Manifest = $wanted; Lock = $lockFile }
    return
}
Write-Host $found
Import-Module (Join-Path $release 'build\zsdk.psm1') -Force
$extra = @{}
if ($Verify) { $extra['Verify'] = $true }
if ($WaitMinutes -ge 0) { $extra['WaitMinutes'] = $WaitMinutes }
if ($Exe) { $extra['Exe'] = $Exe }
if ($LiveExe) { $extra['LiveExe'] = $true }
if ($RequireExe) { $extra['RequireExe'] = $true }
$step = if ($KeepSymbols) { 'Invoke-ZsdkRelease' } else { 'Invoke-ZsdkBuild' }
$known = Get-Command $step -ErrorAction SilentlyContinue
if (-not $known) { throw ('ZoneSDK ' + $version + ' has no ' + $step + '; -KeepSymbols needs ZoneSDK 1.0.0 or newer') }
foreach ($name in @($extra.Keys)) { if (-not $known.Parameters.ContainsKey($name)) { throw ('ZoneSDK ' + $version + ' does not know -' + $name + ' yet; drop it or move the add-on to a newer ZoneSDK') } }
$log = Join-Path $PSScriptRoot 'last-build.log'
$started = [DateTime]::Now.AddSeconds(-2)
try {
    if ($KeepSymbols) {
        if ($Config -ne 'Release') { throw '-KeepSymbols keeps a Release build only; drop -Config Debug' }
        Invoke-ZsdkRelease -Root $root -Lock $lock -Name 'ZoneFieldWatch' -Define 'FIELDWATCH' @extra
    }
    else { Invoke-ZsdkBuild -Root $root -Lock $lock -Name 'ZoneFieldWatch' -Define 'FIELDWATCH' -Config $Config -Tests:$Tests -Deterministic:$Deterministic @extra }
}
finally {
    if ((Test-Path -LiteralPath $log) -and (Get-Item -LiteralPath $log).LastWriteTime -ge $started) { [System.IO.File]::AppendAllText($log, $found + [Environment]::NewLine, (New-Object System.Text.UTF8Encoding($false))) }
}
