$ErrorActionPreference = "Stop"

function Get-SmokeUserStateSnapshot {
    $FileSystemRoots = @(
        (Join-Path $env:LOCALAPPDATA "SmartKeyAI"),
        (Join-Path $env:APPDATA "SmartKeyAI"),
        (Join-Path $env:LOCALAPPDATA "SmartKey AI\ChatHistory")
    ) | Sort-Object -Unique

    $FileSystem = foreach ($Root in $FileSystemRoots) {
        $ResolvedRoot = [System.IO.Path]::GetFullPath($Root)
        if (-not (Test-Path -LiteralPath $ResolvedRoot)) {
            [ordered]@{ root = $ResolvedRoot; exists = $false; entries = @() }
            continue
        }

        $Entries = @(Get-ChildItem -LiteralPath $ResolvedRoot -Force -Recurse |
            Sort-Object FullName |
            ForEach-Object {
                [ordered]@{
                    relative_path = $_.FullName.Substring($ResolvedRoot.Length).TrimStart('\', '/')
                    kind = if ($_.PSIsContainer) { "directory" } else { "file" }
                    length = if ($_.PSIsContainer) { 0 } else { $_.Length }
                    last_write_utc = $_.LastWriteTimeUtc.ToString("o")
                    attributes = [int]$_.Attributes
                }
            })
        [ordered]@{ root = $ResolvedRoot; exists = $true; entries = $Entries }
    }

    function Get-RegistryKeyState([string]$Path) {
        if (-not (Test-Path -LiteralPath $Path)) {
            return [ordered]@{ path = $Path; exists = $false; keys = @() }
        }
        $Keys = @((Get-Item -LiteralPath $Path)) + @(Get-ChildItem -LiteralPath $Path -Recurse)
        $KeyStates = foreach ($Key in ($Keys | Sort-Object Name)) {
            $Properties = Get-ItemProperty -LiteralPath $Key.PSPath
            $Values = [ordered]@{}
            foreach ($Property in ($Properties.PSObject.Properties |
                    Where-Object { $_.Name -notmatch '^PS(Path|ParentPath|ChildName|Drive|Provider)$' } |
                    Sort-Object Name)) {
                $Value = $Property.Value
                if ($Value -is [byte[]]) {
                    $Value = [Convert]::ToBase64String($Value)
                } elseif ($Value -is [string[]]) {
                    $Value = @($Value)
                }
                $Values[$Property.Name] = $Value
            }
            [ordered]@{ name = $Key.Name; values = $Values }
        }
        return [ordered]@{ path = $Path; exists = $true; keys = @($KeyStates) }
    }

    $RunPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
    $RunValue = $null
    if (Test-Path -LiteralPath $RunPath) {
        $RunValue = (Get-ItemProperty -LiteralPath $RunPath -Name "SmartKey AI" `
            -ErrorAction SilentlyContinue)."SmartKey AI"
    }

    $State = [ordered]@{
        file_system = @($FileSystem)
        application_registry = Get-RegistryKeyState "HKCU:\Software\SmartKeyAI"
        run_value = $RunValue
    }
    $Json = $State | ConvertTo-Json -Depth 12 -Compress
    $Bytes = [System.Text.Encoding]::UTF8.GetBytes($Json)
    $Hasher = [System.Security.Cryptography.SHA256]::Create()
    try {
        $Fingerprint = ([BitConverter]::ToString($Hasher.ComputeHash($Bytes))).Replace("-", "")
    } finally {
        $Hasher.Dispose()
    }
    return [ordered]@{ fingerprint = $Fingerprint; json = $Json }
}

function New-SmokeDataRoot([string]$Purpose) {
    $SafePurpose = if ($Purpose -match '^[A-Za-z0-9_-]+$') { $Purpose } else { "test" }
    $Root = Join-Path ([System.IO.Path]::GetTempPath()) `
        ("SmartKeyAI-smoke-{0}-{1}" -f $SafePurpose, [Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $Root | Out-Null
    return [System.IO.Path]::GetFullPath($Root)
}

function Test-SmokeDataRoot([string]$Root) {
    if ([string]::IsNullOrWhiteSpace($Root)) { return $false }
    $Resolved = [System.IO.Path]::GetFullPath($Root)
    $TempPrefix = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\', '/') + `
        [System.IO.Path]::DirectorySeparatorChar
    return $Resolved.StartsWith($TempPrefix, [System.StringComparison]::OrdinalIgnoreCase) -and `
        ([System.IO.Path]::GetFileName($Resolved) -match '^SmartKeyAI-smoke-[A-Za-z0-9_-]+-[0-9a-f]{32}$')
}

function Remove-SmokeDataRoot([string]$Root) {
    if (-not (Test-SmokeDataRoot $Root)) {
        throw "refusing to remove an unsafe smoke data root: $Root"
    }
    if (Test-Path -LiteralPath $Root) {
        Remove-Item -LiteralPath $Root -Recurse -Force
    }
}

function Test-SmokeUserStateUnchanged($Before, $After) {
    return $null -ne $Before -and $null -ne $After -and `
        $Before.fingerprint -ceq $After.fingerprint
}
