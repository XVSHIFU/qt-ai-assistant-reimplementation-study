$ErrorActionPreference = "Stop"
$ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$BuildRoot = [System.IO.Path]::GetFullPath((Join-Path $ProjectRoot "build"))
$ToolchainRoot = Join-Path $ProjectRoot "_toolchain\Qt"
$QtBin = Join-Path $ToolchainRoot "5.15.2\mingw81_32\bin"
$MingwBin = Join-Path $ToolchainRoot "Tools\mingw810_32\bin"
$Qmake = Join-Path $QtBin "qmake.exe"
$Gxx = Join-Path $MingwBin "g++.exe"
$Make = Join-Path $MingwBin "mingw32-make.exe"
$ReportDir = Join-Path $ProjectRoot "reports"
$Log = Join-Path $ReportDir "build.log"
. (Join-Path $PSScriptRoot "artifact_provenance.ps1")
New-Item -ItemType Directory -Force $ReportDir | Out-Null
$RunId = "{0}-{1}" -f [DateTime]::UtcNow.ToString("yyyyMMddTHHmmssfffZ"), ([Guid]::NewGuid().ToString("N"))
$SourceHash = ""
$AppVersion = Get-ApplicationVersion $ProjectRoot

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

foreach ($Required in @($Qmake, $Gxx, $Make)) {
    if (-not (Test-Path $Required)) { throw "missing toolchain file: $Required; run setup_toolchain.ps1" }
}

