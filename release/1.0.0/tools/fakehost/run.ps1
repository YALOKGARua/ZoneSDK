[CmdletBinding()]
param(
    [string]$Addon,
    [string]$Label,
    [string]$Out,
    [int]$Presents = 300,
    [string]$Folder,
    [string]$LogName,
    [string]$Sandbox,
    [ValidateSet('free', 'exit', 'kill', 'live', 'worker', 'crash', 'quit')][string]$Mode,
    [switch]$All,
    [string[]]$Expect = @(),
    [ValidateSet('', 'frame_av', 'frame_throw', 'mutex_av', 'loglock', 'page_av', 'gpu_av', 'retry')][string]$Fault = '',
    [int]$FaultAfter = 5,
    [switch]$Help
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'layout.ps1')

if ($Help -or -not $Addon) {
    Write-Host 'usage: fakehost.cmd -Addon <file.addon64> [-All] [-Label <name>] [-Out <folder>] [-Expect <gates>] [-Mode <mode>]'
    Write-Host '  -Addon   the built add-on, for example bin\MyHud.addon64'
    Write-Host '  -All     run every fake host mode and print a check summary, about three minutes'
    Write-Host '  -Label   a name for the sandbox and the result files, the add-on file name by default'
    Write-Host '  -Out     the folder for the logs and results, a folder under %LOCALAPPDATA%\ZoneSDK by default'
    Write-Host '  -Expect  with -All, also run the checks that need the ZoneSDK fault test add-on: gpu, job, seh, report, quit'
    Write-Host '  -Mode    one run in one mode: free, exit, kill, live, worker, crash or quit'
    Write-Host 'the game must be closed; the fake host never starts next to it'
    if ($Help) { exit 0 }
    exit 1
}
if (Get-Process -Name 'Stalker2-Win64-Shipping' -ErrorAction SilentlyContinue) { throw 'a process named Stalker2-Win64-Shipping is running (the game or another fake host); the fake host does not start next to it, start it again when that process has ended' }
if (-not (Test-Path -LiteralPath $Addon)) { throw ('the add-on does not exist: ' + $Addon) }
$Addon = (Resolve-Path -LiteralPath $Addon).Path
if (-not $Label) { $Label = [System.IO.Path]::GetFileNameWithoutExtension($Addon) }

$exe = Get-FakehostExe -ScriptRoot $PSScriptRoot
$stamp = Get-FakehostStamp -ScriptRoot $PSScriptRoot
$built = if (Test-Path -LiteralPath ($exe + '.stamp')) { ([System.IO.File]::ReadAllText($exe + '.stamp')).Trim() } else { '' }
if (-not (Test-Path -LiteralPath $exe) -or $built -ne $stamp) {
    Write-Host $(if (Test-Path -LiteralPath $exe) { 'the fake host was built from other sources, building it again now' } else { 'the fake host is not built yet, building it now' })
    & (Join-Path $PSScriptRoot 'build.ps1') | Out-Null
}
if (-not (Test-Path -LiteralPath $exe)) { throw ('the fake host did not build: ' + $exe) }

$sandboxRoot = Get-FakehostSandboxRoot -ScriptRoot $PSScriptRoot -Sandbox $Sandbox
$utf8 = New-Object System.Text.UTF8Encoding($false)

function Text([string]$path) {
    if (-not $path -or -not (Test-Path -LiteralPath $path)) { return '' }
    return [System.IO.File]::ReadAllText($path, $utf8)
}

function Find-Data([string]$sandbox) {
    $dir = Get-ChildItem -LiteralPath $sandbox -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $dir) { return $null }
    $log = Get-ChildItem -LiteralPath $dir.FullName -Filter '*.log' -File -ErrorAction SilentlyContinue | Select-Object -First 1
    $ini = Get-ChildItem -LiteralPath $dir.FullName -Filter '*.ini' -File -ErrorAction SilentlyContinue | Select-Object -First 1
    return [pscustomobject]@{ Folder = $dir.Name; Dir = $dir.FullName; Log = $(if ($log) { $log.FullName } else { '' }); Ini = $(if ($ini) { $ini.FullName } else { '' }) }
}

