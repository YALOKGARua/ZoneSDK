[CmdletBinding()]
param(
    [string]$Game,
    [string]$Addon,
    [switch]$Selftest,
    [string]$Workspace,
    [switch]$Help
)

$ErrorActionPreference = 'Stop'
if ($Help) {
    Write-Host 'usage: doctor.cmd [-Game <game folder>] [-Addon <name>] [-Selftest] [-Workspace <folder>]'
    Write-Host '  checks PowerShell, the execution policy, Mark-of-the-Web, the toolchain, Python, the game folder, ReShade and the game build; it only reads'
    Write-Host '  -Game       the game folder, found through Steam by default'
    Write-Host '  -Addon      also check that ReShade.ini and ReShade.log do not disable or miss this add-on'
    Write-Host '  -Selftest   also run the ZoneSDK scaffold self-test, about seven minutes'
    Write-Host '  -Workspace  the folder the self-test works in, a new folder in %TEMP% by default, removed after a pass'
    exit 0
}
$tree = Split-Path $PSScriptRoot -Parent
$inside = Test-Path -LiteralPath (Join-Path $tree 'MANIFEST.sha256')
$script:Problems = 0
$script:Cautions = 0

function Write-Doctor([string]$state, [string]$text) {
    if ($state -eq 'FAIL') { $script:Problems++ } elseif ($state -eq 'warn') { $script:Cautions++ }
    Write-Host ('{0,-5} {1}' -f $state, $text)
}

function Get-DoctorVersion([string]$root) {
    $file = Join-Path $root 'include\zsdk\version.h'
    if (-not (Test-Path -LiteralPath $file)) { return '?' }
    $text = [System.IO.File]::ReadAllText($file)
    $parts = foreach ($part in 'MAJOR', 'MINOR', 'PATCH') { [regex]::Match($text, 'ZSDK_VERSION_' + $part + ' (\d+)').Groups[1].Value }
    return ($parts -join '.')
}

