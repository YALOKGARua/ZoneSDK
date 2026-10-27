[CmdletBinding()]
param(
    [string]$Workspace,
    [string]$Version,
    [string]$Release,
    [switch]$Candidate
)

$ErrorActionPreference = 'Stop'
$tree = [System.IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent)).TrimEnd('\')
$inside = Test-Path -LiteralPath (Join-Path $tree 'MANIFEST.sha256')
$utf8 = New-Object System.Text.UTF8Encoding($false)
$invariant = [System.Globalization.CultureInfo]::InvariantCulture
$results = New-Object System.Collections.Generic.List[object]
$id = [guid]::NewGuid().ToString('N').Substring(0, 8)

function Get-CheckVersion([string]$root) {
    $text = [System.IO.File]::ReadAllText((Join-Path $root 'include\zsdk\version.h'))
    $parts = foreach ($part in 'MAJOR', 'MINOR', 'PATCH') { [regex]::Match($text, 'ZSDK_VERSION_' + $part + ' (\d+)').Groups[1].Value }
    return ($parts -join '.')
}

function Get-CheckSha([string]$path) {
    return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
}

function Read-CheckLock([string]$path) {
    $table = @{}
    foreach ($line in Get-Content -LiteralPath $path -Encoding UTF8) { $pair = $line -split '=', 2; if ($pair.Count -eq 2) { $table[$pair[0].Trim()] = $pair[1].Trim() } }
    return $table
}

function Write-CheckText([string]$path, [string]$text) {
    New-Item -ItemType Directory -Force -Path (Split-Path $path -Parent) | Out-Null
    [System.IO.File]::WriteAllText($path, $text, $utf8)
}

function Get-CheckRefusal([scriptblock]$action) {
    try { & $action | Out-Null; return $null } catch { return $_.Exception.Message }
}

function Add-Check([string]$area, [string]$name, [bool]$ok) {
    $results.Add([pscustomobject]@{ area = $area; name = $name; state = $(if ($ok) { 'ok' } else { 'FAIL' }); why = '' })
}

function Add-Skip([string]$area, [string]$name, [string]$why) {
    $results.Add([pscustomobject]@{ area = $area; name = $name; state = 'skip'; why = $why })
}

function Invoke-CheckChild([string]$policy, [string]$script) {
    $folder = Join-Path $Workspace 'child'
    New-Item -ItemType Directory -Force -Path $folder | Out-Null
    $out = Join-Path $folder ([guid]::NewGuid().ToString('N').Substring(0, 8))
    $encoded = [Convert]::ToBase64String([System.Text.Encoding]::Unicode.GetBytes($script))
    $proc = Start-Process -FilePath 'powershell.exe' -ArgumentList ('-NoProfile -NonInteractive -ExecutionPolicy ' + $policy + ' -EncodedCommand ' + $encoded) -NoNewWindow -PassThru -RedirectStandardOutput ($out + '.out') -RedirectStandardError ($out + '.err')
    $null = $proc.Handle
    $proc.WaitForExit()
    return @{ code = $proc.ExitCode; text = ([System.IO.File]::ReadAllText($out + '.out') + [System.IO.File]::ReadAllText($out + '.err')) }
}

function Get-CheckAscii([string]$path) {
    if ($path -notmatch '[^\x20-\x7E]') { return $path }
    $short = $null
    try { $short = (New-Object -ComObject Scripting.FileSystemObject).GetFolder($path).ShortPath } catch { $short = $null }
    if (-not $short -or $short -match '[^\x20-\x7E]') { return $null }
    return $short
}

function Get-CheckQuoted([string]$text) {
    return "'" + $text.Replace("'", "''") + "'"
}

function Get-CheckRows([string]$root) {
    $rows = New-Object System.Collections.Generic.List[object]
    $patterns = @(
        '\{\s*"([a-z0-9_]+)"\s*,\s*"([A-Za-z0-9]+)"\s*,\s*([0-9]*\.?[0-9]+)f\s*,\s*"((?:[^"\\]|\\.)*)"\s*\}',
        '\{\s*\.id\s*=\s*"([a-z0-9_]+)"\s*,\s*\.tag\s*=\s*"([A-Za-z0-9]+)"\s*,\s*\.lane\s*=\s*([0-9]*\.?[0-9]+)f\s*,\s*\.keys\s*=\s*"((?:[^"\\]|\\.)*)"\s*\}')
    foreach ($file in @(Get-ChildItem -LiteralPath (Join-Path $root 'src') -Recurse -File -Include '*.cpp', '*.h', '*.inc' -ErrorAction SilentlyContinue)) {
        $text = [System.IO.File]::ReadAllText($file.FullName)
        if ($text -notmatch 'registry') { continue }
        foreach ($pattern in $patterns) {
            foreach ($m in [regex]::Matches($text, $pattern)) { $rows.Add([pscustomobject]@{ id = $m.Groups[1].Value; tag = $m.Groups[2].Value; lane = [double]::Parse($m.Groups[3].Value, $invariant); keys = $m.Groups[4].Value.Replace('\\', '\') }) }
        }
    }
    return $rows
}

function Write-CheckManifest([string]$release) {
    $manifest = Join-Path $release 'MANIFEST.sha256'
    if (Test-Path -LiteralPath $manifest) { (Get-Item -LiteralPath $manifest).IsReadOnly = $false; Remove-Item -LiteralPath $manifest -Force }
    $lines = New-Object System.Collections.Generic.List[string]
    foreach ($item in @(Get-ChildItem -LiteralPath $release -Recurse -File | Sort-Object { $_.FullName.Substring($release.Length + 1).ToLowerInvariant() })) {
        $lines.Add(('{0}  {1}' -f (Get-CheckSha $item.FullName), $item.FullName.Substring($release.Length + 1)))
    }
    [System.IO.File]::WriteAllLines($manifest, $lines, $utf8)
    return (Get-CheckSha $manifest)
}

function Copy-CheckRelease([string]$from, [string]$to) {
    New-Item -ItemType Directory -Force -Path (Split-Path $to -Parent) | Out-Null
    Copy-Item -LiteralPath $from -Destination $to -Recurse -Force
}

function Copy-CheckFiles([string]$from, [string]$to, [string]$keep, [string]$drop) {
    foreach ($item in @(Get-ChildItem -LiteralPath $from -Recurse -File)) {
        $rel = $item.FullName.Substring($from.Length + 1)
        if ($rel -notmatch $keep -or ($drop -and $rel -match $drop)) { continue }
        $out = Join-Path $to $rel
        New-Item -ItemType Directory -Force -Path (Split-Path $out -Parent) | Out-Null
        Copy-Item -LiteralPath $item.FullName -Destination $out -Force
    }
}

function New-CheckCandidate([string]$out, [string]$v) {
    New-Item -ItemType Directory -Force -Path $out | Out-Null
    foreach ($part in 'include', 'src', 'extern', 'data', 'api') {
        if (Test-Path -LiteralPath (Join-Path $tree $part)) { Copy-Item -LiteralPath (Join-Path $tree $part) -Destination (Join-Path $out $part) -Recurse -Force }
    }
    Copy-CheckFiles (Join-Path $tree 'tests\generic') (Join-Path $out 'tests\generic') '.' ''
    Copy-CheckFiles (Join-Path $tree 'build') (Join-Path $out 'build') '^zsdk\.psm1$' ''
    Copy-CheckFiles (Join-Path $tree 'tools') (Join-Path $out 'tools') '^(new-addon\.ps1|check-scaffold\.ps1|doctor\.ps1|pe\\[^\\]+\.py|fakehost\\[^\\]+\.(ps1|cpp|h|inc|py))$' ''
    Copy-CheckFiles (Join-Path $tree 'template\ZoneTemplate') (Join-Path $out 'template\ZoneTemplate') '^((src|tests|build|dist)\\|[^\\]+\.cmd$|\.gitignore$)' '^(bin\\|build\\obj\\|build\\last-build\.log$|build\\\.zsdk-build\.lock$|build\\local\.cfg$|dist\\[^\\]+\\symbols\\|dist\\.*\.zip$|dist\\.*-SHA256\.txt$)'
    Write-CheckText (Join-Path $out 'template\ZoneTemplate\build\zsdk.lock') ('version=' + $v + "`n")
    foreach ($item in @(Get-ChildItem -LiteralPath $tree -File)) {
        if ($item.Name -match '^([^.]+\.cmd|LICENSE.*|NOTICE.*|THIRD-PARTY-NOTICES.*)$') { Copy-Item -LiteralPath $item.FullName -Destination (Join-Path $out $item.Name) -Force }
    }
    $hash = Write-CheckManifest $out
    Get-ChildItem -LiteralPath $out -Recurse -File | ForEach-Object { $_.IsReadOnly = $true }
    return $hash
}

function Get-CheckPrivate([string]$name) {
    return [Environment]::GetEnvironmentVariable($name, 'Process')
}

function Set-CheckPrivate([string]$name, $value) {
    [Environment]::SetEnvironmentVariable($name, $value, 'Process')
}

$ownWorkspace = -not $Workspace
$saved = @{}
foreach ($name in 'ZSDK_VCVARS', 'ZSDK_ROOT', 'ZSDK_NOPAUSE') { $saved[$name] = Get-CheckPrivate $name }
$stopped = $null
$away = $null
try {
    if ($Candidate) {
        if ($inside) { throw '-Candidate builds a release candidate from the working tree, but this check-scaffold already runs inside a release' }
        $v = Get-CheckVersion $tree
        if (-not $Workspace) { $Workspace = Join-Path ([System.IO.Path]::GetTempPath()) ('zsdk-scaffold-' + $id) }
        $Workspace = [System.IO.Path]::GetFullPath($Workspace).TrimEnd('\')
        if (Test-Path -LiteralPath $Workspace) { throw ('the scaffold workspace exists and is never reused: ' + $Workspace) }
        $cand = Join-Path $Workspace ('candidate\release\' + $v)
        $hash = New-CheckCandidate $cand $v
        Write-Host ('candidate release {0} at {1}, manifest {2}' -f $v, $cand, $hash.Substring(0, 8))
        if (-not $env:ZSDK_VCVARS) { Import-Module (Join-Path $tree 'build\zsdk.psm1') -Force; $env:ZSDK_VCVARS = Resolve-ZsdkWrapper }
        $proc = Start-Process -FilePath 'powershell.exe' -ArgumentList ('-NoProfile -ExecutionPolicy Bypass -File "' + (Join-Path $cand 'tools\check-scaffold.ps1') + '" -Workspace "' + (Join-Path $Workspace 'inside') + '"') -NoNewWindow -PassThru
        $null = $proc.Handle
        $proc.WaitForExit()
        Import-Module (Join-Path $cand 'build\zsdk.psm1') -Force
        $changed = Get-CheckRefusal { Test-ZsdkManifest $cand }
        Write-Host ('{0}: the candidate release is unchanged after the check' -f $(if ($changed) { 'FAIL' } else { 'ok' }))
        if ($proc.ExitCode -ne 0) { throw ('the scaffold check inside the candidate release failed, exit ' + $proc.ExitCode) }
        if ($changed) { throw ('the candidate release was changed by the check: ' + $changed) }
        Write-Host ('candidate check passed, workspace {0}' -f $Workspace)
        $passed = $true
        return
    }

    if ($inside) { $source = $tree }
    elseif ($Release) { $source = [System.IO.Path]::GetFullPath($ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Release)).TrimEnd('\') }
    else {
        if (-not $Version) { $Version = Get-CheckVersion $tree }
        $source = Join-Path $tree ('release\' + $Version)
    }
    if (-not (Test-Path -LiteralPath (Join-Path $source 'MANIFEST.sha256'))) { throw ('release ' + (Split-Path $source -Leaf) + ' is not cut yet') }
    $v = Get-CheckVersion $source
    if (-not $Workspace) { $Workspace = Join-Path ([System.IO.Path]::GetTempPath()) ('zsdk-scaffold-' + $id) }
    $Workspace = [System.IO.Path]::GetFullPath($Workspace).TrimEnd('\')
    if (Test-Path -LiteralPath $Workspace) { throw ('the scaffold workspace exists and is never reused: ' + $Workspace) }
    $env:ZSDK_NOPAUSE = '1'
    Set-CheckPrivate 'ZSDK_ROOT' $null
    if (-not $env:ZSDK_VCVARS) { Import-Module (Join-Path $source 'build\zsdk.psm1') -Force; $env:ZSDK_VCVARS = Resolve-ZsdkWrapper }
    Write-Host ('scaffold check of ZoneSDK {0} at {1}, {2}' -f $v, $source, $(if ($inside) { 'inside the release' } else { 'tools of the working tree' }))
    $copy = Join-Path $Workspace ('_sdk\release\' + $v)
    Copy-CheckRelease $source $copy
    $copyHash = Get-CheckSha (Join-Path $copy 'MANIFEST.sha256')
    $tools = if ($inside) { Join-Path $copy 'tools' } else { $PSScriptRoot }
    $newAddon = Join-Path $tools 'new-addon.ps1'
    $rows = @(Get-CheckRows $copy)
    $away = Join-Path ([System.IO.Path]::GetTempPath()) ('zsdk-away-' + $id)

    $target = Join-Path $Workspace 'ZoneFoo'
    $options = @{ Name = 'ZoneFoo'; Target = $target; Author = 'Scaffold Check' }
    if (-not $inside) { $options['Release'] = $copy }
    & $newAddon @options
    $lock = Join-Path $target 'build\zsdk.lock'
    $table = Read-CheckLock $lock
    Add-Check 'lock' 'new-addon writes build\zsdk.lock with the version and manifest of the release, no home= when the walk finds it' ($table['version'] -eq $v -and $table['manifest'] -eq $copyHash -and -not $table.ContainsKey('home'))
    if ($inside) { Add-Check 'release' 'new-addon inside a release pins that release without -Release' ($table['manifest'] -eq $copyHash) }
    else { Add-Skip 'release' 'new-addon inside a release pins that release without -Release' 'runs only inside a release, use -Candidate' }
    $app = [System.IO.File]::ReadAllText((Join-Path $target 'src\app.cpp'))
    Add-Check 'name' 'without -Tag the tag is the name' ($app.Contains('"ZoneFoo"') -and (Test-Path -LiteralPath (Join-Path $target 'src\foo.h')))

    & (Join-Path $target 'build\make.ps1') -Tests
    $log = Get-Content -LiteralPath (Join-Path $target 'build\last-build.log') -Encoding UTF8
    Add-Check 'build' 'scaffold builds and passes its tests' ([bool]($log | Where-Object { $_ -match '^tests passed \d+ failed 0 ' }))
    Add-Check 'build' 'scaffold has 0 warnings' ([bool]($log | Where-Object { $_ -eq 'warnings 0' }))
    Add-Check '6 lookup' 'make logs the release it resolved' ([bool]($log | Where-Object { $_ -eq ('zsdk release ' + $copy + ' via parent walk') }))
    $releaseFile = Join-Path $target 'bin\ZoneFoo.addon64'
    $testedFile = Join-Path $target 'bin\ZoneFoo.tested'
    Add-Check '3 tests' 'a build whose tests pass writes bin\ZoneFoo.tested with the sha256 of the binary' ((Test-Path -LiteralPath $testedFile) -and (Test-Path -LiteralPath $releaseFile) -and ([System.IO.File]::ReadAllText($testedFile)).Trim() -eq ('passed ' + (Get-CheckSha $releaseFile)))
    $before = if (Test-Path -LiteralPath $releaseFile) { Get-CheckSha $releaseFile } else { '' }
    $debugFile = Join-Path $target 'bin\Debug\ZoneFoo.addon64'
    $message = Get-CheckRefusal { & (Join-Path $target 'build\make.ps1') -Config Debug }
    Add-Check '13 debug' 'a Debug build writes bin\Debug\ZoneFoo.addon64 and leaves the Release file untouched' (-not $message -and $before -and (Test-Path -LiteralPath $debugFile) -and (Test-Path -LiteralPath $releaseFile) -and (Get-CheckSha $releaseFile) -eq $before)
    $debugFlag = (Test-Path -LiteralPath $debugFile) -and [System.Diagnostics.FileVersionInfo]::GetVersionInfo($debugFile).IsDebug
    Add-Check '13 debug' 'the Debug build carries VS_FF_DEBUG and the Release build does not' ($debugFlag -and (Test-Path -LiteralPath $releaseFile) -and -not [System.Diagnostics.FileVersionInfo]::GetVersionInfo($releaseFile).IsDebug)
    $make = Join-Path $target 'build\make.ps1'
    $buildLog = Join-Path $target 'build\last-build.log'
    $message = Get-CheckRefusal { & $make -Verify }
    $log = @(Get-Content -LiteralPath $buildLog -Encoding UTF8)
    Add-Check '18 verify' '-Verify passes on a clean Release build: fake host run, exports, version resource, byte scan' (-not $message -and [bool]($log | Where-Object { $_ -eq 'verify: passed (fake host, exports, version resource, byte scan)' }) -and [bool]($log | Where-Object { $_ -like 'verify: fake host done, *' }))
    $pySaved = Get-CheckPrivate 'ZSDK_PYTHON'
    Set-CheckPrivate 'ZSDK_PYTHON' 'none'
    try { $message = Get-CheckRefusal { & $make -Verify } }
    finally { Set-CheckPrivate 'ZSDK_PYTHON' $pySaved }
    $log = @(Get-Content -LiteralPath $buildLog -Encoding UTF8)
    Add-Check '18 verify' 'without Python the exports are read by the PowerShell fallback and -Verify still passes' (-not $message -and [bool]($log | Where-Object { $_ -eq 'verify: exports DESCRIPTION, NAME, read with PowerShell' }))
    $message = Get-CheckRefusal { & $make -Config Debug -Verify }
    Add-Check '18 verify' '-Verify refuses a Debug build by its VS_FF_DEBUG flag' ($message -and $message.Contains('VS_FF_DEBUG'))
    $rcFile = Join-Path $target 'src\version.rc'
    $rcText = [System.IO.File]::ReadAllText($rcFile)
    $plainRc = [regex]::Replace($rcText, '#if defined\(ZSDK_DEBUG\)\r?\nFILEFLAGS VS_FF_DEBUG\r?\n#else\r?\n(FILEFLAGS [^\r\n]*)\r?\n#endif', '$1')
    Write-CheckText $rcFile $plainRc
    try { $message = Get-CheckRefusal { & $make -Config Debug -Verify } }
    finally { Write-CheckText $rcFile $rcText }
    $log = @(Get-Content -LiteralPath $buildLog -Encoding UTF8)
    Add-Check '18 verify' '-Verify refuses a Debug build whose version resource has no VS_FF_DEBUG branch, by the build config and the Debug CRT it links' ($plainRc -ne $rcText -and $message -and $message.Contains('Debug build') -and [bool]($log | Where-Object { $_ -like 'verify: FAIL the binary links the Debug CRT*' }))
    $versionH = Join-Path $target 'src\version.h'
    $original = [System.IO.File]::ReadAllText($versionH)
    $planted = [regex]::Replace($original, '(#define FOO_DESCRIPTION ")', ('${1}' + $target.Replace('\', '\\') + '\\planted '))
    Write-CheckText $versionH $planted
    try { $message = Get-CheckRefusal { & $make -Verify } }
    finally { Write-CheckText $versionH $original }
    $log = @(Get-Content -LiteralPath $buildLog -Encoding UTF8)
    Add-Check '18 verify' '-Verify finds the build root planted in the binary' ($planted -ne $original -and $message -and $message.Contains('build path') -and [bool]($log | Where-Object { $_ -like 'verify: FAIL the binary contains a build path: *' }))
    $elseDrive = [string][char]0x51
    $elsewhere = [regex]::Replace($original, '(#define FOO_DESCRIPTION ")', ('${1}' + $elseDrive + ':\\elsewhere\\planted\\ '))
    Write-CheckText $versionH $elsewhere
    try { $message = Get-CheckRefusal { & $make -Verify } }
    finally { Write-CheckText $versionH $original }
    $log = @(Get-Content -LiteralPath $buildLog -Encoding UTF8)
    Add-Check '18 verify' '-Verify finds an absolute path of another folder planted in the binary' ($elsewhere -ne $original -and $message -and $message.Contains('absolute path') -and [bool]($log | Where-Object { $_ -like ('verify: FAIL the binary contains an absolute path: ' + $elseDrive + ':\elsewhere\*') }))
    $null = Get-CheckRefusal { & $make }
    $run = Invoke-CheckChild 'Bypass' ('try { & ' + (Get-CheckQuoted (Join-Path $target 'build\pack.ps1')) + '; exit 0 } catch { Write-Output (''refused: '' + $_.Exception.Message); exit 3 }')
    Add-Check '3 tests' 'after a build without -Tests there is no tested stamp and pack warns before it packs' (-not (Test-Path -LiteralPath $testedFile) -and $run.text.Contains('was not built with passing tests'))
    $fooRow = @(Get-CheckRows $target | Where-Object { $_.id -eq 'foo' }) | Select-Object -First 1
    $echo = Join-Path $Workspace 'EchoHud'
    $echoOptions = @{ Name = 'EchoHud'; Target = $echo; Lane = $fooRow.lane; Key = $fooRow.keys }
    if (-not $inside) { $echoOptions['Release'] = $copy }
    $echoSaid = @(& $newAddon @echoOptions 3>&1 6>$null | ForEach-Object { [string]$_ })
    Add-Check '10 lane' 'new-addon warns when -Lane and -Key equal those of an add-on next to the target' ([bool]($echoSaid | Where-Object { $_ -like '*is shared with the add-on ZoneFoo*' }) -and [bool]($echoSaid | Where-Object { $_ -like ('*the key ' + $fooRow.keys + ' is also used by the add-on ZoneFoo*') }))
    $twin = Join-Path $Workspace 'TwinHud'
    $twinOptions = @{ Name = 'TwinHud'; Target = $twin }
    if (-not $inside) { $twinOptions['Release'] = $copy }
    & $newAddon @twinOptions 6>$null 3>$null
    $twinRow = @(Get-CheckRows $twin | Where-Object { $_.id -eq 'twinhud' }) | Select-Object -First 1
    Add-Check '10 lane' 'without -Lane and -Key new-addon picks a lane and a key that the add-ons next to the target do not use' ($twinRow -and $fooRow -and [math]::Abs($twinRow.lane - $fooRow.lane) -ge 0.005 -and $twinRow.keys -ne $fooRow.keys -and [math]::Abs($twinRow.lane - $echoOptions.Lane) -ge 0.005)
    New-Item -ItemType Directory -Force -Path (Join-Path $target 'src\sub') | Out-Null
    $unit = "#include <zsdk/present.h>`n`n#if !defined(FOO_VARIANT)`n#error the define did not reach the unit`n#endif`n`nnamespace zfoo::sub`n{`n    int Variant()`n    {`n        return FOO_VARIANT;`n    }`n}`n"
    Write-CheckText (Join-Path $target 'src\sub\variant.cpp') $unit
    Import-Module (Join-Path $copy 'build\zsdk.psm1') -Force
    Invoke-ZsdkBuild -Root $target -Lock $table -Name 'ZoneFoo' -Define 'FOO' -Pure @('app.cpp', 'settings.cpp', 'lang_rows.cpp') -Libs @('ole32.lib') -Defines @('FOO_VARIANT=7')
    Add-Check 'build' 'a unit under src\sub is compiled' (Test-Path -LiteralPath (Join-Path $target 'build\obj\Release\addon\variant.obj'))
    $rsp = [System.IO.File]::ReadAllText((Join-Path $target 'build\obj\Release\link.rsp'), [System.Text.Encoding]::Unicode)
    Add-Check 'build' '-Libs reaches the linker' ($rsp.Contains('ole32.lib'))
    $message = Get-CheckRefusal { Invoke-ZsdkBuild -Root $target -Lock $table -Name 'ZoneFoo' -Define 'FOO' -Libs @('..\outside.lib') }
    Add-Check 'build' 'a library outside the add-on is refused' ($message -like '*outside the add-on folder*')
    $message = Get-CheckRefusal { Invoke-ZsdkBuild -Root $target -Lock $table -Name 'ZoneFoo' -Define 'FOO' -CompilerFlags @('/wd4100') }
    Add-Check 'build' 'a compiler flag off the allow-list is refused' ($message -like '*allow-list*')
    Write-CheckText (Join-Path $target 'src\sub\core.cpp') "#include <zsdk/present.h>`n"
    $message = Get-CheckRefusal { Invoke-ZsdkBuild -Root $target -Lock $table -Name 'ZoneFoo' -Define 'FOO' -Defines @('FOO_VARIANT=7') }
    Add-Check 'build' 'two units with one object name are refused' ($message -like '*share the object name*')
    $message = Get-CheckRefusal { Invoke-ZsdkBuild -Root $target -Lock $table -Name 'ZoneFoo' -Define 'FOO' -CompilerFlags @('/GL') }
    Add-Check '12 flags' 'the /GL compiler flag is refused with its reason' ($message -and $message.Contains('/GL'))
    $message = Get-CheckRefusal { Invoke-ZsdkBuild -Root $target -Lock $table -Name 'ZoneFoo' -Define 'FOO' -LinkFlags @('/LTCG') }
    Add-Check '12 flags' 'the /LTCG linker flag is refused with its reason' ($message -and $message.Contains('/LTCG'))
    $message = Get-CheckRefusal { Invoke-ZsdkBuild -Root $target -Lock $table -Name 'ZoneFoo' -Define 'FOO' -Tests -Exe (Join-Path $Workspace 'no-such-game.exe') }
    Add-Check '17 guards' 'make -Exe with a missing file is refused before the build' ($message -and $message.Contains('-Exe names no file'))
    $message = Get-CheckRefusal { Invoke-ZsdkBuild -Root $target -Lock $table -Name 'ZoneFoo' -Define 'FOO' -Tests -Exe (Join-Path $Workspace 'no-such-game.exe') -LiveExe }
    Add-Check '17 guards' 'make -Exe together with -LiveExe is refused' ($message -and $message.Contains('not both'))
    Remove-Item -LiteralPath (Join-Path $target 'src\sub') -Recurse -Force
    $probeSets = Join-Path $target 'src\probe_sets.h'
    Write-CheckText $probeSets "#pragma once`n`n#include <zsdk/present.h>`n`nnamespace zfoo`n{`n    inline constexpr zsdk::guard::Guard kProbeList[] = { { 0x1000u, `"48 8B`" } };`n    inline constexpr zsdk::guard::Set kProbeSet{ `"foo.probe`", &zsdk::guard::k61bc1e03, kProbeList, 1, zsdk::guard::Need::Bytes };`n}`n"
    $wanted = 'the guard set foo.probe (kProbeSet in src/probe_sets.h) is not listed in App.sets'
    $message = Get-CheckRefusal { & (Join-Path $target 'build\make.ps1') -Tests }
    $log = @(Get-Content -LiteralPath (Join-Path $target 'build\last-build.log') -Encoding UTF8)
    Add-Check '17 guards' 'a guard set declared in the sources but missing from .sets is a warning in a default build' (-not $message -and [bool]($log | Where-Object { $_.StartsWith('warning: guards: ' + $wanted) }) -and [bool]($log | Where-Object { $_ -match '^tests passed \d+ failed 0 ' }))
    Add-Check '8 identity' 'the identity suite warns about a sibling add-on that shares the toast lane and a default key' ([bool]($log | Where-Object { $_ -like 'warning: identity: toast lane * is also used by the add-on in EchoHud*' }) -and [bool]($log | Where-Object { $_ -like 'warning: identity: the key * is also a default key of the add-on in EchoHud' }))
    $message = Get-CheckRefusal { & (Join-Path $target 'build\make.ps1') -Tests -RequireExe }
    $log = @(Get-Content -LiteralPath (Join-Path $target 'build\last-build.log') -Encoding UTF8)
    Add-Check '17 guards' 'under -RequireExe a guard set missing from .sets fails the guards suite' ($message -and [bool]($log | Where-Object { $_.StartsWith('FAIL ' + $wanted) -and $_.Contains('-RequireExe') }))
    Add-Check '3 tests' 'a build whose tests fail removes its binary, so neither pack nor an install can ship it' ($message -and $message.Contains('the binary is removed') -and -not (Test-Path -LiteralPath $releaseFile) -and -not (Test-Path -LiteralPath $testedFile))
    Add-Check '6 suites' 'a failing generic suite points at its section of docs\RULES.md' ([bool]($log | Where-Object { $_ -like 'zsdk-guards: docs\RULES.md (docs\RULES-UA.md in Ukrainian), section 10, zsdk-guards,*' }))
    Remove-Item -LiteralPath $probeSets -Force

    $refused = Join-Path $Workspace 'refused'
    $base = @{}
    if (-not $inside) { $base['Release'] = $copy }
    $message = Get-CheckRefusal { & $newAddon @base -Name 'MyTemplateHud' -Target (Join-Path $refused 'a') 6>$null }
    Add-Check '10 name' 'a name containing template is refused' ($message -and $message.Contains('template') -and -not (Test-Path -LiteralPath (Join-Path $refused 'a')))
    $row = $rows | Where-Object { $_.id -ne 'template' } | Select-Object -First 1
    if ($row) {
        $message = Get-CheckRefusal { & $newAddon @base -Name $row.tag -Target (Join-Path $refused 'b') 6>$null }
        Add-Check '10 name' ('a name taken in the registry table is refused (' + $row.tag + ')') ($message -and $message.Contains('registry table'))
        $message = Get-CheckRefusal { & $newAddon @base -Name 'FreeHud' -Tag $row.tag -Target (Join-Path $refused 'c') 6>$null }
        Add-Check '10 name' ('a tag taken in the registry table is refused (' + $row.tag + ')') ($message -and $message.Contains('registry table'))
    }
    else { Add-Skip '10 name' 'a name taken in the registry table is refused' 'the release has no registry row besides the template' }
    $first = Get-CheckRefusal { & $newAddon @base -Name 'Version' -Target (Join-Path $refused 'd') 6>$null }
    $second = Get-CheckRefusal { & $newAddon @base -Name 'ZoneSdk' -Target (Join-Path $refused 'e') 6>$null }
    $third = Get-CheckRefusal { & $newAddon @base -Name 'Windows' -Target (Join-Path $refused 'f') 6>$null }
    Add-Check '10 name' 'names whose header, namespace or file would clash are refused (Version, ZoneSdk, Windows)' ($first -and $first.Contains('version.h') -and $second -and $second.Contains('zsdk') -and $third -and $third.Contains('windows.h'))
    $message = Get-CheckRefusal { & $newAddon @base -Name 'PadHud' -Key 'NUM5' -Target (Join-Path $refused 'g') 6>$null }
    Add-Check '10 key' 'a numpad key is refused' ($message -and $message.Contains('numpad'))
    $message = Get-CheckRefusal { & $newAddon @base -Name 'ZoneFoo' -Target $target 6>$null }
    Add-Check '10 name' 'an existing target is never overwritten' ($message -and $message.Contains('never overwritten'))
    $message = Get-CheckRefusal { & $newAddon @base -Name 'OleksaHud' -Author ([string][char]0x041E + [char]0x043B + [char]0x0435 + [char]0x043A + [char]0x0441 + [char]0x0430) -Target (Join-Path $refused 'h') 6>$null }
    Add-Check '10 author' 'a Cyrillic -Author is refused with the hint to write it in Latin letters' ($message -and $message.Contains('Latin letters') -and -not (Test-Path -LiteralPath (Join-Path $refused 'h')))
    $usage = Invoke-CheckChild 'Bypass' ('& ' + (Get-CheckQuoted $newAddon) + ' -Help; exit $LASTEXITCODE')
    $bare = Invoke-CheckChild 'Bypass' ('& ' + (Get-CheckQuoted $newAddon) + '; exit $LASTEXITCODE')
    Add-Check '12 usage' 'new-addon -Help prints its usage and exits 0, with no arguments it prints the usage and exits 1' ($usage.code -eq 0 -and $usage.text.Contains('usage: new-addon.cmd') -and $bare.code -eq 1 -and $bare.text.Contains('usage: new-addon.cmd'))
    $hostRun = Join-Path $tools 'fakehost\run.ps1'
    if (Test-Path -LiteralPath $hostRun) {
        $bare = Invoke-CheckChild 'Bypass' ('& ' + (Get-CheckQuoted $hostRun) + '; exit $LASTEXITCODE')
        Add-Check '12 usage' 'the fake host without -Addon prints its usage and exits 1' ($bare.code -eq 1 -and $bare.text.Contains('usage: fakehost.cmd'))
    }

    $pkgRoot = Join-Path $Workspace 'pkg\ZoneSDK'
    $pkgRelease = Join-Path $pkgRoot ('release\' + $v)
    Copy-CheckRelease $source $pkgRelease
    Write-CheckText (Join-Path $pkgRoot 'samples\ZoneSampleHud\src\app.cpp') (".row = zsdk::registry::Row{ .id = ""samplehud"", .tag = ""ZoneSampleTag"", .lane = 0.66f, .keys = ""ALT+H"" }, .registry = ""samplehud""`n")
    $message = Get-CheckRefusal { & $newAddon -Name 'ZoneSampleHud' -Target (Join-Path $refused 'i') -Release $pkgRelease 6>$null }
    $tagged = Get-CheckRefusal { & $newAddon -Name 'FreeHud' -Tag 'ZoneSampleTag' -Target (Join-Path $refused 'j') -Release $pkgRelease 6>$null }
    Add-Check '10 name' 'the name and the tag of a ZoneSDK sample in the package are refused' ($message -and $message.Contains('ZoneSDK sample') -and $tagged -and $tagged.Contains('ZoneSDK sample'))
    $pkgHud = Join-Path $pkgRoot 'addons\PkgHud'
    & $newAddon -Name 'PkgHud' -Target $pkgHud -Release $pkgRelease 6>$null 3>$null
    $where = & (Join-Path $pkgHud 'build\make.ps1') -Where
    Add-Check '6 lookup' 'an add-on inside the unpacked package finds release\<v> by the parent walk' ($where.Via -eq 'parent walk' -and $where.Release -eq $pkgRelease -and -not (Read-CheckLock (Join-Path $pkgHud 'build\zsdk.lock')).ContainsKey('home'))
    Add-Check '10 lane' 'an add-on made in a package picks no lane and no key of the samples there' (@(Get-CheckRows $pkgHud | Where-Object { [math]::Abs($_.lane - 0.66) -lt 0.005 -or $_.keys -eq 'ALT+H' }).Count -eq 0)
    Add-Check '6 lookup' 'make -Where gives the release, how it was found, the version, the manifest and the lock, shown as a list' ($where.Lock -eq (Join-Path $pkgHud 'build\zsdk.lock') -and @($where.PSObject.Properties).Count -ge 5)
    $homeHud = Join-Path $Workspace 'mods\HomeHud'
    & $newAddon -Name 'HomeHud' -Target $homeHud -Release $pkgRelease 6>$null 3>$null
    $homeLock = Read-CheckLock (Join-Path $homeHud 'build\zsdk.lock')
    Add-Check '10 lock' 'new-addon writes a relative home= when the parent walk cannot reach the release' ($homeLock['home'] -and -not [System.IO.Path]::IsPathRooted($homeLock['home']))
    $where = & (Join-Path $homeHud 'build\make.ps1') -Where
    Add-Check '6 lookup' 'an add-on outside the package finds it through home= in build\zsdk.lock' ($where.Via -like 'home=*' -and $where.Release -eq $pkgRelease)
    $rootHud = Join-Path $away 'RootHud'
    & $newAddon -Name 'RootHud' -Target $rootHud -Release $pkgRelease 6>$null 3>$null
    $rootLock = Join-Path $rootHud 'build\zsdk.lock'
    Write-CheckText $rootLock ('version=' + $v + "`nmanifest=" + $copyHash + "`n")
    $env:ZSDK_ROOT = $pkgRoot
    $where = & (Join-Path $rootHud 'build\make.ps1') -Where
    Add-Check '6 lookup' 'an add-on in %TEMP% finds the release through ZSDK_ROOT' ($where.Via -eq 'ZSDK_ROOT' -and $where.Release -eq $pkgRelease)
    Write-CheckText $rootLock ("version=9.9.9`nmanifest=" + $copyHash + "`n")
    $message = Get-CheckRefusal { & (Join-Path $rootHud 'build\make.ps1') -Where }
    $wantedPlaces = @((Join-Path $pkgRoot 'release\9.9.9'), (Join-Path $away 'RootHud\_sdk\release\9.9.9'), (Join-Path $away 'release\9.9.9'), 'has no home=')
    Add-Check '6 lookup' 'a missing release names every place it tried' ($message -and @($wantedPlaces | Where-Object { -not $message.Contains($_) }).Count -eq 0)
    Set-CheckPrivate 'ZSDK_ROOT' $null
    $decoyRoot = Join-Path $Workspace 'decoy'
    Write-CheckText (Join-Path $decoyRoot ('release\' + $v + '\MANIFEST.sha256')) "not a release`n"
    $decoyHud = Join-Path $decoyRoot 'DecoyHud'
    $options = @{ Name = 'DecoyHud'; Target = $decoyHud }
    if (-not $inside) { $options['Release'] = $copy }
    & $newAddon @options 6>$null 3>$null
    $where = & (Join-Path $decoyHud 'build\make.ps1') -Where
    Add-Check '6 lookup' 'the parent walk goes past a release folder whose manifest differs' ($where.Via -eq 'parent walk' -and $where.Release -eq $copy)

    $inRelease = Join-Path $Workspace ('inrel\release\' + $v)
    Write-CheckText (Join-Path $inRelease 'MANIFEST.sha256') "a release`n"
    $inMake = Join-Path $inRelease 'template\InRel\build\make.ps1'
    Write-CheckText $inMake ([System.IO.File]::ReadAllText((Join-Path $target 'build\make.ps1')))
    Write-CheckText (Join-Path $inRelease 'template\InRel\build\zsdk.lock') ('version=' + $v + "`nmanifest=" + $copyHash + "`n")
    $message = Get-CheckRefusal { & $inMake -Where }
    Add-Check '7 release' 'make refuses an add-on folder inside a release' ($message -and $message.Contains('inside the ZoneSDK release'))
    if ($inside) {
        $message = Get-CheckRefusal { & (Join-Path $copy 'template\ZoneTemplate\build\make.ps1') -Where }
        Add-Check '7 release' 'the starter add-on inside the release refuses to build in place' ($message -and $message.Contains('inside the ZoneSDK release'))
        $starter = Read-CheckLock (Join-Path $copy 'template\ZoneTemplate\build\zsdk.lock')
        Add-Check '7 release' 'the starter add-on inside the release carries a lock without a manifest' ($starter['version'] -eq $v -and -not $starter.ContainsKey('manifest'))
    }
    else {
        Add-Skip '7 release' 'the starter add-on inside the release refuses to build in place' 'runs only inside a release, use -Candidate'
        Add-Skip '7 release' 'the starter add-on inside the release carries a lock without a manifest' 'runs only inside a release, use -Candidate'
    }
    $noLock = Join-Path $Workspace 'nolock\NoLock\build'
    Write-CheckText (Join-Path $noLock 'make.ps1') ([System.IO.File]::ReadAllText((Join-Path $target 'build\make.ps1')))
    Write-CheckText (Join-Path $noLock 'zsdk.lock') ('version=' + $v + "`n")
    $message = Get-CheckRefusal { & (Join-Path $noLock 'make.ps1') -Where }
    Add-Check '7 release' 'a lock without a manifest is refused with a pointer to new-addon' ($message -and $message.Contains('new-addon'))

    $motwRoot = Join-Path $Workspace 'motw'
    $motwRelease = Join-Path $motwRoot ('_sdk\release\' + $v)
    Copy-CheckRelease $source $motwRelease
    Get-ChildItem -LiteralPath $motwRelease -Recurse -File | ForEach-Object { $_.IsReadOnly = $false }
    $fixtureNotice = -not (Test-Path -LiteralPath (Join-Path $motwRelease 'NOTICE-ADDON.txt'))
    if ($fixtureNotice) {
        Write-CheckText (Join-Path $motwRelease 'NOTICE-ADDON.txt') "fixture notice written by check-scaffold, the release under test ships none`r`n"
        [void](Write-CheckManifest $motwRelease)
    }
    $hud = Join-Path $motwRoot 'MyHud'
    & $newAddon -Name 'MyHud' -Target $hud -Author 'Jane Doe' -Tag 'JaneMyHud' -Release $motwRelease
    $hudText = @{}
    foreach ($item in @(Get-ChildItem -LiteralPath $hud -Recurse -File)) { $hudText[$item.FullName.Substring($hud.Length + 1)] = [System.IO.File]::ReadAllText($item.FullName) }
    $named = (Test-Path -LiteralPath (Join-Path $hud 'src\myhud.h')) -and $hudText['src\myhud.h'].Contains('namespace zmyhud') -and $hudText['src\version.h'].Contains('#define MYHUD_VERSION_MAJOR') -and $hudText['tests\zsdk.cfg'].Contains('define=MYHUD') -and $hudText['tests\zsdk.cfg'].Contains('name=MyHud') -and $hudText['build\make.ps1'].Contains("-Name 'MyHud' -Define 'MYHUD'")
    Add-Check '10 name' 'MyHud gives define MYHUD, namespace zmyhud and src\myhud.h' $named
    $leftover = @($hudText.Keys | Where-Object { $_ -match '(?i)template' -or $hudText[$_] -match '(?i)template(?!\s*<)' })
    Add-Check '10 name' 'no file of MyHud keeps the starter name' ($leftover.Count -eq 0)
    $own = @(Get-CheckRows $hud | Where-Object { $_.id -eq 'myhud' })
    Add-Check '10 tag' '-Tag reaches the identity and the registry row' ($own.Count -eq 1 -and $own[0].tag -eq 'JaneMyHud' -and $hudText['src\app.cpp'] -match 'Identity\{\s*"MyHud"\s*,\s*"JaneMyHud"|\.tag\s*=\s*"JaneMyHud"|kTag\s*=\s*"JaneMyHud"')
    if ($own.Count -eq 1) {
        $usedLanes = @($rows | Where-Object { $_.lane -gt 0 -and [math]::Abs($_.lane - $own[0].lane) -lt 0.005 })
        $usedKeys = @($rows | Where-Object { ($_.keys -split '\|') -contains $own[0].keys })
        Add-Check '10 lane' ('a free lane and a free key are picked: ' + $own[0].lane.ToString('0.00', $invariant) + ', ' + $own[0].keys) ($own[0].lane -gt 0 -and $usedLanes.Count -eq 0 -and $usedKeys.Count -eq 0 -and $own[0].keys -notmatch 'NUM')
    }
    else { Add-Check '10 lane' 'a free lane and a free key are picked' $false }
    foreach ($item in @(Get-ChildItem -LiteralPath $motwRoot -Recurse -File)) {
        $item.IsReadOnly = $false
        Set-Content -LiteralPath $item.FullName -Stream 'Zone.Identifier' -Value "[ZoneTransfer]`r`nZoneId=3"
    }
    $forced = (Get-ExecutionPolicy -Scope MachinePolicy) -ne 'Undefined' -or (Get-ExecutionPolicy -Scope UserPolicy) -ne 'Undefined'
    $probe = 'try { & ' + (Get-CheckQuoted (Join-Path $hud 'build\make.ps1')) + ' -Where | Out-String -Width 4096 | Write-Output; exit 0 } catch { Write-Output (''refused: '' + $_.Exception.Message); exit 3 }'
    if ($forced) {
        Add-Skip '5 launchers' 'the plain make.ps1 is refused under the Restricted policy' 'a Group Policy sets the execution policy'
        Add-Skip '5 launchers' 'the plain make.ps1 with Mark-of-the-Web is refused under RemoteSigned' 'a Group Policy sets the execution policy'
    }
    else {
        $run = Invoke-CheckChild 'Restricted' $probe
        Add-Check '5 launchers' 'the plain make.ps1 is refused under the Restricted policy' ($run.code -eq 3)
        $run = Invoke-CheckChild 'RemoteSigned' $probe
        Add-Check '5 launchers' 'the plain make.ps1 with Mark-of-the-Web is refused under RemoteSigned' ($run.code -eq 3)
    }
    $run = Invoke-CheckChild 'Restricted' ('& ' + (Get-CheckQuoted (Join-Path $hud 'test.cmd')) + '; exit $LASTEXITCODE')
    $hudLog = @(Get-Content -LiteralPath (Join-Path $hud 'build\last-build.log') -Encoding UTF8 -ErrorAction SilentlyContinue)
    Add-Check '5 launchers' 'test.cmd builds and tests MyHud under a Restricted parent policy with Mark-of-the-Web on every file' ($run.code -eq 0 -and [bool]($hudLog | Where-Object { $_ -match '^tests passed \d+ failed 0 ' }) -and [bool]($hudLog | Where-Object { $_ -eq 'warnings 0' }))
    $binary = Join-Path $hud 'bin\MyHud.addon64'
    if (Test-Path -LiteralPath $binary) {
        $info = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($binary)
        Add-Check '10 author' '-Author reaches CompanyName and LegalCopyright of the built version resource' ($info.CompanyName -eq 'Jane Doe' -and $info.LegalCopyright -like '*Jane Doe*')
        $python = @(Get-Command python.exe -ErrorAction SilentlyContinue | Where-Object { $_.Source -notmatch '\\WindowsApps\\' }) | Select-Object -First 1
        $peFacts = Join-Path $tree 'tools\pe\pe_facts.py'
        if ($python -and (Test-Path -LiteralPath $peFacts)) {
            $savedOut = $null
            try { $savedOut = [Console]::OutputEncoding; [Console]::OutputEncoding = $utf8 } catch { $savedOut = $null }
            try { $json = (@(& cmd /d /c ('"' + $python.Source + '" -I -B -X utf8 "' + $peFacts + '" hud "' + $binary + '" 2>&1')) | ForEach-Object { [string]$_ }) -join "`n" }
            finally { if ($savedOut) { try { [Console]::OutputEncoding = $savedOut } catch { } } }
            $facts = $null
            try { $facts = $json | ConvertFrom-Json } catch { $facts = $null }
            Add-Check '10 author' 'pe_facts reads the author back from the version resource' ($facts -and $facts.hud.version.strings.CompanyName -eq 'Jane Doe')
        }
        else { Add-Skip '10 author' 'pe_facts reads the author back from the version resource' 'no Python 3 or no tools\pe\pe_facts.py' }
        $packVersion = '{0}.{1}.{2}' -f $info.FileMajorPart, $info.FileMinorPart, $info.FileBuildPart
        $zip = Join-Path $hud ('dist\MyHud-' + $packVersion + '.zip')
        $once = Invoke-CheckChild 'Restricted' ('& ' + (Get-CheckQuoted (Join-Path $hud 'pack.cmd')) + '; exit $LASTEXITCODE')
        $firstSha = if (Test-Path -LiteralPath $zip) { Get-CheckSha $zip } else { '' }
        $twice = Invoke-CheckChild 'Restricted' ('& ' + (Get-CheckQuoted (Join-Path $hud 'pack.cmd')) + '; exit $LASTEXITCODE')
        $secondSha = if (Test-Path -LiteralPath $zip) { Get-CheckSha $zip } else { '' }
        Add-Check '21 pack' ('pack.cmd writes dist\MyHud-' + $packVersion + '.zip and packing twice gives the same sha256' + $(if ($fixtureNotice) { ' (fixture NOTICE-ADDON.txt)' } else { '' })) ($once.code -eq 0 -and $twice.code -eq 0 -and $firstSha -and $firstSha -eq $secondSha)
        if ($firstSha) {
            Add-Type -AssemblyName System.IO.Compression
            $stream = [System.IO.File]::OpenRead($zip)
            $listing = @()
            $readme = @{}
            $sums = ''
            try {
                $archive = New-Object System.IO.Compression.ZipArchive ($stream, [System.IO.Compression.ZipArchiveMode]::Read)
                foreach ($entry in $archive.Entries) {
                    $listing += $entry.FullName
                    if ($entry.FullName -like 'README-*.txt' -or $entry.FullName -eq 'SHA256.txt') {
                        $reader = New-Object System.IO.StreamReader ($entry.Open(), [System.Text.Encoding]::UTF8)
                        try { $body = $reader.ReadToEnd() } finally { $reader.Dispose() }
                        if ($entry.FullName -eq 'SHA256.txt') { $sums = $body } else { $readme[$entry.FullName] = $body }
                    }
                }
                $archive.Dispose()
            }
            finally { $stream.Dispose() }
            $expected = @('NOTICE-ADDON.txt', 'README-EN.txt', 'README-UA.txt', 'SHA256.txt', 'Stalker2/Binaries/Win64/MyHud.addon64')
            Add-Check '21 pack' 'the zip holds exactly the add-on, both readmes, NOTICE-ADDON.txt and SHA256.txt, sorted' (($listing -join '|') -ceq ($expected -join '|'))
            $russian = [string][char]0x044B + [char]0x044D + [char]0x044A + [char]0x0451
            $filled = $true
            foreach ($key in 'README-EN.txt', 'README-UA.txt') {
                $body = [string]$readme[$key]
                $filled = $filled -and $body.Contains('MyHud ' + $packVersion) -and $body.Contains('Jane Doe') -and -not $body.Contains('{{') -and $body.IndexOfAny($russian.ToCharArray()) -lt 0
            }
            Add-Check '21 pack' 'both readmes are filled (name, version, author) and carry no Russian-only letters' $filled
            Add-Check '21 pack' 'SHA256.txt lists the add-on with its sha256' ($sums.Contains((Get-CheckSha $binary) + '  Stalker2/Binaries/Win64/MyHud.addon64') -and (Test-Path -LiteralPath (Join-Path $hud ('dist\MyHud-' + $packVersion + '-SHA256.txt'))))
        }
        $good = [System.IO.File]::ReadAllBytes($binary)
        $flagged = [byte[]]$good.Clone()
        $at = -1
        for ($i = [Array]::IndexOf($flagged, [byte]0xBD); $i -ge 0 -and $i -lt $flagged.Length - 32; $i = [Array]::IndexOf($flagged, [byte]0xBD, $i + 1)) {
            if ($flagged[$i + 1] -eq 0x04 -and $flagged[$i + 2] -eq 0xEF -and $flagged[$i + 3] -eq 0xFE) { $at = $i; break }
        }
        if ($at -ge 0) {
            $flagged[$at + 28] = $flagged[$at + 28] -bor 1
            [System.IO.File]::WriteAllBytes($binary, $flagged)
            $run = Invoke-CheckChild 'Bypass' ('try { & ' + (Get-CheckQuoted (Join-Path $hud 'build\pack.ps1')) + '; exit 0 } catch { Write-Output (''refused: '' + $_.Exception.Message); exit 3 }')
            [System.IO.File]::WriteAllBytes($binary, $good)
            Add-Check '21 pack' 'pack refuses a binary whose version resource carries VS_FF_DEBUG' ($run.code -eq 3 -and $run.text.Contains('Debug'))
        }
        else { Add-Check '21 pack' 'pack refuses a binary whose version resource carries VS_FF_DEBUG' $false }
    }
    else {
        Add-Check '10 author' '-Author reaches CompanyName and LegalCopyright of the built version resource' $false
        Add-Check '21 pack' 'pack.cmd writes a deterministic zip' $false
    }

    if (Test-Path -LiteralPath (Join-Path $copy 'NOTICE-ADDON.txt')) {
        Add-Check '2 notice' 'the release ships NOTICE-ADDON.txt for add-on zips' $true
        Add-Skip '21 pack' 'pack refuses a release without NOTICE-ADDON.txt' 'the release under test ships it'
    }
    else {
        Add-Check '2 notice' 'the release ships NOTICE-ADDON.txt for add-on zips' $false
        $message = Get-CheckRefusal { & (Join-Path $target 'build\pack.ps1') }
        Add-Check '21 pack' 'pack refuses a release without NOTICE-ADDON.txt' ($message -and $message.Contains('NOTICE-ADDON.txt'))
    }

    $busy = Join-Path $Workspace 'ZoneBusy'
    $options = @{ Name = 'ZoneBusy'; Target = $busy }
    if (-not $inside) { $options['Release'] = $copy }
    & $newAddon @options 6>$null 3>$null
    $hold = [System.IO.File]::Open((Join-Path $busy 'build\.zsdk-build.lock'), [System.IO.FileMode]::OpenOrCreate, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
    $busyOut = Join-Path $Workspace 'child\busy'
    New-Item -ItemType Directory -Force -Path (Split-Path $busyOut -Parent) | Out-Null
    $proc = Start-Process -FilePath 'powershell.exe' -ArgumentList ('-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' + (Join-Path $busy 'build\make.ps1') + '"') -NoNewWindow -PassThru -RedirectStandardOutput ($busyOut + '.out') -RedirectStandardError ($busyOut + '.err')
    $null = $proc.Handle
    $waited = $true
    $until = [DateTime]::Now.AddSeconds(20)
    while ([DateTime]::Now -lt $until) {
        if ($proc.HasExited -or (Test-Path -LiteralPath (Join-Path $busy 'build\obj\Release'))) { $waited = $false; break }
        Start-Sleep -Milliseconds 500
    }
    $hold.Dispose()
    if (-not $waited -and -not $proc.HasExited) { & cmd /d /c ('taskkill /T /F /PID ' + $proc.Id + ' >nul 2>&1') }
    $proc.WaitForExit()
    Add-Check '13 build lock' 'a second build waits while build\.zsdk-build.lock is held, then finishes' ($waited -and $proc.ExitCode -eq 0)

    $isolated = Join-Path ([System.IO.Path]::GetTempPath()) ('zsdk-vsw-' + $id)
    $isolatedRelease = Join-Path $isolated ('release\' + $v)
    $toolchainAbove = $false
    for ($walk = $isolatedRelease; $walk; $walk = Split-Path $walk -Parent) { if (Test-Path -LiteralPath (Join-Path $walk '_toolchain\vcvars-winsdk.bat')) { $toolchainAbove = $true } }
    if ($toolchainAbove) {
        Add-Skip '3 toolchain' 'ZSDK_VSWHERE selects the Build Tools it returns' 'a _toolchain wrapper lies above the temp folder'
    }
    else {
        Copy-CheckRelease $source $isolatedRelease
        $plain = Get-CheckAscii $isolated
    }
    if (-not $toolchainAbove -and -not $plain) {
        Add-Skip '3 toolchain' 'ZSDK_VSWHERE selects the Build Tools it returns' 'the temp folder has letters a batch file cannot name and no 8.3 short name'
        Remove-Item -LiteralPath $isolated -Recurse -Force -ErrorAction SilentlyContinue
    }
    elseif (-not $toolchainAbove) {
        $isolated = $plain
        $isolatedRelease = Join-Path $isolated ('release\' + $v)
        $fakeVs = Join-Path $isolated 'BuildTools'
        $fakeBin = Join-Path $fakeVs 'bin'
        foreach ($tool in 'cl.exe', 'link.exe', 'dumpbin.exe') { Write-CheckText (Join-Path $fakeBin $tool) '' }
        $vcvars = Join-Path $fakeVs 'VC\Auxiliary\Build\vcvars64.bat'
        Write-CheckText $vcvars ("@echo off`r`nset `"PATH=" + $fakeBin + ";%PATH%`"`r`nset `"VCToolsVersion=14.50.35717`"`r`nexit /b 0`r`n")
        Write-CheckText (Join-Path $fakeVs 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt') "14.50.35717`r`n"
        $stub = Join-Path $isolated 'vswhere.cmd'
        $argsFile = Join-Path $isolated 'vswhere-args.txt'
        $stubLines = @('@echo off', 'setlocal', ('>>"' + $argsFile + '" echo %*'), 'set "want="', 'set "json="', ':next', 'if "%~1"=="" goto done', 'if /i "%~1"=="-property" set "want=%~2"', 'if /i "%~1"=="-format" set "json=%~2"', 'shift', 'goto next', ':done',
            ('if /i "%want%"=="installationPath" echo ' + $fakeVs), 'if /i "%want%"=="installationVersion" echo 18.0.11111.16', 'if /i "%want%"=="catalog_productDisplayVersion" echo 18.0.0', 'if /i "%want%"=="displayName" echo Visual Studio Build Tools 2026',
            ('if "%want%"=="" if /i "%json%"=="json" echo [{"installationPath":"' + $fakeVs.Replace('\', '\\') + '","installationVersion":"18.0.11111.16","displayName":"Visual Studio Build Tools 2026","catalog":{"productDisplayVersion":"18.0.0"}}]'), 'exit /b 0')
        Write-CheckText $stub (($stubLines -join "`r`n") + "`r`n")
        $system = Join-Path $env:SystemRoot 'System32'
        $script = '$ErrorActionPreference = ''Stop''; Remove-Item Env:\ZSDK_VCVARS -ErrorAction SilentlyContinue; Remove-Item Env:\ZSDK_VCVARS_VER -ErrorAction SilentlyContinue; $env:ZSDK_VSWHERE = ' + (Get-CheckQuoted $stub) + '; $env:PATH = ' + (Get-CheckQuoted ($system + ';' + $env:SystemRoot + ';' + (Join-Path $system 'WindowsPowerShell\v1.0'))) + '; Import-Module ' + (Get-CheckQuoted (Join-Path $isolatedRelease 'build\zsdk.psm1')) + ' -Force; try { Write-Output (''wrapper='' + (Resolve-ZsdkWrapper)) } catch { Write-Output (''wrapper-error='' + $_.Exception.Message) }; try { Import-ZsdkToolchain; Write-Output ''imported'' } catch { Write-Output (''toolchain-error='' + $_.Exception.Message) }'
        $run = Invoke-CheckChild 'Bypass' $script
        $asked = if (Test-Path -LiteralPath $argsFile) { [System.IO.File]::ReadAllText($argsFile) } else { '' }
        Add-Check '3 toolchain' 'ZSDK_VSWHERE selects the Build Tools it returns' ($run.text.Contains('wrapper=' + $vcvars))
        Add-Check '3 toolchain' 'vswhere is asked for VS 17.10 up to 19.0 with the C++ tools and no prerelease' ($asked.Contains('-version') -and $asked.Contains('17.10') -and $asked.Contains('19.0') -and $asked.Contains('Microsoft.VisualStudio.Component.VC.Tools.x86.x64') -and -not $asked.Contains('-prerelease'))
        $failure = [regex]::Match($run.text, 'toolchain-error=([^\r\n]*)').Groups[1].Value
        Add-Check '3 toolchain' 'a missing rc.exe is reported as a missing Windows SDK with an install hint' ($failure.Contains('rc.exe') -and $failure.Contains('Windows SDK'))
        Remove-Item -LiteralPath $isolated -Recurse -Force -ErrorAction SilentlyContinue
    }

    $doctor = Join-Path $tree 'tools\doctor.ps1'
    if (Test-Path -LiteralPath $doctor) {
        $stubDir = Join-Path $Workspace 'stub'
        $win64 = Join-Path (Join-Path (Join-Path (Join-Path $Workspace 'game') 'Stalker2') 'Binaries') 'Win64'
        New-Item -ItemType Directory -Force -Path $stubDir, $win64 | Out-Null
        $made = @{}
        foreach ($pair in @(@('new', '6,8,0,0', '6.8.0.0'), @('old', '6,7,3,0', '6.7.3.0'))) {
            $rc = Join-Path $stubDir ($pair[0] + '.rc')
            $rcText = "#include <winver.h>`r`n1 VERSIONINFO`r`nFILEVERSION " + $pair[1] + "`r`nPRODUCTVERSION " + $pair[1] + "`r`nFILEFLAGSMASK 0x3fL`r`nFILEFLAGS 0x0L`r`nFILEOS VOS_NT_WINDOWS32`r`nFILETYPE VFT_DLL`r`nFILESUBTYPE 0`r`nBEGIN`r`n BLOCK `"StringFileInfo`"`r`n BEGIN`r`n  BLOCK `"040904B0`"`r`n  BEGIN`r`n   VALUE `"FileDescription`", `"crosire's ReShade post-processing injector for 64-bit\0`"`r`n   VALUE `"FileVersion`", `"" + $pair[2] + "\0`"`r`n   VALUE `"ProductName`", `"ReShade\0`"`r`n   VALUE `"ProductVersion`", `"" + $pair[2] + "\0`"`r`n  END`r`n END`r`n BLOCK `"VarFileInfo`"`r`n BEGIN`r`n  VALUE `"Translation`", 0x0409, 1200`r`n END`r`nEND`r`n2 RCDATA { `"Searching for add-ons (*.addon, *.addon64) ReShadeRegisterAddon\0`" }`r`n"
            Write-CheckText $rc $rcText
            $res = Join-Path $stubDir ($pair[0] + '.res')
            $dll = Join-Path $stubDir ($pair[0] + '.dll')
            & cmd /d /c ('rc /nologo /fo "' + $res + '" "' + $rc + '" >nul 2>&1 && link /nologo /DLL /NOENTRY /MACHINE:X64 /OUT:"' + $dll + '" "' + $res + '" >nul 2>&1')
            $made[$pair[0]] = $dll
        }
        if ((Test-Path -LiteralPath $made['new']) -and (Test-Path -LiteralPath $made['old'])) {
            Copy-Item -LiteralPath $made['new'] -Destination (Join-Path $win64 'dxgi.dll') -Force
            Write-CheckText (Join-Path $win64 'ReShade.ini') "[ADDON]`r`nDisabledAddons=Generic Depth,MyHud@MyHud.addon64`r`n[GENERAL]`r`n"
            Write-CheckText (Join-Path $win64 'Stalker2-Win64-Shipping.exe') 'not the game'
            Write-CheckText (Join-Path $win64 'ReShade.log') ("00:00:00:000 [1] | INFO  | Registered add-on `"ZoneFoo`" v0.1.0.0 using ReShade API version 20.`r`n")
            $doctorRun = 'try { & ' + (Get-CheckQuoted $doctor) + ' -Game ' + (Get-CheckQuoted (Join-Path $Workspace 'game')) + ' -Addon MyHud; exit $LASTEXITCODE } catch { Write-Output (''doctor-error: '' + $_.Exception.Message); exit 4 }'
            $run = Invoke-CheckChild 'Bypass' $doctorRun
            Add-Check '19 doctor' 'doctor reads ReShade 6.8.0 and its add-on loader in a fake game folder' ($run.text -match 'ok\s+ReShade 6\.8\.0 \(dxgi\.dll\), add-on loader present')
            Add-Check '19 doctor' 'doctor lists DisabledAddons and flags a disabled add-on' ($run.text.Contains('MyHud@MyHud.addon64') -and $run.text -match 'FAIL\s+MyHud is disabled in ReShade\.ini' -and $run.code -eq 1)
            Add-Check '19 doctor' 'doctor reports an unknown game exe sha' ($run.text -match 'warn\s+game build [0-9a-f]{8} is unknown')
            Copy-Item -LiteralPath $made['old'] -Destination (Join-Path $win64 'dxgi.dll') -Force
            $run = Invoke-CheckChild 'Bypass' $doctorRun
            Add-Check '19 doctor' 'doctor flags ReShade older than 6.8' ($run.text -match 'FAIL\s+ReShade 6\.7\.3 \(dxgi\.dll\) is older than 6\.8')
        }
        else { Add-Check '19 doctor' 'the stub ReShade dlls for the doctor are built' $false }
    }
    else { Add-Skip '19 doctor' 'doctor reads a fake game folder' 'tools\doctor.ps1 is missing' }

    if ($inside) {
        $listed = [System.IO.File]::ReadAllText((Join-Path $copy 'MANIFEST.sha256'))
        $parts = @('tools\new-addon.ps1', 'tools\check-scaffold.ps1', 'tools\doctor.ps1', 'template\ZoneTemplate\build\make.ps1', 'template\ZoneTemplate\build\pack.ps1', 'template\ZoneTemplate\build.cmd', 'template\ZoneTemplate\dist\README-EN.txt')
        Add-Check '56 release' 'the release manifest lists the tools and the starter add-on' (@($parts | Where-Object { -not $listed.Contains('  ' + $_) }).Count -eq 0)
    }
    else { Add-Skip '56 release' 'the release manifest lists the tools and the starter add-on' 'runs only inside a release, use -Candidate' }
    Import-Module (Join-Path $copy 'build\zsdk.psm1') -Force
    $changed = @(@($copy, $pkgRelease, $source) | Where-Object { Get-CheckRefusal ([scriptblock]::Create('Test-ZsdkManifest ' + (Get-CheckQuoted $_))) })
    Add-Check '7 release' 'nothing was written into the release under test or its copies' ($changed.Count -eq 0)
}
catch { $stopped = $_ }
finally {
    foreach ($name in $saved.Keys) { Set-CheckPrivate $name $saved[$name] }
    if ($away -and (Test-Path -LiteralPath $away)) { Remove-Item -LiteralPath $away -Recurse -Force -ErrorAction SilentlyContinue }
}

function Clear-CheckWorkspace {
    if (-not $ownWorkspace -or -not $Workspace -or -not (Test-Path -LiteralPath $Workspace)) { return }
    Remove-Item -LiteralPath $Workspace -Recurse -Force -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $Workspace) { Write-Host ('the workspace could not be removed completely, delete it by hand: ' + $Workspace) } else { Write-Host ('workspace removed after the pass: ' + $Workspace) }
}

if ($Candidate) {
    if ($stopped) { throw $stopped }
    if ($passed) { Clear-CheckWorkspace }
    return
}
$failed = 0
$skipped = 0
foreach ($one in $results) {
    $label = if ($one.area) { '[' + $one.area + '] ' } else { '' }
    if ($one.state -eq 'skip') { $skipped++; Write-Host ('skip: {0}{1} ({2})' -f $label, $one.name, $one.why) }
    else { Write-Host ('{0}: {1}{2}' -f $one.state, $label, $one.name) }
    if ($one.state -eq 'FAIL') { $failed++ }
}
$counted = $results.Count - $skipped
Write-Host ('scaffold check: {0} of {1} passed, {2} skipped, workspace {3}' -f ($counted - $failed), $counted, $skipped, $Workspace)
if ($stopped) {
    Write-Host ('scaffold check stopped early: ' + $stopped.Exception.Message)
    throw $stopped
}
if ($failed -gt 0) { throw ('scaffold check failed: ' + $failed + ', the workspace is kept for a look: ' + $Workspace) }
Clear-CheckWorkspace
