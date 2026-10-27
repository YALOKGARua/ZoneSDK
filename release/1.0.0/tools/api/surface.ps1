[CmdletBinding()]
param(
    [string]$Include,
    [string]$Version,
    [string]$Out,
    [string]$Compare,
    [string]$With,
    [ValidateSet('major', 'minor', 'patch')]
    [string]$Kind = 'minor',
    [string]$Tags
)

$ErrorActionPreference = 'Stop'
$script:Format = 'zsdk-api 1'
if (-not $Tags) { $Tags = Join-Path $PSScriptRoot 'tags.txt' }

function Read-Tags([string]$path) {
    $rules = New-Object System.Collections.Generic.List[object]
    foreach ($line in [System.IO.File]::ReadAllLines($path)) {
        $t = $line.Trim()
        if (-not $t) { continue }
        $parts = @($t -split '\s+')
        if ($parts.Count -lt 2 -or @('Internal', 'Provisional', 'Legacy', 'Stable') -notcontains $parts[0]) { throw ('a line of ' + $path + ' is not "<Tag> <name pattern>" or "<Tag> header <file>": ' + $t) }
        if ($parts[1] -eq 'header') { $rules.Add(@{ tag = $parts[0]; header = $parts[2]; pattern = $null }) }
        else { $rules.Add(@{ tag = $parts[0]; header = $null; pattern = $parts[1] }) }
    }
    return $rules
}

function Get-Tag($rules, [string]$header, [string]$name) {
    foreach ($r in $rules) {
        if ($r.header) { if ($header -eq $r.header) { return $r.tag }; continue }
        if ($name -like $r.pattern) { return $r.tag }
    }
    return 'Stable'
}

function Format-Text([string]$text) {
    $t = [regex]::Replace($text, '\s+', ' ').Trim()
    $t = [regex]::Replace($t, '\(\s+', '(')
    $t = [regex]::Replace($t, '\s+\)', ')')
    $t = [regex]::Replace($t, '\s+,', ',')
    $t = [regex]::Replace($t, '<\s+', '<')
    $t = [regex]::Replace($t, '\s+>', '>')
    return $t
}

