# 编译脚本：RelaxAssistant
$ErrorActionPreference = "Continue"

$env:INCLUDE="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\ucrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\shared;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\winrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\cppwinrt;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.8\Include\um"
$env:LIB="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\lib\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.8\Lib\um\x64"
$env:PATH="E:\Qt\Qt5.14.2\5.14.2\msvc2017_64\bin;C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64;C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64;$env:PATH"

$qmake = "E:\Qt\Qt5.14.2\5.14.2\msvc2017_64\bin\qmake.exe"
$srcDir = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $srcDir "build"

# ---- 第 1 步：清理旧 Makefile ----
if (Test-Path (Join-Path $buildDir "Makefile")) { Remove-Item -Force (Join-Path $buildDir "Makefile") }
if (Test-Path (Join-Path $buildDir "Makefile.Release")) { Remove-Item -Force (Join-Path $buildDir "Makefile.Release") }
if (Test-Path (Join-Path $buildDir "Makefile.Debug")) { Remove-Item -Force (Join-Path $buildDir "Makefile.Debug") }

Set-Location $buildDir

# ---- 第 2 步：qmake ----
Write-Output "=== QMAKE ==="
& $qmake (Join-Path $srcDir "RelaxAssistant.pro") 2>&1
Write-Output ("QMAKE_EXIT=" + $LASTEXITCODE)

if ($LASTEXITCODE -ne 0) { exit 1 }

# ---- 第 3 步：nmake release ----
Write-Output "=== NMAKE RELEASE ==="
& nmake release 2>&1
Write-Output ("NMAKE_EXIT=" + $LASTEXITCODE)
