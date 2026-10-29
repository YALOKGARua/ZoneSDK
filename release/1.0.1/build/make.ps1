[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Config = 'Release',
    [switch]$Tests,
    [string]$Out,
    [int]$WaitMinutes = 30
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'zsdk.psm1') -Force
$sdk = Get-ZsdkRoot
$sdkFull = [System.IO.Path]::GetFullPath($sdk).TrimEnd('\')
$released = Test-Path -LiteralPath (Join-Path $sdk 'MANIFEST.sha256')
if (-not $Out) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    $tag = [System.BitConverter]::ToString($sha.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($sdkFull.ToLowerInvariant()))).Replace('-', '').Substring(0, 8)
    $sha.Dispose()
    $Out = Get-ZsdkWork ('selftest-' + $tag)
}
$Out = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Out).TrimEnd('\')
if ($released -and ($Out + '\').StartsWith($sdkFull + '\', [System.StringComparison]::OrdinalIgnoreCase)) { throw ('-Out lies inside the ZoneSDK release ' + $sdkFull + '; the self-test never writes into a release: ' + $Out) }
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$log = Join-Path $Out 'last-build.log'
$treeLog = Join-Path $PSScriptRoot 'last-build.log'
$save = {
    Save-ZsdkLog $log
    if (-not $released) { Save-ZsdkLog $treeLog }
}
$gate = Join-Path $Out '.zsdk-build.lock'
Enter-ZsdkLock $gate $WaitMinutes
$names = @('ZSDK_TEST_HOME', 'ZSDK_HOME')
$saved = @{}
foreach ($n in $names) { $saved[$n] = [Environment]::GetEnvironmentVariable($n, 'Process') }
try {
    Reset-ZsdkLog ('ZoneSDK {0} build {1} {2}' -f (Get-ZsdkVersion), $Config, (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))
    Add-ZsdkLine ('sdk ' + $sdkFull + $(if ($released) { ', a release' } else { ', a working tree' }))
    Add-ZsdkLine ('output ' + $Out)
    Write-Host ('self-test output {0}' -f $Out)
    Import-ZsdkToolchain
    $obj = Join-Path $Out ('obj\' + $Config)
    if (Test-Path -LiteralPath $obj) { Remove-Item -LiteralPath $obj -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $obj | Out-Null
    $manifest = if ($released) { (Get-ZsdkManifestHash $sdk).Substring(0, 8) } else { '' }
    [void](Write-ZsdkGenerated $sdk (Join-Path $obj 'gen') $manifest)
    [void](Test-ZsdkDeterministic $obj)
    $present = (Get-ZsdkFlags $Config -Deterministic -Async) + ' /WX /DZSDK_TESTS /DZSDK_PRESENT_UNIT /DIMGUI_DISABLE ' + (Get-ZsdkIncludes '')
    $overlay = (Get-ZsdkFlags $Config -Deterministic -Async) + ' /WX ' + (Get-ZsdkIncludes '' -Overlay)
    $entry = (Get-ZsdkFlags $Config -Deterministic -Async) + ' /WX ' + (Get-ZsdkIncludes '' -Overlay)
    $vcPdb = Join-Path $obj 'vc.pdb'
    $failures = 0

    $code = Invoke-ZsdkCompile @(Get-ZsdkUnits (Join-Path $sdk 'src\present') @()) $present (Join-Path $obj 'present') $vcPdb 'present units'
    if ($code -ne 0) { throw 'present units failed to compile' }
    $code = Invoke-ZsdkCompile @(Get-ZsdkUnits (Join-Path $sdk 'src\overlay') @()) $overlay (Join-Path $obj 'overlay') $vcPdb 'overlay units'
    if ($code -ne 0) { throw 'overlay units failed to compile' }
    $code = Invoke-ZsdkCompile @(Get-ZsdkUnits (Join-Path $sdk 'src\entry') @()) $entry (Join-Path $obj 'entry') $vcPdb 'entry units'
    if ($code -ne 0) { throw 'entry units failed to compile' }
    [void](Write-ZsdkSymbols @(Get-ZsdkObjects @((Join-Path $obj 'present'))) @(Get-ZsdkObjects @((Join-Path $obj 'overlay'), (Join-Path $obj 'entry'))) (Join-Path $obj 'symbols.txt'))
    $symbolText = Get-Content -LiteralPath (Join-Path $obj 'symbols.txt')
    $violations = @($symbolText | Where-Object { $_ -like 'violation=*' })
    Add-ZsdkLine ('> symbol check: {0} SDK present units call an overlay-only symbol' -f $violations.Count)
    Write-Host ('symbol check: {0} violations' -f $violations.Count)
    if ($violations.Count -gt 0) { $failures++; foreach ($one in $violations) { Add-ZsdkLine $one; Write-Host $one } }

    $expected = [ordered]@{
        'imgui_in_present.cpp' = 'C1083'
        'overlay_in_present.cpp' = 'C1189'
        'present_header_with_imgui.cpp' = 'C1189'
        'detail_in_addon.cpp' = 'C1083'
        'field_unproven.cpp' = 'C2131'
        'field_wrong_guard.cpp' = 'C2131'
        'field_as_offset.cpp' = 'C2677'
        'raw_ledger_in_addon.cpp' = 'C1189'
        'mem_stamp.cpp' = 'C2039'
        'lend_temporary.cpp' = 'C2280'
    }
    $failDir = Join-Path $obj 'fail'
    New-Item -ItemType Directory -Force -Path $failDir | Out-Null
    $failFlags = $present.Replace(' /DZSDK_TESTS', '')
    foreach ($name in $expected.Keys) {
        $file = Join-Path $sdk ('tests\fail\' + $name)
        $wanted = $expected[$name]
        $extra = if ($name -eq 'present_header_with_imgui.cpp') { ' /external:I"' + $sdk + '\extern\imgui"' } else { '' }
        $text = & cmd /d /c ('cl /c {0}{1} /Fo"{2}\\" "{3}" 2>&1' -f $failFlags, $extra, $failDir, $file)
        $exit = $LASTEXITCODE
        $joined = (@($text) | ForEach-Object { [string]$_ }) -join "`n"
        $ok = $exit -ne 0 -and $joined.Contains($wanted)
        Add-ZsdkLine ('> expected failure {0}: exit {1}, wants {2}, {3}' -f $name, $exit, $wanted, $(if ($ok) { 'refused as designed' } else { 'NOT REFUSED' }))
        Write-Host ('expected failure {0}: {1}' -f $name, $(if ($ok) { 'ok' } else { 'FAIL' }))
        if (-not $ok) { $failures++; Add-ZsdkLine $joined }
    }

    if ($Tests) {
        $testObj = Join-Path $obj 'tests'
        $testBin = Join-Path $Out ('bin\' + $Config)
        New-Item -ItemType Directory -Force -Path $testObj, $testBin | Out-Null
        $support = Join-Path $testObj 'support'
        $code = Invoke-ZsdkCompile @(Get-ZsdkUnits (Join-Path $sdk 'tests\support') @()) $present $support $vcPdb 'test support'
        if ($code -ne 0) { throw 'test support failed to compile' }
        $objects = @(Get-ZsdkObjects @((Join-Path $obj 'present'), $support))
        $linkItems = @('/link', '/WX') + @($objects) + @('user32.lib', 'shell32.lib', 'bcrypt.lib')
        $passed = 0
        $failed = 0
        $checks = 0
        $notes = 0
        foreach ($case in Get-ChildItem -LiteralPath (Join-Path $sdk 'tests') -Filter '*.cpp' -File | Sort-Object Name) {
            $caseObj = Join-Path $testObj $case.BaseName
            New-Item -ItemType Directory -Force -Path $caseObj | Out-Null
            $exe = Join-Path $testBin ('zsdk-' + $case.BaseName + '.exe')
            $rsp = Write-ZsdkResponse (Join-Path $caseObj 'build.rsp') (@($present, $case.FullName, ('/Fo"' + $caseObj + '\\"'), ('/Fe"' + $exe + '"')) + $linkItems)
            $code = Invoke-ZsdkTool ('cl @"{0}"' -f $rsp) ('test build ' + $case.Name)
            if ($code -ne 0) { $failed++; continue }
            $sandbox = Join-Path $testObj ($case.BaseName + '.home')
            if (Test-Path -LiteralPath $sandbox) { Remove-Item -LiteralPath $sandbox -Recurse -Force }
            New-Item -ItemType Directory -Force -Path $sandbox | Out-Null
            $env:ZSDK_TEST_HOME = $sandbox
            $env:ZSDK_HOME = $sdk
            Add-ZsdkLine ('> run ' + $case.BaseName)
            $encoding = Enter-ZsdkUtf8
            try {
                $text = & cmd /d /c ('"' + $exe + '" 2>&1')
                $exit = $LASTEXITCODE
            }
            finally { Exit-ZsdkUtf8 $encoding }
            foreach ($one in @($text)) {
                $l = [string]$one
                Add-ZsdkLine $l
                Write-Host $l
                $m = [regex]::Match($l, ': (\d+) checks, (\d+) failures')
                if ($m.Success) { $checks += [int]$m.Groups[1].Value }
                if ($l.StartsWith('warning: ')) { $notes++ }
            }
            Add-ZsdkLine ('exit {0}' -f $exit)
            if ($exit -ne 0) { $failed++ } else { $passed++ }
        }
        $summary = 'tests passed {0} failed {1} checks {2}' -f $passed, $failed, $checks
        if ($notes -gt 0) { $summary += (', {0} test warnings' -f $notes) }
        Add-ZsdkLine $summary
        Write-Host $summary
        $failures += $failed
    }

    if ($released) {
        Test-ZsdkManifest $sdk
        Add-ZsdkLine 'the release still matches its manifest'
    }
    Add-ZsdkLine ('expected-failure and test failures {0}' -f $failures)
    Add-ZsdkLine ('warnings {0}' -f (Get-ZsdkWarnings))
    Write-Host ('warnings {0}' -f (Get-ZsdkWarnings))
    & $save
    if ($failures -gt 0) { throw ('ZoneSDK checks failed: ' + $failures + ' (log: ' + $log + ')') }
    if ((Get-ZsdkWarnings) -gt 0) { throw ('ZoneSDK build has ' + (Get-ZsdkWarnings) + ' warnings, 0 are allowed (log: ' + $log + ')') }
}
catch {
    Add-ZsdkLine ('failed: ' + $_.Exception.Message)
    & $save
    throw
}
finally {
    foreach ($n in $names) { [Environment]::SetEnvironmentVariable($n, $saved[$n], 'Process') }
    Exit-ZsdkLock $gate
}