function Get-Clean([string]$text) {
    $keep = New-Object System.Text.StringBuilder
    $macros = New-Object System.Collections.Generic.List[string]
    $join = $false
    foreach ($line in $text.Replace("`r", '').Split("`n")) {
        $t = $line.TrimStart()
        if ($join -or $t.StartsWith('#')) {
            if (-not $join -and $t -match '^#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)') { $macros.Add($Matches[1]) }
            $join = $line.TrimEnd().EndsWith('\')
            [void]$keep.Append("`n")
            continue
        }
        [void]$keep.Append($line).Append("`n")
    }
    return @{ text = $keep.ToString(); macros = $macros }
}

function Skip-Quoted([string]$s, [int]$i) {
    $q = $s[$i]
    $j = $i - 1
    while ($j -ge 0 -and [char]::IsLetterOrDigit($s[$j]) -or ($j -ge 0 -and $s[$j] -eq '_')) { $j-- }
    $run = $s.Substring($j + 1, $i - $j - 1)
    if ($q -eq [char]39 -and $run -match '^[0-9]') { return $i + 1 }
    if ($q -eq '"' -and $run -cmatch '^(u8|u|U|L)?R$') {
        $open = $s.IndexOf('(', $i)
        $delim = $s.Substring($i + 1, $open - $i - 1)
        $close = $s.IndexOf(')' + $delim + '"', $open)
        if ($close -lt 0) { throw 'an unterminated raw string' }
        return $close + $delim.Length + 2
    }
    $k = $i + 1
    while ($k -lt $s.Length) {
        $c = $s[$k]
        if ($c -eq '\') { $k += 2; continue }
        if ($c -eq $q) { return $k + 1 }
        if ($c -eq "`n") { return $k }
        $k++
    }
    return $k
}

function Skip-Comment([string]$s, [int]$i) {
    if ($s[$i + 1] -eq '/') { $end = $s.IndexOf("`n", $i); if ($end -lt 0) { return $s.Length }; return $end }
    $end = $s.IndexOf('*/', $i + 2)
    if ($end -lt 0) { return $s.Length }
    return $end + 2
}

function Test-CommentAt([string]$s, [int]$i) {
    return $s[$i] -eq '/' -and $i + 1 -lt $s.Length -and ($s[$i + 1] -eq '/' -or $s[$i + 1] -eq '*')
}

function Skip-Block([string]$s, [int]$i) {
    $depth = 0
    $k = $i
    while ($k -lt $s.Length) {
        $c = $s[$k]
        if ($c -eq '"' -or $c -eq [char]39) { $k = Skip-Quoted $s $k; continue }
        if (Test-CommentAt $s $k) { $k = Skip-Comment $s $k; continue }
        if ($c -eq '{') { $depth++ }
        elseif ($c -eq '}') { $depth--; if ($depth -eq 0) { return $k + 1 } }
        $k++
    }
    throw 'unbalanced braces'
}

function Split-Template([string]$text) {
    $t = $text.Trim()
    if ($t -notmatch '^template\s*<') { return @('', $t) }
    $start = $t.IndexOf('<')
    $depth = 0
    for ($k = $start; $k -lt $t.Length; $k++) {
        if ($t[$k] -eq '<') { $depth++ }
        elseif ($t[$k] -eq '>' -and $t[$k - 1] -ne '-') { $depth--; if ($depth -eq 0) { return @((Format-Text $t.Substring(0, $k + 1)), $t.Substring($k + 1).Trim()) } }
    }
    throw ('an unbalanced template head: ' + $t)
}

function Remove-Attributes([string]$text) {
    return [regex]::Replace($text, '\b(alignas|__declspec)\s*\([^()]*(\([^()]*\)[^()]*)*\)\s*', '')
}

function Close-Paren([string]$t, [int]$open) {
    $depth = 0
    for ($k = $open; $k -lt $t.Length; $k++) {
        if ($t[$k] -eq '(') { $depth++ }
        elseif ($t[$k] -eq ')') { $depth--; if ($depth -eq 0) { return $k } }
    }
    return -1
}

function Find-Call([string]$core) {
    $m = [regex]::Match($core, 'operator\s*(\(\)|\[\]|[^\s(]+)\s*\(')
    if ($m.Success) { return @{ name = 'operator' + $m.Groups[1].Value; open = $m.Index + $m.Length - 1 } }
    foreach ($m in [regex]::Matches($core, '(~?[A-Za-z_][A-Za-z0-9_]*)\s*\(')) {
        if (@('alignas', 'decltype', '__declspec', 'noexcept', 'sizeof', 'alignof', 'static_cast', 'reinterpret_cast', 'const_cast', 'requires') -contains $m.Groups[1].Value) { continue }
        $before = $core.Substring(0, $m.Index).Replace('->', '')
        $angle = ([regex]::Matches($before, '<')).Count - ([regex]::Matches($before, '>')).Count
        if ($angle -gt 0) { continue }
        return @{ name = $m.Groups[1].Value; open = $m.Index + $m.Length - 1 }
    }
    return $null
}

function Split-Initializer([string]$t) {
    $depth = 0
    for ($k = 0; $k -lt $t.Length; $k++) {
        $c = $t[$k]
        if ($c -eq '"' -or $c -eq [char]39) { $k = (Skip-Quoted $t $k) - 1; continue }
        if ($c -eq '(' -or $c -eq '[') { $depth++; continue }
        if ($c -eq ')' -or $c -eq ']') { $depth--; continue }
        if ($depth -ne 0) { continue }
        if ($c -eq '{') { return @($t.Substring(0, $k).Trim(), $t.Substring($k).Trim()) }
        if ($c -ne '=') { continue }
        $prev = if ($k -gt 0) { $t[$k - 1] } else { ' ' }
        $next = if ($k + 1 -lt $t.Length) { $t[$k + 1] } else { ' ' }
        if ('=!<>'.Contains([string]$prev) -or $next -eq '=') { continue }
        if ($t.Substring(0, $k).TrimEnd() -match 'operator$') { continue }
        return @($t.Substring(0, $k).Trim(), $t.Substring($k + 1).Trim())
    }
    return @($t.Trim(), '')
}

function Get-Signature([string]$core, $call, [string]$init) {
    $close = Close-Paren $core $call.open
    if ($close -lt 0) { return Format-Text $core }
    $sig = $core.Substring(0, $close + 1)
    $rest = $core.Substring($close + 1).Trim()
    if ($rest -match '^:(?!:)') { $rest = '' }
    $text = $sig
    if ($rest) { $text += ' ' + $rest }
    if (@('0', 'default', 'delete') -contains $init) { $text += ' = ' + $init }
    return Format-Text $text
}

function Read-Header([string]$file, [string]$rel, $rules, $items) {
    $clean = Get-Clean ([System.IO.File]::ReadAllText($file))
    $s = $clean.text
    foreach ($macro in $clean.macros) {
        if ($macro.EndsWith('_')) { continue }
        Add-Item $items $rules $rel 'macro' $macro '-' '-'
    }
    $stack = New-Object System.Collections.Generic.List[object]
    $buf = New-Object System.Text.StringBuilder
    $i = 0
    while ($i -lt $s.Length) {
        $c = $s[$i]
        if ($c -eq '"' -or $c -eq [char]39) { $j = Skip-Quoted $s $i; [void]$buf.Append($s.Substring($i, $j - $i)); $i = $j; continue }
        if (Test-CommentAt $s $i) { $i = Skip-Comment $s $i; continue }
        if ($c -eq ';') { Add-Statement $items $rules $rel $stack $buf.ToString(); [void]$buf.Clear(); $i++; continue }
        if ($c -eq '}') { if ($stack.Count -gt 0) { $stack.RemoveAt($stack.Count - 1) }; [void]$buf.Clear(); $i++; continue }
        if ($c -ne '{') { [void]$buf.Append($c); $i++; continue }
        $head = Format-Text (Use-Access $stack $buf.ToString())
        $scope = Get-Scope $stack
        $hidden = $scope.hidden
        if ($head -match '^(inline )?namespace( ([\w:]+))?$') {
            $name = $Matches[3]
            $stack.Add(@{ kind = 'ns'; name = $name; hidden = ($hidden -or -not $name); access = 'public' })
            if ($name -and -not $hidden) { Add-Item $items $rules $rel 'namespace' (Join-Name $scope.path $name) '-' '-' }
            [void]$buf.Clear(); $i++; continue
        }
        if ($head -match '^extern "C(\+\+)?"$') {
            $stack.Add(@{ kind = 'ns'; name = $null; hidden = $hidden; access = 'public' })
            [void]$buf.Clear(); $i++; continue
        }
        if ($head -match '^enum( (class|struct))? (\w+)') {
            $end = Skip-Block $s $i
            if (-not $hidden) {
                $enum = Join-Name $scope.path $Matches[3]
                Add-Item $items $rules $rel 'enum' $enum '-' $head
                $index = 0
                foreach ($value in (Split-Top $s.Substring($i + 1, $end - $i - 2))) {
                    $v = Format-Text $value
                    if (-not $v) { continue }
                    $vname = ($v -split '[\s=]')[0]
                    Add-Item $items $rules $rel 'value' (Join-Name $enum $vname) ([string]$index) $v
                    $index++
                }
            }
            [void]$buf.Clear(); $i = $end; continue
        }
        $parts = Split-Template $head
        $core = Remove-Attributes $parts[1]
        if ($core -match '^(struct|class|union)( (\w+))?( final)?( ?:(?!:).*)?$') {
            $name = $Matches[3]
            $isClass = $Matches[1] -eq 'class'
            $typeHidden = $hidden -or -not $name
            if (-not $typeHidden) { Add-Item $items $rules $rel 'type' (Join-Name $scope.path $name) '-' $head }
            $stack.Add(@{ kind = 'type'; name = $name; hidden = $typeHidden; access = $(if ($isClass) { 'private' } else { 'public' }) })
            [void]$buf.Clear(); $i++; continue
        }
        $isFunction = $core.Contains('(') -and ($core -match '\)\s*((const|volatile|noexcept(\s*\(.*\))?|override|final|&&|&)\s*)*$' -or $core -match '\)\s*->\s*[^()]+$' -or $core -match '\)\s*:(?!:)\s*\w')
        if ($isFunction) {
            if (-not $hidden) { Add-Function $items $rules $rel $scope $parts[0] $core '' }
            $i = Skip-Block $s $i
            [void]$buf.Clear(); continue
        }
        $end = Skip-Block $s $i
        [void]$buf.Append($s.Substring($i, $end - $i))
        $i = $end
    }
}

function Split-Top([string]$body) {
    $out = New-Object System.Collections.Generic.List[string]
    $depth = 0
    $start = 0
    for ($k = 0; $k -lt $body.Length; $k++) {
        $c = $body[$k]
        if ($c -eq '(' -or $c -eq '{' -or $c -eq '[') { $depth++ }
        elseif ($c -eq ')' -or $c -eq '}' -or $c -eq ']') { $depth-- }
        elseif ($c -eq ',' -and $depth -eq 0) { $out.Add($body.Substring($start, $k - $start)); $start = $k + 1 }
    }
    $out.Add($body.Substring($start))
    return $out
}

function Get-Scope($stack) {
    $names = New-Object System.Collections.Generic.List[string]
    $hidden = $false
    $access = 'public'
    $kind = 'ns'
    foreach ($one in $stack) {
        if ($one.name) { $names.Add($one.name) }
        if ($one.hidden) { $hidden = $true }
    }
    if ($stack.Count -gt 0) {
        $top = $stack[$stack.Count - 1]
        $kind = $top.kind
        $access = $top.access
        if ($top.kind -eq 'type' -and $top.access -ne 'public') { $hidden = $true }
    }
    return @{ path = ($names -join '::'); hidden = $hidden; kind = $kind }
}

function Join-Name([string]$path, [string]$name) {
    if (-not $path) { return $name }
    return $path + '::' + $name
}

function Use-Access($stack, [string]$text) {
    $t = $text
    while ($t -match '^\s*(public|private|protected)\s*:(?!:)') {
        if ($stack.Count -gt 0) { $stack[$stack.Count - 1].access = $Matches[1] }
        $t = $t.Substring($Matches[0].Length)
    }
    return $t
}

function Add-Item($items, $rules, [string]$header, [string]$kind, [string]$name, [string]$slot, [string]$text) {
    $tag = Get-Tag $rules $header $name
    if ($tag -eq 'Internal') { return }
    $items.Add([pscustomobject]@{ header = $header; kind = $kind; name = $name; slot = $slot; text = $text; tag = $tag })
}

function Add-Function($items, $rules, [string]$header, $scope, [string]$template, [string]$core, [string]$init) {
    $call = Find-Call $core
    if (-not $call) { throw ('a declaration in ' + $header + ' could not be read: ' + $core) }
    $text = Get-Signature $core $call $init
    if ($template) { $text = $template + ' ' + $text }
    Add-Item $items $rules $header 'fn' (Join-Name $scope.path $call.name) '-' $text
}

function Add-Statement($items, $rules, [string]$header, $stack, [string]$raw) {
    $text = Format-Text (Use-Access $stack $raw)
    $scope = Get-Scope $stack
    if (-not $text -or $scope.hidden) { return }
    if ($text -match '^static_assert\b') { return }
    if ($text -match '^(template ?<.*> ?)?(struct|class|union) [\w:]+$') { return }
    if ($text -match '^enum( (class|struct))? \w+( ?: ?[\w:]+)?$') { return }
    if ($text -match '^friend (class|struct) ') { return }
    if ($text -match '^using namespace (.+)$') { Add-Item $items $rules $header 'using' (Join-Name $scope.path ('namespace ' + $Matches[1])) '-' $text; return }
    if ($text -match '^using (\w+) ?= ?(.+)$') { Add-Item $items $rules $header 'alias' (Join-Name $scope.path $Matches[1]) '-' $text; return }
    if ($text -match '^using (.+)$') { $last = ($Matches[1] -split '::')[-1]; Add-Item $items $rules $header 'using' (Join-Name $scope.path $last) '-' $text; return }
    if ($text -match '^typedef .*?(\w+)$') { Add-Item $items $rules $header 'alias' (Join-Name $scope.path $Matches[1]) '-' $text; return }
    $parts = Split-Template $text
    $split = Split-Initializer $parts[1]
    $core = Remove-Attributes $split[0]
    $inType = $scope.kind -eq 'type'
    $fp = [regex]::Match($core, '\(\s*\*\s*(\w+)\s*\)\s*\(')
    if ($fp.Success -and $fp.Index -eq $core.IndexOf('(')) {
        $kind = if ($inType -and $core -notmatch '^static ') { 'field' } else { 'var' }
        Add-Field $items $rules $header $scope $kind $fp.Groups[1].Value (Format-Text $core)
        return
    }
    $probe = $core
    while ($probe -match '\[[^\[\]]*\]') { $probe = $probe.Replace($Matches[0], '') }
    if ($probe.Contains('(')) { Add-Function $items $rules $header $scope $parts[0] $core $split[1]; return }
    $bare = $core
    while ($bare -match '\[[^\]]*\]\s*$') { $bare = $bare.Substring(0, $bare.Length - $Matches[0].Length) }
    if ($bare -notmatch '(\w+)\s*$') { throw ('a declaration in ' + $header + ' could not be read: ' + $text) }
    $name = $Matches[1]
    $kind = if ($inType -and $core -notmatch '^static ') { 'field' } elseif ($inType) { 'const' } else { 'var' }
    Add-Field $items $rules $header $scope $kind $name (Format-Text $core)
}

function Add-Field($items, $rules, [string]$header, $scope, [string]$kind, [string]$name, [string]$text) {
    $slot = '-'
    if ($kind -eq 'field') {
        $owner = $scope.path
        if (-not $script:FieldCount.ContainsKey($owner)) { $script:FieldCount[$owner] = 0 }
        $slot = [string]$script:FieldCount[$owner]
        $script:FieldCount[$owner] = $script:FieldCount[$owner] + 1
    }
    Add-Item $items $rules $header $kind (Join-Name $scope.path $name) $slot $text
}

function New-Surface([string]$include, [string]$version, $rules) {
    $items = New-Object System.Collections.Generic.List[object]
    $script:FieldCount = @{}
    $root = [System.IO.Path]::GetFullPath($include).TrimEnd('\')
    $files = New-Object System.Collections.Generic.List[string]
    foreach ($f in Get-ChildItem -LiteralPath $root -Recurse -File -Filter '*.h') { $files.Add($f.FullName.Substring($root.Length + 1).Replace('\', '/')) }
    $files.Sort([System.StringComparer]::Ordinal)
    foreach ($rel in $files) {
        if ((Get-Tag $rules $rel '') -eq 'Internal' -and ($rules | Where-Object { $_.header -eq $rel })) { continue }
        Read-Header (Join-Path $root $rel.Replace('/', '\')) $rel $rules $items
    }
    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add($script:Format)
    $lines.Add('version ' + $version)
    foreach ($it in $items) {
        foreach ($v in $it.header, $it.kind, $it.name, $it.slot, $it.text, $it.tag) { if ($v.Contains("`t")) { throw ('a tab in the surface of ' + $it.name) } }
        $lines.Add(($it.header, $it.kind, $it.name, $it.slot, $it.text, $it.tag) -join "`t")
    }
    return $lines
}

function Read-Surface([string]$path) {
    $lines = [System.IO.File]::ReadAllLines($path)
    if ($lines.Count -lt 2 -or $lines[0] -ne $script:Format) { throw ('not an API surface file (' + $script:Format + '): ' + $path) }
    $items = New-Object System.Collections.Generic.List[object]
    foreach ($line in ($lines | Select-Object -Skip 2)) {
        if (-not $line) { continue }
        $f = $line.Split("`t")
        if ($f.Count -ne 6) { throw ('a line of ' + $path + ' does not have 6 columns: ' + $line) }
        $items.Add([pscustomobject]@{ header = $f[0]; kind = $f[1]; name = $f[2]; slot = $f[3]; text = $f[4]; tag = $f[5] })
    }
    return $items
}

function Get-Owner([string]$name) {
    $at = $name.LastIndexOf('::')
    if ($at -lt 0) { return '' }
    return $name.Substring(0, $at)
}

function Get-Declared([string]$text) {
    return (Split-Initializer $text)[0]
}

function Compare-Surface([string]$oldPath, [string]$newPath, [string]$kind) {
    $old = Read-Surface $oldPath
    $new = Read-Surface $newPath
    $refused = New-Object System.Collections.Generic.List[string]
    $soft = New-Object System.Collections.Generic.List[string]
    $added = New-Object System.Collections.Generic.List[string]
    $plain = @('type', 'enum', 'fn', 'const', 'var', 'alias', 'using', 'macro')
    $keyOf = { param($it) if ($it.kind -eq 'fn') { $it.kind + '|' + $it.name + '|' + $it.text } else { $it.kind + '|' + $it.name } }
    $report = {
        param([string]$what, $a, $b)
        $loose = ($a -and $a.tag -eq 'Provisional') -or ($b -and $b.tag -eq 'Provisional')
        if ($kind -eq 'major' -or ($loose -and $kind -eq 'minor')) { $soft.Add($what) } else { $refused.Add($what) }
    }
    $newBy = @{}
    foreach ($it in $new) { if ($plain -contains $it.kind) { $newBy[(& $keyOf $it)] = $it } }
    $oldBy = @{}
    foreach ($it in $old) {
        if ($plain -notcontains $it.kind) { continue }
        $k = & $keyOf $it
        $oldBy[$k] = $it
        if (-not $newBy.ContainsKey($k)) {
            & $report ('removed or changed {0} {1}: {2}' -f $it.kind, $it.name, $it.text) $it $null
            continue
        }
        $now = $newBy[$k]
        $was = if (@('const', 'var') -contains $it.kind) { Get-Declared $it.text } else { $it.text }
        $is = if (@('const', 'var') -contains $it.kind) { Get-Declared $now.text } else { $now.text }
        if ($it.kind -ne 'fn' -and $was -ne $is) { & $report ('changed {0} {1}: {2} -> {3}' -f $it.kind, $it.name, $it.text, $now.text) $it $now }
        if ($it.tag -ne $now.tag) { $soft.Add(('tag of {0} {1}: {2} -> {3}' -f $it.kind, $it.name, $it.tag, $now.tag)) }
    }
    foreach ($it in $new) { if ($plain -contains $it.kind -and -not $oldBy.ContainsKey((& $keyOf $it))) { $added.Add(('{0} {1}: {2}' -f $it.kind, $it.name, $it.text)) } }
    foreach ($group in @('field', 'value')) {
        $oldGroups = [ordered]@{}
        $newGroups = @{}
        foreach ($it in $old) { if ($it.kind -eq $group) { $o = Get-Owner $it.name; if (-not $oldGroups.Contains($o)) { $oldGroups[$o] = New-Object System.Collections.Generic.List[object] }; $oldGroups[$o].Add($it) } }
        foreach ($it in $new) { if ($it.kind -eq $group) { $o = Get-Owner $it.name; if (-not $newGroups.ContainsKey($o)) { $newGroups[$o] = New-Object System.Collections.Generic.List[object] }; $newGroups[$o].Add($it) } }
        foreach ($o in $oldGroups.Keys) {
            $was = @($oldGroups[$o] | Sort-Object { [int]$_.slot })
            $is = if ($newGroups.ContainsKey($o)) { @($newGroups[$o] | Sort-Object { [int]$_.slot }) } else { @() }
            if ($group -eq 'field') {
                for ($k = 0; $k -lt $was.Count; $k++) {
                    if ($k -ge $is.Count) { & $report ('removed field {0} of {1}' -f $was[$k].text, $o) $was[$k] $null; continue }
                    if ($is[$k].name -ne $was[$k].name -or $is[$k].text -ne $was[$k].text) { & $report ('field {0} of {1} is now {2}: removed, reordered or changed' -f $was[$k].text, $o, $is[$k].text) $was[$k] $is[$k] }
                }
                for ($k = $was.Count; $k -lt $is.Count; $k++) { $added.Add(('field {0} of {1} (appended)' -f $is[$k].text, $o)) }
                continue
            }
            $oldNames = @($was | ForEach-Object { $_.name })
            $kept = @($is | Where-Object { $oldNames -contains $_.name })
            $same = $kept.Count -eq $was.Count
            for ($k = 0; $same -and $k -lt $was.Count; $k++) { if ($kept[$k].name -ne $was[$k].name -or $kept[$k].text -ne $was[$k].text) { $same = $false } }
            if (-not $same) { & $report ('values of enum {0} were removed, reordered or renumbered' -f $o) $was[0] $null }
            $names = @($is | ForEach-Object { $_.name })
            $countAt = [array]::IndexOf($names, $o + '::Count')
            $lastOld = if ($was.Count -gt 0) { [array]::IndexOf($names, $was[$was.Count - 1].name) } else { -1 }
            $hasCount = $was.Count -gt 0 -and $was[$was.Count - 1].name -eq ($o + '::Count')
            for ($k = 0; $k -lt $is.Count; $k++) {
                if ($oldNames -contains $is[$k].name) { continue }
                $ok = if ($hasCount) { $countAt -ge 0 -and $k -lt $countAt } else { $k -gt $lastOld }
                if ($ok) { $added.Add(('value {0}' -f $is[$k].text + ' of ' + $o)) }
                else { & $report ('value {0} of {1} is not added {2}' -f $is[$k].text, $o, $(if ($hasCount) { 'before Count' } else { 'at the end' })) $is[$k] $null }
            }
        }
    }
    foreach ($one in $added) { Write-Host ('api added: ' + $one) }
    foreach ($one in $soft) { Write-Host ('api note: ' + $one) }
    foreach ($one in $refused) { Write-Host ('api REFUSED: ' + $one) }
    Write-Host ('api compare ({0}): {1} added, {2} notes, {3} refused' -f $kind, $added.Count, $soft.Count, $refused.Count)
    if ($refused.Count -gt 0) { throw ('the API surface breaks the ' + $kind + ' promise: ' + $refused.Count + ' refused changes') }
    return [pscustomobject]@{ added = $added.Count; notes = $soft.Count; refused = 0 }
}

if ($Compare) {
    if (-not $With) { throw '-Compare needs -With <new surface file>' }
    Compare-Surface $Compare $With $Kind
    return
}
if (-not $Include) { $Include = Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'include\zsdk' }
if (-not $Version) {
    $text = [System.IO.File]::ReadAllText((Join-Path $Include 'version.h'))
    $Version = '{0}.{1}.{2}' -f [regex]::Match($text, 'ZSDK_VERSION_MAJOR (\d+)').Groups[1].Value, [regex]::Match($text, 'ZSDK_VERSION_MINOR (\d+)').Groups[1].Value, [regex]::Match($text, 'ZSDK_VERSION_PATCH (\d+)').Groups[1].Value
}
$lines = New-Surface $Include $Version (Read-Tags $Tags)
if ($Out) {
    $parent = Split-Path $Out -Parent
    if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
    [System.IO.File]::WriteAllText($Out, (($lines -join "`n") + "`n"), (New-Object System.Text.UTF8Encoding($false)))
    Write-Host ('api surface {0}: {1} declarations -> {2}' -f $Version, ($lines.Count - 2), $Out)
}
else { $lines }
