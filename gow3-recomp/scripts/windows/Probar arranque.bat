@echo off
rem Doble clic: arranca God of War III recompilado 2 minutos y guarda el registro en logs\
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Probar-arranque.ps1" %*
pause
