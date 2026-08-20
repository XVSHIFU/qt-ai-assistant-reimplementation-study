$ErrorActionPreference = "Stop"
$ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$QtBin = Join-Path $ProjectRoot "_toolchain\Qt\5.15.2\mingw81_32\bin"
$MingwBin = Join-Path $ProjectRoot "_toolchain\Qt\Tools\mingw810_32\bin"
$QmlRoot = Join-Path $ProjectRoot "_toolchain\Qt\5.15.2\mingw81_32\qml"
$Exe = Join-Path $ProjectRoot "build\release\SmartKeyAIReconstruction.exe"
$ReportDir = Join-Path $ProjectRoot "reports"
$Screenshot = Join-Path $ReportDir "mainview_smoke.png"
$SettingsScreenshot = Join-Path $ReportDir "settings_smoke.png"
$ModelPickerScreenshot = Join-Path $ReportDir "model_picker_smoke.png"
$HistoryScreenshot = Join-Path $ReportDir "history_smoke.png"
$DeleteConfirmScreenshot = Join-Path $ReportDir "delete_confirm_smoke.png"
$TooltipScreenshot = Join-Path $ReportDir "tooltip_smoke.png"
$Log = Join-Path $ReportDir "qml_engine.log"
$StdoutLog = Join-Path $ReportDir "qml_engine.stdout.log"
$StderrLog = Join-Path $ReportDir "qml_engine.stderr.log"
. (Join-Path $PSScriptRoot "smoke_isolation.ps1")
. (Join-Path $PSScriptRoot "artifact_provenance.ps1")
$RunContext = Read-ArtifactRunContext $ProjectRoot
if ([System.IO.Path]::GetFullPath($Exe) -ne [System.IO.Path]::GetFullPath($RunContext.executable)) {
    throw "smoke executable is not the current build artifact"
}
New-Item -ItemType Directory -Force $ReportDir | Out-Null
if (-not (Test-Path $Exe)) { throw "missing executable: $Exe; run build.ps1" }
foreach ($OldScreenshot in @($Screenshot, $SettingsScreenshot, $ModelPickerScreenshot, $HistoryScreenshot, $DeleteConfirmScreenshot, $TooltipScreenshot)) {
    if (Test-Path $OldScreenshot) { Remove-Item -LiteralPath $OldScreenshot -Force }
}

