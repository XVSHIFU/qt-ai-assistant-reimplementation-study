$ErrorActionPreference = "Stop"

$StageRoot = [System.IO.Path]::GetFullPath($PSScriptRoot)
$ReferenceRoot = [System.IO.Path]::GetFullPath((Join-Path $StageRoot ".."))
$Exe = Join-Path $ReferenceRoot "SmartKey AI\SmartKey AI.exe"
$V2Manifest = Join-Path $ReferenceRoot "stage2_v2\heuristic_zlib_manifest.json"
$Calls = Join-Path $StageRoot "qresource_call_candidates_v3.json"
$Manifest = Join-Path $StageRoot "authoritative_qrc_manifest_v3.json"
$Tree = Join-Path $StageRoot "recovered_qrc_tree"
$Verification = Join-Path $StageRoot "verification"
$Tools = Join-Path $StageRoot "tools"

function Reset-StageChild([string] $Path) {
    $Resolved = [System.IO.Path]::GetFullPath($Path)
    if ([System.IO.Path]::GetDirectoryName($Resolved) -ne $StageRoot) {
        throw "Refusing to reset a path outside stage2_v3: $Resolved"
    }
    if (Test-Path -LiteralPath $Resolved) {
        Remove-Item -LiteralPath $Resolved -Recurse -Force
    }
}

Reset-StageChild $Tree
Reset-StageChild $Verification

& python (Join-Path $Tools "locate_qresource_calls.py") `
    --input-exe $Exe `
    --output-json $Calls
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& python (Join-Path $Tools "extract_qt_rcc_arrays.py") `
    --input-exe $Exe `
    --calls-json $Calls `
    --output-dir $Tree `
    --manifest $Manifest `
    --v2-zlib-manifest $V2Manifest
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& python (Join-Path $Tools "verify_qrc_tree.py") `
    --input-exe $Exe `
    --calls-json $Calls `
    --manifest $Manifest `
    --output-dir $Verification `
    --v2-zlib-manifest $V2Manifest
exit $LASTEXITCODE
