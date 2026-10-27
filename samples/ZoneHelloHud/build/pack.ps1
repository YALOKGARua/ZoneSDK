[CmdletBinding()]
param(
    [switch]$Build,
    [switch]$Help
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if ($Help) {
    Write-Host 'usage: pack.cmd [-Build]'
    Write-Host '  writes dist\<name>-<version>.zip with the add-on, the player readmes, NOTICE-ADDON.txt and SHA256.txt'
    Write-Host '  -Build   build the Release binary with its tests first'
    return
}
$name = 'ZoneHelloHud'
$make = Join-Path $PSScriptRoot 'make.ps1'
$where = & $make -Where
if ($Build) { & $make -Tests }
$binary = Join-Path $root ('bin\' + $name + '.addon64')
if (-not (Test-Path -LiteralPath $binary)) { throw ('there is no Release build to pack, ' + $binary + ' is missing: run build.cmd first') }
$info = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($binary)
if (-not $info.FileVersion) { throw ('the add-on has no version resource: ' + $binary) }
if ($info.IsDebug) { throw ('refused: ' + $binary + ' is a Debug build (VS_FF_DEBUG in its version resource); pack only a Release build') }
$proof = Join-Path $root ('bin\' + $name + '.tested')
$passed = 'passed ' + (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash
if (-not (Test-Path -LiteralPath $proof) -or ([System.IO.File]::ReadAllText($proof)).Trim() -ne $passed) { Write-Warning ($binary + ' was not built with passing tests (build.cmd -Tests or test.cmd): pack.cmd -Build tests it first') }
$built = (Get-Item -LiteralPath $binary).LastWriteTime
$newer = @(Get-ChildItem -LiteralPath (Join-Path $root 'src') -Recurse -File | Where-Object { $_.LastWriteTime -gt $built })
if ($newer.Count -gt 0) { Write-Warning ('src\' + $newer[0].FullName.Substring((Join-Path $root 'src').Length + 1) + ' is newer than ' + $binary + ': rebuild before you pack (pack.cmd -Build)') }
$version = '{0}.{1}.{2}' -f $info.FileMajorPart, $info.FileMinorPart, $info.FileBuildPart
$notice = Join-Path $where.Release 'NOTICE-ADDON.txt'
if (-not (Test-Path -LiteralPath $notice)) { throw ('NOTICE-ADDON.txt is missing from the ZoneSDK release ' + $where.Release + ': every add-on zip must carry it, so pack needs a ZoneSDK release that ships it') }
$games = New-Object System.Collections.Generic.List[string]
foreach ($folder in @(Get-ChildItem -LiteralPath (Join-Path $where.Release 'data\builds') -Directory -ErrorAction SilentlyContinue | Sort-Object Name)) {
    $json = Join-Path $folder.FullName 'build.json'
    if (-not (Test-Path -LiteralPath $json)) { continue }
    $facts = Get-Content -LiteralPath $json -Raw | ConvertFrom-Json
    if ($facts.game_build_date) { $games.Add($folder.Name + ' (' + $facts.game_build_date + ')') } else { $games.Add($folder.Name) }
}
$fill = @{ VERSION = $version; AUTHOR = [string]$info.CompanyName; ZSDK = $where.Version; GAME = ($games -join ', ') }
$utf8 = New-Object System.Text.UTF8Encoding($false)
$entries = New-Object 'System.Collections.Generic.SortedDictionary[string,byte[]]' ([System.StringComparer]::Ordinal)
foreach ($lang in 'EN', 'UA') {
    $source = Join-Path $root ('dist\README-' + $lang + '.txt')
    if (-not (Test-Path -LiteralPath $source)) { throw ('the player readme is missing: ' + $source) }
    $text = [System.IO.File]::ReadAllText($source, $utf8)
    foreach ($match in [regex]::Matches($text, '\{\{([A-Za-z]+)\}\}')) {
        $key = $match.Groups[1].Value
        if (-not $fill.ContainsKey($key)) { throw ('dist\README-' + $lang + '.txt uses {{' + $key + '}}, which pack does not fill; known: ' + (($fill.Keys | Sort-Object) -join ', ')) }
        if (-not $fill[$key]) { throw ('dist\README-' + $lang + '.txt needs {{' + $key + '}}, but it is empty' + $(if ($key -eq 'AUTHOR') { ': the version resource names no author (CompanyName), create the add-on with new-addon -Author or set the author in src\version.h' } else { '' })) }
        $text = $text.Replace($match.Value, $fill[$key])
    }
    $text = [regex]::Replace($text, '\r?\n', "`r`n")
    $entries['README-' + $lang + '.txt'] = [byte[]]((New-Object System.Text.UTF8Encoding($true)).GetPreamble() + $utf8.GetBytes($text))
}
$entries['NOTICE-ADDON.txt'] = [System.IO.File]::ReadAllBytes($notice)
$entries['Stalker2/Binaries/Win64/' + $name + '.addon64'] = [System.IO.File]::ReadAllBytes($binary)
$hasher = [System.Security.Cryptography.SHA256]::Create()
$sums = New-Object System.Collections.Generic.List[string]
foreach ($key in $entries.Keys) { $sums.Add(([System.BitConverter]::ToString($hasher.ComputeHash($entries[$key])).Replace('-', '')) + '  ' + $key) }
$entries['SHA256.txt'] = $utf8.GetBytes(($sums -join "`r`n") + "`r`n")
Add-Type -AssemblyName System.IO.Compression
$dist = Join-Path $root 'dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
$zipName = $name + '-' + $version + '.zip'
$zipPath = Join-Path $dist $zipName
if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
$stamp = New-Object System.DateTimeOffset (2026, 1, 1, 0, 0, 0, [System.TimeSpan]::Zero)
$stream = [System.IO.File]::Open($zipPath, [System.IO.FileMode]::CreateNew)
try {
    $zip = New-Object System.IO.Compression.ZipArchive ($stream, [System.IO.Compression.ZipArchiveMode]::Create, $true)
    try {
        foreach ($key in $entries.Keys) {
            $entry = $zip.CreateEntry($key, [System.IO.Compression.CompressionLevel]::Optimal)
            $entry.LastWriteTime = $stamp
            $out = $entry.Open()
            try { $out.Write($entries[$key], 0, $entries[$key].Length) } finally { $out.Dispose() }
        }
    }
    finally { $zip.Dispose() }
}
finally { $stream.Dispose() }
$zipHash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash
[System.IO.File]::WriteAllText((Join-Path $dist ($name + '-' + $version + '-SHA256.txt')), ($zipHash + '  ' + $zipName + "`r`n"), $utf8)
foreach ($key in $entries.Keys) { Write-Host ('  ' + $key) }
Write-Host ('packed {0}' -f $zipPath)
Write-Host ('sha256 {0}' -f $zipHash)
