param([switch]$DevelopmentPackage)
$ErrorActionPreference = "Stop"
$ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$QtRoot = Join-Path $ProjectRoot "_toolchain\Qt\5.15.2\mingw81_32"
$QtBin = Join-Path $QtRoot "bin"
$QtPlugins = Join-Path $QtRoot "plugins"
$QtQml = Join-Path $QtRoot "qml"
$MingwBin = Join-Path $ProjectRoot "_toolchain\Qt\Tools\mingw810_32\bin"
$SourceExe = Join-Path $ProjectRoot "build\release\SmartKeyAIReconstruction.exe"
$DistParent = Join-Path $ProjectRoot "dist"
$ReportRoot = Join-Path $ProjectRoot "reports"
. (Join-Path $PSScriptRoot "smoke_isolation.ps1")
. (Join-Path $PSScriptRoot "artifact_provenance.ps1")
$RunContext = Read-ArtifactRunContext $ProjectRoot
$AppVersion = Get-ApplicationVersion $ProjectRoot
$ReleaseName = "SmartKeyAI-$AppVersion-win-x86"
$FinalDistRoot = Join-Path $DistParent $ReleaseName
$DistRoot = Join-Path $DistParent (".staging-{0}-{1}" -f $ReleaseName, $RunContext.run_id)
if ([System.IO.Path]::GetFullPath($SourceExe) -ne [System.IO.Path]::GetFullPath($RunContext.executable)) {
    throw "package source is not the current build artifact"
}

function Get-PeSubsystem([string]$Path) {
    $Bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($Bytes.Length -lt 96 -or $Bytes[0] -ne 0x4D -or $Bytes[1] -ne 0x5A) {
        throw "invalid PE image: $Path"
    }
    $PeOffset = [BitConverter]::ToInt32($Bytes, 0x3C)
    if ($PeOffset -lt 0 -or $PeOffset + 94 -gt $Bytes.Length -or
            $Bytes[$PeOffset] -ne 0x50 -or $Bytes[$PeOffset + 1] -ne 0x45) {
        throw "invalid PE header: $Path"
    }
    return [BitConverter]::ToUInt16($Bytes, $PeOffset + 24 + 68)
}

foreach ($Required in @($QtBin, $QtPlugins, $QtQml, $MingwBin, $SourceExe)) {
    if (-not (Test-Path -LiteralPath $Required)) { throw "missing required path: $Required" }
}

$ExpectedParent = [System.IO.Path]::GetFullPath($DistParent) + [System.IO.Path]::DirectorySeparatorChar
$ResolvedDist = [System.IO.Path]::GetFullPath($DistRoot)
if (-not $ResolvedDist.StartsWith($ExpectedParent, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "refusing to replace package directory outside project dist: $ResolvedDist"
}
if (Test-Path -LiteralPath $FinalDistRoot) {
    throw "release directory already exists and is immutable: $FinalDistRoot"
}
if (Test-Path -LiteralPath $ResolvedDist) { throw "staging directory already exists: $ResolvedDist" }
New-Item -ItemType Directory -Force $ResolvedDist, $ReportRoot | Out-Null
Copy-Item -LiteralPath $SourceExe -Destination (Join-Path $ResolvedDist "SmartKeyAI.exe")
$PackagedAt = Get-ArtifactUtcTimestamp
$PackageExe = Join-Path $ResolvedDist "SmartKeyAI.exe"
$PackageHash = Get-ArtifactFileHash $PackageExe
if ($PackageHash -ne $RunContext.exe_sha256) {
    throw "packaged executable differs from current build artifact"
}

function Copy-RequiredFile([string]$SourceRoot, [string]$Name, [string]$DestinationRoot) {
    $Source = Join-Path $SourceRoot $Name
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) { throw "missing deployment file: $Source" }
    New-Item -ItemType Directory -Force $DestinationRoot | Out-Null
    Copy-Item -LiteralPath $Source -Destination (Join-Path $DestinationRoot ([System.IO.Path]::GetFileName($Name))) -Force
}

