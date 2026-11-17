@echo off
setlocal
rem Use Windows PowerShell modules even when launched from PowerShell 7.
set "PSModulePath=%SystemRoot%\System32\WindowsPowerShell\v1.0\Modules"
title Radar CSK - Restore Infineon firmware
pushd "%~dp0" || exit /b 1
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\install-firmware.ps1" -Firmware Original %*
set "flash_exit=%errorlevel%"
echo.
if not "%flash_exit%"=="0" echo FAILED. Read the error above. The operation did not complete successfully.
popd
pause
exit /b %flash_exit%
