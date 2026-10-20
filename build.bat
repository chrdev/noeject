:: Build NoEject x64 and x86
:: chrdev, 0BSD
::@echo off
setlocal

set cmd_vswhere="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist %cmd_vswhere% (
echo Microsoft Visual Studio not installed.
set errorlevel=1
exit /b
)

for /f "delims=" %%# in ('%cmd_vswhere% -latest -property resolvedInstallationPath') do @set path_vs=%%#

call "%path_vs%\VC\Auxiliary\Build\vcvars64.bat"
if not %VSCMD_ARG_TGT_ARCH%==x64 (
echo No building environment for x64.
set errorlevel=1
exit /b
)
nmake release
call :CLEAR_VSENV

call "%path_vs%\VC\Auxiliary\Build\vcvars32.bat"
if not %VSCMD_ARG_TGT_ARCH%==x86 (
echo No building environment for x86.
set errorlevel=1
exit /b
)
nmake release
goto :EOF

:CLEAR_VSENV
set EXTERNAL_INCLUDE=
set INCLUDE=
set LIB=
set LIBPATH=
set path=%__VSCMD_PREINIT_PATH%
exit /b