function Copy-RequiredTree([string]$Source, [string]$Destination) {
    if (-not (Test-Path -LiteralPath $Source -PathType Container)) { throw "missing deployment directory: $Source" }
    New-Item -ItemType Directory -Force $Destination | Out-Null
    Get-ChildItem -LiteralPath $Source -Force | Copy-Item -Destination $Destination -Recurse -Force
}

# This aqt archive's windeployqt cannot locate qwindows.dll even though qmake -query
# resolves it correctly. Keep deployment deterministic and auditable instead of
# silently accepting a partial package.
$RuntimeDlls = @(
    "Qt5Core.dll", "Qt5Gui.dll", "Qt5Network.dll", "Qt5Qml.dll",
    "Qt5QmlModels.dll", "Qt5QmlWorkerScript.dll", "Qt5Quick.dll",
    "Qt5QuickControls2.dll", "Qt5QuickTemplates2.dll", "Qt5Sql.dll",
    "Qt5Svg.dll", "Qt5Widgets.dll", "opengl32sw.dll", "d3dcompiler_47.dll",
    "libEGL.dll", "libGLESv2.dll"
)
foreach ($Dll in $RuntimeDlls) { Copy-RequiredFile $QtBin $Dll $ResolvedDist }
foreach ($Dll in @("libgcc_s_dw2-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")) {
    Copy-RequiredFile $MingwBin $Dll $ResolvedDist
}

Copy-RequiredTree (Join-Path $QtPlugins "platforms") (Join-Path $ResolvedDist "platforms")
Copy-RequiredTree (Join-Path $QtPlugins "imageformats") (Join-Path $ResolvedDist "imageformats")
Copy-RequiredTree (Join-Path $QtPlugins "iconengines") (Join-Path $ResolvedDist "iconengines")
Copy-RequiredTree (Join-Path $QtPlugins "styles") (Join-Path $ResolvedDist "styles")
Copy-RequiredFile (Join-Path $QtPlugins "sqldrivers") "qsqlite.dll" (Join-Path $ResolvedDist "sqldrivers")

$QmlModules = @(
    @("QtQuick.2", "QtQuick.2"),
    @("QtQuick\Window.2", "QtQuick\Window.2"),
    @("QtQuick\Controls.2", "QtQuick\Controls.2"),
    @("QtQuick\Controls", "QtQuick\Controls"),
    @("QtQuick\Dialogs", "QtQuick\Dialogs"),
    @("QtQuick\PrivateWidgets", "QtQuick\PrivateWidgets"),
    @("QtQuick\Templates.2", "QtQuick\Templates.2"),
    @("QtQuick\Layouts", "QtQuick\Layouts"),
    @("QtGraphicalEffects", "QtGraphicalEffects"),
    @("QtQml", "QtQml"),
    @("Qt\labs\folderlistmodel", "Qt\labs\folderlistmodel"),
    @("Qt\labs\settings", "Qt\labs\settings")
)
foreach ($Module in $QmlModules) {
    Copy-RequiredTree (Join-Path $QtQml $Module[0]) (Join-Path (Join-Path $ResolvedDist "qml") $Module[1])
}

$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$QtConf = "[Paths]`nPrefix=.`nPlugins=.`nQml2Imports=qml`nImports=qml`n"
[System.IO.File]::WriteAllText((Join-Path $ResolvedDist "qt.conf"), $QtConf, $Utf8NoBom)

$Forbidden = @(Get-ChildItem -LiteralPath $ResolvedDist -Recurse -File |
    Where-Object { $_.Name -match '^(libssl|libcrypto|ssleay|libeay)' })
