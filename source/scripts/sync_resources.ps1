$ErrorActionPreference = "Stop"
$ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
& python (Join-Path $PSScriptRoot "sync_resources.py") --project $ProjectRoot
exit $LASTEXITCODE

