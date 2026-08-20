$ErrorActionPreference = "Stop"
$ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$QtRoot = Join-Path $ProjectRoot "_toolchain\Qt\5.15.2\mingw81_32"
$QtBin = Join-Path $QtRoot "bin"
$MingwBin = Join-Path $ProjectRoot "_toolchain\Qt\Tools\mingw810_32\bin"
$Qmake = Join-Path $QtBin "qmake.exe"
$Make = Join-Path $MingwBin "mingw32-make.exe"
$TestBuildRoot = Join-Path $ProjectRoot "build\module_tests"
$ReportRoot = Join-Path $ProjectRoot "reports\module_tests"
. (Join-Path $PSScriptRoot "smoke_isolation.ps1")
. (Join-Path $PSScriptRoot "artifact_provenance.ps1")
$RunContext = Read-ArtifactRunContext $ProjectRoot

New-Item -ItemType Directory -Force $TestBuildRoot, $ReportRoot | Out-Null
$OldPath = $env:Path
$OldPluginPath = $env:QT_PLUGIN_PATH
$OldPlatform = $env:QT_QPA_PLATFORM
$OldBackend = $env:QT_QUICK_BACKEND
$OldDiskCache = $env:QML_DISABLE_DISK_CACHE
$UserStateBefore = Get-SmokeUserStateSnapshot
$UserStateAfter = $null
$UserStateUnchanged = $false
$env:Path = "$MingwBin;$QtBin;$OldPath"
$env:QT_PLUGIN_PATH = Join-Path $QtRoot "plugins"
$env:QT_QPA_PLATFORM = "offscreen"
$env:QT_QUICK_BACKEND = "software"
$env:QML_DISABLE_DISK_CACHE = "1"

$Results = @()
try {
    foreach ($Name in @("ai", "settings", "storage", "platform", "startup", "ui")) {
        $Source = Join-Path $ProjectRoot "tests\$Name"
        $Project = Get-ChildItem -LiteralPath $Source -Filter "*.pro" | Select-Object -First 1
        if (-not $Project) { throw "missing test project for $Name" }
        $Build = Join-Path $TestBuildRoot $Name
        New-Item -ItemType Directory -Force $Build | Out-Null
        Push-Location $Build
        try {
            & $Qmake $Project.FullName "CONFIG+=release" | Out-Null
            if ($LASTEXITCODE -ne 0) { throw "$Name qmake failed" }
            & $Make -j2 | Out-Null
            if ($LASTEXITCODE -ne 0) { throw "$Name build failed" }
            $Executable = Get-ChildItem -LiteralPath $Build -Recurse -Filter "*.exe" |
                Where-Object { $_.Name -notmatch "^qmake" } | Select-Object -First 1
            if (-not $Executable) { throw "$Name test executable not found" }
            $OutputPath = Join-Path $ReportRoot "$Name.txt"
            if (Test-Path $OutputPath) { Remove-Item -LiteralPath $OutputPath -Force }
            if ($Name -eq "ui") {
                $Output = (& $Executable.FullName 2>&1 | Out-String).Trim()
                $Rc = $LASTEXITCODE
                [System.IO.File]::WriteAllText($OutputPath, $Output + "`n", [System.Text.UTF8Encoding]::new($false))
                $Totals = if ($Output -match "QML_UI_SMOKE=PASS WARNINGS=0") {
                    "Markdown structures and security PASS, 0 QML warnings"
                } else { $Output }
            } else {
                & $Executable.FullName -o "$OutputPath,txt"
                $Rc = $LASTEXITCODE
                $Output = if (Test-Path $OutputPath) { [System.IO.File]::ReadAllText($OutputPath) } else { "" }
                $Totals = if ($Output -match "Totals:\s*([^\r\n]+)") { $Matches[1].Trim() } else { "unknown" }
            }
            $Results += [ordered]@{ name = $Name; status = if ($Rc -eq 0) { "PASS" } else { "FAIL" }; exit_code = $Rc; totals = $Totals; log = $OutputPath }
            if ($Rc -ne 0) { throw "$Name tests failed: $Totals" }
        } finally { Pop-Location }
    }
} finally {
    $env:Path = $OldPath
    $env:QT_PLUGIN_PATH = $OldPluginPath
    $env:QT_QPA_PLATFORM = $OldPlatform
    $env:QT_QUICK_BACKEND = $OldBackend
    $env:QML_DISABLE_DISK_CACHE = $OldDiskCache
    $UserStateAfter = Get-SmokeUserStateSnapshot
    $UserStateUnchanged = Test-SmokeUserStateUnchanged $UserStateBefore $UserStateAfter
}

$Document = [ordered]@{
    status = if (@($Results | Where-Object status -ne "PASS").Count -eq 0 -and $Results.Count -eq 6 -and $UserStateUnchanged) { "PASS" } else { "FAIL" }
    qt = "5.15.2"
    architecture = "i386"
    validation_kind = "behavioral"
    run_id = $RunContext.run_id
    source_hash = $RunContext.source_hash
    built_at = $RunContext.built_at
    tested_at = Get-ArtifactUtcTimestamp
    exe_sha256 = $RunContext.exe_sha256
    environment = Get-ArtifactEnvironment
    modules = $Results
    qml_disk_cache_disabled = $true
    real_user_state_before_sha256 = $UserStateBefore.fingerprint
    real_user_state_after_sha256 = $UserStateAfter.fingerprint
    real_user_state_unchanged = $UserStateUnchanged
}
[System.IO.File]::WriteAllText((Join-Path $ProjectRoot "reports\module_test_results.json"),
    (($Document | ConvertTo-Json -Depth 6) + "`n"), [System.Text.UTF8Encoding]::new($false))
Write-Output "[tests] $($Document.status) modules=$($Results.Count) user_state_unchanged=$UserStateUnchanged"
if ($Document.status -ne "PASS") { exit 1 }
