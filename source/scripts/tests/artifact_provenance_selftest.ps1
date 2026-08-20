$ErrorActionPreference = "Stop"
$ScriptsRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$Files = @(
    "artifact_provenance.ps1", "build.ps1", "test.ps1", "smoke_test.ps1",
    "package.ps1", "live_deepseek_smoke.ps1"
)
foreach ($Name in $Files) {
    $Tokens = $null
    $Errors = $null
    [void][System.Management.Automation.Language.Parser]::ParseFile(
        (Join-Path $ScriptsRoot $Name), [ref]$Tokens, [ref]$Errors)
    if ($Errors.Count -ne 0) {
        throw "PowerShell parse failed for ${Name}: $($Errors -join '; ')"
    }
}
& python (Join-Path $PSScriptRoot "test_artifact_provenance.py")
if ($LASTEXITCODE -ne 0) { throw "artifact provenance Python self-test failed" }
& python (Join-Path $PSScriptRoot "test_release_artifacts.py")
if ($LASTEXITCODE -ne 0) { throw "release artifact Python self-test failed" }
Write-Output "ARTIFACT_PROVENANCE_POWERSHELL_SELFTEST=PASS parsed=$($Files.Count)"
