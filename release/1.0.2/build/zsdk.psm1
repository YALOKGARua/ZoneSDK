$script:SdkRoot = Split-Path (Split-Path $PSCommandPath -Parent) -Parent
$script:Common = '/nologo /std:c++20 /W4 /permissive- /utf-8 /Zc:__cplusplus /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS'
$script:BaseLibs = @('user32.lib', 'shell32.lib', 'bcrypt.lib')
$script:Wrapper = $null
$script:Generated = $null
$script:Lines = $null
$script:Warnings = 0
$script:SdkWarnings = 0
$script:SdkPaths = @()
$script:Imported = $false
$script:Toolchain = @()
$script:Studio = $null
$script:Brepro = $null
$script:Held = @{}
$script:VsRange = '[17.10,19.0)'
$script:VcTools = 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64'
$script:Hint = 'Install Visual Studio 2022 17.10 or newer or Visual Studio 2026 (the free Build Tools are enough) with the workload "Desktop development with C++" (Microsoft.VisualStudio.Workload.VCTools) and a Windows 11 SDK, for example: winget install --id Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended". A custom wrapper works too: set ZSDK_VCVARS to its vcvars64.bat.'
$script:Kept = @('TEMP', 'TMP', 'USERPROFILE', 'LOCALAPPDATA', 'APPDATA', 'HOMEDRIVE', 'HOMEPATH', 'HOMESHARE', 'USERNAME', 'USERDOMAIN', 'USERDNSDOMAIN', 'ComSpec', 'SystemRoot', 'SystemDrive', 'windir', 'PSModulePath', 'PROMPT', 'PUBLIC', 'ProgramData', 'ALLUSERSPROFILE', 'OneDrive')

function Get-ZsdkRoot {
    return $script:SdkRoot
}

function Get-ZsdkVersion {
    $text = Get-Content -LiteralPath (Join-Path $script:SdkRoot 'include\zsdk\version.h') -Raw
    $major = [regex]::Match($text, 'ZSDK_VERSION_MAJOR (\d+)').Groups[1].Value
    $minor = [regex]::Match($text, 'ZSDK_VERSION_MINOR (\d+)').Groups[1].Value
    $patch = [regex]::Match($text, 'ZSDK_VERSION_PATCH (\d+)').Groups[1].Value
    return ('{0}.{1}.{2}' -f $major, $minor, $patch)
}

