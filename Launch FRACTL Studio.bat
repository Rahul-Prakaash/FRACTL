@echo off
title FRACTL Studio - close this window to stop the studio
cd /d "%~dp0"
where wsl >nul 2>&1
if errorlevel 1 (
  echo WSL is not installed on this PC. Run "Check FRACTL Setup.bat" for help.
  pause
  exit /b 1
)
wsl -e pkill -f studio.py >nul 2>&1
echo Starting FRACTL Studio at http://127.0.0.1:8732 ...
echo Everything runs on this laptop: no internet needed.
echo.
start "" /b powershell -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -Command "$u='http://127.0.0.1:8732/'; for($i=0;$i -lt 90;$i++){try{Invoke-WebRequest ($u+'api/health') -UseBasicParsing -TimeoutSec 2 | Out-Null; $e=Join-Path ${env:ProgramFiles(x86)} 'Microsoft\Edge\Application\msedge.exe'; if(Test-Path $e){Start-Process $e -ArgumentList ('--app='+$u),'--start-maximized'}else{Start-Process $u}; break}catch{Start-Sleep -Seconds 1}}"
wsl --cd "%~dp0." -e sh -c "chmod +x fractlc 2>/dev/null; exec python3 -u studio.py 8732"
echo.
echo The studio stopped. If it stopped straight away, run "Check FRACTL Setup.bat".
pause
