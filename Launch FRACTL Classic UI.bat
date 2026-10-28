@echo off
title FRACTL classic UI (Stage 1 web form) - close this window to stop
cd /d "%~dp0"
echo Stopping any old FRACTL classic UI server...
wsl -e pkill -f web_ui.py >nul 2>&1
echo Starting the Stage 1 web form at http://127.0.0.1:8733 ...
echo (FRACTL Studio, the full Stage 3 front end, uses port 8732 and can run at the same time)
echo.
start "" /b powershell -NoProfile -WindowStyle Hidden -Command "for($i=0;$i -lt 90;$i++){try{Invoke-WebRequest 'http://127.0.0.1:8733/' -UseBasicParsing -TimeoutSec 2 | Out-Null; Start-Process 'http://127.0.0.1:8733/'; break}catch{Start-Sleep -Seconds 1}}"

wsl --cd "%~dp0." -e sh -c "chmod +x fractlc 2>/dev/null; exec python3 -u web_ui.py 8733"
echo.
echo The server stopped. If it stopped straight away, run "Check FRACTL Setup.bat".
pause
