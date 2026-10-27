[CmdletBinding()]
param(
    [string]$Doc,
    [string]$Twin,
    [string]$Include,
    [string]$Tags,
    [string]$Release,
    [string]$Work,
    [switch]$Keep
)

$ErrorActionPreference = 'Stop'
$sdk = [System.IO.Path]::GetFullPath((Split-Path (Split-Path $PSScriptRoot -Parent) -Parent)).TrimEnd('\')
if (-not $Tags) { $Tags = Join-Path $PSScriptRoot 'tags.txt' }
if (-not $Include) { $Include = Join-Path $sdk 'include\zsdk' }
$explicit = [bool]$Doc
if (-not $Doc) {
    foreach ($one in @((Join-Path $sdk 'docs\API.md'), (Join-Path (Split-Path (Split-Path $sdk -Parent) -Parent) 'docs\API.md'))) {
        if (Test-Path -LiteralPath $one) { $Doc = $one; break }
    }
    if (-not $Doc) { throw 'docs\API.md was found neither in this tree nor at the package root: pass -Doc' }
}
$Doc = [System.IO.Path]::GetFullPath($Doc)
$docs = New-Object System.Collections.Generic.List[object]
if ($Twin) { $Twin = [System.IO.Path]::GetFullPath($Twin) }
elseif ($explicit -and (Split-Path $Doc -Leaf) -match '^(.+)-UA\.md$') {
    $english = Join-Path (Split-Path $Doc -Parent) ($Matches[1] + '.md')
    if (Test-Path -LiteralPath $english) { $Twin = $english }
}
$docs.Add([pscustomobject]@{ path = $Doc; twin = $Twin })
if (-not $explicit -and -not $Twin) {
    $ukrainian = Join-Path (Split-Path $Doc -Parent) 'API-UA.md'
    if (Test-Path -LiteralPath $ukrainian) { $docs.Add([pscustomobject]@{ path = $ukrainian; twin = $Doc }) }
}
foreach ($d in $docs) {
    if (-not (Test-Path -LiteralPath $d.path)) { throw ('the API reference is missing: ' + $d.path) }
    if ($d.twin -and -not (Test-Path -LiteralPath $d.twin)) { throw ('the twin reference is missing: ' + $d.twin) }
}
$uaLabel = -join ([char[]]@(0x0421, 0x0442, 0x0430, 0x0431, 0x0456, 0x043B, 0x044C, 0x043D, 0x0456, 0x0441, 0x0442, 0x044C))
$stabilityLabel = '^\*\*(?:Stability|' + $uaLabel + '):\*\*\s*(Stable|Provisional|Legacy|Internal)\b'
$Include = [System.IO.Path]::GetFullPath($Include).TrimEnd('\')
$script:Tagged = @('Stable', 'Provisional', 'Legacy', 'Internal')
$failures = New-Object System.Collections.Generic.List[string]
$warnings = New-Object System.Collections.Generic.List[string]

function Read-Surface([string]$tagFile) {
    $lines = @(& (Join-Path $PSScriptRoot 'surface.ps1') -Include $Include -Tags $tagFile -Version 'doc')
    if ($lines.Count -lt 2 -or $lines[0] -ne 'zsdk-api 1') { throw 'tools\api\surface.ps1 gave no surface list' }
    $out = New-Object System.Collections.Generic.List[object]
    foreach ($line in ($lines | Select-Object -Skip 2)) {
        if (-not $line) { continue }
        $f = $line.Split("`t")
        $out.Add([pscustomobject]@{ header = $f[0]; kind = $f[1]; name = $f[2]; slot = $f[3]; text = $f[4]; tag = $f[5]; key = ($f[0] + '|' + $f[1] + '|' + $f[2] + '|' + $f[4]) })
    }
    return $out
}

function Get-Leaf([string]$name) {
    $at = $name.LastIndexOf('::')
    if ($at -lt 0) { return $name }
    return $name.Substring($at + 2)
}

function Get-Owner([string]$name) {
    $at = $name.LastIndexOf('::')
    if ($at -lt 0) { return '' }
    return $name.Substring(0, $at)
}

function Get-Words([string]$code) {
    $set = New-Object 'System.Collections.Generic.HashSet[string]'
    foreach ($m in [regex]::Matches($code, '~?[A-Za-z_][A-Za-z0-9_]*')) { [void]$set.Add($m.Value) }
    return , $set
}

function Get-Spans([string]$line) {
    $out = New-Object System.Collections.Generic.List[string]
    foreach ($m in [regex]::Matches($line, '`([^`]+)`')) { $out.Add($m.Groups[1].Value) }
    return , $out
}

function Close-Block($section, $block) {
    if ($null -ne $block -and $null -ne $section -and $block.code.Trim()) { $section.blocks.Add($block) }
}

function New-Block {
    return [pscustomobject]@{ code = ''; words = (New-Object 'System.Collections.Generic.HashSet[string]'); fenced = $false }
}

function Read-Doc([string]$path) {
    $leaf = Split-Path $path -Leaf
    $lines = [System.IO.File]::ReadAllLines($path, (New-Object System.Text.UTF8Encoding($false)))
    $sections = New-Object System.Collections.Generic.List[object]
    $snippets = New-Object System.Collections.Generic.List[object]
    $section = $null
    $block = $null
    $fence = $null
    $marker = $null
    $markerLine = 0
    $number = 0
    foreach ($raw in $lines) {
        $number++
        $line = $raw.TrimEnd()
        if ($null -ne $fence) {
            if ($line -match '^\s*```\s*$') {
                $code = ($fence.lines -join "`n")
                if ($null -ne $section) {
                    $section.blocks.Add([pscustomobject]@{ code = $code; words = (Get-Words $code); fenced = $true })
                    $section.code.Add($code)
                    $section.fences.Add([pscustomobject]@{ key = ($fence.lang + '|' + $fence.marker + '|' + $code); start = $fence.start })
                }
                if ($fence.lang -eq 'cpp') {
                    if (-not $fence.marker) { $failures.Add(('a cpp snippet without a doccheck marker at {0}:{1}' -f $leaf, $fence.start)) }
                    else { $snippets.Add([pscustomobject]@{ marker = $fence.marker; lines = $fence.lines; start = $fence.start; header = $(if ($null -ne $section) { $section.header } else { '' }) }) }
                }
                $fence = $null
                continue
            }
            $fence.lines.Add($raw)
            continue
        }
        if ($line -match '^\s*```\s*([A-Za-z0-9+_-]*)\s*$') {
            Close-Block $section $block
            $block = $null
            $use = $null
            if ($marker -and $markerLine -eq $number - 1) { $use = $marker }
            $fence = @{ lang = $Matches[1].ToLowerInvariant(); lines = (New-Object System.Collections.Generic.List[string]); start = $number + 1; marker = $use }
            $marker = $null
            continue
        }
        if ($line -match '^<!--\s*doccheck:\s*(.+?)\s*-->$') {
            Close-Block $section $block
            $block = $null
            $marker = $Matches[1]
            $markerLine = $number
            continue
        }
        if ($line -match '^##\s+(.*)$' -and $line -notmatch '^###') {
            Close-Block $section $block
            $block = $null
            $title = $Matches[1]
            $m = [regex]::Match($title, '`(?:zsdk/)?((?:test/|verbs/)?[A-Za-z_][A-Za-z0-9_]*\.h)`')
            $section = [pscustomobject]@{ header = $(if ($m.Success) { $m.Groups[1].Value } else { $null }); title = $title; line = $number; blocks = (New-Object System.Collections.Generic.List[object]); code = (New-Object System.Collections.Generic.List[string]); stability = $null; tagLines = (New-Object System.Collections.Generic.List[object]); fences = (New-Object System.Collections.Generic.List[object]); spans = (New-Object 'System.Collections.Generic.HashSet[string]') }
            $sections.Add($section)
            continue
        }
        if ($null -eq $section) { continue }
        if (-not $line.Trim() -or $line -match '^#{3,6}\s') {
            Close-Block $section $block
            $block = $null
            if ($line -match '^#{3,6}\s') {
                $spans = Get-Spans $line
                foreach ($one in $spans) { [void]$section.spans.Add($one) }
                if ($spans.Count -gt 0) { $text = $spans -join ' '; $section.code.Add($text); $section.blocks.Add([pscustomobject]@{ code = $text; words = (Get-Words $text); fenced = $false }) }
            }
            continue
        }
        if ($line -match $stabilityLabel) {
            if ($section.stability) { $failures.Add(('{0}: two Stability lines in the section {1} (line {2})' -f $leaf, $section.title, $number)) }
            $section.stability = $Matches[1]
        }
        elseif ($line -match '^\s*(?:[-*]\s+)?\*\*(Stable|Provisional|Legacy|Internal)\b[^*]*\*\*') {
            $section.tagLines.Add([pscustomobject]@{ tag = $Matches[1]; spans = (Get-Spans $line); line = $number })
        }
        $spans = Get-Spans $line
        foreach ($one in $spans) { [void]$section.spans.Add($one) }
        if ($null -eq $block) { $block = New-Block }
        if ($spans.Count -eq 0) { continue }
        $text = $spans -join ' '
        $section.code.Add($text)
        $block.code = $block.code + ' ' + $text
        foreach ($w in (Get-Words $text)) { [void]$block.words.Add($w) }
    }
    if ($null -ne $fence) { $failures.Add(('{0}: an unclosed code fence starting at line {1}' -f $leaf, $fence.start)) }
    Close-Block $section $block
    return @{ sections = $sections; snippets = $snippets }
}

function Test-SpanNames($entry, [string]$span) {
    $s = ($span -split '[\s(<{\[]', 2)[0].Trim()
    if (-not $s) { return $false }
    if ($s.StartsWith('zsdk::')) { $s = $s.Substring(6) }
    return $entry.name -eq $s -or $entry.name.EndsWith('::' + $s)
}

function Test-Tagged($section, $entry, [string]$tag, $owners) {
    foreach ($tl in $section.tagLines) {
        if ($tl.tag -ne $tag) { continue }
        foreach ($span in $tl.spans) {
            if (Test-SpanNames $entry $span) { return $true }
            $owner = Get-Owner $entry.name
            if ($owners.ContainsKey($owner) -and (Test-SpanNames ([pscustomobject]@{ name = $owner }) $span)) { return $true }
        }
    }
    return $false
}

function Test-Mentioned($section, $entry, $owners) {
    $leaf = Get-Leaf $entry.name
    $owner = Get-Owner $entry.name
    $ownerLeaf = Get-Leaf $owner
    $all = $section.code -join "`n"
    if ($entry.kind -eq 'namespace') {
        $tail = if ($entry.name.StartsWith('zsdk::')) { $entry.name.Substring(6) } else { $entry.name }
        return [regex]::IsMatch($all, '(?<![A-Za-z0-9_])' + [regex]::Escape($tail) + '(?![A-Za-z0-9_])')
    }
    $member = $owners.ContainsKey($owner)
    if ($member) {
        if ($leaf.StartsWith('~') -or $leaf -eq $ownerLeaf -or ($leaf.StartsWith('operator') -and $leaf -ne 'operator==')) { $leaf = $ownerLeaf }
        foreach ($b in $section.blocks) {
            if (-not $b.words.Contains($ownerLeaf)) { continue }
            if ($leaf -eq 'operator==') { if ($b.code.Contains('==')) { return $true }; continue }
            if ($b.words.Contains($leaf)) { return $true }
        }
        return $false
    }
    foreach ($b in $section.blocks) { if ($b.words.Contains($leaf)) { return $true } }
    return $false
}

$rules = New-Object System.Collections.Generic.List[object]
$plain = New-Object System.Collections.Generic.List[string]
foreach ($line in [System.IO.File]::ReadAllLines($Tags)) {
    $t = $line.Trim()
    if (-not $t) { continue }
    $parts = @($t -split '\s+')
    if ($parts[1] -eq 'header') { $rules.Add(@{ tag = $parts[0]; header = $parts[2]; pattern = $null }) } else { $rules.Add(@{ tag = $parts[0]; header = $null; pattern = $parts[1] }) }
    if ($parts[0] -ne 'Internal') { $plain.Add($t) }
}
$plainTags = Join-Path ([System.IO.Path]::GetTempPath()) ('zsdk-doccheck-tags-' + [guid]::NewGuid().ToString('N').Substring(0, 8) + '.txt')
[System.IO.File]::WriteAllLines($plainTags, $plain, (New-Object System.Text.UTF8Encoding($false)))
try {
    $public = Read-Surface $Tags
    $full = Read-Surface $plainTags
}
finally { Remove-Item -LiteralPath $plainTags -Force -ErrorAction SilentlyContinue }
$publicKeys = New-Object 'System.Collections.Generic.HashSet[string]'
foreach ($e in $public) { [void]$publicKeys.Add($e.key) }
$internal = New-Object System.Collections.Generic.List[object]
foreach ($e in $full) { if (-not $publicKeys.Contains($e.key)) { $internal.Add($e) } }
$owners = @{}
foreach ($e in $full) { if ($e.kind -eq 'type' -or $e.kind -eq 'enum') { $owners[$e.name] = $true } }

$headers = New-Object System.Collections.Generic.List[string]
foreach ($f in Get-ChildItem -LiteralPath $Include -Recurse -File -Filter '*.h') { $headers.Add($f.FullName.Substring($Include.Length + 1).Replace('\', '/')) }
$internalHeaders = @{}
foreach ($r in $rules) { if ($r.header -and $r.tag -eq 'Internal') { $internalHeaders[$r.header] = $true } }
foreach ($r in $rules) {
    if ($r.header) { if (-not $headers.Contains($r.header)) { $warnings.Add('tags.txt names the header ' + $r.header + ', which does not exist') }; continue }
    $hit = @($full | Where-Object { $_.name -like $r.pattern }).Count
    if ($hit -eq 0) { $warnings.Add('tags.txt pattern "' + $r.tag + ' ' + $r.pattern + '" matches no declaration in the headers') }
}

$parsedDocs = @{}
function Get-Parsed([string]$path) {
    if (-not $parsedDocs.ContainsKey($path)) { $parsedDocs[$path] = Read-Doc $path }
    return $parsedDocs[$path]
}

function Add-Warning([string]$text) {
    if (-not $warnings.Contains($text)) { $warnings.Add($text) }
}

function Test-Doc([string]$path) {
    $leaf = Split-Path $path -Leaf
    $parsed = Get-Parsed $path
    $byHeader = @{}
    foreach ($s in $parsed.sections) {
        if (-not $s.header) { continue }
        if ($byHeader.ContainsKey($s.header)) { $failures.Add($leaf + ': two sections for ' + $s.header + ': ' + $byHeader[$s.header].title + ' and ' + $s.title); continue }
        if (-not $headers.Contains($s.header)) { $failures.Add($leaf + ': the section ' + $s.title + ' names ' + $s.header + ', which is not a header in ' + $Include) }
        if (-not $s.stability) { $failures.Add($leaf + ': the section ' + $s.title + ' has no "**Stability:** <tag>" line') }
        $byHeader[$s.header] = $s
    }
    foreach ($h in $headers) {
        if (-not $byHeader.ContainsKey($h)) { $failures.Add($leaf + ': no section for the header zsdk/' + $h); continue }
        if ($internalHeaders.ContainsKey($h) -and $byHeader[$h].stability -ne 'Internal') { $failures.Add($leaf + ': tags.txt marks zsdk/' + $h + ' Internal, its section says ' + $byHeader[$h].stability) }
    }
    $undocumented = New-Object System.Collections.Generic.List[string]
    $mismatch = New-Object System.Collections.Generic.List[string]
    $documented = 0
    foreach ($e in $public) {
        if (-not $byHeader.ContainsKey($e.header)) { $undocumented.Add(('{0} {1} {2} (no section)' -f $e.header, $e.kind, $e.name)); continue }
        $s = $byHeader[$e.header]
        if (-not (Test-Mentioned $s $e $owners)) { $undocumented.Add(('{0} {1} {2}' -f $e.header, $e.kind, $e.name)); continue }
        $documented++
        if ($s.stability -and $e.tag -ne $s.stability -and -not (Test-Tagged $s $e $e.tag $owners)) { $mismatch.Add(('{0} {1} is {2} in tags.txt; the section says {3} and no "**{2}**" line names it' -f $e.header, $e.name, $e.tag, $s.stability)) }
    }
    $hidden = 0
    foreach ($e in $internal) {
        if ($internalHeaders.ContainsKey($e.header)) { $hidden++; continue }
        if (-not $byHeader.ContainsKey($e.header)) { $undocumented.Add(('{0} {1} {2} (Internal, no section)' -f $e.header, $e.kind, $e.name)); continue }
        $s = $byHeader[$e.header]
        $detail = $e.name -match '::detail(::|$)'
        if ($detail -and $e.kind -ne 'namespace' -and $e.name -notmatch '::detail::') {
            Add-Warning ('tags.txt pattern *::detail marks the {0} {1} Internal, so the API surface file leaves it out; it is documented as public' -f $e.kind, $e.name)
            if (Test-Mentioned $s $e $owners) { $documented++ } else { $undocumented.Add(('{0} {1} {2}' -f $e.header, $e.kind, $e.name)) }
            continue
        }
        if ($detail) {
            $ok = $false
            foreach ($tl in $s.tagLines) { if ($tl.tag -eq 'Internal') { foreach ($span in $tl.spans) { if ($span -match '(^|::)detail$') { $ok = $true } } } }
            if ($ok) { $hidden++ } else { $undocumented.Add(('{0} {1} {2} (Internal: name the detail namespace on a "**Internal**" line)' -f $e.header, $e.kind, $e.name)) }
            continue
        }
        if (Test-Tagged $s $e 'Internal' $owners) { $hidden++ } else { $undocumented.Add(('{0} {1} {2} (Internal: name it on a "**Internal**" line)' -f $e.header, $e.kind, $e.name)) }
    }
    foreach ($s in $byHeader.Values) {
        foreach ($tl in $s.tagLines) {
            foreach ($span in $tl.spans) {
                $hits = @($full | Where-Object { $_.header -eq $s.header -and (Test-SpanNames $_ $span) })
                if ($hits.Count -eq 0) { continue }
                $tags = @($hits | ForEach-Object { $tag = $_.tag; if (-not $publicKeys.Contains($_.key)) { $tag = 'Internal' }; $tag } | Sort-Object -Unique)
                if ($tags -notcontains $tl.tag) { $mismatch.Add(('{0} line {1} calls `{2}` {3}, tags.txt says {4}' -f $s.header, $tl.line, $span, $tl.tag, ($tags -join '/'))) }
            }
        }
    }
    foreach ($one in $undocumented) { Write-Host ('UNDOCUMENTED ' + $leaf + ': ' + $one) }
    foreach ($one in $mismatch) { Write-Host ('TAG ' + $leaf + ': ' + $one) }
    Write-Host ('doccheck {7}: {0} public declarations in {1} headers, {2} documented, {3} undocumented, {4} internal names named, {5} tag mismatches, {6} snippets' -f $public.Count, $headers.Count, $documented, $undocumented.Count, $hidden, $mismatch.Count, $parsed.snippets.Count, $leaf)
    return [pscustomobject]@{ undocumented = $undocumented.Count; mismatch = $mismatch.Count; parsed = $parsed }
}

function Compare-Section($x, $y, [string]$label, [string]$leaf, [string]$twinLeaf) {
    $fences = 0
    $one = $(if ($x.stability) { $x.stability } else { '(none)' })
    $two = $(if ($y.stability) { $y.stability } else { '(none)' })
    if ($one -ne $two) { $failures.Add(('twin: {0} is {1} in {2} and {3} in {4}' -f $label, $one, $leaf, $two, $twinLeaf)) }
    $tx = (@($x.tagLines | ForEach-Object { $_.tag + ' ' + ($_.spans -join ' ') }) -join ' / ')
    $ty = (@($y.tagLines | ForEach-Object { $_.tag + ' ' + ($_.spans -join ' ') }) -join ' / ')
    if ($tx -ne $ty) { $failures.Add(('twin: the tag lines of {0} differ: {1} has "{2}", {3} has "{4}"' -f $label, $leaf, $tx, $twinLeaf, $ty)) }
    if ($x.fences.Count -ne $y.fences.Count) { $failures.Add(('twin: {0} has {1} code blocks in {2} and {3} in {4}' -f $label, $x.fences.Count, $leaf, $y.fences.Count, $twinLeaf)) }
    for ($k = 0; $k -lt [Math]::Min($x.fences.Count, $y.fences.Count); $k++) {
        $fences++
        if ($x.fences[$k].key -ne $y.fences[$k].key) { $failures.Add(('twin: code block {0} of {1} differs: {2} line {3}, {4} line {5}; keep the code and its marker identical' -f ($k + 1), $label, $leaf, $x.fences[$k].start, $twinLeaf, $y.fences[$k].start)) }
    }
    $lost = @($y.spans | Where-Object { -not $x.spans.Contains($_) } | Sort-Object)
    $added = @($x.spans | Where-Object { -not $y.spans.Contains($_) } | Sort-Object)
    if ($lost.Count -gt 0) { $failures.Add(('twin: {0} in {1} lacks {2} code spans of {3}: {4}' -f $label, $leaf, $lost.Count, $twinLeaf, ((@($lost | Select-Object -First 8) | ForEach-Object { '`' + $_ + '`' }) -join ', '))) }
    if ($added.Count -gt 0) { $failures.Add(('twin: {0} in {1} has {2} code spans that {3} lacks: {4}' -f $label, $leaf, $added.Count, $twinLeaf, ((@($added | Select-Object -First 8) | ForEach-Object { '`' + $_ + '`' }) -join ', '))) }
    return @($fences, $y.spans.Count)
}

function Compare-Twin([string]$path, [string]$twinPath) {
    $leaf = Split-Path $path -Leaf
    $twinLeaf = Split-Path $twinPath -Leaf
    $a = (Get-Parsed $path).sections
    $b = (Get-Parsed $twinPath).sections
    $before = $failures.Count
    $fences = 0
    $spans = 0
    $compared = 0
    $ah = @{}
    $bh = @{}
    foreach ($s in $a) { if ($s.header) { $ah[$s.header] = $s } }
    foreach ($s in $b) { if ($s.header) { $bh[$s.header] = $s } }
    foreach ($s in $b) {
        if (-not $s.header) { continue }
        if (-not $ah.ContainsKey($s.header)) { $failures.Add(('twin: {0} has no section for zsdk/{1} ({2} line {3})' -f $leaf, $s.header, $twinLeaf, $s.line)); continue }
        $counts = Compare-Section $ah[$s.header] $s ('zsdk/' + $s.header) $leaf $twinLeaf
        $fences += $counts[0]
        $spans += $counts[1]
        $compared++
    }
    foreach ($s in $a) { if ($s.header -and -not $bh.ContainsKey($s.header)) { $failures.Add(('twin: {0} has the section zsdk/{1} (line {2}), which {3} lacks' -f $leaf, $s.header, $s.line, $twinLeaf)) } }
    $ao = @($a | Where-Object { $_.header -and $bh.ContainsKey($_.header) } | ForEach-Object { $_.header }) -join ', '
    $bo = @($b | Where-Object { $_.header -and $ah.ContainsKey($_.header) } | ForEach-Object { $_.header }) -join ', '
    if ($ao -ne $bo) { $failures.Add(('twin: the header sections of {0} are in another order than in {1}' -f $leaf, $twinLeaf)) }
    $ap = @($a | Where-Object { -not $_.header })
    $bp = @($b | Where-Object { -not $_.header })
    if ($ap.Count -ne $bp.Count) { $failures.Add(('twin: {0} has {1} sections without a header, {2} has {3}' -f $leaf, $ap.Count, $twinLeaf, $bp.Count)) }
    for ($i = 0; $i -lt [Math]::Min($ap.Count, $bp.Count); $i++) {
        $counts = Compare-Section $ap[$i] $bp[$i] ('"' + $bp[$i].title + '" (' + $twinLeaf + ' line ' + $bp[$i].line + ')') $leaf $twinLeaf
        $fences += $counts[0]
        $spans += $counts[1]
        $compared++
    }
    $found = $failures.Count - $before
    Write-Host ('twin {0} = {1}: {2} sections, {3} code blocks and {4} code spans compared, {5} differences' -f $leaf, $twinLeaf, $compared, $fences, $spans, $found)
}
$results = New-Object System.Collections.Generic.List[object]
foreach ($d in $docs) {
    $results.Add((Test-Doc $d.path))
    if ($d.twin) { Compare-Twin $d.path $d.twin }
}
$undocumentedCount = 0
$mismatchCount = 0
foreach ($r in $results) { $undocumentedCount += $r.undocumented; $mismatchCount += $r.mismatch }
$snipDoc = Split-Path $docs[0].path -Leaf

$snips = $results[0].parsed.snippets
$units = @{ present = 0; overlay = 0; entry = 0; test = 0 }
foreach ($sn in $snips) {
    $words = @($sn.marker -split '\s+')
    if (@('present', 'overlay', 'entry', 'test') -notcontains $words[0]) { $failures.Add(('{0}: the snippet at line {1} has the marker "{2}"; it starts with present, overlay, entry or test' -f $snipDoc, $sn.start, $sn.marker)); continue }
    $units[$words[0]]++
}
foreach ($one in $warnings) { Write-Host ('warning: ' + $one) }
foreach ($one in $failures) { Write-Host ('FAIL ' + $one) }
Write-Host ('doccheck: {0} reference(s), {1} undocumented, {2} tag mismatches, {3} failures, {4} warnings' -f $docs.Count, $undocumentedCount, $mismatchCount, $failures.Count, $warnings.Count)
Write-Host ('snippets of {5}: {0} marked, present {1}, overlay {2}, entry {3}, test {4}' -f $snips.Count, $units.present, $units.overlay, $units.entry, $units.test, $snipDoc)
$broken = $failures.Count -gt 0 -or $undocumentedCount -gt 0 -or $mismatchCount -gt 0
if (-not $Release) {
    if ($broken) { throw ('doccheck failed: ' + $undocumentedCount + ' undocumented, ' + $mismatchCount + ' tag mismatches, ' + $failures.Count + ' failures') }
    Write-Host 'snippets were not compiled: pass -Release <a cut ZoneSDK release folder> to build them in a scratch add-on'
    return
}

$Release = [System.IO.Path]::GetFullPath($Release).TrimEnd('\')
if (-not (Test-Path -LiteralPath (Join-Path $Release 'MANIFEST.sha256'))) { throw ('-Release is not a cut ZoneSDK release, MANIFEST.sha256 is missing: ' + $Release) }
$ownWork = -not $Work
if ($ownWork) { $Work = Join-Path ([System.IO.Path]::GetTempPath()) ('zsdk-doccheck-' + [guid]::NewGuid().ToString('N').Substring(0, 8)) }
$Work = [System.IO.Path]::GetFullPath($ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Work)).TrimEnd('\')
if (($Work + '\').StartsWith($Release + '\', [System.StringComparison]::OrdinalIgnoreCase)) { throw ('the work folder lies inside the release, which is never edited: ' + $Work) }
$run = Join-Path $Work ('run-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$addon = Join-Path $run 'ZoneDocCheck'
New-Item -ItemType Directory -Force -Path $run | Out-Null
function Invoke-Child([string]$script, [string[]]$arguments) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $out = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $script @arguments 2>&1
        $exit = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $previous }
    return @{ lines = @($out | ForEach-Object { [string]$_ }); code = $exit }
}
$made = Invoke-Child (Join-Path $Release 'tools\new-addon.ps1') @('-Name', 'ZoneDocCheck', '-Target', $addon, '-Release', $Release, '-Author', 'ZoneSDK docs')
if ($made.code -ne 0) { foreach ($l in $made.lines) { Write-Host $l }; throw 'new-addon could not create the scratch add-on' }
$utf8 = New-Object System.Text.UTF8Encoding($false)
$rename = { param([string]$text) $text.Replace('ztemplate', 'zdoccheck').Replace('ZoneTemplate', 'ZoneDocCheck').Replace('TEMPLATE_', 'DOCCHECK_').Replace('"template.h"', '"doccheck.h"') }
$groups = [ordered]@{}
$entry = New-Object System.Collections.Generic.List[string]
$where = @{}
$n = 0
foreach ($sn in $snips) {
    $words = @($sn.marker -split '\s+')
    $unit = $words[0]
    $body = $words -contains 'body'
    $group = $null
    foreach ($w in $words) { if ($w -match '^group=([A-Za-z0-9_]+)$') { $group = $Matches[1] } }
    $n++
    $lines = @($sn.lines | ForEach-Object { & $rename $_ })
    if ($unit -eq 'entry') {
        $entry.Add('')
        foreach ($l in $lines) { $entry.Add($l) }
        continue
    }
    if ($words -contains 'file') {
        $name = 'docf_{0:D2}.cpp' -f $n
        $file = if ($unit -eq 'overlay') { Join-Path $addon ('src\overlay\' + $name) } else { Join-Path $addon ('src\' + $name) }
        [System.IO.File]::WriteAllText($file, (($lines -join "`n") + "`n"), $utf8)
        $where[$name] = $sn
        continue
    }
    if ($unit -eq 'test') {
        $file = Join-Path $addon ('tests\doc_{0:D2}.cpp' -f $n)
        $text = New-Object System.Collections.Generic.List[string]
        foreach ($inc in 'zsdk/present.h', 'zsdk/test/addon.h', 'zsdk/test/check.h', 'zsdk/test/scan.h') { $text.Add('#' + 'include <' + $inc + '>') }
        $text.Add('')
        $text.Add('#' + 'include "doccheck.h"')
        $text.Add('')
        $text.Add('int main()')
        $text.Add('{')
        foreach ($l in $lines) { $text.Add('    ' + $l) }
        $text.Add(('    return check::Finish("doc_{0:D2}");' -f $n))
        $text.Add('}')
        [System.IO.File]::WriteAllText($file, (($text -join "`n") + "`n"), $utf8)
        $where[[System.IO.Path]::GetFileName($file)] = $sn
        continue
    }
    if (-not $group) { $group = 'g{0:D2}' -f $n }
    $key = $unit + '|' + $group
    if (-not $groups.Contains($key)) { $groups[$key] = @{ unit = $unit; group = $group; parts = (New-Object System.Collections.Generic.List[object]) } }
    $groups[$key].parts.Add(@{ body = $body; lines = $lines; n = $n; snippet = $sn })
}
foreach ($g in $groups.Values) {
    $isOverlay = $g.unit -eq 'overlay'
    $name = if ($isOverlay) { 'doco_' + $g.group + '.cpp' } else { 'docp_' + $g.group + '.cpp' }
    $file = if ($isOverlay) { Join-Path $addon ('src\overlay\' + $name) } else { Join-Path $addon ('src\' + $name) }
    $text = New-Object System.Collections.Generic.List[string]
    if ($isOverlay) { $text.Add('#' + 'include <zsdk/overlay.h>') } else { $text.Add('#' + 'include <zsdk/present.h>') }
    $text.Add('')
    $text.Add('#' + 'include "doccheck.h"')
    if ($isOverlay) { $text.Add('#' + 'include "overlay/page.h"') }
    $text.Add('')
    $text.Add('namespace zdoccheck::doc_' + $g.group)
    $text.Add('{')
    foreach ($p in $g.parts) {
        if ($p.body) {
            if ($isOverlay) { $text.Add(('    void Overlay{0:D2}([[maybe_unused]] zsdk::Overlay &o)' -f $p.n)) } else { $text.Add(('    void Present{0:D2}([[maybe_unused]] uint64_t now)' -f $p.n)) }
            $text.Add('    {')
            foreach ($l in $p.lines) { $text.Add('        ' + $l) }
            $text.Add('    }')
        }
        else { foreach ($l in $p.lines) { $text.Add('    ' + $l) } }
        $text.Add('')
    }
    $text.Add('}')
    [System.IO.File]::WriteAllText($file, (($text -join "`n") + "`n"), $utf8)
    $where[$name] = $g.parts[0].snippet
}
if ($entry.Count -gt 0) {
    $text = New-Object System.Collections.Generic.List[string]
    $text.Add('#' + 'include <zsdk/shell.h>')
    $text.Add('')
    $text.Add('#' + 'include "overlay/page.h"')
    $text.Add('#' + 'include "doccheck.h"')
    foreach ($l in $entry) { $text.Add($l) }
    [System.IO.File]::WriteAllText((Join-Path $addon 'src\entry.cpp'), (($text -join "`n") + "`n"), $utf8)
}
$log = Join-Path $run 'build.txt'
$rootBefore = $env:ZSDK_ROOT
$env:ZSDK_ROOT = $Release
try { $result = Invoke-Child (Join-Path $addon 'build\make.ps1') @('-Tests') }
finally { $env:ZSDK_ROOT = $rootBefore }
$built = $result.lines
$code = $result.code
[System.IO.File]::WriteAllLines($log, [string[]]$built, $utf8)
$summary = @($built | Where-Object { $_ -match '^tests passed \d+ failed \d+' }) | Select-Object -Last 1
$own = @($built | Where-Object { $_ -match '^warnings \d+' }) | Select-Object -Last 1
$buildLog = Join-Path $addon 'build\last-build.log'
if ($code -ne 0) {
    $lines = if (Test-Path -LiteralPath $buildLog) { @(Get-Content -LiteralPath $buildLog -Encoding UTF8) } else { $built }
    foreach ($l in $lines) {
        $m = [regex]::Match($l, '(doc[opf]?_[A-Za-z0-9_]+\.cpp|entry\.cpp)\((\d+)\)')
        if ($m.Success -or $l -match '^FAIL ' -or $l -match ' error ') {
            $note = ''
            if ($m.Success -and $where.ContainsKey($m.Groups[1].Value)) { $note = ' [snippet at ' + $snipDoc + ' line ' + $where[$m.Groups[1].Value].start + ']' }
            Write-Host ($l + $note)
        }
    }
    Write-Host ('doccheck: the snippets did not build, see ' + $log + ' and ' + $buildLog)
    throw 'doccheck failed: the marked snippets do not compile in the scratch add-on'
}
Write-Host ('snippets compiled in the scratch add-on ZoneDocCheck from ' + $Release + ': ' + $(if ($summary) { $summary } else { 'no test summary' }) + $(if ($own) { ', ' + $own } else { '' }))
if (-not $Keep) {
    Remove-Item -LiteralPath $run -Recurse -Force
    if ($ownWork) { Remove-Item -LiteralPath $Work -Recurse -Force }
}
else { Write-Host ('kept the scratch add-on at ' + $addon) }
if ($broken) { throw ('doccheck failed: ' + $undocumentedCount + ' undocumented, ' + $mismatchCount + ' tag mismatches, ' + $failures.Count + ' failures') }
