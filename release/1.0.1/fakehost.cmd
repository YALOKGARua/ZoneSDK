@echo off
setlocal
set "zsdk_script=%~dp0tools\fakehost\run.ps1"
if not exist "%zsdk_script%" for /d %%r in ("%~dp0release\*") do if exist "%%~fr\tools\fakehost\run.ps1" set "zsdk_script=%%~fr\tools\fakehost\run.ps1"
if not exist "%zsdk_script%" echo ZoneSDK tool not found: tools\fakehost\run.ps1 & exit /b 1
if "%~1"=="-?" goto zsdk_help
if "%~1"=="/?" goto zsdk_help
powershell -NoProfile -ExecutionPolicy Bypass -File "%zsdk_script%" %*
goto zsdk_done
:zsdk_help
powershell -NoProfile -ExecutionPolicy Bypass -File "%zsdk_script%" -Help
:zsdk_done
set "zsdk_exit=%ERRORLEVEL%"
setlocal enabledelayedexpansion
set "zsdk_line=!CMDCMDLINE!"
set "zsdk_cut=!zsdk_line:.cmd" "=!"
if not defined ZSDK_NOPAUSE if "%~1"=="" if not "!zsdk_cut!"=="!zsdk_line!" pause
exit /b %zsdk_exit%