function Start-Fake([string]$sandbox, [string]$json, [string]$mode, [string]$fault, [int]$faultAfter, [int]$timeoutMs, [int]$count = 0, [int]$stall = 0, [string]$hostExe = '', [bool]$asked = $false) {
    $frames = if ($count -gt 0) { $count } else { $Presents }
    $argv = @(('"' + $Addon + '"'), ('"' + $sandbox + '"'), ('"' + $json + '"'), $frames)
    if ($mode) { $argv += $mode }
    $hadFault = $null -ne $env:ZSDK_FAULT
    $oldFault = $env:ZSDK_FAULT
    $oldAfter = $env:ZSDK_FAULT_AFTER
    $oldStall = $env:ZSDK_FAKE_STALL
    $oldReport = $env:ZSDK_FAKE_REPORT
    if ($fault) {
        $env:ZSDK_FAULT = $fault
        $env:ZSDK_FAULT_AFTER = [string]$faultAfter
    }
    if ($stall -gt 0) { $env:ZSDK_FAKE_STALL = [string]$stall }
    if ($asked) { $env:ZSDK_FAKE_REPORT = '1' }
    $runExe = if ($hostExe) { $hostExe } else { $exe }
    try {
        $proc = Start-Process -FilePath $runExe -ArgumentList $argv -PassThru -NoNewWindow
        $null = $proc.Handle
        if (-not $proc.WaitForExit($timeoutMs)) {
            Stop-Process -Id $proc.Id -Force
            return [pscustomobject]@{ code = -1; json = $null; timedOut = $true }
        }
        $data = $null
        if (Test-Path -LiteralPath $json) { $data = Get-Content -LiteralPath $json -Raw | ConvertFrom-Json }
        return [pscustomobject]@{ code = $proc.ExitCode; json = $data; timedOut = $false }
    }
    finally {
        if ($fault) {
            if ($hadFault) { $env:ZSDK_FAULT = $oldFault } else { Remove-Item Env:\ZSDK_FAULT -ErrorAction SilentlyContinue }
            if ($null -ne $oldAfter) { $env:ZSDK_FAULT_AFTER = $oldAfter } else { Remove-Item Env:\ZSDK_FAULT_AFTER -ErrorAction SilentlyContinue }
        }
        if ($stall -gt 0) {
            if ($null -ne $oldStall) { $env:ZSDK_FAKE_STALL = $oldStall } else { Remove-Item Env:\ZSDK_FAKE_STALL -ErrorAction SilentlyContinue }
        }
        if ($asked) {
            if ($null -ne $oldReport) { $env:ZSDK_FAKE_REPORT = $oldReport } else { Remove-Item Env:\ZSDK_FAKE_REPORT -ErrorAction SilentlyContinue }
        }
    }
}