if ($Forbidden.Count -gt 0) {
    throw "package unexpectedly contains OpenSSL libraries: $($Forbidden.FullName -join ', ')"
}
foreach ($Required in @("SmartKeyAI.exe", "platforms\qwindows.dll", "platforms\qoffscreen.dll", "sqldrivers\qsqlite.dll", "qml\QtQuick.2\qtquick2plugin.dll")) {
    if (-not (Test-Path -LiteralPath (Join-Path $ResolvedDist $Required) -PathType Leaf)) {
        throw "package is missing required runtime: $Required"
    }
}

$PackageScreenshot = Join-Path $ResolvedDist "package_smoke.png"
$PackageSettingsScreenshot = Join-Path $ResolvedDist "settings_smoke.png"
$PackageModelPickerScreenshot = Join-Path $ResolvedDist "model_picker_smoke.png"
$PackageHistoryScreenshot = Join-Path $ResolvedDist "history_smoke.png"
$PackageDeleteConfirmScreenshot = Join-Path $ResolvedDist "delete_confirm_smoke.png"
$PackageTooltipScreenshot = Join-Path $ResolvedDist "tooltip_smoke.png"
$StdoutLog = Join-Path $ReportRoot "package_smoke.stdout.log"
$StderrLog = Join-Path $ReportRoot "package_smoke.stderr.log"
foreach ($OldFile in @($PackageScreenshot, $PackageSettingsScreenshot, $PackageModelPickerScreenshot, $PackageHistoryScreenshot, $PackageDeleteConfirmScreenshot, $PackageTooltipScreenshot, $StdoutLog, $StderrLog)) {
    if (Test-Path -LiteralPath $OldFile) { Remove-Item -LiteralPath $OldFile -Force }
}