function Find-DoctorGame {
    $steam = New-Object System.Collections.Generic.List[string]
    foreach ($entry in @(@('HKCU:\Software\Valve\Steam', 'SteamPath'), @('HKLM:\SOFTWARE\WOW6432Node\Valve\Steam', 'InstallPath'), @('HKLM:\SOFTWARE\Valve\Steam', 'InstallPath'))) {
        $value = Get-ItemProperty -LiteralPath $entry[0] -Name $entry[1] -ErrorAction SilentlyContinue
        if ($value -and $value.($entry[1])) { $steam.Add([System.IO.Path]::GetFullPath(([string]$value.($entry[1])).Replace('/', '\'))) }
    }
    $libraries = New-Object System.Collections.Generic.List[string]
    foreach ($one in $steam) { if ($libraries -notcontains $one) { $libraries.Add($one) } }
    foreach ($one in @($steam)) {
        foreach ($vdf in @((Join-Path $one 'steamapps\libraryfolders.vdf'), (Join-Path $one 'config\libraryfolders.vdf'))) {
            if (-not (Test-Path -LiteralPath $vdf)) { continue }
            foreach ($m in [regex]::Matches([System.IO.File]::ReadAllText($vdf), '"path"\s+"([^"\r\n]+)"')) {
                $path = $m.Groups[1].Value.Replace('\\', '\')
                if ([System.IO.Path]::IsPathRooted($path) -and $libraries -notcontains $path) { $libraries.Add($path) }
            }
        }
    }
    $found = New-Object System.Collections.Generic.List[string]
    foreach ($library in $libraries) {
        $acf = Join-Path $library 'steamapps\appmanifest_1643320.acf'
        if (-not (Test-Path -LiteralPath $acf)) { continue }
        $m = [regex]::Match([System.IO.File]::ReadAllText($acf), '"installdir"\s+"([^"\r\n]+)"')
        if (-not $m.Success) { continue }
        $folder = Join-Path (Join-Path $library 'steamapps\common') $m.Groups[1].Value
        if ((Test-Path -LiteralPath $folder) -and -not $found.Contains($folder)) { $found.Add($folder) }
    }
    return @{ steam = $steam; libraries = $libraries; games = $found }
}

function Get-DoctorText([string]$path, [int]$limit) {
    $stream = [System.IO.File]::Open($path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
    try {
        $size = [int][Math]::Min($stream.Length, $limit)
        $bytes = New-Object byte[] $size
        $read = 0
        while ($read -lt $size) { $n = $stream.Read($bytes, $read, $size - $read); if ($n -le 0) { break }; $read += $n }
        return [System.Text.Encoding]::GetEncoding(28591).GetString($bytes, 0, $read)
    }
    finally { $stream.Dispose() }
}

$version = Get-DoctorVersion $tree
Write-Host ('ZoneSDK {0} doctor, {1}, read-only' -f $version, $(if ($inside) { 'release ' + $tree } else { 'working tree ' + $tree }))

$edition = if ($PSVersionTable.PSEdition) { $PSVersionTable.PSEdition } else { 'Desktop' }
Write-Doctor 'ok' ('PowerShell {0} {1}' -f $PSVersionTable.PSVersion, $edition)
$scopes = [ordered]@{}
foreach ($scope in 'MachinePolicy', 'UserPolicy', 'Process', 'CurrentUser', 'LocalMachine') { $scopes[$scope] = [string](Get-ExecutionPolicy -Scope $scope) }
$policyText = (@($scopes.Keys | ForEach-Object { $_ + ' ' + $scopes[$_] }) -join ', ')
$forced = @('MachinePolicy', 'UserPolicy') | Where-Object { $scopes[$_] -ne 'Undefined' } | Select-Object -First 1
$lasting = @('MachinePolicy', 'UserPolicy', 'CurrentUser', 'LocalMachine') | Where-Object { $scopes[$_] -ne 'Undefined' } | Select-Object -First 1
$lastingPolicy = if ($lasting) { $scopes[$lasting] } else { 'Restricted' }
if ($forced -and @('Restricted', 'AllSigned') -contains $scopes[$forced]) { Write-Doctor 'FAIL' ('execution policy: ' + $policyText + '. A Group Policy forces ' + $scopes[$forced] + ', which the .cmd launchers cannot bypass; ask the PC administrator') }
elseif (@('Restricted', 'AllSigned', 'Undefined') -contains $lastingPolicy) { Write-Doctor 'warn' ('execution policy: ' + $policyText + '. A plain .ps1 does not run in a new PowerShell window: use the .cmd launchers (they pass -ExecutionPolicy Bypass), or run Set-ExecutionPolicy -Scope CurrentUser RemoteSigned') }
else { Write-Doctor 'ok' ('execution policy: ' + $policyText) }

$top = $tree
if ($inside -and (Split-Path (Split-Path $tree -Parent) -Leaf) -eq 'release') { $top = Split-Path (Split-Path $tree -Parent) -Parent }
$scripts = @(Get-ChildItem -LiteralPath $top -Recurse -File -ErrorAction SilentlyContinue | Where-Object { @('.ps1', '.psm1', '.cmd', '.py') -contains $_.Extension.ToLowerInvariant() })
$marked = @($scripts | Where-Object { Get-Item -LiteralPath $_.FullName -Stream 'Zone.Identifier' -ErrorAction SilentlyContinue })
if ($marked.Count -gt 0) { Write-Doctor 'warn' ('Mark-of-the-Web: {0} of {1} scripts under {2} came from the internet, for example {3}. The .cmd launchers still run; to run the .ps1 files directly: Get-ChildItem -LiteralPath "{2}" -Recurse | Unblock-File' -f $marked.Count, $scripts.Count, $top, $marked[0].FullName.Substring($top.Length + 1)) }
else { Write-Doctor 'ok' ('Mark-of-the-Web: none on the {0} scripts under {1}' -f $scripts.Count, $top) }

$module = Join-Path $tree 'build\zsdk.psm1'
$wrapper = $null
try {
    Import-Module $module -Force
    $wrapper = Resolve-ZsdkWrapper
    Write-Doctor 'ok' ('toolchain wrapper ' + $wrapper)
}
catch { Write-Doctor 'FAIL' ('toolchain: ' + $_.Exception.Message) }
if ($wrapper) {
    try {
        Import-ZsdkToolchain
        foreach ($tool in 'cl.exe', 'link.exe', 'rc.exe', 'dumpbin.exe') {
            $found = Get-Command $tool -ErrorAction SilentlyContinue | Select-Object -First 1
            $fileVersion = if ($found -and $found.FileVersionInfo) { $found.FileVersionInfo.ProductVersion } else { '?' }
            Write-Doctor 'ok' ('{0} {1} at {2}' -f $tool, $fileVersion, $found.Source)
        }
        $sdkVersion = if ($env:WindowsSDKVersion) { $env:WindowsSDKVersion.TrimEnd('\') } else { [regex]::Match((Get-Command rc.exe).Source, '\\(10\.0\.\d+\.\d+)\\').Groups[1].Value }
        Write-Doctor 'ok' ('Windows SDK {0}, MSVC tools {1}' -f $(if ($sdkVersion) { $sdkVersion } else { '?' }), $(if ($env:VCToolsVersion) { $env:VCToolsVersion } else { '?' }))
    }
    catch { Write-Doctor 'FAIL' ('toolchain: ' + $_.Exception.Message) }
}

$python = @(Get-Command python.exe, py.exe -ErrorAction SilentlyContinue | Where-Object { $_.Source -notmatch '\\WindowsApps\\' }) | Select-Object -First 1
if ($python) {
    $said = (@(& cmd /d /c ('"' + $python.Source + '" --version 2>&1')) | ForEach-Object { [string]$_ }) -join ' '
    Write-Doctor 'ok' ('{0} at {1} (optional, used by tools\pe\pe_facts.py)' -f $said.Trim(), $python.Source)
}
else { Write-Doctor 'info' 'Python 3 not found; it is optional, only tools\pe\pe_facts.py uses it' }

$win64 = $null
if ($Game) {
    $full = [System.IO.Path]::GetFullPath($ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Game))
    $nested = Join-Path (Join-Path (Join-Path $full 'Stalker2') 'Binaries') 'Win64'
    if (Test-Path -LiteralPath $nested) { $win64 = $nested } elseif (Test-Path -LiteralPath (Join-Path $full 'Stalker2-Win64-Shipping.exe')) { $win64 = $full }
    if ($win64) { Write-Doctor 'ok' ('game folder ' + $full + ' (given with -Game)') } else { Write-Doctor 'FAIL' ('-Game is not a S.T.A.L.K.E.R. 2 folder (no Stalker2 binaries folder inside): ' + $full) }
}
else {
    $look = Find-DoctorGame
    if ($look.steam.Count -eq 0) { Write-Doctor 'warn' 'Steam is not installed for this user; pass -Game <game folder>' }
    elseif ($look.games.Count -eq 0) { Write-Doctor 'warn' ('S.T.A.L.K.E.R. 2 (Steam app 1643320) is in none of the {0} Steam libraries; pass -Game <game folder>' -f $look.libraries.Count) }
    else {
        $win64 = Join-Path (Join-Path (Join-Path $look.games[0] 'Stalker2') 'Binaries') 'Win64'
        Write-Doctor 'ok' ('game folder ' + $look.games[0] + ' (Steam libraryfolders.vdf, ' + $look.libraries.Count + ' libraries)')
        foreach ($other in @($look.games | Select-Object -Skip 1)) { Write-Doctor 'info' ('another copy of the game: ' + $other) }
    }
}

if ($win64) {
    $named = @(Get-Process -Name 'Stalker2-Win64-Shipping' -ErrorAction SilentlyContinue)
    $game = @($named | Where-Object { $_.Path -and $_.Path -match '\\Binaries\\Win64\\Stalker2-Win64-Shipping\.exe$' })
    if ($game.Count -gt 0) { Write-Doctor 'info' 'the game is running; the doctor only reads its files' }
    elseif ($named.Count -gt 0) { Write-Doctor 'info' 'a ZoneSDK fake host is running (it has the game exe name); the doctor only reads the game files' }
    $reshade = $null
    foreach ($dll in 'dxgi.dll', 'd3d12.dll', 'd3d11.dll', 'ReShade64.dll') {
        $path = Join-Path $win64 $dll
        if (-not (Test-Path -LiteralPath $path)) { continue }
        $info = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($path)
        if ($info.ProductName -eq 'ReShade') { $reshade = @{ path = $path; dll = $dll; info = $info }; break }
    }
    if (-not $reshade) { Write-Doctor 'FAIL' ('ReShade is not installed in ' + $win64 + ': install ReShade 6.8 or newer, the "with full add-on support" download, for the game with DirectX 12') }
    else {
        $have = New-Object System.Version ($reshade.info.FileMajorPart, $reshade.info.FileMinorPart, $reshade.info.FileBuildPart)
        $text = Get-DoctorText $reshade.path 67108864
        $loader = $text.Contains('ReShadeRegisterAddon') -and $text.Contains('Searching for add-ons')
        if ($have -lt (New-Object System.Version (6, 8, 0))) { Write-Doctor 'FAIL' ('ReShade {0} ({1}) is older than 6.8: ZoneSDK add-ons need ReShade 6.8 or newer with full add-on support' -f $have, $reshade.dll) }
        elseif (-not $loader) { Write-Doctor 'FAIL' ('ReShade {0} ({1}) has no add-on loader: install the "with full add-on support" download' -f $have, $reshade.dll) }
        else { Write-Doctor 'ok' ('ReShade {0} ({1}), add-on loader present; ZoneSDK add-ons need the "with full add-on support" download' -f $have, $reshade.dll) }
    }
    $ini = Join-Path $win64 'ReShade.ini'
    if (Test-Path -LiteralPath $ini) {
        $section = ''
        $disabled = @()
        $addonPath = $null
        foreach ($line in [System.IO.File]::ReadAllLines($ini)) {
            $trim = $line.Trim()
            if ($trim -match '^\[(.+)\]$') { $section = $Matches[1]; continue }
            if ($section -ne 'ADDON') { continue }
            if ($trim -match '^DisabledAddons\s*=(.*)$') { $disabled = @($Matches[1].Split(',') | ForEach-Object { $_.Trim() } | Where-Object { $_ }) }
            if ($trim -match '^AddonPath\s*=(.*)$' -and $Matches[1].Trim()) { $addonPath = $Matches[1].Trim() }
        }
        if ($disabled.Count -gt 0) { Write-Doctor 'info' ('ReShade.ini DisabledAddons: ' + ($disabled -join ', ')) } else { Write-Doctor 'ok' 'ReShade.ini DisabledAddons: none' }
        if ($addonPath) { Write-Doctor 'warn' ('ReShade.ini AddonPath=' + $addonPath + ': ReShade looks for add-ons there instead of next to the game') }
        if ($Addon) {
            $off = @($disabled | Where-Object { $_ -eq $Addon -or $_ -like ($Addon + '@*') })
            if ($off.Count -gt 0) { Write-Doctor 'FAIL' ($Addon + ' is disabled in ReShade.ini (' + ($off -join ', ') + '): enable it on the Add-ons page of ReShade') } else { Write-Doctor 'ok' ($Addon + ' is not disabled in ReShade.ini') }
        }
    }
    elseif ($reshade) { Write-Doctor 'info' 'ReShade.ini does not exist yet; ReShade writes it on the first game start' }
    $logFile = Join-Path $win64 'ReShade.log'
    if (Test-Path -LiteralPath $logFile) {
        $lines = @((Get-DoctorText $logFile 8388608) -split "`r?`n")
        $registered = @($lines | ForEach-Object { [regex]::Match($_, 'Registered add-on "([^"]+)"') } | Where-Object { $_.Success } | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
        $bad = @($lines | Where-Object { $_ -match '\|\s*(ERROR|WARN)\s*\|' -and $_ -match '(?i)add-?on' -and $_ -notmatch 'still loaded' })
        Write-Doctor 'info' ('ReShade.log: {0} add-ons registered in the last session' -f $registered.Count)
        if ($Addon) {
            if ($registered -contains $Addon) { Write-Doctor 'ok' ($Addon + ' was registered by ReShade in the last session') } else { Write-Doctor 'warn' ($Addon + ' is not among the add-ons ReShade registered in the last session') }
        }
        foreach ($one in @($bad | Select-Object -Last 5)) { Write-Doctor 'warn' ('ReShade.log: ' + $one.Trim()) }
    }
    $exe = Join-Path $win64 'Stalker2-Win64-Shipping.exe'
    if (Test-Path -LiteralPath $exe) {
        $sha = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
        $known = New-Object System.Collections.Generic.List[string]
        $match = $null
        foreach ($folder in @(Get-ChildItem -LiteralPath (Join-Path $tree 'data\builds') -Directory -ErrorAction SilentlyContinue | Sort-Object Name)) {
            $json = Join-Path $folder.FullName 'build.json'
            if (-not (Test-Path -LiteralPath $json)) { continue }
            $facts = Get-Content -LiteralPath $json -Raw | ConvertFrom-Json
            $known.Add($folder.Name)
            if ([string]$facts.sha256 -eq $sha) { $match = $facts }
        }
        if ($match) { Write-Doctor 'ok' ('game build {0} ({1}) is known to ZoneSDK {2}' -f $match.id, $match.game_build_date, $version) }
        else { Write-Doctor 'warn' ('game build {0} is unknown to ZoneSDK {1}, which knows {2}: add-ons built on it start, but their guarded game features stay off until an update' -f $sha.Substring(0, 8).ToLowerInvariant(), $version, $(if ($known.Count) { $known -join ', ' } else { 'no build' })) }
    }
    else { Write-Doctor 'warn' ('the game exe is missing: ' + $exe) }
}

if ($Selftest) {
    $check = Join-Path $PSScriptRoot 'check-scaffold.ps1'
    $options = @{}
    if ($Workspace) { $options['Workspace'] = $Workspace }
    if (-not $inside) { $options['Candidate'] = $true }
    try {
        & $check @options
        Write-Doctor 'ok' ('self-test: check-scaffold passed' + $(if ($inside) { ' inside the release' } else { ' on a candidate release of the working tree' }))
    }
    catch { Write-Doctor 'FAIL' ('self-test: ' + $_.Exception.Message) }
}

Write-Host ('doctor: {0} problems, {1} warnings' -f $script:Problems, $script:Cautions)
if ($script:Problems -gt 0) { exit 1 }
