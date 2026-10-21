@echo off
rem Doble clic: 7 arranques seguidos de 2 minutos para localizar el modulo SPU que bloquea el juego
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Probar-modulos.ps1"
pause
