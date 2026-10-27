function Get-FakehostRoot {
    param([Parameter(Mandatory = $true)][string]$ScriptRoot)
    return (Split-Path (Split-Path $ScriptRoot -Parent) -Parent)
}

function Test-FakehostInRelease {
    param([Parameter(Mandatory = $true)][string]$Sdk)
    return (Test-Path -LiteralPath (Join-Path $Sdk 'MANIFEST.sha256'))
}

function Get-FakehostSdkVersion {
    param([Parameter(Mandatory = $true)][string]$Sdk)
    $text = Get-Content -LiteralPath (Join-Path $Sdk 'include\zsdk\version.h') -Raw
    return ('{0}.{1}.{2}' -f [regex]::Match($text, 'ZSDK_VERSION_MAJOR (\d+)').Groups[1].Value, [regex]::Match($text, 'ZSDK_VERSION_MINOR (\d+)').Groups[1].Value, [regex]::Match($text, 'ZSDK_VERSION_PATCH (\d+)').Groups[1].Value)
}

function Get-FakehostWorkRoot {
    param([Parameter(Mandatory = $true)][string]$ScriptRoot, [string]$Out)
    if ($Out) { return $Out }
    $sdk = Get-FakehostRoot -ScriptRoot $ScriptRoot
    if (Test-FakehostInRelease -Sdk $sdk) {
        $ver = Get-FakehostSdkVersion -Sdk $sdk
        return (Join-Path $env:LOCALAPPDATA ('ZoneSDK\' + $ver + '\fakehost'))
    }
    return $ScriptRoot
}

function Get-FakehostSandboxRoot {
    param([Parameter(Mandatory = $true)][string]$ScriptRoot, [string]$Sandbox)
    if ($Sandbox) { return $Sandbox }
    $sdk = Get-FakehostRoot -ScriptRoot $ScriptRoot
    $ver = Get-FakehostSdkVersion -Sdk $sdk
    return (Join-Path $env:LOCALAPPDATA ('ZoneSDK\' + $ver + '\fakehost'))
}

function Get-FakehostExe {
    param([Parameter(Mandatory = $true)][string]$ScriptRoot, [string]$Out)
    return (Join-Path (Join-Path (Get-FakehostWorkRoot -ScriptRoot $ScriptRoot -Out $Out) 'bin') 'Stalker2-Win64-Shipping.exe')
}

function Get-FakehostStamp {
    param([Parameter(Mandatory = $true)][string]$ScriptRoot)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $parts = New-Object System.Collections.Generic.List[string]
        foreach ($file in @(Get-ChildItem -LiteralPath $ScriptRoot -File | Where-Object { @('.cpp', '.inc', '.h') -contains $_.Extension -or $_.Name -eq 'build.ps1' } | Sort-Object Name)) {
            $parts.Add($file.Name + ' ' + [System.BitConverter]::ToString($sha.ComputeHash([System.IO.File]::ReadAllBytes($file.FullName))).Replace('-', ''))
        }
        return [System.BitConverter]::ToString($sha.ComputeHash([System.Text.Encoding]::UTF8.GetBytes(($parts -join "`n")))).Replace('-', '')
    }
    finally { $sha.Dispose() }
}
