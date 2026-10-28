@echo off
title FRACTL setup check
cd /d "%~dp0"
echo FRACTL setup check
echo ==================
where wsl >nul 2>&1
if errorlevel 1 (
  echo [FAIL] WSL is not installed.
  echo        Before presentation day, open PowerShell as administrator and run:
  echo            wsl --install -d Ubuntu
  echo        then restart the PC and open "Ubuntu" once to create a user.
  pause
  exit /b 1
)
echo [ ok ] WSL is installed
wsl --cd "%~dp0." -e sh tests/check.sh
echo.
echo GPU: in Windows Settings ^> System ^> Display ^> Graphics, set Microsoft Edge
echo      to "High performance" so the 3D view runs on the RTX GPU.
echo      The studio shows the GPU it is using in the top-right corner of the 3D view.
pause
