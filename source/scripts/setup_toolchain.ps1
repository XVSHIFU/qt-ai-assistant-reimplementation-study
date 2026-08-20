$ErrorActionPreference = "Stop"
$ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$ToolchainRoot = Join-Path $ProjectRoot "_toolchain"
$QtRoot = Join-Path $ToolchainRoot "Qt"
$VenvRoot = Join-Path $ToolchainRoot "aqt-venv"
$Aqt = Join-Path $VenvRoot "Scripts\aqt.exe"
$AqtPython = Join-Path $VenvRoot "Scripts\python.exe"
$Qmake = Join-Path $QtRoot "5.15.2\mingw81_32\bin\qmake.exe"
$Gxx = Join-Path $QtRoot "Tools\mingw810_32\bin\g++.exe"
$Make = Join-Path $QtRoot "Tools\mingw810_32\bin\mingw32-make.exe"

New-Item -ItemType Directory -Force $ToolchainRoot | Out-Null
if (-not (Test-Path $Aqt)) {
    python -m venv $VenvRoot
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & (Join-Path $VenvRoot "Scripts\python.exe") -m pip install --disable-pip-version-check --upgrade pip aqtinstall
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

if (-not (Test-Path $Qmake)) {
    & $Aqt install-qt windows desktop 5.15.2 win32_mingw81 --outputdir $QtRoot
    if ($LASTEXITCODE -ne 0) {
        & $Aqt install-qt windows desktop 5.15.2 win32_mingw81 --outputdir $QtRoot --base "https://mirrors.ocf.berkeley.edu/qt/"
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
}

if (-not (Test-Path $Gxx) -or -not (Test-Path $Make)) {
    & $Aqt install-tool windows desktop tools_mingw qt.tools.win32_mingw810 --outputdir $QtRoot
    if ($LASTEXITCODE -ne 0) {
        & $Aqt install-tool windows desktop tools_mingw qt.tools.win32_mingw810 --outputdir $QtRoot --base "https://mirrors.ocf.berkeley.edu/qt/"
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
}

$Result = [ordered]@{
    status = if ((Test-Path $Qmake) -and (Test-Path $Gxx) -and (Test-Path $Make)) { "PASS" } else { "FAIL" }
    qt_requested = "5.15.2"
    qt_arch_requested = "win32_mingw81"
    compiler_requested = "MinGW 8.1 32-bit"
    qmake = $Qmake
    gxx = $Gxx
    mingw32_make = $Make
    install_commands = @(
        "aqt install-qt windows desktop 5.15.2 win32_mingw81 --outputdir <project>/_toolchain/Qt",
        "aqt install-tool windows desktop tools_mingw qt.tools.win32_mingw810 --outputdir <project>/_toolchain/Qt"
    )
    fallback_mirror = "https://mirrors.ocf.berkeley.edu/qt/"
    observed_first_install_redirect = "qt.mirror.constant.com"
    qmake_version = if (Test-Path $Qmake) { (& $Qmake -v | Out-String).Trim() } else { $null }
    qmake_query = if (Test-Path $Qmake) { (& $Qmake -query 2>&1 | Out-String).Trim() } else { $null }
    compiler_version = if (Test-Path $Gxx) { (& $Gxx --version | Out-String).Trim() } else { $null }
    compiler_target = if (Test-Path $Gxx) { (& $Gxx -dumpmachine | Out-String).Trim() } else { $null }
    make_version = if (Test-Path $Make) { (& $Make --version | Select-Object -First 1 | Out-String).Trim() } else { $null }
    aqt_version = if (Test-Path $AqtPython) { (& $AqtPython -c "import importlib.metadata; print(importlib.metadata.version('aqtinstall'))" | Out-String).Trim() } else { $null }
}
New-Item -ItemType Directory -Force (Join-Path $ProjectRoot "reports") | Out-Null
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$ReportJson = ($Result | ConvertTo-Json -Depth 5) + "`n"
[System.IO.File]::WriteAllText((Join-Path $ProjectRoot "reports\toolchain_setup.json"), $ReportJson, $Utf8NoBom)
if ($Result.status -ne "PASS") { exit 1 }
Write-Output "[toolchain] PASS qmake=$Qmake"
exit 0
