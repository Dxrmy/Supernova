# Builds the SMG2 (Garigari) game module (native/smg2/CMakeLists.txt) on Windows with clang++ and Ninja
# inside the MSVC environment.
param(
    [string]$Garigari = (Join-Path $PSScriptRoot "garigari"),
    [Parameter(Mandatory = $true)][string]$Build,
    [string]$Config = "Release",
    [string]$Target = "all",
    [switch]$NoBaseline,
    [string]$Llvm = "C:\Program Files\LLVM\bin"
)
$ErrorActionPreference = "Stop"
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) {
    foreach ($cand in @(
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools",
        "C:\Program Files\Microsoft Visual Studio\2022\Community"
    )) {
        if (Test-Path -LiteralPath (Join-Path $cand "VC\Auxiliary\Build\vcvars64.bat")) {
            $vs = $cand
            break
        }
    }
}
if (-not $vs) { throw "Visual Studio C++ tools (vcvars64.bat) were not found" }
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path -LiteralPath $vcvars)) { throw "vcvars64.bat not found at $vcvars" }

$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) { $cmake = Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" }
if (-not (Test-Path -LiteralPath $cmake)) { throw "CMake was not found" }

$ninja = (Get-Command ninja -ErrorAction SilentlyContinue).Source
if (-not $ninja) { $ninja = Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" }
if (-not (Test-Path -LiteralPath $ninja)) { throw "Ninja was not found" }

$llvm = $Llvm.Replace("\", "/").TrimEnd("/")
if (-not (Test-Path -LiteralPath "$llvm/clang++.exe")) { throw "No clang++.exe in $Llvm" }
$src = (Join-Path $PSScriptRoot "smg2").Replace("\", "/")
$Garigari = $Garigari.Replace("\", "/")
$Build = $Build.Replace("\", "/")
$levels = [ordered]@{ "x86-64-v3" = $Build }
if (-not $NoBaseline) { $levels["x86-64-v2"] = "$Build/x86-64-v2" }
$env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;$env:PATH"

foreach ($level in $levels.Keys) {
    $folder = $levels[$level]
    $configure = "`"$cmake`" -S `"$src`" -B `"$folder`" -G Ninja -DCMAKE_MAKE_PROGRAM=`"$($ninja.Replace("\", "/"))`" " +
        "-DCMAKE_BUILD_TYPE=$Config -DCMAKE_C_COMPILER=`"$llvm/clang.exe`" -DCMAKE_CXX_COMPILER=`"$llvm/clang++.exe`" " +
        "-DCMAKE_RC_COMPILER=`"$llvm/llvm-rc.exe`" -DNEBULA_GARIGARI_DIR=`"$Garigari`" -DNEBULA_CPU_LEVEL=$level"
    $compile = "`"$cmake`" --build `"$folder`" --target $Target"
    cmd.exe /d /c "call `"$vcvars`" >nul && $configure && $compile"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
exit 0
