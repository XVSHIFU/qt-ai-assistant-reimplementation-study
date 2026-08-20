$ErrorActionPreference = "Stop"
$AnalysisRoot = [System.IO.Path]::GetFullPath($PSScriptRoot)
$Workspace = [System.IO.Path]::GetFullPath((Join-Path $AnalysisRoot ".."))
$AppDir = Join-Path $Workspace "参考\SmartKey AI"
$ResourceRoot = Join-Path $Workspace "参考\stage2_v3\recovered_qrc_tree"
$Manifest = Join-Path $Workspace "参考\stage2_v3\authoritative_qrc_manifest_v3.json"
$Inventory = Join-Path $AnalysisRoot "toolchain_inventory.json"

& python (Join-Path $AnalysisRoot "tools\inventory_toolchain.py") `
    --workspace $Workspace --app-dir $AppDir --output $Inventory
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& python (Join-Path $AnalysisRoot "tools\analyze_recovered_resources.py") `
    --resource-root $ResourceRoot `
    --manifest $Manifest `
    --inventory $Inventory `
    --input-exe (Join-Path $AppDir "SmartKey AI.exe") `
    --output-dir $AnalysisRoot
exit $LASTEXITCODE
