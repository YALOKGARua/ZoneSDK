@echo off
if "%~1"=="/?" goto zsdk_help
call "%~dp0release\1.0.2\fakehost.cmd" %*
exit /b %errorlevel%
:zsdk_help
call "%~dp0release\1.0.2\fakehost.cmd" -?
exit /b %errorlevel%