$ExpectedBuildParent = [System.IO.Path]::GetFullPath($ProjectRoot) + [System.IO.Path]::DirectorySeparatorChar
if (-not $BuildRoot.StartsWith($ExpectedBuildParent, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "refusing to clean build directory outside project: $BuildRoot"
}
if (Test-Path $BuildRoot) { Remove-Item -LiteralPath $BuildRoot -Recurse -Force }
New-Item -ItemType Directory -Force $BuildRoot | Out-Null

$OldPath = $env:Path
$env:Path = "$MingwBin;$QtBin;$OldPath"
try {
    $SyncOutput = (& python (Join-Path $PSScriptRoot "sync_resources.py") --project $ProjectRoot 2>&1 | Out-String).Trim()
    $SyncRc = $LASTEXITCODE
    if ($SyncRc -ne 0) { throw "resource sync failed ($SyncRc): $SyncOutput" }
    $SourceHash = Get-ArtifactSourceHash $ProjectRoot
    $env:SMARTKEY_RUN_ID = $RunId
    $env:SMARTKEY_SOURCE_HASH = $SourceHash
    $CoverageOutput = (& python (Join-Path $PSScriptRoot "generate_mock_coverage.py") 2>&1 | Out-String).Trim()
    $CoverageRc = $LASTEXITCODE
    if ($CoverageRc -ne 0) { throw "mock coverage failed ($CoverageRc): $CoverageOutput" }

    Push-Location $BuildRoot
    try {
        $NativeErrorPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        $QmakeOutput = (& $Qmake (Join-Path $ProjectRoot "SmartKeyAIReconstruction.pro") "CONFIG+=release" 2>&1 | Tee-Object -FilePath $Log | Out-String).Trim()
        $QmakeRc = $LASTEXITCODE
        if ($QmakeRc -eq 0) {
            $MakeOutput = (& $Make -j2 2>&1 | Tee-Object -FilePath $Log -Append | Out-String).Trim()
            $MakeRc = $LASTEXITCODE
        } else {
            $MakeOutput = "not run"
            $MakeRc = -1
        }
        $ErrorActionPreference = $NativeErrorPreference
    } finally { Pop-Location }
} finally { $env:Path = $OldPath }

$Exe = Join-Path $BuildRoot "release\SmartKeyAIReconstruction.exe"
$PeSubsystem = if (Test-Path $Exe) { Get-PeSubsystem $Exe } else { -1 }
$ExeHash = if (Test-Path $Exe) { Get-ArtifactFileHash $Exe } else { "" }
$VersionInfo = if (Test-Path $Exe) { (Get-Item -LiteralPath $Exe).VersionInfo } else { $null }
$ExpectedPeVersion = "$AppVersion.0"
$VersionResourceValid = $null -ne $VersionInfo -and `
    $VersionInfo.FileVersion -eq $ExpectedPeVersion -and `
    $VersionInfo.ProductVersion -eq $ExpectedPeVersion -and `
    $VersionInfo.ProductName -eq "SmartKey AI" -and `
    $VersionInfo.CompanyName -eq "SmartKeyAI" -and `
    $VersionInfo.FileDescription -eq "SmartKey AI Desktop Assistant" -and `
    $VersionInfo.OriginalFilename -eq "SmartKeyAIReconstruction.exe"
$BuiltAt = Get-ArtifactUtcTimestamp
$Toolchain = [ordered]@{
    qmake = $Qmake
    gxx = $Gxx
    mingw32_make = $Make
    qmake_version = (& $Qmake -v 2>&1 | Out-String).Trim()
    qt_version = (& $Qmake -query QT_VERSION 2>&1 | Out-String).Trim()
    qt_install_prefix = (& $Qmake -query QT_INSTALL_PREFIX 2>&1 | Out-String).Trim()
    compiler_dumpmachine = (& $Gxx -dumpmachine 2>&1 | Out-String).Trim()
    compiler_version = (& $Gxx --version 2>&1 | Select-Object -First 1 | Out-String).Trim()
    make_version = (& $Make --version 2>&1 | Select-Object -First 1 | Out-String).Trim()
}
$Result = [ordered]@{
    status = if ($SyncRc -eq 0 -and $CoverageRc -eq 0 -and $QmakeRc -eq 0 -and $MakeRc -eq 0 -and (Test-Path $Exe) -and $PeSubsystem -eq 2 -and $VersionResourceValid) { "PASS" } else { "FAIL" }
    clean_build = $true
    project = Join-Path $ProjectRoot "SmartKeyAIReconstruction.pro"
    build_directory = $BuildRoot
    executable = $Exe
    run_id = $RunId
    source_hash = $SourceHash
    built_at = $BuiltAt
    exe_sha256 = $ExeHash
    application_version = $AppVersion
    pe_version_resource_valid = $VersionResourceValid
    pe_version_resource = if ($null -ne $VersionInfo) { [ordered]@{
        file_version = $VersionInfo.FileVersion
        product_version = $VersionInfo.ProductVersion
        product_name = $VersionInfo.ProductName
        company_name = $VersionInfo.CompanyName
        file_description = $VersionInfo.FileDescription
        original_filename = $VersionInfo.OriginalFilename
    } } else { $null }
    environment = Get-ArtifactEnvironment
    executable_size = if (Test-Path $Exe) { (Get-Item $Exe).Length } else { 0 }
    pe_subsystem = $PeSubsystem
    pe_subsystem_name = if ($PeSubsystem -eq 2) { "IMAGE_SUBSYSTEM_WINDOWS_GUI" } else { "unexpected" }
    release_has_no_console_subsystem = $PeSubsystem -eq 2
    tools = $Toolchain
    exit_codes = [ordered]@{ resource_sync = $SyncRc; mock_coverage = $CoverageRc; qmake = $QmakeRc; mingw32_make = $MakeRc }
    log = $Log
}
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$ReportJson = ($Result | ConvertTo-Json -Depth 6) + "`n"
[System.IO.File]::WriteAllText((Join-Path $ReportDir "build_verification.json"), $ReportJson, $Utf8NoBom)
$Manifest = [ordered]@{
    schema_version = 1
    run_id = $RunId
    source_hash = $SourceHash
    built_at = $BuiltAt
    executable = $Exe
    exe_sha256 = $ExeHash
    application_version = $AppVersion
    toolchain = $Toolchain
    environment = Get-ArtifactEnvironment
}
[System.IO.File]::WriteAllText((Join-Path $ReportDir "run_manifest.json"),
    (($Manifest | ConvertTo-Json -Depth 6) + "`n"), $Utf8NoBom)
Write-Output "[build] $($Result.status) sync=$SyncRc qmake=$QmakeRc make=$MakeRc exe=$Exe"
if ($Result.status -ne "PASS") { exit 1 }
exit 0