$OldPath = $env:Path
$OldPlatform = $env:QT_QPA_PLATFORM
$OldBackend = $env:QT_QUICK_BACKEND
$OldImport = $env:QML2_IMPORT_PATH
$OldDiskCache = $env:QML_DISABLE_DISK_CACHE
$OldFontDir = $env:QT_QPA_FONTDIR
$SmokeDataRoot = New-SmokeDataRoot "qml-engine"
$UserStateBefore = Get-SmokeUserStateSnapshot
$UserStateAfter = $null
$UserStateUnchanged = $false
$env:Path = "$MingwBin;$QtBin;$OldPath"
$env:QT_QPA_PLATFORM = "offscreen"
$env:QT_QUICK_BACKEND = "software"
$env:QML2_IMPORT_PATH = $QmlRoot
$env:QML_DISABLE_DISK_CACHE = "1"
$env:QT_QPA_FONTDIR = Join-Path $env:SystemRoot "Fonts"
try {
    foreach ($OldLog in @($Log, $StdoutLog, $StderrLog)) {
        if (Test-Path $OldLog) { Remove-Item -LiteralPath $OldLog -Force }
    }
    $Process = Start-Process -FilePath $Exe `
        -ArgumentList @("--smoke-test", $Screenshot, "--smoke-data-root", $SmokeDataRoot) `
        -Wait -NoNewWindow -PassThru -RedirectStandardOutput $StdoutLog `
        -RedirectStandardError $StderrLog
    $Rc = $Process.ExitCode
    $Output = ((Get-Content -Raw $StdoutLog), (Get-Content -Raw $StderrLog) -join "`r`n").Trim()
    Get-Content $StdoutLog, $StderrLog | Set-Content -Encoding UTF8 $Log
} finally {
    $env:Path = $OldPath
    $env:QT_QPA_PLATFORM = $OldPlatform
    $env:QT_QUICK_BACKEND = $OldBackend
    $env:QML2_IMPORT_PATH = $OldImport
    $env:QML_DISABLE_DISK_CACHE = $OldDiskCache
    $env:QT_QPA_FONTDIR = $OldFontDir
    $UserStateAfter = Get-SmokeUserStateSnapshot
    $UserStateUnchanged = Test-SmokeUserStateUnchanged $UserStateBefore $UserStateAfter
    Remove-SmokeDataRoot $SmokeDataRoot
}

$RootNonNull = $Output -match "SMOKE_ROOT_OBJECT=non-null"
$InputFocused = $Output -match "SMOKE_FOCUSED_ITEM=chatInput"
$SettingsWidth = if ($Output -match "SMOKE_SETTINGS_SIZE=([0-9]+)x([0-9]+)") { [int]$Matches[1] } else { 0 }
$SettingsHeight = if ($Output -match "SMOKE_SETTINGS_SIZE=([0-9]+)x([0-9]+)") { [int]$Matches[2] } else { 0 }
$SettingsAdaptive = $SettingsWidth -ge 420 -and $SettingsHeight -ge 480
$SettingsTaskbarWindow = $Output -match "SMOKE_SETTINGS_TASKBAR_WINDOW=1"
$ScreenshotExists = Test-Path $Screenshot
$ScreenshotSize = if ($ScreenshotExists) { (Get-Item $Screenshot).Length } else { 0 }
$SettingsScreenshotExists = Test-Path $SettingsScreenshot
$SettingsScreenshotSize = if ($SettingsScreenshotExists) { (Get-Item $SettingsScreenshot).Length } else { 0 }
$ModelPickerScreenshotExists = Test-Path $ModelPickerScreenshot
$ModelPickerScreenshotSize = if ($ModelPickerScreenshotExists) { (Get-Item $ModelPickerScreenshot).Length } else { 0 }
$ModelPickerReopens = $Output -match "SMOKE_MODEL_PICKER_REOPEN=1"
$HistoryScreenshotExists = Test-Path $HistoryScreenshot
$HistoryScreenshotSize = if ($HistoryScreenshotExists) { (Get-Item $HistoryScreenshot).Length } else { 0 }
$DeleteConfirmScreenshotExists = Test-Path $DeleteConfirmScreenshot
$DeleteConfirmScreenshotSize = if ($DeleteConfirmScreenshotExists) { (Get-Item $DeleteConfirmScreenshot).Length } else { 0 }
$TooltipScreenshotExists = Test-Path $TooltipScreenshot
$TooltipScreenshotSize = if ($TooltipScreenshotExists) { (Get-Item $TooltipScreenshot).Length } else { 0 }
$WarningLines = @($Output -split "`r?`n" | Where-Object { $_ -match "(?i)(QML_WARNING|QML_COMPONENT_ERROR|warning:|error:|failed|cannot)" })
$CaptureMethod = if ($Output -match "SMOKE_CAPTURE_METHOD=([^`r`n]+)") { $Matches[1] } else { $null }
$ReportedDataRoot = if ($Output -match "SMOKE_DATA_ROOT=([^`r`n]+)") { $Matches[1].Trim() } else { "" }
$ExpectedDataRoot = [System.IO.Path]::GetFullPath($SmokeDataRoot)
$DataRootReported = -not [string]::IsNullOrWhiteSpace($ReportedDataRoot) -and `
    [System.IO.Path]::GetFullPath($ReportedDataRoot).Equals($ExpectedDataRoot, [System.StringComparison]::OrdinalIgnoreCase)
$IsolatedPathMarkers = @("SMOKE_SETTINGS_PATH", "SMOKE_CREDENTIAL_PATH", "SMOKE_DATABASE_PATH", "SMOKE_LOG_PATH")
$AllPathsIsolated = $true
foreach ($Marker in $IsolatedPathMarkers) {
    if ($Output -notmatch ("{0}=([^`r`n]+)" -f $Marker)) {
        $AllPathsIsolated = $false
        break
    }
    $ReportedPath = [System.IO.Path]::GetFullPath($Matches[1].Trim())
    $RootPrefix = $ExpectedDataRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    if (-not $ReportedPath.StartsWith($RootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        $AllPathsIsolated = $false
        break
    }
}
$ExternalStateDisabled = $Output -match "SMOKE_LEGACY_IMPORT_ENABLED=0" -and `
    $Output -match "SMOKE_AUTOSTART_ACCESS_ENABLED=0" -and `
    $Output -match "SMOKE_HOTKEY_STATE_ACCESS_ENABLED=0"
$Result = [ordered]@{
    status = if ($Rc -eq 0 -and $RootNonNull -and $InputFocused -and $SettingsAdaptive -and $SettingsTaskbarWindow -and $ScreenshotSize -gt 0 -and $SettingsScreenshotSize -gt 0 -and $ModelPickerScreenshotSize -gt 0 -and $ModelPickerReopens -and $HistoryScreenshotSize -gt 0 -and $DeleteConfirmScreenshotSize -gt 0 -and $TooltipScreenshotSize -gt 0 -and $WarningLines.Count -eq 0 -and $DataRootReported -and $AllPathsIsolated -and $ExternalStateDisabled -and $UserStateUnchanged) { "PASS" } else { "FAIL" }
    validation_mode = "real_QQuickView_engine_offscreen"
    validation_kind = "behavioral"
    run_id = $RunContext.run_id
    source_hash = $RunContext.source_hash
    built_at = $RunContext.built_at
    tested_at = Get-ArtifactUtcTimestamp
    exe_sha256 = $RunContext.exe_sha256
    environment = Get-ArtifactEnvironment
    entry_url = "qrc:/MainView.qml"
    executable = $Exe
    exit_code = $Rc
    root_object_non_null = $RootNonNull
    input_focus_is_chat_input = $InputFocused
    settings_size = @($SettingsWidth, $SettingsHeight)
    settings_adaptive_minimum_met = $SettingsAdaptive
    settings_taskbar_window_contract = $SettingsTaskbarWindow
    settings_screenshot = $SettingsScreenshot
    settings_screenshot_exists = $SettingsScreenshotExists
    settings_screenshot_size = $SettingsScreenshotSize
    model_picker_screenshot = $ModelPickerScreenshot
    model_picker_screenshot_exists = $ModelPickerScreenshotExists
    model_picker_screenshot_size = $ModelPickerScreenshotSize
    model_picker_reopens_after_close = $ModelPickerReopens
    history_screenshot = $HistoryScreenshot
    history_screenshot_exists = $HistoryScreenshotExists
    history_screenshot_size = $HistoryScreenshotSize
    delete_confirm_screenshot = $DeleteConfirmScreenshot
    delete_confirm_screenshot_exists = $DeleteConfirmScreenshotExists
    delete_confirm_screenshot_size = $DeleteConfirmScreenshotSize
    tooltip_screenshot = $TooltipScreenshot
    tooltip_screenshot_exists = $TooltipScreenshotExists
    tooltip_screenshot_size = $TooltipScreenshotSize
    screenshot = $Screenshot
    screenshot_exists = $ScreenshotExists
    screenshot_size = $ScreenshotSize
    capture_method = $CaptureMethod
    platform = "offscreen"
    quick_backend = "software"
    smoke_data_root = $ExpectedDataRoot
    smoke_data_root_reported = $DataRootReported
    config_database_and_log_paths_isolated = $AllPathsIsolated
    legacy_autostart_and_hotkey_state_access_disabled = $ExternalStateDisabled
    real_user_state_before_sha256 = $UserStateBefore.fingerprint
    real_user_state_after_sha256 = $UserStateAfter.fingerprint
    real_user_state_unchanged = $UserStateUnchanged
    smoke_data_root_cleaned = -not (Test-Path -LiteralPath $SmokeDataRoot)
    log = $Log
    qml_warning_or_error_lines = $WarningLines
}
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$ReportJson = ($Result | ConvertTo-Json -Depth 6) + "`n"
[System.IO.File]::WriteAllText((Join-Path $ReportDir "qml_engine_validation.json"), $ReportJson, $Utf8NoBom)
Write-Output "[smoke] $($Result.status) rc=$Rc root_non_null=$RootNonNull input_focused=$InputFocused settings=${SettingsWidth}x${SettingsHeight} settings_taskbar=$SettingsTaskbarWindow isolated=$AllPathsIsolated user_state_unchanged=$UserStateUnchanged picker_shot=$ModelPickerScreenshotSize history_shot=$HistoryScreenshotSize delete_shot=$DeleteConfirmScreenshotSize settings_shot=$SettingsScreenshotSize main_shot=$ScreenshotSize"
if ($Result.status -ne "PASS") { exit 1 }
exit 0
