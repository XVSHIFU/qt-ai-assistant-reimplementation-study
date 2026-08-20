$ErrorActionPreference = "Stop"
$ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$QtRoot = Join-Path $ProjectRoot "_toolchain\Qt\5.15.2\mingw81_32"
$QtBin = Join-Path $QtRoot "bin"
$MingwBin = Join-Path $ProjectRoot "_toolchain\Qt\Tools\mingw810_32\bin"
$BuildRoot = Join-Path $ProjectRoot "build\live_ai"
$Qmake = Join-Path $QtBin "qmake.exe"
$Make = Join-Path $MingwBin "mingw32-make.exe"
. (Join-Path $PSScriptRoot "artifact_provenance.ps1")
$RunContext = Read-ArtifactRunContext $ProjectRoot
New-Item -ItemType Directory -Force $BuildRoot | Out-Null

$OldPath = $env:Path
$env:Path = "$MingwBin;$QtBin;$OldPath"
try {
    Push-Location $BuildRoot
    try {
        & $Qmake (Join-Path $ProjectRoot "tests\live_ai\live_deepseek_smoke.pro") "CONFIG+=release" | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "live smoke qmake failed" }
        & $Make -j2 | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "live smoke build failed" }
        $LiveOutput = @(& (Join-Path $BuildRoot "release\live_deepseek_smoke.exe") 2>&1)
        $LiveRc = $LASTEXITCODE
        $TextOutput = ($LiveOutput | ForEach-Object { $_.ToString() }) -join "`n"
        $Result = [ordered]@{
            status = if ($LiveRc -eq 0 -and $TextOutput -match "LIVE_SMOKE=PASS") { "PASS" } else { "FAIL" }
            validation_kind = "behavioral_live_vendor"
            opt_in = $true
            run_id = $RunContext.run_id
            source_hash = $RunContext.source_hash
            built_at = $RunContext.built_at
            tested_at = Get-ArtifactUtcTimestamp
            exe_sha256 = $RunContext.exe_sha256
            environment = Get-ArtifactEnvironment
            endpoint = "https://api.deepseek.com/chat/completions"
            model = "deepseek-v4-flash"
            transport = "WinHTTP/Schannel"
            thinking_type = "enabled"
            reasoning_effort = "high"
            credential_source = if ([string]::IsNullOrWhiteSpace($env:DEEPSEEK_API_KEY)) { "current-user DPAPI profile" } else { "process environment" }
            exit_code = $LiveRc
            content_nonempty = $TextOutput -match "content_nonempty=1"
            reasoning_nonempty = $TextOutput -match "reasoning_nonempty=1"
            configured_model_discovered = $TextOutput -match "configured_model_discovered=1"
        }
        $Reports = Join-Path $ProjectRoot "reports"
        New-Item -ItemType Directory -Force $Reports | Out-Null
        [System.IO.File]::WriteAllText((Join-Path $Reports "live_deepseek_validation.json"),
            (($Result | ConvertTo-Json -Depth 5) + "`n"), [System.Text.UTF8Encoding]::new($false))
        Write-Output $TextOutput
        exit $LiveRc
    } finally { Pop-Location }
} finally {
    $env:Path = $OldPath
}
