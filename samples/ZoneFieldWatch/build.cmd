@echo off
setlocal
if "%~1"=="-?" goto zsdk_help
if "%~1"=="/?" goto zsdk_help
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build\make.ps1" %*
goto zsdk_done
:zsdk_help
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build\make.ps1" -Help
:zsdk_done
set "zsdk_exit=%ERRORLEVEL%"
setlocal enabledelayedexpansion
set "zsdk_line=!CMDCMDLINE!"
set "zsdk_cut=!zsdk_line:.cmd" "=!"
if not defined ZSDK_NOPAUSE if "%~1"=="" if not "!zsdk_cut!"=="!zsdk_line!" pause
exit /b %zsdk_exit%
