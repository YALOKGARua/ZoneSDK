[CmdletBinding()]
param(
    [string]$Name,
    [string]$Target,
    [string]$Define,
    [string]$Author,
    [string]$Tag,
    [double]$Lane = -1,
    [string]$Key,
    [string]$Release,
    [switch]$Help
)

$ErrorActionPreference = 'Stop'
if ($Help -or -not $Name -or -not $Target) {
    Write-Host 'usage: new-addon.cmd -Name <AddonName> -Target <new folder> [-Author "<your name>"] [-Tag <tag>] [-Lane <0.06..0.95 or 0>] [-Key <chord>] [-Define <MACRO>] [-Release <release folder>]'
    Write-Host '  -Name     2 to 40 ASCII letters and digits starting with a letter, for example MyHud; Zone in front is only a suggestion'
    Write-Host '  -Target   the folder of the new add-on, it must not exist yet'
    Write-Host '  -Author   your name in Latin letters, for the version resource and the player readme'
    Write-Host '  -Tag      the unique name of the game-thread message and the mutex, the name by default; put your author name in front'
    Write-Host '  -Lane     the toast lane as a fraction of the screen height, 0 for no toasts; a free one by default'
    Write-Host '  -Key      the default key, for example ALT+K; a free ALT chord by default'
    Write-Host '  -Define   the C++ macro prefix, the upper-case name by default'
    Write-Host '  -Release  the ZoneSDK release folder to pin, the release new-addon runs from by default'
    Write-Host 'example: new-addon.cmd -Name MyHud -Target ..\MyHud -Author "Your Name" -Tag YourNameMyHud'
    if ($Help) { exit 0 }
    exit 1
}
$utf8 = New-Object System.Text.UTF8Encoding($false)
$invariant = [System.Globalization.CultureInfo]::InvariantCulture
$tree = Split-Path $PSScriptRoot -Parent
$inside = Test-Path -LiteralPath (Join-Path $tree 'MANIFEST.sha256')
$script:Named = @{ INSERT = 0x2D; DELETE = 0x2E; HOME = 0x24; END = 0x23; PAGEUP = 0x21; PAGEDOWN = 0x22; ENTER = 0x0D; SPACE = 0x20; BACKSPACE = 0x08; UP = 0x26; DOWN = 0x28; LEFT = 0x25; RIGHT = 0x27; CAPSLOCK = 0x14; SCROLLLOCK = 0x91; PAUSE = 0x13; OEM102 = 0xE2 }
$script:Symbols = @{ '.' = 0xBE; ',' = 0xBC; '-' = 0xBD; '=' = 0xBB; '[' = 0xDB; ']' = 0xDD; ';' = 0xBA; "'" = 0xDE; '/' = 0xBF; '\' = 0xDC; '`' = 0xC0 }
$script:Headers = @('windows', 'winver', 'winuser', 'winbase', 'windef', 'winnt', 'winerror', 'winreg', 'winsock', 'winsock2', 'ws2tcpip', 'shellapi', 'shlobj', 'shlwapi', 'objbase', 'combaseapi', 'unknwn', 'ole2', 'oleauto', 'commctrl', 'mmsystem', 'psapi', 'tlhelp32', 'dbghelp', 'bcrypt', 'wincrypt', 'strsafe', 'tchar', 'dxgi', 'd3d11', 'd3d12', 'd3dcompiler', 'dinput', 'xinput', 'intrin', 'immintrin', 'string', 'stdio', 'stdlib', 'math', 'time', 'memory', 'malloc', 'assert', 'ctype', 'errno', 'float', 'limits', 'locale', 'setjmp', 'signal', 'stdarg', 'stddef', 'stdint', 'inttypes', 'wchar', 'wctype', 'uchar', 'fenv', 'complex', 'process', 'io', 'direct', 'conio', 'fcntl', 'share', 'search', 'new', 'sal', 'excpt', 'crtdbg', 'vadefs', 'mbstring', 'resource', 'imgui', 'imconfig', 'imgui_internal', 'reshade', 'reshade_api')

function Get-ZsdkTreeVersion([string]$root) {
    $file = Join-Path $root 'include\zsdk\version.h'
    if (-not (Test-Path -LiteralPath $file)) { throw ('not a ZoneSDK folder, include\zsdk\version.h is missing: ' + $root) }
    $text = [System.IO.File]::ReadAllText($file)
    $parts = foreach ($part in 'MAJOR', 'MINOR', 'PATCH') { [regex]::Match($text, 'ZSDK_VERSION_' + $part + ' (\d+)').Groups[1].Value }
    return ($parts -join '.')
}

function Get-ZsdkRows([string]$root) {
    $rows = New-Object System.Collections.Generic.List[object]
    $src = Join-Path $root 'src'
    if (-not (Test-Path -LiteralPath $src)) { return $rows }
    $patterns = @(
        '\{\s*"([a-z0-9_]+)"\s*,\s*"([A-Za-z0-9]+)"\s*,\s*([0-9]*\.?[0-9]+)f\s*,\s*"((?:[^"\\]|\\.)*)"\s*\}',
        '\{\s*\.id\s*=\s*"([a-z0-9_]+)"\s*,\s*\.tag\s*=\s*"([A-Za-z0-9]+)"\s*,\s*\.lane\s*=\s*([0-9]*\.?[0-9]+)f\s*,\s*\.keys\s*=\s*"((?:[^"\\]|\\.)*)"\s*\}')
    foreach ($file in @(Get-ChildItem -LiteralPath $src -Recurse -File -Include '*.cpp', '*.h', '*.inc' | Sort-Object FullName)) {
        $text = [System.IO.File]::ReadAllText($file.FullName)
        if ($text -notmatch 'registry') { continue }
        foreach ($pattern in $patterns) {
            foreach ($m in [regex]::Matches($text, $pattern)) {
                $rows.Add([pscustomobject]@{ id = $m.Groups[1].Value; tag = $m.Groups[2].Value; lane = [double]::Parse($m.Groups[3].Value, $invariant); keys = $m.Groups[4].Value.Replace('\\', '\') })
            }
        }
    }
    return $rows
}

function ConvertTo-ZsdkChord([string]$text) {
    $rest = $text.Trim().ToUpperInvariant()
    $mods = 0
    while ($true) {
        if ($rest.StartsWith('CTRL+') -and $rest.Length -gt 5) { $mods = $mods -bor 1; $rest = $rest.Substring(5) }
        elseif ($rest.StartsWith('CONTROL+') -and $rest.Length -gt 8) { $mods = $mods -bor 1; $rest = $rest.Substring(8) }
        elseif ($rest.StartsWith('ALT+') -and $rest.Length -gt 4) { $mods = $mods -bor 4; $rest = $rest.Substring(4) }
        elseif ($rest.StartsWith('SHIFT+') -and $rest.Length -gt 6) { $mods = $mods -bor 2; $rest = $rest.Substring(6) }
        else { break }
    }
    $code = 0
    if ($rest -cmatch '^[A-Z0-9]$') { $code = [int][char]$rest }
    elseif ($rest.Length -eq 1 -and $script:Symbols.ContainsKey($rest)) { $code = $script:Symbols[$rest] }
    elseif ($script:Named.ContainsKey($rest)) { $code = $script:Named[$rest] }
    elseif ($rest -match '^F([1-9]|1[0-9]|2[0-4])$') { $code = 0x6F + [int]$Matches[1] }
    if (-not $code) { return [pscustomobject]@{ code = 0; numpad = $rest.StartsWith('NUM'); name = $rest } }
    $name = ''
    if ($mods -band 1) { $name += 'CTRL+' }
    if ($mods -band 4) { $name += 'ALT+' }
    if ($mods -band 2) { $name += 'SHIFT+' }
    $flags = @()
    if ($mods -band 1) { $flags += 'zsdk::keys::kCtrl' }
    if ($mods -band 2) { $flags += 'zsdk::keys::kShift' }
    if ($mods -band 4) { $flags += 'zsdk::keys::kAlt' }
    $modsText = if ($flags.Count -eq 0) { '0' } else { $flags -join ' | ' }
    $keyText = if ($rest -cmatch '^[A-Z0-9]$') { "'" + $rest + "'" } else { '0x{0:X2}' -f $code }
    $game = $mods -eq 0 -and ($rest -cmatch '^[WASDERFCQVZXBMJPITLHUYOKNG1-8=]$' -or $rest -eq 'SPACE' -or $rest -eq 'F5' -or $rest -eq 'F8')
    return [pscustomobject]@{ code = $code; numpad = $false; mods = $mods; name = ($name + $rest); keyText = $keyText; modsText = $modsText; modsParen = $(if ($flags.Count -gt 1) { '(' + $modsText + ')' } else { $modsText }); game = $game }
}

function Get-ZsdkRelative([string]$from, [string]$to) {
    $a = [System.IO.Path]::GetFullPath($from).TrimEnd('\').Split('\')
    $b = [System.IO.Path]::GetFullPath($to).TrimEnd('\').Split('\')
    if ($a[0] -ne $b[0] -or -not $a[0]) { return $null }
    $common = 0
    while ($common -lt $a.Count -and $common -lt $b.Count -and $a[$common] -eq $b[$common]) { $common++ }
    $parts = New-Object System.Collections.Generic.List[string]
    for ($i = $common; $i -lt $a.Count; $i++) { $parts.Add('..') }
    for ($i = $common; $i -lt $b.Count; $i++) { $parts.Add($b[$i]) }
    if ($parts.Count -eq 0) { return '.' }
    return ($parts -join '\')
}

function Set-ZsdkSites([System.Collections.IDictionary]$texts, [string]$filter, [string]$pattern, [string]$replacement) {
    $count = 0
    foreach ($rel in @($texts.Keys)) {
        if ($rel -notmatch $filter) { continue }
        $found = [regex]::Matches($texts[$rel], $pattern).Count
        if ($found -eq 0) { continue }
        $count += $found
        $texts[$rel] = [regex]::Replace($texts[$rel], $pattern, $replacement)
    }
    return $count
}

if ($Name -notmatch '^[A-Za-z][A-Za-z0-9]{1,39}$') { throw ('the name is 2 to 40 ASCII letters and digits starting with a letter, for example MyHud or ZoneHud: ' + $Name) }
if ($Name -match 'template') { throw ('the name may not contain "template", which names the ZoneSDK starter add-on: ' + $Name) }
$short = if ($Name -cmatch '^Zone[A-Z]') { $Name.Substring(4) } else { $Name }
if ($short.Length -lt 2) { throw ('the name without the Zone prefix needs at least 2 characters: ' + $Name) }
$lower = $short.ToLowerInvariant()
$space = 'z' + $lower
if (-not $Define) { $Define = $short.ToUpperInvariant() }
if ($Define -cnotmatch '^[A-Z][A-Z0-9_]{1,31}$') { throw ('the define is 2 to 32 upper-case letters, digits and _ starting with a letter: ' + $Define) }
if ($Define -match 'TEMPLATE' -or $Define -match '^(ZSDK|IMGUI|RESHADE)' -or @('WIN32', 'WIN64', 'UNICODE', 'NDEBUG', 'DEBUG', 'STRICT', 'NOMINMAX') -contains $Define) { throw ('the define ' + $Define + ' clashes with a ZoneSDK, ImGui, ReShade or Windows macro; pass another one with -Define') }
if ($space -cnotmatch '^z[a-z][a-z0-9]*$' -or $space.StartsWith('zsdk')) { throw ('the name ' + $Name + ' gives the C++ namespace ' + $space + ', which clashes with the ZoneSDK namespace zsdk; pick another name') }
$Tag = if ($Tag) { $Tag } else { $Name }
if ($Tag -notmatch '^[A-Za-z0-9]{2,40}$') { throw ('the tag is 2 to 40 ASCII letters and digits: ' + $Tag) }
if ($Tag -match 'template') { throw ('the tag may not contain "template": ' + $Tag) }
if ($Author -and $Author -match '[^\x00-\x7F]') { throw ('the author goes into the version resource, which this ZoneSDK compiles as plain ASCII, so it takes Latin letters only: write it in Latin letters, for example -Author "Oleksa Honta", and keep the Cyrillic spelling for your mod page: ' + $Author) }
if ($Author -and $Author -notmatch "^[A-Za-z0-9][A-Za-z0-9 .,_&()'+-]{0,63}$") { throw ('the author is 1 to 64 Latin letters, digits, spaces and . , _ & ( ) '' + - (it goes into the version resource, which the resource compiler reads as plain ASCII): ' + $Author) }

$targetFull = [System.IO.Path]::GetFullPath($ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Target)).TrimEnd('\')
if (Test-Path -LiteralPath $targetFull) { throw ('the target exists and is never overwritten: ' + $targetFull) }
for ($walk = $targetFull; $walk; $walk = Split-Path $walk -Parent) {
    if (Test-Path -LiteralPath (Join-Path $walk 'MANIFEST.sha256')) { throw ('the target lies inside the ZoneSDK release ' + $walk + ', which is never edited: pick a folder outside it') }
}

if ($Release) {
    $pinned = [System.IO.Path]::GetFullPath($ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Release)).TrimEnd('\')
    if (-not (Test-Path -LiteralPath (Join-Path $pinned 'MANIFEST.sha256'))) { throw ('-Release is not a cut ZoneSDK release, MANIFEST.sha256 is missing: ' + $pinned) }
}
elseif ($inside) { $pinned = [System.IO.Path]::GetFullPath($tree).TrimEnd('\') }
else {
    $own = Get-ZsdkTreeVersion $tree
    $pinned = [System.IO.Path]::GetFullPath((Join-Path $tree ('release\' + $own))).TrimEnd('\')
    if (-not (Test-Path -LiteralPath (Join-Path $pinned 'MANIFEST.sha256'))) { throw ('ZoneSDK ' + $own + ' is not cut yet, ' + $pinned + ' has no MANIFEST.sha256: pass -Release with the folder of a cut release') }
}
$version = Get-ZsdkTreeVersion $pinned
$manifest = (Get-FileHash -LiteralPath (Join-Path $pinned 'MANIFEST.sha256') -Algorithm SHA256).Hash
$rows = @(Get-ZsdkRows $pinned)
if ($rows.Count -eq 0) { Write-Warning ('the registry table of ' + $pinned + ' could not be read: names, tags, lanes and keys are not checked against it') }
foreach ($row in $rows) {
    if ($row.tag -eq $Name -or $row.id -eq $lower) { throw ('the name ' + $Name + ' is taken in the ZoneSDK registry table by ' + $row.id + ' (' + $row.tag + '); pick another name') }
    if ($row.tag -eq $Tag) { throw ('the tag ' + $Tag + ' is taken in the ZoneSDK registry table by ' + $row.id + '; pick another tag') }
}
$packageTop = $pinned
if ((Split-Path (Split-Path $pinned -Parent) -Leaf) -eq 'release') { $packageTop = Split-Path (Split-Path $pinned -Parent) -Parent }
$nearby = New-Object System.Collections.Generic.List[object]
foreach ($samples in @((Join-Path $packageTop 'samples'), (Join-Path $tree 'samples')) | Select-Object -Unique) {
    foreach ($sample in @(Get-ChildItem -LiteralPath $samples -Directory -ErrorAction SilentlyContinue | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'src') })) {
        if ($sample.Name -eq $Name -or $sample.Name -eq $Tag) { throw ('the ' + $(if ($sample.Name -eq $Name) { 'name' } else { 'tag' }) + ' ' + $sample.Name + ' belongs to a ZoneSDK sample, and players may have the sample installed; pick another one') }
        foreach ($one in @(Get-ZsdkRows $sample.FullName)) {
            if ($one.tag -eq $Tag -or $one.tag -eq $Name) { throw ('the tag ' + $one.tag + ' belongs to the ZoneSDK sample ' + $sample.Name + ', and players may have the sample installed; pick another one') }
            $nearby.Add([pscustomobject]@{ id = ('the sample ' + $sample.Name); tag = $one.tag; lane = $one.lane; keys = $one.keys })
        }
    }
}
$targetParent = Split-Path ([System.IO.Path]::GetFullPath($ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Target)).TrimEnd('\')) -Parent
$siblings = New-Object System.Collections.Generic.List[string]
$looked = 0
foreach ($one in @(Get-ChildItem -LiteralPath $targetParent -Directory -ErrorAction SilentlyContinue | Select-Object -First 500)) {
    if ($looked -gt 2000) { break }
    $inner = @(Get-ChildItem -LiteralPath $one.FullName -Directory -ErrorAction SilentlyContinue | Select-Object -First 50)
    $looked += 1 + $inner.Count
    foreach ($root in @($one.FullName, (Join-Path $one.FullName 'addon')) + @($inner | ForEach-Object { Join-Path $_.FullName 'addon' })) {
        if ((Test-Path -LiteralPath (Join-Path $root 'build\zsdk.lock')) -and (Test-Path -LiteralPath (Join-Path $root 'src')) -and $siblings -notcontains $root) { $siblings.Add($root) }
    }
}
foreach ($root in $siblings) {
    foreach ($one in @(Get-ZsdkRows $root)) {
        if ($one.tag -eq $Tag) { throw ('the tag ' + $Tag + ' is used by the add-on in ' + $root + ' next to the target; two add-ons with one tag cannot both load, pick another tag') }
        $nearby.Add([pscustomobject]@{ id = ('the add-on ' + (Split-Path $root -Leaf)); tag = $one.tag; lane = $one.lane; keys = $one.keys })
    }
}

$template = [System.IO.Path]::GetFullPath((Join-Path $tree 'template\ZoneTemplate')).TrimEnd('\')
if (-not (Test-Path -LiteralPath (Join-Path $template 'src'))) { throw ('the starter add-on is missing: ' + $template) }
$own = @(Get-ZsdkRows $template | Where-Object { $_.id -eq 'template' })
$baseKeys = if ($own.Count -gt 0) { $own[0].keys } else { $null }

$taken = New-Object 'System.Collections.Generic.Dictionary[string,string]'
foreach ($row in @($rows) + $nearby.ToArray()) {
    foreach ($one in ($row.keys -split '\|')) {
        if (-not $one) { continue }
        $chord = ConvertTo-ZsdkChord $one
        if ($chord.code -and -not $taken.ContainsKey($chord.name)) { $taken[$chord.name] = $row.id }
    }
}
$notes = New-Object System.Collections.Generic.List[string]
if ($Key) {
    $chord = ConvertTo-ZsdkChord $Key
    if ($chord.numpad) { throw ('numpad keys are refused, many keyboards have none: ' + $Key) }
    if (-not $chord.code) { throw ('not a key ZoneSDK can bind: ' + $Key + ' (use names like ALT+K, CTRL+SHIFT+F7, F7, PAGEUP)') }
    if ($taken.ContainsKey($chord.name)) { $notes.Add('the key ' + $chord.name + ' is also used by ' + $taken[$chord.name]) }
    if ($chord.game) { $notes.Add('the game uses ' + $chord.name + ' on its own; a chord such as ALT+' + $chord.name + ' is safer') }
    $keyHow = 'given'
}
else {
    $chord = $null
    foreach ($one in 'ALT+J', 'ALT+H', 'ALT+U', 'ALT+I', 'ALT+O', 'ALT+P', 'ALT+N', 'ALT+B', 'ALT+Y', 'ALT+M', 'ALT+K', 'ALT+F9', 'ALT+F10', 'ALT+F11') {
        $probe = ConvertTo-ZsdkChord $one
        if (-not $taken.ContainsKey($probe.name)) { $chord = $probe; break }
    }
    if (-not $chord) { throw 'no free default key is left in the ZoneSDK registry table: pass -Key' }
    $keyHow = 'picked: free in the ZoneSDK registry table, the samples and the add-ons next to the target, not a numpad key'
    $notes.Add('the key ' + $chord.name + ' was picked automatically; another add-on a player has may still use it, check it in game')
}
$lanes = @(@($rows) + $nearby.ToArray() | Where-Object { $_.lane -gt 0 })
if ($Lane -lt 0) {
    $picked = $null
    foreach ($one in 0.60, 0.66, 0.72, 0.78, 0.84, 0.90, 0.06) {
        if (-not ($lanes | Where-Object { [math]::Abs($_.lane - $one) -lt 0.005 })) { $picked = $one; break }
    }
    if ($null -eq $picked) { throw 'no free default toast lane is left in the ZoneSDK registry table: pass -Lane' }
    $Lane = $picked
    $laneHow = 'picked: free in the ZoneSDK registry table, the samples and the add-ons next to the target'
    $notes.Add('the toast lane ' + $Lane.ToString('0.00', $invariant) + ' was picked automatically; another add-on a player has may still use it')
}
else {
    if ($Lane -gt 0.95) { throw ('a toast lane is 0 (no toasts) or a screen fraction up to 0.95: ' + $Lane) }
    $laneHow = 'given'
    $same = @($lanes | Where-Object { [math]::Abs($_.lane - $Lane) -lt 0.005 } | ForEach-Object { $_.id })
    if ($same.Count -gt 0) { $notes.Add('the toast lane ' + $Lane.ToString('0.00', $invariant) + ' is shared with ' + ($same -join ', ')) }
}
$laneText = $Lane.ToString('0.00', $invariant) + 'f'

$skip = '^(bin\\|build\\obj\\|build\\last-build\.log$|build\\\.zsdk-build\.lock$|build\\local\.cfg$|dist\\[^\\]+\\symbols\\|dist\\.*\.zip$|dist\\.*-SHA256\.txt$)'
$texts = [ordered]@{}
$sources = @{}
foreach ($item in @(Get-ChildItem -LiteralPath $template -Recurse -File | Sort-Object FullName)) {
    $rel = $item.FullName.Substring($template.Length + 1)
    if ($rel -match $skip) { continue }
    if ($rel -notmatch '^(src|tests|build|dist)\\' -and $rel -notmatch '^[^\\]+\.cmd$' -and $rel -ne '.gitignore') { continue }
    $out = $rel -creplace 'template', $lower
    if ($texts.Contains($out)) { throw ('the name ' + $Name + ' makes ' + $sources[$out] + ' and ' + $rel + ' both become ' + $out + '; pick another name') }
    $text = [System.IO.File]::ReadAllText($item.FullName)
    $text = $text.Replace('ZoneTemplate', $Name).Replace('TEMPLATE', $Define).Replace('ztemplate', $space).Replace('template.h', $lower + '.h').Replace('Template.', $short + '.')
    $texts[$out] = [regex]::Replace($text, 'template(?!\s*<)', $lower)
    $sources[$out] = $rel
}
$header = 'src\' + $lower + '.h'
$clash = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
foreach ($one in $script:Headers) { [void]$clash.Add($one) }
foreach ($file in @(Get-ChildItem -LiteralPath (Join-Path $pinned 'extern') -Recurse -File -Include '*.h', '*.hpp' -ErrorAction SilentlyContinue)) { [void]$clash.Add($file.BaseName) }
if ($env:INCLUDE) {
    foreach ($dir in $env:INCLUDE.Split(';')) {
        if ($dir -and (Test-Path -LiteralPath $dir)) { foreach ($file in @(Get-ChildItem -LiteralPath $dir -File -Filter '*.h' -ErrorAction SilentlyContinue)) { [void]$clash.Add($file.BaseName) } }
    }
}
if ($texts.Contains($header) -and $clash.Contains($lower)) { throw ('the name ' + $Name + ' adds ' + $header + ', which would hide the system or SDK header ' + $lower + '.h for every unit of the add-on; pick another name') }

$code = '^src\\.*\.(h|cpp|inc)$'
$quotedName = [regex]::Escape('"' + $Name + '"')
$tagSites = (Set-ZsdkSites $texts $code '(\bkTag\s*=\s*)"[^"]*"' ('${1}"' + $Tag + '"'))
$tagSites += (Set-ZsdkSites $texts $code ('(\.tag\s*=\s*)' + $quotedName) ('${1}"' + $Tag + '"'))
$tagSites += (Set-ZsdkSites $texts $code ('(Identity\s*\{\s*"[^"]*"\s*,\s*)' + $quotedName) ('${1}"' + $Tag + '"'))
$tagSites += (Set-ZsdkSites $texts $code ('(registry::Row\s*\{\s*"[^"]*"\s*,\s*)' + $quotedName) ('${1}"' + $Tag + '"'))
if ($tagSites -eq 0) { throw 'the starter add-on marks no tag (kTag, .tag or the Identity and registry row literals): new-addon cannot place -Tag' }
$laneSites = (Set-ZsdkSites $texts $code '(\bkLane\s*=\s*)[0-9]*\.?[0-9]+f' ('${1}' + $laneText))
$laneSites += (Set-ZsdkSites $texts $code '(\.lane\s*=\s*)[0-9]*\.?[0-9]+f' ('${1}' + $laneText))
$laneSites += (Set-ZsdkSites $texts $code '(registry::Row\s*\{\s*"[^"]*"\s*,\s*"[^"]*"\s*,\s*)[0-9]*\.?[0-9]+f' ('${1}' + $laneText))
if ($laneSites -eq 0) { throw 'the starter add-on marks no toast lane (kLane, .lane or the registry row literal): new-addon cannot place -Lane' }
$keyLiteral = '"' + $chord.name.Replace('\', '\\') + '"'
$keySites = (Set-ZsdkSites $texts $code '(\bkKeys\s*=\s*)"[^"]*"' ('${1}' + $keyLiteral))
$keySites += (Set-ZsdkSites $texts $code '(\.keys\s*=\s*)"[^"]*"' ('${1}' + $keyLiteral))
$keySites += (Set-ZsdkSites $texts $code '(registry::Row\s*\{\s*"[^"]*"\s*,\s*"[^"]*"\s*,\s*[0-9]*\.?[0-9]+f\s*,\s*)"[^"]*"' ('${1}' + $keyLiteral))
if ($keySites -eq 0) { throw 'the starter add-on marks no default key (kKeys, .keys or the registry row literal): new-addon cannot place -Key' }
[void](Set-ZsdkSites $texts $code '(\bkChord\s*)\{[^}]*\}' ('${1}{ ' + $chord.keyText + ', ' + $chord.modsText + ' }'))
[void](Set-ZsdkSites $texts $code '(\bkKey\s*=\s*)[^;]+;' ('${1}' + $chord.keyText + ';'))
[void](Set-ZsdkSites $texts $code '(zsdk::keys::Chord\s+locate\s*)\{\s*kKey\s*,[^}]*\}' ('${1}{ kKey, ' + $chord.modsText + ' }'))
[void](Set-ZsdkSites $texts '^tests\\.*\.cpp$' '(Settings\{\}\.locate\.mods\s*==\s*)[^&]+?(\s*&&\s*Settings\{\}\.locate\.key\s*==\s*)[^&]+?(\s*&&)' ('${1}' + $chord.modsParen + '${2}' + $chord.keyText + '${3}'))
[void](Set-ZsdkSites $texts '^tests\\.*\.cpp$' 'the default key is [^,"]+,' ('the default key is ' + $chord.name + ','))
if ($baseKeys) { [void](Set-ZsdkSites $texts '^dist\\' ([regex]::Escape($baseKeys)) $chord.name.Replace('$', '$$')) }

$year = (Get-Date).Year.ToString($invariant)
$copyright = if ($Author) { '(c) ' + $year + ' ' + $Author } else { '' }
$authorSites = (Set-ZsdkSites $texts '^src\\.*\.(h|rc)$' ('(#define\s+' + $Define + '_AUTHOR\s+)"[^"]*"') ('${1}"' + $Author + '"'))
[void](Set-ZsdkSites $texts '^src\\.*\.(h|rc)$' ('(#define\s+' + $Define + '_COPYRIGHT\s+)"[^"]*"') ('${1}"' + $copyright + '"'))
$authorSites += (Set-ZsdkSites $texts '^src\\.*\.rc$' '(VALUE\s+"CompanyName"\s*,\s*)"[^"]*"' ('${1}"' + $Author + '\0"'))
[void](Set-ZsdkSites $texts '^src\\.*\.rc$' '(VALUE\s+"LegalCopyright"\s*,\s*)"[^"]*"' ('${1}"' + $copyright + '\0"'))
if ($authorSites -eq 0) { throw ('the starter add-on has no author placeholder (' + $Define + '_AUTHOR in version.h or the CompanyName literal in version.rc): new-addon cannot place -Author') }
if (-not $Author) { $notes.Add('no -Author: the version resource names no author, ReShade shows none and pack refuses {{AUTHOR}}; run again with -Author or set ' + $Define + '_AUTHOR in src\version.h') }

$reach = $false
for ($walk = $targetFull; $walk; $walk = Split-Path $walk -Parent) {
    foreach ($at in @((Join-Path $walk ('_sdk\release\' + $version)), (Join-Path $walk ('release\' + $version)))) {
        if ([System.IO.Path]::GetFullPath($at).TrimEnd('\') -eq $pinned) { $reach = $true }
    }
}
$homeLine = $null
$packageRoot = $pinned
if ((Split-Path (Split-Path $pinned -Parent) -Leaf) -eq 'release') { $packageRoot = Split-Path (Split-Path $pinned -Parent) -Parent }
if (-not $reach) {
    $base = if ($packageRoot -ne $pinned -or (Split-Path $pinned -Leaf) -eq $version) { $packageRoot } else { $null }
    if ($base) { $homeLine = Get-ZsdkRelative $targetFull $base }
    if (-not $homeLine) { $notes.Add('this folder does not find ZoneSDK by itself: set ZSDK_ROOT=' + $packageRoot + ' before you build') }
}
$lock = 'version=' + $version + "`n" + 'manifest=' + $manifest + "`n"
if ($homeLine) { $lock += 'home=' + $homeLine + "`n" }
$texts['build\zsdk.lock'] = $lock

$left = @($texts.Keys | Where-Object { $texts[$_] -match '(?i)template(?!\s*<)' })
foreach ($rel in $texts.Keys) {
    $out = Join-Path $targetFull $rel
    New-Item -ItemType Directory -Force -Path (Split-Path $out -Parent) | Out-Null
    [System.IO.File]::WriteAllText($out, $texts[$rel], $utf8)
}

$prefix = if ($Author) { ($Author -replace '[^A-Za-z0-9]', '') } else { 'YourName' }
Write-Host ('created {0} in {1}' -f $Name, $targetFull)
Write-Host ('  define {0}, namespace {1}, header {2}' -f $Define, $space, $header)
Write-Host ('  tag {0}, toast lane {1} ({2}), key {3} ({4})' -f $Tag, $Lane.ToString('0.00', $invariant), $laneHow, $chord.name, $keyHow)
Write-Host ('  ZoneSDK {0} manifest {1} from {2} pinned in build\zsdk.lock{3}' -f $version, $manifest.Substring(0, 8), $pinned, $(if ($homeLine) { ', home=' + $homeLine } else { '' }))
Write-Host ('  the tag names the game-thread message and the one-instance mutex of the add-on. It must be unique among all ReShade add-ons a player has: with two add-ons of one tag the second one silently does not load.')
if ($Tag -eq $Name) { Write-Host ('  recommended: put your author name in front, for example -Tag {0}{1}' -f $prefix, $Name) }
foreach ($one in $notes) { Write-Warning $one }
foreach ($one in $left) { Write-Warning ('the word template is still in ' + $one + ': rename it by hand') }
$fakehost = @((Join-Path $packageRoot 'fakehost.cmd'), (Join-Path $tree 'fakehost.cmd')) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $fakehost) { $fakehost = 'fakehost.cmd' }
Write-Host 'next steps:'
Write-Host ('  1. cd "{0}"' -f $targetFull)
Write-Host '  2. .\test.cmd                      build Release and run the tests (build.cmd -Tests)'
Write-Host ('  3. "{0}" -Addon "{1}" -Label {2} -Out "{3}"' -f $fakehost, (Join-Path $targetFull ('bin\' + $Name + '.addon64')), $lower, (Join-Path $targetFull 'build\fakehost'))
Write-Host '                                    offline smoke run without the game'
Write-Host '  4. .\build.cmd -Verify            check the finished file before you ship it'
Write-Host ('  5. edit dist\README-EN.txt and dist\README-UA.txt, then .\pack.cmd, which writes dist\{0}-<version>.zip' -f $Name)
