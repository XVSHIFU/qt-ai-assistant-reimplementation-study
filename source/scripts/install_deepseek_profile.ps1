$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($env:DEEPSEEK_API_KEY)) {
    throw "DEEPSEEK_API_KEY must be supplied through the process environment"
}
$ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$QtRoot = Join-Path $ProjectRoot "_toolchain\Qt\5.15.2\mingw81_32"
$QtBin = Join-Path $QtRoot "bin"
$MingwBin = Join-Path $ProjectRoot "_toolchain\Qt\Tools\mingw810_32\bin"
$BuildRoot = Join-Path $ProjectRoot "build\install_profile"
$Qmake = Join-Path $QtBin "qmake.exe"
$Make = Join-Path $MingwBin "mingw32-make.exe"
New-Item -ItemType Directory -Force $BuildRoot | Out-Null

$OldPath = $env:Path
$env:Path = "$MingwBin;$QtBin;$OldPath"
try {
    Push-Location $BuildRoot
    try {
        & $Qmake (Join-Path $ProjectRoot "tests\live_ai\install_deepseek_profile.pro") "CONFIG+=release" | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "profile installer qmake failed" }
        & $Make -j2 | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "profile installer build failed" }
        & (Join-Path $BuildRoot "release\write_deepseek_profile.exe")
        exit $LASTEXITCODE
    } finally { Pop-Location }
} finally {
    $env:Path = $OldPath
}