$SavedEnvironment = @{
    Path = $env:Path
    QT_QPA_PLATFORM = $env:QT_QPA_PLATFORM
    QT_QUICK_BACKEND = $env:QT_QUICK_BACKEND
    QT_PLUGIN_PATH = $env:QT_PLUGIN_PATH
    QML2_IMPORT_PATH = $env:QML2_IMPORT_PATH
    QML_DISABLE_DISK_CACHE = $env:QML_DISABLE_DISK_CACHE
    QT_QPA_FONTDIR = $env:QT_QPA_FONTDIR
}
$SmokeDataRoot = New-SmokeDataRoot "package"
$UserStateBefore = Get-SmokeUserStateSnapshot
$UserStateAfter = $null
$UserStateUnchanged = $false
$StandalonePath = "$ResolvedDist;$env:SystemRoot\System32;$env:SystemRoot"
try {
    # Deliberately omit the local Qt/MinGW toolchain from PATH. The smoke test
    # must resolve every non-system runtime from the package itself.
    $env:Path = $StandalonePath
    $env:QT_QPA_PLATFORM = "offscreen"
    $env:QT_QUICK_BACKEND = "software"
    $env:QT_PLUGIN_PATH = $ResolvedDist
    $env:QML2_IMPORT_PATH = Join-Path $ResolvedDist "qml"
    $env:QML_DISABLE_DISK_CACHE = "1"
    $env:QT_QPA_FONTDIR = Join-Path $env:SystemRoot "Fonts"
    $Process = Start-Process -FilePath (Join-Path $ResolvedDist "SmartKeyAI.exe") `
        -ArgumentList @("--smoke-test", $PackageScreenshot, "--smoke-data-root", $SmokeDataRoot) `
        -Wait -NoNewWindow -PassThru `
        -RedirectStandardOutput $StdoutLog -RedirectStandardError $StderrLog
    $SmokeExitCode = $Process.ExitCode
} finally {
    $env:Path = $SavedEnvironment.Path
    $env:QT_QPA_PLATFORM = $SavedEnvironment.QT_QPA_PLATFORM
    $env:QT_QUICK_BACKEND = $SavedEnvironment.QT_QUICK_BACKEND
    $env:QT_PLUGIN_PATH = $SavedEnvironment.QT_PLUGIN_PATH
    $env:QML2_IMPORT_PATH = $SavedEnvironment.QML2_IMPORT_PATH
    $env:QML_DISABLE_DISK_CACHE = $SavedEnvironment.QML_DISABLE_DISK_CACHE
    $env:QT_QPA_FONTDIR = $SavedEnvironment.QT_QPA_FONTDIR
    $UserStateAfter = Get-SmokeUserStateSnapshot
    $UserStateUnchanged = Test-SmokeUserStateUnchanged $UserStateBefore $UserStateAfter
    Remove-SmokeDataRoot $SmokeDataRoot
}

$SmokeOutput = ((Get-Content -Raw $StdoutLog), (Get-Content -Raw $StderrLog) -join "`r`n").Trim()
$PackagePeSubsystem = Get-PeSubsystem (Join-Path $ResolvedDist "SmartKeyAI.exe")
$RootNonNull = $SmokeOutput -match "SMOKE_ROOT_OBJECT=non-null"
$ScreenshotSize = if (Test-Path -LiteralPath $PackageScreenshot) { (Get-Item $PackageScreenshot).Length } else { 0 }
$SettingsScreenshotSize = if (Test-Path -LiteralPath $PackageSettingsScreenshot) { (Get-Item $PackageSettingsScreenshot).Length } else { 0 }
$ModelPickerScreenshotSize = if (Test-Path -LiteralPath $PackageModelPickerScreenshot) { (Get-Item $PackageModelPickerScreenshot).Length } else { 0 }
$HistoryScreenshotSize = if (Test-Path -LiteralPath $PackageHistoryScreenshot) { (Get-Item $PackageHistoryScreenshot).Length } else { 0 }
$DeleteConfirmScreenshotSize = if (Test-Path -LiteralPath $PackageDeleteConfirmScreenshot) { (Get-Item $PackageDeleteConfirmScreenshot).Length } else { 0 }
$TooltipScreenshotSize = if (Test-Path -LiteralPath $PackageTooltipScreenshot) { (Get-Item $PackageTooltipScreenshot).Length } else { 0 }
$SettingsTaskbarWindow = $SmokeOutput -match "SMOKE_SETTINGS_TASKBAR_WINDOW=1"
$ModelPickerReopens = $SmokeOutput -match "SMOKE_MODEL_PICKER_REOPEN=1"
$WarningLines = @($SmokeOutput -split "`r?`n" | Where-Object { $_ -match "(?i)(QML_WARNING|QML_COMPONENT_ERROR|warning:|error:|failed|cannot)" })
$ReportedDataRoot = if ($SmokeOutput -match "SMOKE_DATA_ROOT=([^`r`n]+)") { $Matches[1].Trim() } else { "" }
$ExpectedDataRoot = [System.IO.Path]::GetFullPath($SmokeDataRoot)
$DataRootReported = -not [string]::IsNullOrWhiteSpace($ReportedDataRoot) -and `
    [System.IO.Path]::GetFullPath($ReportedDataRoot).Equals($ExpectedDataRoot, [System.StringComparison]::OrdinalIgnoreCase)
$AllPathsIsolated = $true
foreach ($Marker in @("SMOKE_SETTINGS_PATH", "SMOKE_CREDENTIAL_PATH", "SMOKE_DATABASE_PATH", "SMOKE_LOG_PATH")) {
    if ($SmokeOutput -notmatch ("{0}=([^`r`n]+)" -f $Marker)) {
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
$ExternalStateDisabled = $SmokeOutput -match "SMOKE_LEGACY_IMPORT_ENABLED=0" -and `
    $SmokeOutput -match "SMOKE_AUTOSTART_ACCESS_ENABLED=0" -and `
    $SmokeOutput -match "SMOKE_HOTKEY_STATE_ACCESS_ENABLED=0"
$SmokePassed = $SmokeExitCode -eq 0 -and $RootNonNull -and $SettingsTaskbarWindow -and $ModelPickerReopens -and $ScreenshotSize -gt 0 -and $SettingsScreenshotSize -gt 0 -and $ModelPickerScreenshotSize -gt 0 -and $HistoryScreenshotSize -gt 0 -and $DeleteConfirmScreenshotSize -gt 0 -and $TooltipScreenshotSize -gt 0 -and $WarningLines.Count -eq 0 -and $DataRootReported -and $AllPathsIsolated -and $ExternalStateDisabled -and $UserStateUnchanged -and $PackagePeSubsystem -eq 2
$Signature = Get-AuthenticodeSignature -LiteralPath $PackageExe
$AuthenticodeValid = $Signature.Status -eq [System.Management.Automation.SignatureStatus]::Valid
$ReleaseStatus = if ($AuthenticodeValid) { "signed-release" } elseif ($DevelopmentPackage) { "unsigned-development" } else { "signature-required" }
$SignatureAllowed = $AuthenticodeValid -or $DevelopmentPackage
$MetadataGenerated = $false
$ManifestHashesValid = $false
$Published = $false
if ($SmokePassed -and $SignatureAllowed) {
    $ManifestOutput = (& python (Join-Path $PSScriptRoot "release_artifacts.py") generate `
        --project $ProjectRoot --package-root $ResolvedDist `
        --run-manifest (Join-Path $ReportRoot "run_manifest.json") `
        --release-status $ReleaseStatus --authenticode-status $Signature.Status 2>&1 | Out-String).Trim()
    if ($LASTEXITCODE -ne 0) { throw "release manifest generation failed: $ManifestOutput" }
    $MetadataGenerated = $true
    $VerifyOutput = (& python (Join-Path $PSScriptRoot "release_artifacts.py") verify `
        --package-root $ResolvedDist 2>&1 | Out-String).Trim()
    if ($LASTEXITCODE -ne 0) { throw "release checksum verification failed: $VerifyOutput" }
    $ManifestHashesValid = $true
    $PublishOutput = (& python (Join-Path $PSScriptRoot "release_artifacts.py") publish `
        --staging $ResolvedDist --final $FinalDistRoot 2>&1 | Out-String).Trim()
    if ($LASTEXITCODE -ne 0) { throw "atomic release publish failed: $PublishOutput" }
    $Published = $true
    $ResolvedDist = [System.IO.Path]::GetFullPath($FinalDistRoot)
    $PackageExe = Join-Path $ResolvedDist "SmartKeyAI.exe"
    $PackageScreenshot = Join-Path $ResolvedDist "package_smoke.png"
    $PackageSettingsScreenshot = Join-Path $ResolvedDist "settings_smoke.png"
    $PackageModelPickerScreenshot = Join-Path $ResolvedDist "model_picker_smoke.png"
    $PackageHistoryScreenshot = Join-Path $ResolvedDist "history_smoke.png"
    $PackageDeleteConfirmScreenshot = Join-Path $ResolvedDist "delete_confirm_smoke.png"
    $PackageTooltipScreenshot = Join-Path $ResolvedDist "tooltip_smoke.png"
    $StandalonePath = "$ResolvedDist;$env:SystemRoot\System32;$env:SystemRoot"
}
$SmokePassed = $SmokePassed -and $SignatureAllowed -and $MetadataGenerated -and $ManifestHashesValid -and $Published
$Result = [ordered]@{
    status = if ($SmokePassed) { "PASS" } else { "FAIL" }
    package = $ResolvedDist
    version = $AppVersion
    release_name = $ReleaseName
    release_status = $ReleaseStatus
    development_package = [bool]$DevelopmentPackage
    staging_directory = $DistRoot
    published_atomically = $Published
    signature = [ordered]@{
        status = $ReleaseStatus
        authenticode_status = $Signature.Status.ToString()
        signer_subject = if ($null -ne $Signature.SignerCertificate) { $Signature.SignerCertificate.Subject } else { $null }
        public_release_valid = $AuthenticodeValid
    }
    release_manifest = Join-Path $ResolvedDist "release-manifest.json"
    sha256sums_manifest = Join-Path $ResolvedDist "SHA256SUMS.json"
    sbom = Join-Path $ResolvedDist "sbom.spdx.json"
    manifest_hashes_valid = $ManifestHashesValid
    validation_kind = "behavioral"
    run_id = $RunContext.run_id
    source_hash = $RunContext.source_hash
    built_at = $RunContext.built_at
    packaged_at = $PackagedAt
    tested_at = Get-ArtifactUtcTimestamp
    build_exe_sha256 = $RunContext.exe_sha256
    package_executable = $PackageExe
    package_sha256 = $PackageHash
    environment = Get-ArtifactEnvironment
    file_count = @(Get-ChildItem -LiteralPath $ResolvedDist -Recurse -File).Count
    standalone_path = $StandalonePath
    toolchain_on_smoke_path = $false
    pe_subsystem = $PackagePeSubsystem
    pe_subsystem_name = if ($PackagePeSubsystem -eq 2) { "IMAGE_SUBSYSTEM_WINDOWS_GUI" } else { "unexpected" }
    release_has_no_console_subsystem = $PackagePeSubsystem -eq 2
    smoke_data_root = $ExpectedDataRoot
    smoke_data_root_reported = $DataRootReported
    config_database_and_log_paths_isolated = $AllPathsIsolated
    legacy_autostart_and_hotkey_state_access_disabled = $ExternalStateDisabled
    real_user_state_before_sha256 = $UserStateBefore.fingerprint
    real_user_state_after_sha256 = $UserStateAfter.fingerprint
    real_user_state_unchanged = $UserStateUnchanged
    smoke_data_root_cleaned = -not (Test-Path -LiteralPath $SmokeDataRoot)
    smoke_exit_code = $SmokeExitCode
    root_object_non_null = $RootNonNull
    screenshot = $PackageScreenshot
    screenshot_size = $ScreenshotSize
    settings_screenshot = $PackageSettingsScreenshot
    settings_screenshot_size = $SettingsScreenshotSize
    model_picker_screenshot = $PackageModelPickerScreenshot
    model_picker_screenshot_size = $ModelPickerScreenshotSize
    history_screenshot = $PackageHistoryScreenshot
    history_screenshot_size = $HistoryScreenshotSize
    delete_confirm_screenshot = $PackageDeleteConfirmScreenshot
    delete_confirm_screenshot_size = $DeleteConfirmScreenshotSize
    tooltip_screenshot = $PackageTooltipScreenshot
    tooltip_screenshot_size = $TooltipScreenshotSize
    settings_taskbar_window_contract = $SettingsTaskbarWindow
    model_picker_reopens_after_close = $ModelPickerReopens
    qml_warning_or_error_lines = $WarningLines
    qwindows_present = Test-Path -LiteralPath (Join-Path $ResolvedDist "platforms\qwindows.dll")
    qsqlite_present = Test-Path -LiteralPath (Join-Path $ResolvedDist "sqldrivers\qsqlite.dll")
    openssl_library_count = $Forbidden.Count
}
[System.IO.File]::WriteAllText((Join-Path $ReportRoot "package_verification.json"),
    (($Result | ConvertTo-Json -Depth 6) + "`n"), $Utf8NoBom)

Write-Output "[package] $($Result.status) release_status=$ReleaseStatus version=$AppVersion path=$ResolvedDist files=$($Result.file_count) standalone_smoke_rc=$SmokeExitCode isolated=$AllPathsIsolated user_state_unchanged=$UserStateUnchanged main_shot=$ScreenshotSize settings_shot=$SettingsScreenshotSize"
if (-not $SmokePassed) {
    Write-Output $SmokeOutput
    exit 1
}
exit 0
