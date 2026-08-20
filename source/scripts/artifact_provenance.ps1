$ArtifactProvenanceScript = Join-Path $PSScriptRoot "artifact_provenance.py"

function Get-ArtifactUtcTimestamp {
    return [DateTime]::UtcNow.ToString("o", [Globalization.CultureInfo]::InvariantCulture)
}

function Get-ArtifactSourceHash([string]$ProjectRoot) {
    $Value = (& python $ArtifactProvenanceScript source-hash --project $ProjectRoot 2>&1 | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $Value -notmatch '^[0-9a-f]{64}$') {
        throw "source tree hashing failed: $Value"
    }
    return $Value
}

function Get-ArtifactFileHash([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "missing artifact: $Path" }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-ApplicationVersion([string]$ProjectRoot) {
    $Value = (& python (Join-Path $PSScriptRoot "release_artifacts.py") version --project $ProjectRoot 2>&1 | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $Value -notmatch '^[0-9]+\.[0-9]+\.[0-9]+$') {
        throw "application version lookup failed: $Value"
    }
    return $Value
}

function Get-ArtifactEnvironment {
    return [ordered]@{
        os = [Environment]::OSVersion.VersionString
        process_architecture = [Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString()
        machine = [Environment]::MachineName
        ci = -not [string]::IsNullOrWhiteSpace($env:CI)
    }
}

function Read-ArtifactRunContext([string]$ProjectRoot) {
    $ManifestPath = Join-Path $ProjectRoot "reports\run_manifest.json"
    $BuildReportPath = Join-Path $ProjectRoot "reports\build_verification.json"
    foreach ($Required in @($ManifestPath, $BuildReportPath)) {
        if (-not (Test-Path -LiteralPath $Required -PathType Leaf)) {
            throw "missing build provenance report: $Required; run build.ps1"
        }
    }
    $Manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
    $Build = Get-Content -LiteralPath $BuildReportPath -Raw | ConvertFrom-Json
    foreach ($Field in @("run_id", "source_hash", "built_at", "exe_sha256")) {
        if ([string]::IsNullOrWhiteSpace($Manifest.$Field) -or $Build.$Field -ne $Manifest.$Field) {
            throw "build provenance mismatch for $Field"
        }
    }
    if ($Build.status -ne "PASS") { throw "build report is not PASS" }
    $CurrentSourceHash = Get-ArtifactSourceHash $ProjectRoot
    if ($CurrentSourceHash -ne $Manifest.source_hash) {
        throw "source tree changed after build: expected $($Manifest.source_hash), actual $CurrentSourceHash"
    }
    $ActualExeHash = Get-ArtifactFileHash $Build.executable
    if ($ActualExeHash -ne $Manifest.exe_sha256) {
        throw "build executable changed after build: expected $($Manifest.exe_sha256), actual $ActualExeHash"
    }
    return [ordered]@{
        run_id = $Manifest.run_id
        source_hash = $Manifest.source_hash
        built_at = $Manifest.built_at
        exe_sha256 = $Manifest.exe_sha256
        executable = $Build.executable
        toolchain = $Manifest.toolchain
    }
}