function Get-ZsdkWork([string]$Leaf) {
    $base = $env:LOCALAPPDATA
    if (-not $base) { $base = [Environment]::GetFolderPath('LocalApplicationData') }
    if (-not $base) { throw 'LOCALAPPDATA is not set, so the ZoneSDK work folder is unknown; pass -Out' }
    $work = Join-Path $base ('ZoneSDK\' + (Get-ZsdkVersion) + '\work')
    if ($Leaf) { $work = Join-Path $work $Leaf }
    return $work
}

function Add-ZsdkLine([string]$line) {
    if ($null -ne $script:Lines) { $script:Lines.Add($line) }
}

function Reset-ZsdkLog([string]$head) {
    $script:Lines = New-Object System.Collections.Generic.List[string]
    $script:Warnings = 0
    $script:SdkWarnings = 0
    $script:SdkPaths = @()
    $script:Brepro = $null
    $script:Lines.Add($head)
}

function Get-ZsdkWarnings {
    return $script:Warnings
}

function Get-ZsdkSdkWarnings {
    return $script:SdkWarnings
}

function Get-ZsdkShell {
    if ($env:ComSpec) { return $env:ComSpec }
    return 'cmd.exe'
}

function Invoke-ZsdkCapture([string]$file, [string]$arguments, [System.Text.Encoding]$encoding) {
    $info = New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName = $file
    $info.Arguments = $arguments
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.StandardOutputEncoding = $encoding
    $process = [System.Diagnostics.Process]::Start($info)
    try {
        $err = $process.StandardError.ReadToEndAsync()
        $out = $process.StandardOutput.ReadToEnd()
        $process.WaitForExit()
        return @{ code = $process.ExitCode; out = $out; err = $err.Result }
    }
    finally { $process.Dispose() }
}

function Get-ZsdkVswhere {
    if ($env:ZSDK_VSWHERE) { return $env:ZSDK_VSWHERE }
    $programs = [Environment]::GetFolderPath('ProgramFilesX86')
    if (-not $programs) { return '' }
    return (Join-Path $programs 'Microsoft Visual Studio\Installer\vswhere.exe')
}

function Find-ZsdkStudio([string]$where) {
    $arguments = '-products * -version "{0}" -requires {1} -latest -format json -utf8' -f $script:VsRange, $script:VcTools
    $file = $where
    if ($where -match '\.(cmd|bat)$') {
        $arguments = '/d /s /c ""' + $where + '" ' + $arguments + '"'
        $file = Get-ZsdkShell
    }
    $run = Invoke-ZsdkCapture $file $arguments (New-Object System.Text.UTF8Encoding($false))
    $text = ([string]$run.out).Trim()
    if ($run.code -ne 0 -or -not $text) { return $null }
    try { $items = @(ConvertFrom-Json -InputObject $text | ForEach-Object { $_ } | Where-Object { $_ -and $_.installationPath }) }
    catch { return $null }
    if ($items.Count -eq 0) { return $null }
    $one = $items[0]
    $display = [string]$one.installationVersion
    if ($one.catalog -and $one.catalog.productDisplayVersion) { $display = [string]$one.catalog.productDisplayVersion }
    return @{ path = ([string]$one.installationPath).Trim(); version = [string]$one.installationVersion; display = $display; name = [string]$one.displayName }
}

function Resolve-ZsdkWrapper {
    $tried = New-Object System.Collections.Generic.List[string]
    $script:Studio = $null
    if ($env:ZSDK_VCVARS) {
        $tried.Add('ZSDK_VCVARS=' + $env:ZSDK_VCVARS)
        if (Test-Path -LiteralPath $env:ZSDK_VCVARS) { return $env:ZSDK_VCVARS }
    }
    for ($walk = $script:SdkRoot; $walk; $walk = Split-Path $walk -Parent) {
        $one = Join-Path $walk '_toolchain\vcvars-winsdk.bat'
        $tried.Add($one)
        if (Test-Path -LiteralPath $one) { return $one }
    }
    $where = Get-ZsdkVswhere
    if ($where) {
        $tried.Add('vswhere ' + $where)
        if (Test-Path -LiteralPath $where) {
            $studio = Find-ZsdkStudio $where
            if (-not $studio) { $tried.Add('vswhere lists no Visual Studio ' + $script:VsRange + ' with ' + $script:VcTools + ' (prerelease builds are not used)') }
            else {
                $bat = Join-Path $studio.path 'VC\Auxiliary\Build\vcvars64.bat'
                $tried.Add($bat)
                if (Test-Path -LiteralPath $bat) {
                    $script:Studio = $studio
                    Write-Host ('vswhere picked {0} {1} ({2}) at {3}' -f $studio.name, $studio.display, $studio.version, $studio.path)
                    return $bat
                }
            }
        }
    }
    throw ('no C++ toolchain was found. ' + $script:Hint + ' Tried: ' + ($tried -join '; '))
}

function Import-ZsdkToolchain {
    if ($script:Imported) {
        foreach ($one in $script:Toolchain) { Add-ZsdkLine $one }
        return
    }
    $script:Wrapper = Resolve-ZsdkWrapper
    $wanted = [string]$env:ZSDK_VCVARS_VER
    $extra = ''
    if ($wanted) {
        if ($wanted -notmatch '^14\.\d{1,2}(\.\d{1,5})?$') { throw ('ZSDK_VCVARS_VER must look like 14.44 or 14.44.35207, not ' + $wanted) }
        $extra = ' -vcvars_ver=' + $wanted
    }
    $key = $script:Wrapper + '|' + $wanted
    $ready = [Environment]::GetEnvironmentVariable('ZSDK_TOOLCHAIN', 'Process') -ceq $key -and [string]$env:VCToolsInstallDir -and @(foreach ($tool in 'cl.exe', 'link.exe', 'dumpbin.exe', 'rc.exe') { if (Get-Command $tool -ErrorAction SilentlyContinue) { $tool } }).Count -eq 4
    if (-not $ready) {
        $run = Invoke-ZsdkCapture (Get-ZsdkShell) ('/d /u /s /c "call "{0}"{1} >nul 2>&1 & set"' -f $script:Wrapper, $extra) ([System.Text.Encoding]::Unicode)
        $text = [string]$run.out
        if ($text -notmatch '(?im)^PATH=') { throw ('the toolchain wrapper printed no environment: ' + $script:Wrapper) }
        foreach ($line in ($text -split "`r?`n")) {
            $at = $line.IndexOf('=')
            if ($at -le 0) { continue }
            $name = $line.Substring(0, $at)
            $value = $line.Substring($at + 1)
            if (-not $value -or $script:Kept -contains $name) { continue }
            if ([Environment]::GetEnvironmentVariable($name, 'Process') -ceq $value) { continue }
            [Environment]::SetEnvironmentVariable($name, $value, 'Process')
        }
    }
    $missing = @(foreach ($tool in 'cl.exe', 'link.exe', 'dumpbin.exe', 'rc.exe') { if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { $tool } })
    $tools = @($missing | Where-Object { $_ -ne 'rc.exe' })
    if ($tools.Count -gt 0 -and $wanted) { throw ('ZSDK_VCVARS_VER asks for MSVC ' + $wanted + ', but ' + $script:Wrapper + ' set up no compiler for it (' + ($tools -join ', ') + ' missing); add the MSVC v' + $wanted + ' x64 build tools to the Visual Studio install or clear ZSDK_VCVARS_VER') }
    if ($tools.Count -gt 0) { throw ('the MSVC x64 build tools are missing: ' + ($tools -join ', ') + $(if ($tools.Count -gt 1) { ' do' } else { ' does' }) + ' not resolve after ' + $script:Wrapper + '. ' + $script:Hint) }
    if ($missing -contains 'rc.exe') { throw ('the Windows SDK is missing: rc.exe does not resolve after ' + $script:Wrapper + '. Add a Windows 11 SDK (Microsoft.VisualStudio.Component.Windows11SDK.26100) to the Visual Studio install. ' + $script:Hint) }
    $msvc = [string]$env:VCToolsVersion
    if ($wanted -and -not ($msvc + '.').StartsWith($wanted + '.')) { throw ('ZSDK_VCVARS_VER asks for MSVC ' + $wanted + ' but ' + $script:Wrapper + ' gives ' + $(if ($msvc) { $msvc } else { 'no VCToolsVersion' }) + '; add the MSVC v' + $wanted + ' x64 build tools to the Visual Studio install or clear ZSDK_VCVARS_VER') }
    $env:VSLANG = '1033'
    $winsdk = ([string]$env:WindowsSDKVersion).TrimEnd('\')
    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add('toolchain ' + (Get-Command cl.exe).Source + ' via ' + $script:Wrapper)
    if ($script:Studio) { $lines.Add(('visual studio {0} {1} ({2}) at {3}, vswhere -version {4}' -f $script:Studio.name, $script:Studio.display, $script:Studio.version, $script:Studio.path, $script:VsRange)) }
    elseif ($env:VSCMD_VER) { $lines.Add('visual studio ' + $env:VSCMD_VER) }
    $lines.Add(('msvc {0}, windows sdk {1}' -f $(if ($msvc) { $msvc } else { 'unknown' }), $(if ($winsdk) { $winsdk } else { 'unknown' })))
    $script:Toolchain = @($lines)
    $script:Imported = $true
    [Environment]::SetEnvironmentVariable('ZSDK_TOOLCHAIN', $key, 'Process')
    foreach ($one in $script:Toolchain) { Add-ZsdkLine $one }
    Write-Host ('toolchain msvc {0}, windows sdk {1}' -f $(if ($msvc) { $msvc } else { 'unknown' }), $(if ($winsdk) { $winsdk } else { 'unknown' }))
}

function Test-ZsdkWarning([string]$line) {
    return $line -match '(^|[\s:])warning\s+(C|LNK|RC|D|CVT)\d{4}'
}

function Set-ZsdkSdkPaths([string[]]$paths) {
    $list = New-Object System.Collections.Generic.List[string]
    $encoding = $null
    try { $encoding = [Console]::OutputEncoding } catch { $encoding = $null }
    foreach ($one in $paths) {
        if (-not $one) { continue }
        $full = [System.IO.Path]::GetFullPath($one).TrimEnd('\') + '\'
        $list.Add($full)
        if ($encoding) {
            $seen = $encoding.GetString($encoding.GetBytes($full))
            if ($seen -ne $full) { $list.Add($seen) }
        }
    }
    $script:SdkPaths = @($list)
}

function Test-ZsdkSdkWarning([string]$line) {
    if ($script:SdkPaths.Count -eq 0) { return $false }
    $m = [regex]::Match($line, '^\s*(?:\d+>)?(.+?)\(\d+(?:,\d+)?\)\s*:\s*warning\s')
    if (-not $m.Success) { return $false }
    $file = $m.Groups[1].Value.Trim()
    foreach ($root in $script:SdkPaths) { if ($file.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase)) { return $true } }
    return $false
}

function Invoke-ZsdkTool([string]$command, [string]$title) {
    Add-ZsdkLine ('> ' + $title)
    $text = & cmd /d /c ($command + ' 2>&1')
    $code = $LASTEXITCODE
    foreach ($one in @($text)) {
        if ($null -eq $one) { continue }
        $line = [string]$one
        if (Test-ZsdkWarning $line) { if (Test-ZsdkSdkWarning $line) { $script:SdkWarnings++ } else { $script:Warnings++ } }
        Add-ZsdkLine $line
        Write-Host $line
    }
    Add-ZsdkLine ('exit {0}' -f $code)
    return $code
}

function Save-ZsdkLog([string]$path) {
    [System.IO.File]::WriteAllLines($path, $script:Lines, (New-Object System.Text.UTF8Encoding($false)))
}

function Write-ZsdkResponse([string]$path, [string[]]$items) {
    $parent = Split-Path $path -Parent
    if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
    $body = (@($items | ForEach-Object { if ($_ -match '^/') { $_ } else { '"' + $_ + '"' } }) -join "`r`n")
    [System.IO.File]::WriteAllText($path, $body + "`r`n", [System.Text.Encoding]::Unicode)
    return $path
}

function Get-ZsdkManifestHash([string]$release) {
    return (Get-FileHash -LiteralPath (Join-Path $release 'MANIFEST.sha256') -Algorithm SHA256).Hash
}

function Test-ZsdkManifest([string]$release) {
    $manifest = Join-Path $release 'MANIFEST.sha256'
    if (-not (Test-Path -LiteralPath $manifest)) { throw ('the ZoneSDK snapshot has no manifest: ' + $release) }
    $listed = @{}
    foreach ($line in Get-Content -LiteralPath $manifest) {
        if (-not $line.Trim()) { continue }
        $hash = $line.Substring(0, 64)
        $file = $line.Substring(66)
        $listed[$file.ToLowerInvariant()] = $true
        $path = Join-Path $release $file
        if (-not (Test-Path -LiteralPath $path)) { throw ('the ZoneSDK snapshot was edited: missing ' + $file) }
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $hash) { throw ('the ZoneSDK snapshot was edited: ' + $file) }
    }
    foreach ($item in Get-ChildItem -LiteralPath $release -Recurse -File) {
        $rel = $item.FullName.Substring($release.Length + 1)
        if ($rel -eq 'MANIFEST.sha256') { continue }
        if (-not $listed.ContainsKey($rel.ToLowerInvariant())) { throw ('the ZoneSDK snapshot was edited: unlisted ' + $rel) }
    }
}

function Enter-ZsdkLock([string]$Path, [int]$WaitMinutes = 30) {
    $full = [System.IO.Path]::GetFullPath($Path)
    $key = $full.ToLowerInvariant()
    if ($script:Held.ContainsKey($key)) {
        $script:Held[$key].count++
        return
    }
    $parent = Split-Path $full -Parent
    if (-not (Test-Path -LiteralPath $parent)) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
    $deadline = (Get-Date).AddMinutes([Math]::Max(0, $WaitMinutes))
    $denied = (Get-Date).AddSeconds(15)
    $told = $false
    $stream = $null
    while (-not $stream) {
        try { $stream = New-Object System.IO.FileStream($full, [System.IO.FileMode]::OpenOrCreate, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None, 4096, [System.IO.FileOptions]::DeleteOnClose) }
        catch {
            $cause = $_.Exception
            while ($cause.InnerException -and -not ($cause -is [System.IO.IOException] -or $cause -is [System.UnauthorizedAccessException])) { $cause = $cause.InnerException }
            $now = Get-Date
            if ($cause -is [System.UnauthorizedAccessException]) { if ($now -gt $denied) { throw ('the build lock cannot be opened: ' + $full + ' (' + $cause.Message + ')') } }
            elseif (-not ($cause -is [System.IO.IOException]) -or -not (($cause.HResult -band 0xFFFF) -in 32, 33)) { throw }
            elseif ($now -gt $deadline) { throw ('another build holds ' + $full + ' for longer than ' + $WaitMinutes + ' min') }
            if (-not $told) {
                Write-Host ('waiting for the build lock ' + $full)
                Add-ZsdkLine ('waiting for the build lock ' + $full)
                $told = $true
            }
            Start-Sleep -Milliseconds 500
        }
    }
    if ($told) { Add-ZsdkLine ('took the build lock ' + $full) }
    $script:Held[$key] = @{ stream = $stream; count = 1 }
}

function Exit-ZsdkLock([string]$Path) {
    $key = [System.IO.Path]::GetFullPath($Path).ToLowerInvariant()
    if (-not $script:Held.ContainsKey($key)) { return }
    $one = $script:Held[$key]
    $one.count--
    if ($one.count -gt 0) { return }
    $script:Held.Remove($key)
    $one.stream.Dispose()
}

function Get-ZsdkUnits([string]$folder, [string[]]$skip) {
    if (-not (Test-Path -LiteralPath $folder)) { return @() }
    return @(Get-ChildItem -LiteralPath $folder -Filter '*.cpp' -File | Sort-Object Name | Where-Object { $skip -notcontains $_.Name } | ForEach-Object { $_.FullName })
}

function Get-ZsdkAddonUnits([string]$src) {
    $overlayDir = Join-Path $src 'overlay'
    $entry = Join-Path $src 'entry.cpp'
    $present = New-Object System.Collections.Generic.List[string]
    $overlay = New-Object System.Collections.Generic.List[string]
    foreach ($item in @(Get-ChildItem -LiteralPath $src -Filter '*.cpp' -File -Recurse | Sort-Object FullName)) {
        if ($item.FullName -eq $entry) { continue }
        if ($item.FullName.StartsWith($overlayDir + '\', [System.StringComparison]::OrdinalIgnoreCase)) { $overlay.Add($item.FullName) } else { $present.Add($item.FullName) }
    }
    if (Test-Path -LiteralPath $entry) { $overlay.Add($entry) }
    foreach ($batch in @($present, $overlay)) {
        $seen = @{}
        foreach ($unit in $batch) {
            $name = [System.IO.Path]::GetFileNameWithoutExtension($unit).ToLowerInvariant()
            if ($seen.ContainsKey($name)) { throw ('two units of one batch share the object name ' + $name + ': ' + $seen[$name] + ' and ' + $unit) }
            $seen[$name] = $unit
        }
    }
    return @{ present = @($present); overlay = @($overlay) }
}

function Join-ZsdkQuoted([string[]]$items) {
    return (@($items | ForEach-Object { '"' + $_ + '"' }) -join ' ')
}

function Invoke-ZsdkCompile([string[]]$units, [string]$flags, [string]$objDir, [string]$pdb, [string]$title) {
    if ($units.Count -eq 0) { return 0 }
    New-Item -ItemType Directory -Force -Path $objDir | Out-Null
    $debug = if ($flags.Contains('/Brepro')) { '/Z7' } else { '/Zi /FS /Fd"' + $pdb + '"' }
    $rsp = Write-ZsdkResponse (Join-Path $objDir 'units.rsp') $units
    $line = 'cl /c /MP {0} {1} /Fo"{2}\\" @"{3}"' -f $flags, $debug, $objDir, $rsp
    return Invoke-ZsdkTool $line $title
}

function Get-ZsdkObjects([string[]]$dirs) {
    $out = @()
    foreach ($dir in $dirs) {
        if (Test-Path -LiteralPath $dir) { $out += @(Get-ChildItem -LiteralPath $dir -Filter '*.obj' -File | Sort-Object Name | ForEach-Object { $_.FullName }) }
    }
    return $out
}

function Test-ZsdkDeterministic([string]$dir) {
    if ($null -ne $script:Brepro) { return $script:Brepro }
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $probe = Join-Path $dir 'zsdk_probe.cpp'
    [System.IO.File]::WriteAllText($probe, "int zsdk_probe()`n{`n    return 0;`n}`n", (New-Object System.Text.UTF8Encoding($false)))
    $text = & cmd /d /c ('cl /nologo /c /Zs /experimental:deterministic "' + $probe + '" 2>&1')
    $joined = (@($text) | ForEach-Object { [string]$_ }) -join "`n"
    $script:Brepro = -not $joined.Contains('D9002')
    if (-not $script:Brepro) {
        $note = 'cl does not accept /experimental:deterministic (D9002), so this build drops it and the /pathmap switches that need it; /Brepro stays, and the binary may now depend on the folder it is built in'
        Add-ZsdkLine $note
        Write-Host $note
    }
    return $script:Brepro
}

function Get-ZsdkFlags([string]$config, [switch]$Deterministic, [switch]$Async, [string[]]$Map = @()) {
    $tuning = if ($config -eq 'Release') { '/MT /O2 /DNDEBUG' } else { '/MTd /Od /D_DEBUG' }
    $eh = if ($Async) { '/EHa' } else { '/EHsc' }
    $flags = $script:Common + ' ' + $eh + ' ' + $tuning
    if ($Deterministic) {
        $flags += ' /Brepro'
        if ($script:Brepro -ne $false) {
            $flags += ' /experimental:deterministic'
            foreach ($pair in $Map) { $flags += ' /pathmap:"' + $pair + '"' }
        }
    }
    return $flags
}

function Write-ZsdkGenerated([string]$sdk, [string]$dir, [string]$Manifest = '') {
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $rows = New-Object System.Collections.Generic.List[string]
    $builds = Join-Path $sdk 'data\builds'
    $folders = @()
    if (Test-Path -LiteralPath $builds -PathType Container) { $folders = @(Get-ChildItem -LiteralPath $builds -Directory | Sort-Object Name) }
    else { Add-ZsdkLine ('no data\builds folder in ' + $sdk + ', so the preload table is empty and zsdk-specs skips') }
    foreach ($folder in $folders) {
        if ($folder.Name -notmatch '^[0-9a-f]{8}$') { throw ('a build folder is not an 8-digit sha prefix: ' + $folder.Name) }
        $list = Join-Path $folder.FullName 'proven.txt'
        if (-not (Test-Path -LiteralPath $list)) { continue }
        foreach ($raw in Get-Content -LiteralPath $list) {
            $key = $raw.Trim()
            if (-not $key) { continue }
            if ($key -notmatch '^[A-Za-z0-9_]+(\.[A-Za-z0-9_]+)+$') { throw ('proven.txt of ' + $folder.Name + ' has a row that is not a key: ' + $key) }
            $rows.Add(('{{ "{0}", "{1}" }},' -f $folder.Name, $key))
        }
    }
    $file = Join-Path $dir 'zsdk_proven.inc'
    [System.IO.File]::WriteAllText($file, (($rows -join "`n") + "`n"), (New-Object System.Text.UTF8Encoding($false)))
    $tag = if ($Manifest) { $Manifest.ToUpperInvariant() } else { 'unreleased' }
    if ($tag -notmatch '^([0-9A-F]{8}|unreleased)$') { throw ('the manifest prefix is not 8 hex digits: ' + $Manifest) }
    $header = Join-Path $dir 'zsdk_build.h'
    [System.IO.File]::WriteAllText($header, ("#pragma once`n`n#define ZSDK_MANIFEST `"" + $tag + "`"`n"), (New-Object System.Text.UTF8Encoding($false)))
    $script:Generated = $dir
    Add-ZsdkLine ('preload table {0} rows' -f $rows.Count)
    Add-ZsdkLine ('generated zsdk_build.h, ZSDK_MANIFEST ' + $tag)
    return $dir
}

function Get-ZsdkIncludes([string]$addonSrc, [switch]$Overlay, [string[]]$Extra = @()) {
    $sdk = $script:SdkRoot
    $inc = '/I"{0}\include" /external:W0 /external:I"{0}\extern\reshade"' -f $sdk
    if ($addonSrc) { $inc = ('/I"{0}" ' -f $addonSrc) + $inc }
    foreach ($one in $Extra) { $inc += ' /I"' + $one + '"' }
    if ($script:Generated) { $inc += ' /I"' + $script:Generated + '"' }
    if ($Overlay) { $inc += ' /external:I"{0}\extern\imgui"' -f $sdk }
    return $inc
}

function Test-ZsdkInside([string]$root, [string]$path) {
    $full = [System.IO.Path]::GetFullPath($(if ([System.IO.Path]::IsPathRooted($path)) { $path } else { Join-Path $root $path }))
    $base = [System.IO.Path]::GetFullPath($root).TrimEnd('\') + '\'
    if (-not $full.StartsWith($base, [System.StringComparison]::OrdinalIgnoreCase)) { throw ('a build input lies outside the add-on folder: ' + $path) }
    return $full
}

function Get-ZsdkExtras([string]$Root, [string[]]$Libs, [string[]]$Includes, [string[]]$Defines, [string[]]$CompilerFlags, [string[]]$LinkFlags) {
    $outLibs = New-Object System.Collections.Generic.List[string]
    foreach ($one in $Libs) {
        if ($one -match '^[A-Za-z0-9_\-]+\.lib$') { $outLibs.Add($one); continue }
        $full = Test-ZsdkInside $Root $one
        if ($full -notmatch '\.lib$' -or -not (Test-Path -LiteralPath $full)) { throw ('a library is neither a system .lib nor a .lib file inside the add-on: ' + $one) }
        $outLibs.Add($full)
    }
    $outIncludes = New-Object System.Collections.Generic.List[string]
    foreach ($one in $Includes) {
        $full = Test-ZsdkInside $Root $one
        if (-not (Test-Path -LiteralPath $full -PathType Container)) { throw ('an include folder does not exist: ' + $one) }
        $outIncludes.Add($full)
    }
    $outDefines = ''
    foreach ($one in $Defines) {
        if ($one -notmatch '^[A-Za-z_][A-Za-z0-9_]*(=[A-Za-z0-9_.]*)?$') { throw ('a define is not NAME or NAME=VALUE: ' + $one) }
        if ($one -match '^(ZSDK_|IMGUI_)') { throw ('a define may not change the ZoneSDK unit kinds: ' + $one) }
        $outDefines += ' /D' + $one
    }
    $outCflags = ''
    foreach ($one in $CompilerFlags) {
        if ($one -match '^/GL$') { throw ('the compiler flag /GL is refused: whole-program objects carry no COFF symbol table, so the check that present units never reach ImGui or the overlay would check nothing') }
        if ($one -notmatch '^/(Gy|Gw|Oi|Ot|Os|bigobj|arch:(AVX|AVX2|AVX512)|fp:(fast|precise|strict)|Zc:(inline|throwingNew|preprocessor))$') { throw ('a compiler flag is not on the allow-list: ' + $one) }
        $outCflags += ' ' + $one
    }
    $outLflags = ''
    foreach ($one in $LinkFlags) {
        if ($one -match '^/LTCG(:.*)?$') { throw ('the linker flag ' + $one + ' is refused: link-time code generation needs /GL objects, and /GL is refused because those objects carry no COFF symbol table for the present/overlay symbol check') }
        if ($one -notmatch '^/(OPT:(REF|ICF|NOREF|NOICF)|DELAYLOAD:[A-Za-z0-9_.\-]+\.dll)$') { throw ('a linker flag is not on the allow-list: ' + $one) }
        $outLflags += ' ' + $one
    }
    return @{ libs = @($outLibs); includes = @($outIncludes); defines = $outDefines; cflags = $outCflags; lflags = $outLflags }
}

function Write-ZsdkSymbols([string[]]$present, [string[]]$overlay, [string]$out) {
    $presentDefined = New-Object 'System.Collections.Generic.HashSet[string]'
    $presentUsed = New-Object 'System.Collections.Generic.Dictionary[string,string]'
    $overlayDefined = New-Object 'System.Collections.Generic.HashSet[string]'
    $pattern = '^\s*[0-9A-F]+\s+[0-9A-F]+\s+(UNDEF|SECT[0-9A-F]+)\s+.*External\s+\|\s+(\S+)'
    foreach ($kind in 'present', 'overlay') {
        $list = if ($kind -eq 'present') { $present } else { $overlay }
        foreach ($obj in $list) {
            $text = & cmd /d /c ('dumpbin /nologo /symbols "' + $obj + '"')
            foreach ($one in @($text)) {
                $m = [regex]::Match([string]$one, $pattern)
                if (-not $m.Success) { continue }
                $name = $m.Groups[2].Value
                $undef = $m.Groups[1].Value -eq 'UNDEF'
                if ($kind -eq 'present') {
                    if ($undef) { if (-not $presentUsed.ContainsKey($name)) { $presentUsed[$name] = [System.IO.Path]::GetFileName($obj) } } else { [void]$presentDefined.Add($name) }
                }
                elseif (-not $undef) { [void]$overlayDefined.Add($name) }
            }
        }
    }
    $only = 0
    $bad = New-Object System.Collections.Generic.List[string]
    foreach ($name in $overlayDefined) {
        if ($presentDefined.Contains($name)) { continue }
        $only++
        if ($presentUsed.ContainsKey($name)) { $bad.Add($presentUsed[$name] + ' ' + $name) }
    }
    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add('present_objects=' + $present.Count)
    $lines.Add('overlay_objects=' + $overlay.Count)
    $lines.Add('present_undefined=' + $presentUsed.Count)
    $lines.Add('overlay_only=' + $only)
    foreach ($one in $bad) { $lines.Add('violation=' + $one) }
    [System.IO.File]::WriteAllLines($out, $lines, (New-Object System.Text.UTF8Encoding($false)))
    Add-ZsdkLine ('symbols: {0} present externals used, {1} overlay-only definitions, {2} violations' -f $presentUsed.Count, $only, $bad.Count)
    if ($present.Count -gt 0 -and $presentUsed.Count -eq 0) { throw ('the symbol check found no external symbol in ' + $present.Count + ' present objects; such objects carry no COFF symbol table (whole-program /GL objects do not), so the check that present units never reach ImGui or the overlay cannot run: ' + $out) }
    return $out
}

function Enter-ZsdkUtf8 {
    $previous = $null
    try {
        $previous = [Console]::OutputEncoding
        [Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)
    }
    catch { $previous = $null }
    return $previous
}

function Exit-ZsdkUtf8($previous) {
    if ($null -eq $previous) { return }
    try { [Console]::OutputEncoding = $previous } catch { }
}

function Find-ZsdkGame {
    $steam = New-Object System.Collections.Generic.List[string]
    foreach ($entry in @(@('HKCU:\Software\Valve\Steam', 'SteamPath'), @('HKLM:\SOFTWARE\WOW6432Node\Valve\Steam', 'InstallPath'), @('HKLM:\SOFTWARE\Valve\Steam', 'InstallPath'))) {
        $value = Get-ItemProperty -LiteralPath $entry[0] -Name $entry[1] -ErrorAction SilentlyContinue
        if ($value -and $value.($entry[1])) {
            $one = [System.IO.Path]::GetFullPath(([string]$value.($entry[1])).Replace('/', '\'))
            if (-not $steam.Contains($one)) { $steam.Add($one) }
        }
    }
    $libraries = New-Object System.Collections.Generic.List[string]
    foreach ($one in $steam) { if (-not $libraries.Contains($one)) { $libraries.Add($one) } }
    foreach ($one in @($steam)) {
        foreach ($vdf in @((Join-Path $one 'steamapps\libraryfolders.vdf'), (Join-Path $one 'config\libraryfolders.vdf'))) {
            if (-not (Test-Path -LiteralPath $vdf)) { continue }
            foreach ($m in [regex]::Matches([System.IO.File]::ReadAllText($vdf), '"path"\s+"([^"\r\n]+)"')) {
                $path = $m.Groups[1].Value.Replace('\\', '\')
                if ([System.IO.Path]::IsPathRooted($path) -and -not $libraries.Contains($path)) { $libraries.Add($path) }
            }
        }
    }
    $games = New-Object System.Collections.Generic.List[string]
    foreach ($library in $libraries) {
        $acf = Join-Path $library 'steamapps\appmanifest_1643320.acf'
        if (-not (Test-Path -LiteralPath $acf)) { continue }
        $m = [regex]::Match([System.IO.File]::ReadAllText($acf), '"installdir"\s+"([^"\r\n]+)"')
        if (-not $m.Success) { continue }
        $folder = Join-Path (Join-Path $library 'steamapps\common') $m.Groups[1].Value
        if ((Test-Path -LiteralPath $folder) -and -not $games.Contains($folder)) { $games.Add($folder) }
    }
    return @{ steam = @($steam); libraries = @($libraries); games = @($games) }
}

function Get-ZsdkGameExe {
    $look = Find-ZsdkGame
    foreach ($folder in $look.games) {
        $exe = Join-Path (Join-Path (Join-Path (Join-Path $folder 'Stalker2') 'Binaries') 'Win64') 'Stalker2-Win64-Shipping.exe'
        if (Test-Path -LiteralPath $exe -PathType Leaf) { return $exe }
    }
    if ($look.steam.Count -eq 0) { throw '-LiveExe found no Steam install for this user; pass -Exe with the path of Stalker2-Win64-Shipping.exe' }
    throw ('-LiveExe found S.T.A.L.K.E.R. 2 (Steam app 1643320) with its Stalker2-Win64-Shipping.exe in none of the ' + $look.libraries.Count + ' Steam libraries; pass -Exe with the path of the game exe')
}

function Resolve-ZsdkGuardExe([string]$Exe, [switch]$LiveExe) {
    if ($Exe -and $LiveExe) { throw 'pass -Exe or -LiveExe, not both' }
    if ($LiveExe) { return @{ path = (Get-ZsdkGameExe); how = 'the installed game found through Steam, read in place' } }
    if (-not $Exe) { return $null }
    $full = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Exe)
    if (-not (Test-Path -LiteralPath $full -PathType Leaf)) { throw ('-Exe names no file: ' + $Exe) }
    return @{ path = $full; how = 'given with -Exe, read in place' }
}

function Add-ZsdkCounts([switch]$Quiet) {
    $sdkLine = if ($script:SdkWarnings -gt 0) { 'sdk warnings {0} (compiler newer than tested)' -f $script:SdkWarnings } else { 'sdk warnings 0' }
    $ownLine = 'warnings {0}' -f $script:Warnings
    Add-ZsdkLine $sdkLine
    Add-ZsdkLine $ownLine
    if (-not $Quiet) {
        Write-Host $sdkLine
        Write-Host $ownLine
    }
}

function Invoke-ZsdkBuild {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][hashtable]$Lock,
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Define,
        [ValidateSet('Release', 'Debug')][string]$Config = 'Release',
        [switch]$Tests,
        [switch]$Deterministic,
        [string[]]$Pure = @(),
        [switch]$Legacy,
        [string[]]$Libs = @(),
        [string[]]$Includes = @(),
        [string[]]$Defines = @(),
        [string[]]$CompilerFlags = @(),
        [string[]]$LinkFlags = @(),
        [int]$WaitMinutes = 30,
        [string]$Exe,
        [switch]$LiveExe,
        [switch]$RequireExe,
        [switch]$Verify
    )
    $ErrorActionPreference = 'Stop'
    $sdk = $script:SdkRoot
    $version = Get-ZsdkVersion
    $buildDir = Join-Path $Root 'build'
    $log = Join-Path $buildDir 'last-build.log'
    Reset-ZsdkLog ('{0} build {1} {2}' -f $Name, $Config, (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))
    $sdkFull = [System.IO.Path]::GetFullPath($sdk).TrimEnd('\')
    $rootFull = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Root).TrimEnd('\')
    if ((Test-Path -LiteralPath (Join-Path $sdk 'MANIFEST.sha256')) -and ($rootFull + '\').StartsWith($sdkFull + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
        throw ('the add-on folder lies inside the ZoneSDK release ' + $sdkFull + '; a release is never built in place, so copy the template out with tools\new-addon.ps1 and build the copy: ' + $rootFull)
    }
    $gate = Join-Path $buildDir '.zsdk-build.lock'
    Enter-ZsdkLock $gate $WaitMinutes
    $counted = $false
    try {
        if ($Lock['version'] -ne $version) { throw ('zsdk.lock wants ' + $Lock['version'] + ' but the module is ' + $version) }
        Test-ZsdkManifest $sdk
        $manifest = Get-ZsdkManifestHash $sdk
        if ($Lock['manifest'] -ne $manifest) { throw ('the ZoneSDK snapshot was edited: manifest ' + $manifest + ' differs from zsdk.lock') }
        $prefix = $manifest.Substring(0, 8)
        Add-ZsdkLine ('zsdk {0} manifest {1}' -f $version, $prefix)
        Add-ZsdkLine ('release ' + $sdkFull)
        Write-Host ('zsdk {0} manifest {1} from {2}' -f $version, $prefix, $sdkFull)
        $extras = Get-ZsdkExtras -Root $Root -Libs $Libs -Includes $Includes -Defines $Defines -CompilerFlags $CompilerFlags -LinkFlags $LinkFlags
        $guardExe = Resolve-ZsdkGuardExe -Exe $Exe -LiveExe:$LiveExe
        if ($guardExe) {
            $line = 'guards exe {0} ({1})' -f $guardExe.path, $guardExe.how
            if (-not $Tests) { $line += '; only the test suites read it, and this build runs none: add -Tests' }
            Add-ZsdkLine $line
            Write-Host $line
        }
        if ($RequireExe) { Add-ZsdkLine 'guards: an exe is required (ZSDK_REQUIRE_EXE=1)' }
        Import-ZsdkToolchain
        $src = Join-Path $Root 'src'
        $bin = if ($Config -eq 'Debug') { Join-Path $Root 'bin\Debug' } else { Join-Path $Root 'bin' }
        $obj = Join-Path $buildDir ('obj\' + $Config)
        if (Test-Path -LiteralPath $obj) { Remove-Item -LiteralPath $obj -Recurse -Force }
        New-Item -ItemType Directory -Force -Path $obj, $bin | Out-Null
        $gen = Write-ZsdkGenerated $sdk (Join-Path $obj 'gen') $prefix
        Set-ZsdkSdkPaths @($sdkFull, $gen)
        [void](Test-ZsdkDeterministic $obj)
        $output = Join-Path $bin ($Name + '.addon64')
        $pdbOut = Join-Path $bin ($Name + '.pdb')
        $tested = Join-Path $bin ($Name + '.tested')
        foreach ($stale in @($output, $pdbOut, $tested)) { if (Test-Path -LiteralPath $stale) { [System.IO.File]::Delete($stale) } }
        $vcPdb = Join-Path $obj 'vc.pdb'
        $map = @(([System.IO.Path]::GetFullPath($Root).TrimEnd('\') + '=A'), ($sdkFull + '=Z'))
        $asyncFlags = (Get-ZsdkFlags $Config -Deterministic -Async -Map $map) + $extras.defines + $extras.cflags
        $present = $asyncFlags + ' /DZSDK_PRESENT_UNIT /DIMGUI_DISABLE ' + (Get-ZsdkIncludes $src -Extra $extras.includes)
        $overlay = $asyncFlags + ' ' + (Get-ZsdkIncludes $src -Overlay -Extra $extras.includes)
        $entryFlags = $asyncFlags + ' ' + (Get-ZsdkIncludes $src -Overlay -Extra $extras.includes)
        $resource = Join-Path $obj 'version.res'
        $rcDebug = if ($Config -eq 'Debug') { ' /d ZSDK_DEBUG' } else { '' }
        $code = Invoke-ZsdkTool ('rc /nologo{0} /I"{1}" /fo "{2}" "{3}"' -f $rcDebug, $src, $resource, (Join-Path $src 'version.rc')) 'version resource'
        if ($code -ne 0) { throw ('version resource failed: ' + $code) }
        $units = Get-ZsdkAddonUnits $src
        $pureList = if ($PSBoundParameters.ContainsKey('Pure')) { @($Pure) } else { @($units.present) }
        Add-ZsdkLine ('test units {0}, {1}' -f $pureList.Count, $(if ($PSBoundParameters.ContainsKey('Pure')) { 'listed by make.ps1' } else { 'every src unit except entry.cpp and overlay' }))
        $steps = @(
            @{ units = @(Get-ZsdkUnits (Join-Path $sdk 'src\present') @()); flags = $present; dir = 'sdk'; title = 'present batch, zsdk' },
            @{ units = @($units.present); flags = $present; dir = 'addon'; title = 'present batch, add-on' },
            @{ units = @(Get-ZsdkUnits (Join-Path $sdk 'src\overlay') @()); flags = $overlay; dir = 'sdkov'; title = 'overlay batch, zsdk' },
            @{ units = @(Get-ZsdkUnits (Join-Path $sdk 'src\entry') @()); flags = $entryFlags; dir = 'sdkent'; title = 'entry batch, zsdk' },
            @{ units = @($units.overlay); flags = $overlay; dir = 'addonov'; title = 'overlay batch, add-on' }
        )
        foreach ($step in $steps) {
            $code = Invoke-ZsdkCompile $step.units $step.flags (Join-Path $obj $step.dir) $vcPdb $step.title
            if ($code -ne 0) { throw ('compile failed: ' + $step.title + ' (log: ' + $log + ')') }
        }
        if ($script:Warnings -gt 0) { throw ('the build has ' + $script:Warnings + ' warnings in add-on code, 0 are allowed (log: ' + $log + ')') }
        $presentObjects = @(Get-ZsdkObjects @((Join-Path $obj 'sdk'), (Join-Path $obj 'addon')))
        $overlayObjects = @(Get-ZsdkObjects @((Join-Path $obj 'sdkov'), (Join-Path $obj 'sdkent'), (Join-Path $obj 'addonov')))
        $symbols = Write-ZsdkSymbols $presentObjects $overlayObjects (Join-Path $obj 'symbols.txt')
        $violations = @(Get-Content -LiteralPath $symbols | Where-Object { $_ -like 'violation=*' })
        if ($violations.Count -gt 0) {
            foreach ($one in $violations) {
                Add-ZsdkLine $one
                Write-Host $one
            }
            throw ('a present unit uses ' + $violations.Count + ' symbols that only overlay units define; present units never reach ImGui or the overlay (log: ' + $log + ')')
        }
        $linkLine = '/nologo /DLL /DEBUG /PDB:"{0}" /PDBALTPATH:%_PDB% /OPT:REF /OPT:ICF /INCREMENTAL:NO /NOIMPLIB /NOEXP /Brepro /WX{1}' -f $pdbOut, $extras.lflags
        $linkRsp = Write-ZsdkResponse (Join-Path $obj 'link.rsp') (@($presentObjects) + @($overlayObjects) + @($resource) + $script:BaseLibs + @($extras.libs))
        $code = Invoke-ZsdkTool ('link {0} /OUT:"{1}" @"{2}"' -f $linkLine, $output, $linkRsp) 'link'
        if ($code -ne 0) { throw ('link failed: ' + $code) }
        $hash = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash
        Add-ZsdkLine ('output {0} {1} bytes sha256 {2}' -f $output, (Get-Item -LiteralPath $output).Length, $hash)
        $info = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($output)
        Add-ZsdkLine ('version resource {0}, debug {1}, prerelease {2}' -f $info.FileVersion, [int]$info.IsDebug, [int]$info.IsPreRelease)
        if ($Config -eq 'Debug' -and -not $info.IsDebug) {
            $note = 'note: this Debug build carries no VS_FF_DEBUG flag; src\version.rc sets it when ZSDK_DEBUG is defined'
            Add-ZsdkLine $note
            Write-Host $note
        }
        Write-Host ('built {0}' -f $output)
        Write-Host ('sha256 {0}' -f $hash)
        if ($Tests) {
            $result = Invoke-ZsdkTests -Root $Root -Name $Name -Define $Define -Config $Config -Pure $pureList -Legacy:$Legacy -Obj $obj -Flags ($asyncFlags + ' /DZSDK_TESTS /D' + $Define + '_TESTS') -Extras $extras -Symbols $symbols -Bin (Join-Path $bin 'tests') -Exe $(if ($guardExe) { $guardExe.path } else { '' }) -RequireExe:$RequireExe
            if ($result.failed -gt 0) {
                foreach ($stale in @($output, $pdbOut)) { if (Test-Path -LiteralPath $stale) { [System.IO.File]::Delete($stale) } }
                throw ('tests failed: ' + $result.failed + '; the binary is removed, so neither pack nor an install can ship it: fix the failure and build again (log: ' + $log + ')')
            }
            [System.IO.File]::WriteAllText($tested, ('passed ' + $hash + "`n"), (New-Object System.Text.UTF8Encoding($false)))
        }
        if ($Verify) { Invoke-ZsdkVerify -Root $Root -Name $Name -Output $output -Config $Config }
        Add-ZsdkCounts
        $counted = $true
        Save-ZsdkLog $log
        if ($script:Warnings -gt 0) { throw ('the build has ' + $script:Warnings + ' warnings in add-on code, 0 are allowed (log: ' + $log + ')') }
    }
    catch {
        if (-not $counted) { Add-ZsdkCounts -Quiet }
        Add-ZsdkLine ('failed: ' + $_.Exception.Message)
        Save-ZsdkLog $log
        throw
    }
    finally { Exit-ZsdkLock $gate }
}

function Invoke-ZsdkTests {
    param([string]$Root, [string]$Name, [string]$Define, [string]$Config, [string[]]$Pure, [switch]$Legacy, [string]$Obj, [string]$Flags, [hashtable]$Extras, [string]$Symbols, [string]$Bin, [string]$Exe, [switch]$RequireExe)
    $sdk = $script:SdkRoot
    $src = Join-Path $Root 'src'
    $testObj = Join-Path $Obj 'tests'
    $testBin = if ($Bin) { $Bin } else { Join-Path $Root 'bin\tests' }
    New-Item -ItemType Directory -Force -Path $testObj, $testBin | Out-Null
    $includes = if ($Extras) { $Extras.includes } else { @() }
    $libs = if ($Extras) { @($script:BaseLibs) + @($Extras.libs) } else { $script:BaseLibs }
    $testFlags = $Flags + ' /DZSDK_PRESENT_UNIT /DIMGUI_DISABLE ' + (Get-ZsdkIncludes $src -Extra $includes)
    $supportSdk = Join-Path $testObj 'support\sdk'
    $supportAddon = Join-Path $testObj 'support\addon'
    $vcPdb = Join-Path $testObj 'vc.pdb'
    $code = Invoke-ZsdkCompile @(Get-ZsdkUnits (Join-Path $sdk 'src\present') @()) $testFlags $supportSdk $vcPdb 'test support, zsdk'
    if ($code -ne 0) { return @{ passed = 0; failed = 1 } }
    $pureUnits = @($Pure | Where-Object { $_ } | ForEach-Object { if ([System.IO.Path]::IsPathRooted($_)) { $_ } else { Join-Path $src $_ } })
    $code = Invoke-ZsdkCompile $pureUnits $testFlags $supportAddon $vcPdb 'test support, add-on'
    if ($code -ne 0) { return @{ passed = 0; failed = 1 } }
    $support = @(Get-ZsdkObjects @($supportSdk, $supportAddon))
    $linkItems = @('/link', '/WX') + @($support) + @($libs)
    $cases = @()
    $cases += @(Get-ChildItem -LiteralPath (Join-Path $Root 'tests') -Filter '*.cpp' -File -ErrorAction SilentlyContinue | Sort-Object Name | ForEach-Object { @{ file = $_.FullName; name = $_.BaseName } })
    $cases += @(Get-ChildItem -LiteralPath (Join-Path $sdk 'tests\generic') -Filter '*.cpp' -File -ErrorAction SilentlyContinue | Sort-Object Name | ForEach-Object { @{ file = $_.FullName; name = 'zsdk-' + $_.BaseName } })
    $passed = 0
    $failed = 0
    $checks = 0
    $notes = 0
    $names = @('ZSDK_ADDON_SOURCE', 'ZSDK_TEST_HOME', 'ZSDK_ADDON_NAME', 'ZSDK_ADDON_DEFINE', 'ZSDK_HOME', 'ZSDK_SYMBOLS', 'ZSDK_EXE', 'ZSDK_REQUIRE_EXE')
    if ($Legacy) { $names += @(($Define + '_SOURCE'), ($Define + '_TEST_HOME')) }
    $saved = @{}
    foreach ($n in $names) { $saved[$n] = [Environment]::GetEnvironmentVariable($n, 'Process') }
    try {
        if ($Exe) { $env:ZSDK_EXE = $Exe }
        if ($RequireExe) { $env:ZSDK_REQUIRE_EXE = '1' }
        foreach ($case in $cases) {
            $caseObj = Join-Path $testObj $case.name
            New-Item -ItemType Directory -Force -Path $caseObj | Out-Null
            $exe = Join-Path $testBin ($case.name + '.exe')
            $rsp = Write-ZsdkResponse (Join-Path $caseObj 'build.rsp') (@($testFlags, ('/I"' + $Root + '\tests"'), $case.file, ('/Fo"' + $caseObj + '\\"'), ('/Fe"' + $exe + '"')) + $linkItems)
            $code = Invoke-ZsdkTool ('cl @"{0}"' -f $rsp) ('test build ' + $case.name)
            if ($code -ne 0) { $failed++; Write-Host ('FAIL build {0} ({1})' -f $case.name, $code); continue }
            Add-ZsdkLine ('> run ' + $case.name)
            $sandbox = Join-Path $testObj ($case.name + '.home')
            if (Test-Path -LiteralPath $sandbox) { Remove-Item -LiteralPath $sandbox -Recurse -Force }
            New-Item -ItemType Directory -Force -Path $sandbox | Out-Null
            $env:ZSDK_ADDON_SOURCE = $Root
            $env:ZSDK_TEST_HOME = $sandbox
            $env:ZSDK_ADDON_NAME = $Name
            $env:ZSDK_ADDON_DEFINE = $Define
            $env:ZSDK_HOME = $sdk
            $env:ZSDK_SYMBOLS = $Symbols
            if ($Legacy) {
                [Environment]::SetEnvironmentVariable($Define + '_SOURCE', $Root, 'Process')
                [Environment]::SetEnvironmentVariable($Define + '_TEST_HOME', $sandbox, 'Process')
            }
            $encoding = Enter-ZsdkUtf8
            try {
                $text = & cmd /d /c ('"' + $exe + '" 2>&1')
                $code = $LASTEXITCODE
            }
            finally { Exit-ZsdkUtf8 $encoding }
            foreach ($one in @($text)) {
                if ($null -eq $one) { continue }
                $l = [string]$one
                Add-ZsdkLine $l
                Write-Host $l
                $m = [regex]::Match($l, ': (\d+) checks, (\d+) failures')
                if ($m.Success) { $checks += [int]$m.Groups[1].Value }
                if ($l.StartsWith('warning: ')) { $notes++ }
            }
            Add-ZsdkLine ('exit {0}' -f $code)
            if ($code -ne 0) { $failed++; Write-Host ('FAIL {0} ({1})' -f $case.name, $code) } else { $passed++; Write-Host ('ok {0}' -f $case.name) }
        }
    }
    finally {
        foreach ($n in $names) { [Environment]::SetEnvironmentVariable($n, $saved[$n], 'Process') }
    }
    $summary = 'tests passed {0} failed {1} checks {2}' -f $passed, $failed, $checks
    if ($notes -gt 0) { $summary += (', {0} test warnings' -f $notes) }
    Add-ZsdkLine $summary
    Write-Host $summary
    return @{ passed = $passed; failed = $failed; checks = $checks; warnings = $notes }
}

function Write-ZsdkSourceZip([string]$Root, [string]$Zip) {
    Add-Type -AssemblyName System.IO.Compression
    $base = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Root).TrimEnd('\')
    $files = New-Object System.Collections.Generic.List[string]
    foreach ($part in 'src', 'tests', 'build') {
        $dir = Join-Path $base $part
        if (-not (Test-Path -LiteralPath $dir)) { continue }
        foreach ($item in @(Get-ChildItem -LiteralPath $dir -Recurse -File -Force)) {
            $rel = $item.FullName.Substring($base.Length + 1)
            if ($rel -match '^build\\(obj|bin)\\' -or $rel -match '^build\\(last-build\.log|\.zsdk-build\.lock|\.build\.lock)$') { continue }
            $files.Add($rel)
        }
    }
    $files.Sort([System.StringComparer]::OrdinalIgnoreCase)
    $sorted = @($files)
    $stamp = New-Object System.DateTimeOffset(1980, 1, 1, 0, 0, 0, [System.TimeSpan]::Zero)
    $stream = New-Object System.IO.FileStream($Zip, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    try {
        $archive = New-Object System.IO.Compression.ZipArchive($stream, [System.IO.Compression.ZipArchiveMode]::Create)
        try {
            foreach ($rel in $sorted) {
                $entry = $archive.CreateEntry($rel.Replace('\', '/'), [System.IO.Compression.CompressionLevel]::Optimal)
                $entry.LastWriteTime = $stamp
                $bytes = [System.IO.File]::ReadAllBytes((Join-Path $base $rel))
                $into = $entry.Open()
                try { $into.Write($bytes, 0, $bytes.Length) } finally { $into.Dispose() }
            }
        }
        finally { $archive.Dispose() }
    }
    finally { $stream.Dispose() }
    return $sorted.Count
}

function Invoke-ZsdkRelease {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][hashtable]$Lock,
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Define,
        [string]$Version,
        [switch]$SkipTests,
        [string[]]$Pure = @(),
        [switch]$Legacy,
        [string[]]$Libs = @(),
        [string[]]$Includes = @(),
        [string[]]$Defines = @(),
        [string[]]$CompilerFlags = @(),
        [string[]]$LinkFlags = @(),
        [int]$WaitMinutes = 30,
        [string]$Exe,
        [switch]$LiveExe,
        [switch]$RequireExe,
        [switch]$Verify
    )
    $ErrorActionPreference = 'Stop'
    if ($Version -and $Version -notmatch '^\d+\.\d+\.\d+$') { throw ('-Version must look like 1.2.3, not ' + $Version) }
    $buildDir = Join-Path $Root 'build'
    $gate = Join-Path $buildDir '.zsdk-build.lock'
    Enter-ZsdkLock $gate $WaitMinutes
    try {
        if ($Version) {
            $early = Join-Path $Root ('dist\' + $Version + '\symbols')
            if (Test-Path -LiteralPath $early) { throw ('the kept release ' + $early + ' exists and is never overwritten; bump the version') }
        }
        $build = @{ Root = $Root; Lock = $Lock; Name = $Name; Define = $Define; Config = 'Release'; Tests = (-not $SkipTests); Deterministic = $true; Legacy = $Legacy; Libs = $Libs; Includes = $Includes; Defines = $Defines; CompilerFlags = $CompilerFlags; LinkFlags = $LinkFlags; WaitMinutes = $WaitMinutes }
        if ($PSBoundParameters.ContainsKey('Pure')) { $build['Pure'] = $Pure }
        if ($Exe) { $build['Exe'] = $Exe }
        if ($LiveExe) { $build['LiveExe'] = $true }
        if ($RequireExe) { $build['RequireExe'] = $true }
        if ($Verify) { $build['Verify'] = $true }
        Invoke-ZsdkBuild @build
        $bin = Join-Path $Root 'bin'
        $output = Join-Path $bin ($Name + '.addon64')
        $pdb = Join-Path $bin ($Name + '.pdb')
        foreach ($need in @($output, $pdb)) { if (-not (Test-Path -LiteralPath $need)) { throw ('the release build left no ' + $need) } }
        $info = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($output)
        if ($info.IsDebug) { throw ('the release binary carries VS_FF_DEBUG in its version resource: ' + $output) }
        $stamped = [regex]::Match([string]$info.FileVersion, '^\s*(\d+\.\d+\.\d+)').Groups[1].Value
        if (-not $Version) {
            if (-not $stamped) { throw ('the version resource of ' + $output + ' has no x.y.z FileVersion; pass -Version') }
            $Version = $stamped
        }
        elseif ($stamped -and $stamped -ne $Version) { throw ('-Version ' + $Version + ' differs from the version resource ' + $stamped + ' of ' + $output) }
        $folder = Join-Path $Root ('dist\' + $Version + '\symbols')
        if (Test-Path -LiteralPath $folder) { throw ('the kept release ' + $folder + ' exists and is never overwritten; bump the version') }
        New-Item -ItemType Directory -Path $folder | Out-Null
        $kept = New-Object System.Collections.Generic.List[string]
        foreach ($one in @($output, $pdb, (Join-Path $buildDir 'zsdk.lock'))) {
            $to = Join-Path $folder (Split-Path $one -Leaf)
            [System.IO.File]::Copy($one, $to, $false)
            $kept.Add($to)
        }
        $buildLog = Join-Path $folder 'last-build.log'
        [System.IO.File]::Copy((Join-Path $buildDir 'last-build.log'), $buildLog, $false)
        $kept.Add($buildLog)
        $zip = Join-Path $folder ($Name + '-' + $Version + '-source.zip')
        $count = Write-ZsdkSourceZip $Root $zip
        $kept.Add($zip)
        $sums = New-Object System.Collections.Generic.List[string]
        foreach ($one in $kept) { $sums.Add(('{0}  {1}' -f (Get-FileHash -LiteralPath $one -Algorithm SHA256).Hash, (Split-Path $one -Leaf))) }
        [System.IO.File]::WriteAllLines((Join-Path $folder 'SHA256.txt'), $sums, (New-Object System.Text.UTF8Encoding($false)))
        Write-Host ('kept {0} {1} in {2} ({3} source files zipped)' -f $Name, $Version, $folder, $count)
        foreach ($one in $sums) { Write-Host $one }
    }
    finally { Exit-ZsdkLock $gate }
}

function Find-ZsdkPython {
    $python = $env:ZSDK_PYTHON
    if ($python -eq 'none') { return $null }
    if ($python) {
        if (-not (Test-Path -LiteralPath $python)) { throw ('ZSDK_PYTHON points at a file that does not exist: ' + $python) }
        return $python
    }
    $found = @(Get-Command python.exe -All -ErrorAction SilentlyContinue | Where-Object { $_.Source -and $_.Source -notmatch '\\WindowsApps\\' })
    if ($found.Count -gt 0) { return $found[0].Source }
    return $null
}

function Read-ZsdkExports([byte[]]$bytes) {
    $pe = [BitConverter]::ToInt32($bytes, 0x3C)
    if ($pe -le 0 -or $pe + 24 -ge $bytes.Length -or [BitConverter]::ToUInt32($bytes, $pe) -ne 0x4550) { throw 'the file is not a PE image' }
    $count = [BitConverter]::ToUInt16($bytes, $pe + 6)
    $optional = $pe + 24
    if ([BitConverter]::ToUInt16($bytes, $optional) -ne 0x20B) { throw 'the file is not a 64-bit PE image' }
    $table = $optional + [BitConverter]::ToUInt16($bytes, $pe + 20)
    $sections = @()
    for ($i = 0; $i -lt $count; $i++) {
        $at = $table + 40 * $i
        $size = [Math]::Max([BitConverter]::ToUInt32($bytes, $at + 8), [BitConverter]::ToUInt32($bytes, $at + 16))
        $sections += @{ va = [BitConverter]::ToUInt32($bytes, $at + 12); size = $size; raw = [BitConverter]::ToUInt32($bytes, $at + 20) }
    }
    $offset = {
        param([uint32]$rva)
        foreach ($s in $sections) { if ($rva -ge $s.va -and $rva -lt $s.va + $s.size) { return [int]($s.raw + ($rva - $s.va)) } }
        throw ('an export address lies outside the sections: 0x{0:X}' -f $rva)
    }
    $exportRva = [BitConverter]::ToUInt32($bytes, $optional + 112)
    $names = New-Object System.Collections.Generic.List[string]
    if (-not $exportRva) { return $names }
    $directory = & $offset $exportRva
    $named = [BitConverter]::ToUInt32($bytes, $directory + 24)
    $nameTable = & $offset ([BitConverter]::ToUInt32($bytes, $directory + 32))
    for ($i = 0; $i -lt $named; $i++) {
        $at = & $offset ([BitConverter]::ToUInt32($bytes, $nameTable + 4 * $i))
        $end = [Array]::IndexOf($bytes, [byte]0, $at)
        $names.Add([System.Text.Encoding]::ASCII.GetString($bytes, $at, $end - $at))
    }
    return $names
}

function Test-ZsdkLeaks([byte[]]$bytes, [string[]]$Paths, [string]$User) {
    $found = New-Object System.Collections.Generic.List[string]
    $latin = [System.Text.Encoding]::GetEncoding(28591)
    $views = @(
        @{ name = 'ANSI/UTF-8'; text = $latin.GetString($bytes); make = { param($s) $latin.GetString([System.Text.Encoding]::UTF8.GetBytes($s)) } },
        @{ name = 'UTF-16'; text = [System.Text.Encoding]::Unicode.GetString($bytes); make = { param($s) $s } },
        @{ name = 'UTF-16, odd offset'; text = $(if ($bytes.Length -gt 1) { [System.Text.Encoding]::Unicode.GetString($bytes, 1, $bytes.Length - 1) } else { '' }); make = { param($s) $s } }
    )
    foreach ($path in @($Paths | Where-Object { $_ -and $_.Length -ge 4 } | Sort-Object -Unique)) {
        foreach ($form in @($path, $path.Replace('\', '/'))) {
            foreach ($view in $views) {
                $needle = & $view.make $form
                if ($view.text.IndexOf($needle, [System.StringComparison]::OrdinalIgnoreCase) -ge 0) { $found.Add(('{0} ({1})' -f $form, $view.name)) }
            }
        }
    }
    if ($User -and $User.Length -ge 2) {
        foreach ($view in $views) {
            $needle = [regex]::Escape((& $view.make $User))
            if ([regex]::IsMatch($view.text, ('(?i)[\\/]' + $needle + '(?=[\\/"''\s\x00]|$)'))) { $found.Add(('the Windows user name inside a path ({0})' -f $view.name)) }
        }
    }
    return $found
}

function Find-ZsdkAbsolutePaths([byte[]]$bytes) {
    $found = New-Object System.Collections.Generic.List[string]
    $latin = [System.Text.Encoding]::GetEncoding(28591)
    $texts = @($latin.GetString($bytes), [System.Text.Encoding]::Unicode.GetString($bytes))
    if ($bytes.Length -gt 1) { $texts += [System.Text.Encoding]::Unicode.GetString($bytes, 1, $bytes.Length - 1) }
    $shape = New-Object System.Text.RegularExpressions.Regex('(?<![A-Za-z0-9_])[A-Za-z]:[\\/][\w .()-]+[\\/]')
    foreach ($text in $texts) {
        foreach ($m in $shape.Matches($text)) {
            if (-not $found.Contains($m.Value)) { $found.Add($m.Value) }
            if ($found.Count -ge 8) { return $found }
        }
    }
    return $found
}

function Invoke-ZsdkVerify {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Output,
        [ValidateSet('Release', 'Debug')][string]$Config = 'Release'
    )
    $sdk = $script:SdkRoot
    $problems = New-Object System.Collections.Generic.List[string]
    if (-not (Test-Path -LiteralPath $Output)) { throw ('verify: the add-on binary does not exist: ' + $Output) }
    $bytes = [System.IO.File]::ReadAllBytes($Output)
    $python = Find-ZsdkPython
    $exports = @()
    $how = 'PowerShell'
    if ($python) {
        $savedIo = [Environment]::GetEnvironmentVariable('PYTHONIOENCODING', 'Process')
        [Environment]::SetEnvironmentVariable('PYTHONIOENCODING', 'utf-8', 'Process')
        $encoding = Enter-ZsdkUtf8
        try {
            $raw = @(Invoke-ZsdkPython (Join-Path $sdk 'tools\pe\pe_facts.py') @('addon', $Output)) -join "`n"
            $pyCode = $LASTEXITCODE
        }
        finally {
            Exit-ZsdkUtf8 $encoding
            [Environment]::SetEnvironmentVariable('PYTHONIOENCODING', $savedIo, 'Process')
        }
        $facts = $null
        if ($pyCode -eq 0) { try { $facts = $raw | ConvertFrom-Json } catch { $facts = $null } }
        if ($facts -and $facts.addon) {
            $exports = @($facts.addon.exports.PSObject.Properties | ForEach-Object { $_.Name })
            $how = 'Python pe_facts.py'
        }
        else {
            $exports = @(Read-ZsdkExports $bytes)
            $how = 'PowerShell, pe_facts.py failed with exit ' + $pyCode
        }
    }
    else { $exports = @(Read-ZsdkExports $bytes) }
    $sorted = @($exports | Sort-Object)
    if (($sorted -join ',') -ne 'DESCRIPTION,NAME') { $problems.Add('the add-on must export exactly NAME and DESCRIPTION, it exports: ' + $(if ($sorted.Count) { $sorted -join ', ' } else { 'nothing' })) }
    Add-ZsdkLine ('verify: exports {0}, read with {1}' -f $(if ($sorted.Count) { $sorted -join ', ' } else { 'none' }), $how)
    $info = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($Output)
    if (-not $info.FileVersion) { $problems.Add('the add-on has no version resource with a FileVersion (src\version.rc)') }
    if ($info.IsDebug) { $problems.Add('the version resource carries VS_FF_DEBUG: a Debug build is never shipped, verify a Release build') }
    Add-ZsdkLine ('verify: version resource {0}, debug {1}' -f $(if ($info.FileVersion) { $info.FileVersion } else { 'none' }), [int]$info.IsDebug)
    if ($Config -eq 'Debug') { $problems.Add('this is a Debug build (-Config Debug): a Debug build is never shipped, verify a Release build') }
    $latinText = [System.Text.Encoding]::GetEncoding(28591).GetString($bytes)
    $debugCrt = @('_CrtDbgReport', 'minkernel\crts' | Where-Object { $latinText.IndexOf($_, [System.StringComparison]::Ordinal) -ge 0 })
    if ($debugCrt.Count -gt 0) { $problems.Add('the binary links the Debug CRT (/MTd, it names ' + ($debugCrt -join ' and ') + '): a Debug build is never shipped, verify a Release build') }
    Add-ZsdkLine ('verify: build {0}, Debug CRT signatures {1}' -f $Config, $debugCrt.Count)
    $rootFull = [System.IO.Path]::GetFullPath($Root).TrimEnd('\')
    $sdkFull = [System.IO.Path]::GetFullPath($sdk).TrimEnd('\')
    $leaks = @(Test-ZsdkLeaks $bytes @($rootFull, $env:USERPROFILE, $sdkFull) $env:USERNAME)
    foreach ($one in $leaks) { $problems.Add('the binary contains a build path: ' + $one) }
    Add-ZsdkLine ('verify: byte scan of {0} bytes for the build root, the ZoneSDK root, %USERPROFILE% and the user name: {1} found' -f $bytes.Length, $leaks.Count)
    $found = @(Find-ZsdkAbsolutePaths $bytes)
    $system = @($found | Where-Object { $_ -match '^[A-Za-z]:[\\/](Windows|Program Files( \(x86\))?)[\\/]$' })
    $absolute = @($found | Where-Object { $system -notcontains $_ })
    foreach ($one in $absolute) { $problems.Add('the binary contains an absolute path: ' + $one + ' (a shipped add-on names no folder of the build PC; such a path comes from __FILE__, a literal or a resource)') }
    Add-ZsdkLine ('verify: scan for any absolute drive path: {0} found, {1} system folders' -f $absolute.Count, $system.Count)
    if ($system.Count -gt 0) {
        $note = 'verify: note: the binary names a fixed system folder, which may sit on another drive on a player PC: ' + ($system -join '; ')
        Add-ZsdkLine $note
        Write-Host $note
    }
    $run = Join-Path $sdk 'tools\fakehost\run.ps1'
    if (-not (Test-Path -LiteralPath $run)) { $problems.Add('this ZoneSDK has no tools\fakehost\run.ps1, so the fake host run cannot happen') }
    else {
        $temp = Join-Path ([System.IO.Path]::GetTempPath()) ('zsdk-verify-' + [guid]::NewGuid().ToString('N').Substring(0, 12))
        $label = $Name.ToLowerInvariant()
        $outDir = Join-Path $temp 'out'
        try {
            $shell = (Get-Process -Id $PID).Path
            New-Item -ItemType Directory -Force -Path $temp | Out-Null
            $hostOut = Join-Path $temp 'host-out.txt'
            $hostErr = Join-Path $temp 'host-err.txt'
            $line = '-NoProfile -ExecutionPolicy Bypass -File "' + $run + '" -Addon "' + $Output + '" -Label ' + $label + ' -Out "' + $outDir + '" -Sandbox "' + (Join-Path $temp 'sandbox') + '"'
            $proc = Start-Process -FilePath $shell -ArgumentList $line -NoNewWindow -PassThru -RedirectStandardOutput $hostOut -RedirectStandardError $hostErr
            $null = $proc.Handle
            $proc.WaitForExit()
            $code = $proc.ExitCode
            $said = @(@(Get-Content -LiteralPath $hostOut -ErrorAction SilentlyContinue) + @(Get-Content -LiteralPath $hostErr -ErrorAction SilentlyContinue) | ForEach-Object { [string]$_ })
            $json = Join-Path $outDir ($label + '.fakehost.json')
            $data = $null
            if (Test-Path -LiteralPath $json) { $data = Get-Content -LiteralPath $json -Raw | ConvertFrom-Json }
            if (-not $data) {
                $reason = @($said | Where-Object { $_ -and $_.Trim() -and $_ -notmatch '^\s*(\+|At |~|CategoryInfo|FullyQualifiedErrorId)' })
                $problems.Add('the fake host run gave no result (exit ' + $code + '): ' + $(if ($reason.Count) { ($reason | Select-Object -Last 2) -join ' | ' } else { 'it printed nothing' }))
            }
            else {
                if ($data.status -ne 'done') { $problems.Add('the fake host ended with status ' + $data.status) }
                if (-not $data.second_copy_refused) { $problems.Add('the fake host loaded a second copy of the add-on; the tag mutex did not refuse it') }
                if ($data.imgui_calls_present -ne 0 -or $data.imgui_calls_load -ne 0 -or $data.imgui_calls_open -ne 0) { $problems.Add(('ImGui was called outside the overlay: present {0}, load {1}, open {2}' -f $data.imgui_calls_present, $data.imgui_calls_load, $data.imgui_calls_open)) }
                if ($data.runtime_traps -ne 0) { $problems.Add('the add-on touched the fake effect runtime outside its contract: ' + $data.runtime_traps + ' traps') }
                if ($data.page_calls -le 0 -or $data.overlay_events -le 0) { $problems.Add('the tab page or the overlay event never ran in the fake host') }
                Add-ZsdkLine ('verify: fake host {0}, {1} pages, second copy refused {2}, ImGui calls in present {3}, traps {4}' -f $data.status, $data.page_calls, $data.second_copy_refused, $data.imgui_calls_present, $data.runtime_traps)
                $logCopy = Join-Path $outDir ($label + '.log')
                if (Test-Path -LiteralPath $logCopy) {
                    $logText = [System.IO.File]::ReadAllText($logCopy)
                    $named = @(Test-ZsdkLeaks ([System.Text.Encoding]::UTF8.GetBytes($logText)) @($env:USERPROFILE, (Join-Path $temp 'sandbox')) $env:USERNAME)
                    if ($named.Count -gt 0) {
                        $note = 'verify: note: the add-on log in the fake host names a user path: ' + ($named -join '; ')
                        Add-ZsdkLine $note
                        Write-Host $note
                    }
                }
            }
        }
        finally { if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue } }
    }
    foreach ($one in $problems) {
        Add-ZsdkLine ('verify: FAIL ' + $one)
        Write-Host ('verify: FAIL ' + $one)
    }
    if ($problems.Count -gt 0) { throw ('verify failed: ' + $problems.Count + ' problems, first: ' + $problems[0]) }
    $done = 'verify: passed (fake host, exports, version resource, byte scan)'
    Add-ZsdkLine $done
    Write-Host $done
}

function Invoke-ZsdkPython {
    param([Parameter(Mandatory = $true)][string]$Script, [string[]]$Arguments = @())
    $python = Find-ZsdkPython
    if (-not $python) { throw 'Python 3 was not found: put python.exe on PATH or set ZSDK_PYTHON to it (ZSDK_PYTHON=none means no Python)' }
    $saved = [Environment]::GetEnvironmentVariable('PYTHONDONTWRITEBYTECODE', 'Process')
    [Environment]::SetEnvironmentVariable('PYTHONDONTWRITEBYTECODE', '1', 'Process')
    try { & $python -B $Script @Arguments }
    finally { [Environment]::SetEnvironmentVariable('PYTHONDONTWRITEBYTECODE', $saved, 'Process') }
}

Export-ModuleMember -Function Reset-ZsdkLog, Get-ZsdkWarnings, Get-ZsdkSdkWarnings, Get-ZsdkRoot, Get-ZsdkVersion, Get-ZsdkWork, Resolve-ZsdkWrapper, Import-ZsdkToolchain, Enter-ZsdkUtf8, Exit-ZsdkUtf8, Find-ZsdkGame, Get-ZsdkGameExe, Test-ZsdkWarning, Test-ZsdkSdkWarning, Invoke-ZsdkTool, Invoke-ZsdkCompile, Get-ZsdkUnits, Get-ZsdkAddonUnits, Get-ZsdkObjects, Get-ZsdkFlags, Test-ZsdkDeterministic, Get-ZsdkIncludes, Write-ZsdkGenerated, Write-ZsdkResponse, Write-ZsdkSymbols, Join-ZsdkQuoted, Save-ZsdkLog, Add-ZsdkLine, Test-ZsdkManifest, Get-ZsdkManifestHash, Enter-ZsdkLock, Exit-ZsdkLock, Invoke-ZsdkBuild, Invoke-ZsdkTests, Invoke-ZsdkRelease, Invoke-ZsdkPython, Find-ZsdkPython, Read-ZsdkExports, Test-ZsdkLeaks, Invoke-ZsdkVerify