if (-not $All) {
    if (-not $Out) { $Out = Join-Path $sandboxRoot ('out\' + $Label) }
    $sandbox = Join-Path $sandboxRoot $Label
    if (Test-Path -LiteralPath $sandbox) { Remove-Item -LiteralPath $sandbox -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $sandbox, $Out | Out-Null
    $json = Join-Path $Out ($Label + '.fakehost.json')
    $r = Start-Fake $sandbox $json $Mode $Fault $FaultAfter 35000
    if ($r.timedOut) { throw 'the fake host did not finish in 35 s and was stopped' }
    $data = Find-Data $sandbox
    $logPath = ''
    $iniPath = ''
    if ($Folder -and $LogName) {
        $logPath = Join-Path (Join-Path $sandbox $Folder) $LogName
        $iniPath = Join-Path (Join-Path $sandbox $Folder) ($LogName -replace '\.log$', '.ini')
    }
    elseif ($data) {
        $logPath = $data.Log
        $iniPath = $data.Ini
    }
    if ($logPath -and (Test-Path -LiteralPath $logPath)) { Copy-Item -LiteralPath $logPath -Destination (Join-Path $Out ($Label + '.log')) -Force }
    if ($iniPath -and (Test-Path -LiteralPath $iniPath)) { Copy-Item -LiteralPath $iniPath -Destination (Join-Path $Out ($Label + '.ini')) -Force }
    Write-Host ('fake host exit {0}, result {1}' -f $r.code, $json)
    if ($Mode) { exit $r.code }
    Write-Output $r.code
    return
}

$want = @(@('exitmark', 'lost', 'dupetag', 'paths', 'exename', 'watcher') + @($Expect | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim().ToLowerInvariant() } | Where-Object { $_ })) | Select-Object -Unique
if (-not $Out) { $Out = Join-Path $sandboxRoot ('harness\' + $Label) }
$sandbox = Join-Path $sandboxRoot ('harness-sandbox\' + $Label)
if (Test-Path -LiteralPath $sandbox) { Remove-Item -LiteralPath $sandbox -Recurse -Force }
if (Test-Path -LiteralPath $Out) { Remove-Item -LiteralPath $Out -Recurse -Force }
New-Item -ItemType Directory -Force -Path $sandbox, $Out | Out-Null

$script:fails = 0
$script:passes = 0
$script:skipped = 0
$report = New-Object System.Collections.Generic.List[string]
$data = $null

function Say([string]$line) { $report.Add($line); Write-Host $line }

function Check([bool]$ok, [string]$what, [string]$gate = 'base') {
    if ($gate -ne 'base' -and ($want -notcontains $gate)) { $script:skipped++; Say ('skip (' + $gate + ') ' + $what); return }
    if ($ok) { $script:passes++; Say ('ok   ' + $what) } else { $script:fails++; Say ('FAIL ' + $what) }
}

function IniText { return (Text $(if ($data) { $data.Ini } else { '' })) }
function MarkPath { if ($data -and $data.Ini) { return ($data.Ini -replace '\.ini$', '.exit') } return '' }

function Field([string]$key) {
    $m = [regex]::Match((IniText), ('(?m)^' + [regex]::Escape($key) + '=(.*?)\r?$'))
    if ($m.Success) { return $m.Groups[1].Value }
    return '?'
}

function Session {
    $m = [regex]::Match((IniText), '(?m)^session=([0-9A-Fa-f]*)\r?$')
    if ($m.Success) { return $m.Groups[1].Value }
    return ''
}

function Marked([string]$session) {
    $m = [regex]::Match((Text (MarkPath)), '^clean ([0-9A-F]+) ([0-9A-F]{16})\r?\n$')
    return $m.Success -and $m.Groups[1].Value -eq $session
}

function Inject([string]$watch) {
    if (-not $data -or -not $data.Ini) { return }
    $body = [regex]::Replace((IniText), '(?m)^watch=.*?(\r?)$', ('watch=' + $watch + '$1'))
    [System.IO.File]::WriteAllText($data.Ini, $body, $utf8)
}

function Older([string]$watch) {
    if (-not $data -or -not $data.Ini) { return }
    $body = [regex]::Replace((IniText), '(?m)^session=[^\r\n]*\r?\n', '')
    $body = [regex]::Replace($body, '(?m)^watch=.*?(\r?)$', ('watch=' + $watch + '$1'))
    [System.IO.File]::WriteAllText($data.Ini, $body, $utf8)
}

function Run([int]$step, [string]$mode, [string]$fault = '', [int]$count = 0, [int]$stall = 0, [bool]$asked = $false) {
    $json = Join-Path $Out ('{0}-{1}.json' -f $step, $mode)
    $from = 0
    if ($script:data -and $script:data.Log) { $from = (Text $script:data.Log).Length }
    $r = Start-Fake $sandbox $json $mode $fault $FaultAfter 35000 $count $stall '' $asked
    if ($r.timedOut) { throw ('the fake host did not finish in 35 s, step ' + $step) }
    $script:data = Find-Data $sandbox
    $log = ''
    if ($script:data -and $script:data.Log) { $full = (Text $script:data.Log); if ($full.Length -ge $from) { $log = $full.Substring($from) } else { $log = $full } }
    Add-Member -InputObject $r -NotePropertyName log -NotePropertyValue $log -Force
    foreach ($suffix in @('log', 'ini', 'exit')) {
        $src = ''
        if ($suffix -eq 'log' -and $data) { $src = $data.Log }
        elseif ($suffix -eq 'ini' -and $data) { $src = $data.Ini }
        elseif ($suffix -eq 'exit') { $src = MarkPath }
        if ($src -and (Test-Path -LiteralPath $src)) { Copy-Item -LiteralPath $src -Destination (Join-Path $Out ('{0}-{1}.{2}' -f $step, $mode, $suffix)) -Force }
    }
    Say ('step {0} {1}: exit code {2}' -f $step, $mode, $r.code)
    return $r
}

function Sound($r, [string]$status) {
    Check ($null -ne $r.json -and $r.json.status -eq $status) ('the fake host reports ' + $status)
    Check ($null -ne $r.json -and $r.json.second_copy_refused) 'the second copy is refused'
    Check ($null -ne $r.json -and $r.json.imgui_calls_present -eq 0 -and $r.json.runtime_traps -eq 0) 'no ImGui call in present, no runtime trap'
    Check ($null -ne $r.json -and $r.json.page_calls -gt 0) 'the tab page is drawn'
}

Say ('fake host modes, add-on ' + $Addon + ' sha256 ' + (Get-FileHash -LiteralPath $Addon -Algorithm SHA256).Hash)
Say ('expectations on: ' + ($want -join ', '))

$r = Run 1 'exit'
Sound $r 'exit'
Check ($r.code -eq 0) 'exit: wmain returns 0'
Check ($null -ne $r.json -and $r.log.Contains('hook: game thread ' + $r.json.main_thread + ',')) 'the SDK hooks the main thread that owns the UnrealWindow'
Check ($null -ne $r.json -and $r.json.runtime_dropped -eq 1) 'destroy_effect_runtime came once'
Check ($null -ne $r.json -and $r.json.device_dropped -eq 1) 'destroy_device came once'
Check ($null -ne $r.json -and $r.json.exit_thread -eq $r.json.main_thread) 'the process exit runs on the main thread'
Check ($null -ne $r.json -and -not (@($r.json.journal) -contains 'unregister_addon')) 'the add-on stays registered to the end'
if ($want -notcontains 'gpu') { Check ($null -ne $r.json -and $r.json.pipeline_init -eq 0 -and $r.json.pipeline_bind -eq 0 -and $r.json.pipeline_destroy -eq 0) 'no pipeline events without GPU hooks' }
$s1 = Session
Check ($s1.Length -gt 0) 'the add-on ini has the session of run 1' 'exitmark'
Check ($null -ne $r.json -and $r.json.runtime_made -eq 1) 'init_effect_runtime came once' 'exitmark'
Check (Marked $s1) ('exit: the clean exit mark is written for run 1') 'exitmark'
Check ($null -ne $r.json -and $r.json.pipeline_init -gt 0) 'the pipeline init reaches a counting hook' 'gpu'
Check ($null -ne $r.json -and $r.json.pipeline_bind -gt 0) 'the pipeline bind reaches a counting hook' 'gpu'
Check ($null -ne $r.json -and $r.json.pipeline_destroy -gt 0) 'the pipeline destroy reaches a counting hook' 'gpu'
Check ([regex]::IsMatch($r.log, 'pipeline hooks counted init [1-9]\d*, bind [1-9]\d*, destroy [1-9]\d*')) 'the add-on hooks counted the synthetic init, bind and destroy' 'gpu'
Check ($r.log.Contains('job faulttest.pulse every 200 ms') -and $r.log.Contains('faulttest: job ran 1 times')) 'exit: the pulse job is registered from boot and runs until the exit' 'job'
$taken = @(@($r.json.journal) | Where-Object { $_ -like 'debug: ZoneSDK: * does not load: the tag * is already taken in this process*' })
Check ($null -ne $r.json -and $r.json.second_copy_refused -and $taken.Count -ge 1) 'dupetag: the refused second copy names its taken tag on the debug channel' 'dupetag'
Check ($r.log.Contains(' data at %LOCALAPPDATA%\')) 'paths: the boot line names the data folder under %LOCALAPPDATA%, not the full path' 'paths'
Check ($r.log.Contains('exit watcher off: guard set zsdk.exit 0/2 match, image 0x') -and $r.log.Contains('only WM_QUIT and the exit mark remain') -and -not $r.log.Contains('exit watcher on')) 'watcher: the zsdk.exit bytes do not match the fake exe, the exit watcher is off and the log says so' 'watcher'
Check (-not $r.log.Contains('game exit requested (GIsRequestingExit)')) 'watcher: with the watcher off no exit is read from GIsRequestingExit' 'watcher'

$r = Run 2 'exit'
Sound $r 'exit'
$s2 = Session
Check ($r.log.Contains('ended cleanly')) 'run 2 reads the clean exit of run 1' 'exitmark'
Check ((Field 'disabled') -eq '') 'nothing is disabled after a clean exit' 'exitmark'
Check ($s2 -ne $s1 -and (Marked $s2)) 'run 2 has its own session and its own mark' 'exitmark'

$r = Run 3 'kill'
Check ($r.code -eq 9) 'kill: TerminateProcess(9)'
Check ($null -ne $r.json -and $r.json.status -eq 'kill') 'the fake host reports kill'
Check ($r.log.Contains('faulttest: job ran 1 times')) 'kill: the pulse job was running when the process was killed, no hang' 'job'
$s3 = Session
Check ($r.log.Contains('ended cleanly')) 'run 3 reads the clean exit of run 2' 'exitmark'
Check (-not (Test-Path -LiteralPath (MarkPath))) 'kill: no DllMain runs, no exit mark' 'exitmark'

Inject 'fakehost.open'
Check ((Field 'watch') -eq 'fakehost.open') 'a window is written as open in run 3' 'exitmark'
$r = Run 4 'exit'
Sound $r 'exit'
$s4 = Session
Check ($r.log.Contains('no clean exit mark')) 'run 4 sees no clean exit mark from the killed run 3' 'exitmark'
Check ($r.log.Contains('fakehost.open pending or watched when the last run ended, disabled')) 'the window of the killed run is turned off' 'exitmark'
Check ((Field 'disabled') -eq 'fakehost.open') 'fakehost.open is disabled' 'exitmark'
Check (Marked $s4) 'run 4 ends with its own mark' 'exitmark'

Inject 'fakehost.stale'
$r = Run 5 'exit'
Sound $r 'exit'
$s5 = Session
Check ($r.log.Contains('counts as a crash') -or $r.log.Contains('does not match')) 'a change of the ini after the mark makes the mark stale' 'exitmark'
Check ((Field 'disabled') -eq 'fakehost.open,fakehost.stale') 'the stale mark hides nothing: fakehost.stale is disabled too' 'exitmark'
Check (Marked $s5) 'run 5 ends with its own mark' 'exitmark'

$r = Run 6 'free'
Sound $r 'done'
$s6 = Session
Check ($r.log.Contains('ended cleanly')) 'run 6 reads the clean exit of run 5' 'exitmark'
Check ($null -ne $r.json -and @($r.json.journal) -contains 'freed') 'free: FreeLibrary was called'
Check ($null -ne $r.json -and $r.json.runtime_dropped -eq 1 -and $r.json.device_dropped -eq 1) 'free: the runtime and device were let go before FreeLibrary'
Check (Marked $s6) 'free: the module is pinned, the mark comes at process exit' 'exitmark'

$r = Run 7 'live'
Sound $r 'live'
$s7 = Session
Check ($r.code -eq 0) 'live: wmain returns 0'
Check ($r.log.Contains('ended cleanly')) 'run 7 reads the clean exit of run 6' 'exitmark'
Check ($null -ne $r.json -and $r.json.runtime_dropped -eq 0) 'live: the swap chain was never let go'
Check ($null -ne $r.json -and $r.json.device_dropped -eq 0) 'live: the device was never let go'
Check (-not (Test-Path -LiteralPath (MarkPath))) 'live: an exit while the swap chain still draws writes no mark' 'exitmark'

Inject 'fakehost.open'
$r = Run 8 'worker'
Check ($r.code -eq 777004) 'worker: exit(777004) from a worker thread after the swap chain went'
Check ($null -ne $r.json -and $r.json.status -eq 'worker' -and $r.json.runtime_dropped -eq 1 -and $r.json.exit_thread -ne $r.json.main_thread) 'worker: the exit runs on another thread than the game thread'
$s8 = Session
Check ($r.log.Contains('no clean exit mark')) 'run 8 sees no clean exit mark from run 7' 'exitmark'
Check (-not (Test-Path -LiteralPath (MarkPath))) 'worker: exit() off the game thread writes no mark' 'exitmark'

Inject 'fakehost.open'
$r = Run 9 'crash'
Check ($r.code -eq 777004) 'crash: exit(777004) from a worker thread while the swap chain still draws'
Check ($null -ne $r.json -and $r.json.status -eq 'crash' -and $r.json.runtime_dropped -eq 0 -and $r.json.exit_thread -ne $r.json.main_thread) 'crash: no destroy_effect_runtime, another thread'
$s9 = Session
Check ($r.log.Contains('no clean exit mark')) 'run 9 sees no clean exit mark from run 8' 'exitmark'
Check (-not (Test-Path -LiteralPath (MarkPath))) 'crash: no mark' 'exitmark'

Inject 'fakehost.open'
$r = Run 10 'exit'
Sound $r 'exit'
$s10 = Session
Check ($r.log.Contains('no clean exit mark')) 'run 10 sees no clean exit mark from the crash in run 9' 'exitmark'
Check (Marked $s10) 'run 10 ends with its own mark' 'exitmark'

$seh = $want -contains 'seh'

$throw = Run 11 'exit' $(if ($seh) { 'frame_throw' } else { '' })
Check ($throw.code -eq 0) 'throw: a C++ throw in the frame hook does not crash the process' 'seh'
Check ($throw.log.Contains('frame error')) 'throw: the inner catch logs the frame error' 'seh'
Check ($null -ne $throw.json -and $throw.json.page_calls -gt 0) 'throw: the add-on keeps drawing after the throw' 'seh'

$av = Run 12 'exit' $(if ($seh) { 'frame_av' } else { '' })
Check ($av.code -eq 0) 'av: an access violation in the frame hook is fenced, the host keeps presenting' 'seh'
Check ($av.log.Contains('structured exception')) 'av: one structured-exception line, the frame lane goes off' 'seh'
Check ([regex]::Matches($av.log, 'structured exception 0xC0000005 at \S+\+0x[0-9A-F]+ in the frame lane, the frame and toasts lanes are off for this session and present\.frame is disabled until Try again').Count -eq 1) 'av: the line names the code, the module offset and the lane, frame and toasts go off together' 'seh'
Check (@((Field 'disabled') -split ',') -contains 'present.frame') 'av: present.frame is disabled in the ini' 'seh'
$kept = Run 31 'exit'
Check ($kept.log.Contains('safety: the frame lanes are disabled after a crash in an earlier session')) 'av: the next start keeps the frame lane off' 'seh'
$back = Run 32 'exit' $(if ($seh) { 'retry' } else { '' })
Check ($back.log.Contains('faulttest: Try again pressed from boot') -and $back.log.Contains('safety: the frame lanes run again after Try again')) 'av: Try again brings the frame lane back' 'seh'
Check (-not (@((Field 'disabled') -split ',') -contains 'present.frame')) 'av: after Try again present.frame is no longer disabled' 'seh'

$mutex = Run 13 'exit' $(if ($seh) { 'mutex_av' } else { '' })
Check (-not $mutex.timedOut -and $mutex.code -eq 0) 'mutex: an access violation while a lock is held causes no hang' 'seh'
Check ($mutex.log.Contains('faulttest: the frame mutex is free after the fault')) 'mutex: the unwind released the add-on lock, the pulse job takes it on another thread' 'seh'
$cleared = Run 33 'exit' $(if ($seh) { 'retry' } else { '' })
Check (-not (@((Field 'disabled') -split ',') -contains 'present.frame')) 'mutex: Try again clears the frame lane before the next steps' 'seh'

$work = Run 14 'worker' $(if ($seh) { 'loglock' } else { '' })
Check ($work.code -eq 777004) 'loglock: a worker exits without hanging on the log lock' 'seh'

$page = Run 34 'exit' $(if ($seh) { 'page_av' } else { '' })
Check ($page.code -eq 0 -and [regex]::IsMatch($page.log, 'structured exception 0xC0000005 at \S+\+0x[0-9A-F]+ in the page lane, this lane is off for this session')) 'page: an access violation in the page hook switches off only the page lane' 'seh'
Check ($null -ne $page.json -and $page.json.page_calls -gt 0 -and $page.json.imgui_calls_present -eq 0) 'page: the tab is still drawn, as the SDK fallback with Try again' 'seh'
$pageBack = Run 35 'exit' $(if ($seh) { 'retry' } else { '' })
Check ($pageBack.log.Contains('safety: the page lanes are disabled after a crash in an earlier session') -and $pageBack.log.Contains('safety: the page lanes run again after Try again')) 'page: the next start keeps the page lane off until Try again brings it back' 'seh'
$gpuGate = if ($want -contains 'gpu') { 'seh' } else { 'gpu' }
$gpuOn = $seh -and ($want -contains 'gpu')
$gpuRun = Run 36 'exit' $(if ($gpuOn) { 'gpu_av' } else { '' })
Check ($gpuRun.code -eq 0 -and [regex]::IsMatch($gpuRun.log, 'structured exception 0xC0000005 at \S+\+0x[0-9A-F]+ in the pipeline init lane, this lane is off for this session')) 'gpu: an access violation in the pipeline init hook switches off only that lane' $gpuGate
$gpuBack = Run 37 'exit' $(if ($gpuOn) { 'retry' } else { '' })
Check ($gpuBack.log.Contains('safety: the pipeline init lanes run again after Try again')) 'gpu: Try again brings the pipeline init lane back' $gpuGate

Older 'fakehost.older'
Check ((Session) -eq '' -and (Field 'watch') -eq 'fakehost.older') 'an ini as an older ZoneSDK writes it: no session line, a window open' 'exitmark'
$r = Run 15 'exit'
Sound $r 'exit'
$s15 = Session
Check ($r.log.Contains('not judged, written by an older ZoneSDK')) 'older ini: the ini of an older ZoneSDK is not judged, the log says so' 'exitmark'
Check (-not (@((Field 'disabled') -split ',') -contains 'fakehost.older')) 'older ini: the window of the older ini is cleared, not disabled' 'exitmark'
Check ($s15.Length -gt 0 -and (Marked $s15)) 'the run after the older ini has a session and its own mark' 'exitmark'

$lost = $want -contains 'lost'
$r = Run 16 'live' '' 1200 $(if ($lost) { 3000 } else { 0 })
Sound $r 'live'
Check ($r.code -eq 0) 'long live run: wmain returns 0 with the swap chain alive'
$runs = 0
foreach ($m in [regex]::Matches($r.log, 'faulttest: job ran (\d+) times')) { $runs = [Math]::Max($runs, [int]$m.Groups[1].Value) }
Check ($null -ne $r.json -and $r.json.elapsed_ms -ge 9000 -and $runs -ge 25) ('long live run: the pulse job runs every 200 ms for about 10 s, ' + $runs + ' runs logged') 'job'
Check (-not $r.log.Contains('job faulttest.pulse: structured exception') -and -not $r.log.Contains('job faulttest.pulse error')) 'long live run: the job never faults' 'job'
Check ($null -ne $r.json -and @($r.json.journal) -contains 'stall: the window thread pumps again') 'lost: after the overlay closed the host stopped pumping the window thread for 3 s while it kept presenting' 'lost'
Check ($r.log.Contains('game thread pump lost: no tick for')) 'lost: the SDK logs the game thread pump as lost' 'lost'
Check ($r.log.Contains('game thread pump is back after')) 'lost: the SDK logs the pump back once the window thread pumps again' 'lost'
if ($lost) { Check ($r.log.Contains('faulttest: the phase is Lost') -and $r.log.Contains('faulttest: the phase left Lost')) 'lost: the fault test add-on sees Phase::Lost and then leaves it' 'job' }

$asked = $want -contains 'report'
$r = Run 17 'exit' '' 0 0 $asked
Sound $r 'exit'
$reportFile = if ($data -and $data.Dir) { Join-Path $data.Dir 'report.txt' } else { '' }
$reportText = Text $reportFile
Check ($r.log.Contains('faulttest: the report button ran, the report is in the data folder') -and $r.log.Contains('report saved to %LOCALAPPDATA%\')) 'report: the report button builds the report on the overlay path and saves it, the log names the masked path' 'report'
Check ($reportText.StartsWith('ZoneSDK report, ') -and $reportText.Contains('add-on: ') -and $reportText.Contains("`r`nzsdk: ") -and $reportText.Contains("`r`ngame exe: Stalker2-Win64-Shipping.exe") -and $reportText.Contains("`r`nreshade: ") -and $reportText.Contains("`r`nadd-ons next to the game exe: ")) 'report: the report has the add-on, SDK, exe, ReShade and add-on list lines' 'report'
Check ($reportText.Contains("`r`n[safety]`r`nsession ") -and $reportText.Contains("`r`n[ZoneFaultTest]`r`nfaulttest section: the pulse job ran ") -and $reportText.Contains("`r`n[log] last ")) 'report: the [safety] lists, the add-on section from Hooks::report and the log tail are in the report' 'report'
Check ($reportText.Length -gt 0 -and $reportText -notmatch '(?i)[A-Z]:\\Users\\' -and -not $reportText.Contains($sandboxRoot) -and ($env:USERNAME.Length -lt 2 -or $reportText -notmatch ('(?i)[\\/]' + [regex]::Escape($env:USERNAME) + '([\\/"'' ]|$)'))) 'report: the report has no user path and no user name' 'report'
$whole = Text $(if ($data) { $data.Log } else { '' })
Check ($whole.Length -gt 0 -and $whole -notmatch '(?i)[A-Z]:\\Users\\' -and -not $whole.Contains($sandboxRoot) -and ($env:USERNAME.Length -lt 2 -or $whole -notmatch ('(?i)[\\/]' + [regex]::Escape($env:USERNAME) + '([\\/"'' ]|$)'))) 'paths: the whole log of every run names no user path, no sandbox path and no user name' 'paths'

$plainOut = Join-Path $Out 'plain'
$plain = @(& $PSCommandPath -Addon $Addon -Label ($Label + '-plain') -Out $plainOut -Presents $Presents -Sandbox $sandboxRoot 6>$null)
$plainJson = Join-Path $plainOut ($Label + '-plain.fakehost.json')
$plainData = $null
if (Test-Path -LiteralPath $plainJson) { $plainData = Get-Content -LiteralPath $plainJson -Raw | ConvertFrom-Json }
Check ($plain.Count -eq 1 -and [string]$plain[0] -eq '0') 'a default run without -Mode writes the host exit code to the pipeline'
Check ($null -ne $plainData -and $plainData.status -eq 'done') 'a default run passes no mode, the host runs its own default'

$gdk = $null
$gdkLog = ''
if ($want -contains 'exename') {
    $gdkHost = Join-Path $sandboxRoot ('harness-gdk\' + $Label + '\bin\Stalker2-WinGDK-Shipping.exe')
    $gdkSandbox = Join-Path $sandboxRoot ('harness-sandbox\' + $Label + '-gdk')
    foreach ($one in @((Split-Path $gdkHost -Parent), $gdkSandbox)) { if (Test-Path -LiteralPath $one) { Remove-Item -LiteralPath $one -Recurse -Force } }
    New-Item -ItemType Directory -Force -Path (Split-Path $gdkHost -Parent), $gdkSandbox | Out-Null
    Copy-Item -LiteralPath $exe -Destination $gdkHost -Force
    $gdk = Start-Fake $gdkSandbox (Join-Path $Out '18-gdk.json') 'exit' '' $FaultAfter 35000 0 0 $gdkHost
    $gdkData = Find-Data $gdkSandbox
    $gdkLog = Text $(if ($gdkData) { $gdkData.Log } else { '' })
    foreach ($suffix in @('log', 'ini')) {
        $src = if ($gdkData -and $suffix -eq 'log') { $gdkData.Log } elseif ($gdkData) { $gdkData.Ini } else { '' }
        if ($src -and (Test-Path -LiteralPath $src)) { Copy-Item -LiteralPath $src -Destination (Join-Path $Out ('18-gdk.' + $suffix)) -Force }
    }
    Say ('step 18 exit as Stalker2-WinGDK-Shipping.exe: exit code {0}' -f $gdk.code)
}
Check ($null -ne $gdk -and -not $gdk.timedOut -and $gdk.code -eq 0 -and $null -ne $gdk.json -and $gdk.json.exe_name -eq 'Stalker2-WinGDK-Shipping.exe' -and $gdk.json.status -eq 'exit') 'exename: the add-on loads under another store build name and the host exits cleanly' 'exename'
Check ($gdkLog.Contains('this game build is not supported: Stalker2-WinGDK-Shipping.exe is not the Steam build') -and -not $gdkLog.Contains('native bindings')) 'exename: the add-on says the build is not supported and tries no binding' 'exename'
Check ($null -ne $gdk -and $null -ne $gdk.json -and $gdk.json.page_calls -gt 0 -and $gdk.json.imgui_calls_present -eq 0 -and $gdk.json.runtime_traps -eq 0) 'exename: the tab is still drawn with no ImGui call in present' 'exename'

$r = Run 19 'quit' '' 1200 3000
Sound $r 'quit'
Check ($r.code -eq 0) 'quit: wmain returns 0 after a WM_QUIT from PostQuitMessage at a quarter of the presents'
Check ($null -ne $r.json -and @($r.json.journal) -contains 'quit: PostQuitMessage on the main thread, the host keeps presenting and pumping' -and @($r.json.journal) -contains 'stall: the window thread pumps again') 'quit: after the WM_QUIT the host stalled the window thread for 3 s while it kept presenting'
Check (-not $r.log.Contains('game thread pump lost')) 'quit: no pump-lost line after WM_QUIT, although no tick came for 3 s'
$quitLines = [regex]::Matches($r.log, 'faulttest: quit hook ran (\d+) times, on the game thread (\w+), engine calls stopped (\w+), jobs stopping (\w+)')
Check ($quitLines.Count -eq 1 -and $quitLines[0].Groups[1].Value -eq '1' -and $quitLines[0].Groups[2].Value -eq 'yes' -and $quitLines[0].Groups[3].Value -eq 'yes' -and $quitLines[0].Groups[4].Value -eq 'yes') 'quit: hooks.quit ran once, on the game thread, with engine calls and jobs already stopping' 'quit'
Check ([regex]::Matches($r.log, 'game exit requested \(WM_QUIT\)').Count -eq 1) 'quit: the WM_QUIT path logs game exit requested once' 'quit'
Check ($r.log.Contains('exit watcher off: guard set zsdk.exit') -and [regex]::Matches($r.log, 'game exit requested \(').Count -eq 1 -and [regex]::Matches($r.log, 'game exit requested \(WM_QUIT\)').Count -eq 1) 'watcher: with the exit watcher off the WM_QUIT path still runs the exit sequence, once' 'watcher'
Check ($r.log.Contains('faulttest: job ran 1 times') -and -not [regex]::IsMatch($r.log, 'faulttest: job ran (25|50) times')) 'quit: the pulse job stops at WM_QUIT, it never reaches 25 runs in the 9 s after it' 'job'
$s19 = Session
Check ($s19.Length -gt 0 -and (Marked $s19)) 'quit: the clean exit mark is written for an exit that began with WM_QUIT' 'exitmark'
$r = Run 20 'exit'
Sound $r 'exit'
Check ($r.log.Contains('ended cleanly')) 'run 20 reads the clean exit of the WM_QUIT run 19' 'exitmark'

Say ('fake host modes: {0} checks, {1} failures, {2} skipped' -f ($script:passes + $script:fails), $script:fails, $script:skipped)
[System.IO.File]::WriteAllLines((Join-Path $Out 'summary.txt'), $report, $utf8)
exit $script:fails
