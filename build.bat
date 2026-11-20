@echo off
REM ============================================================
REM 一键编译脚本（Windows + Qt 5.14.2 + VS2022）
REM 用法：双击或在命令行执行 build.bat
REM 产物：build\release\RelaxAssistant.exe
REM ============================================================
setlocal

set QTDIR=E:\Qt\Qt5.14.2\5.14.2\msvc2017_64
set VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat

echo [1/3] 初始化 MSVC 编译环境...
call "%VCVARS%" >nul
if errorlevel 1 goto :fail

echo [2/3] 生成 Makefile...
if not exist build mkdir build
cd build
"%QTDIR%\bin\qmake.exe" ..\RelaxAssistant.pro -spec win32-msvc
if errorlevel 1 goto :fail

echo [3/3] 开始编译...
nmake /NOLOGO release
if errorlevel 1 goto :fail

echo.
echo ============================================
echo  编译成功！可执行文件：
echo  %CD%\release\RelaxAssistant.exe
echo ============================================
cd ..
goto :eof

:fail
echo.
echo *** 编译失败，请查看上面的错误信息 ***
cd ..
exit /b 1
