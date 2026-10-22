@echo off
rem Abre God of War III recompilado SIN limite de tiempo. Cierralo tu cuando quieras
rem (cierra la ventana del juego). El registro queda en logs\ como siempre.
rem Teclado: Z o Espacio = X (cruz), X = circulo, A = cuadrado, S = triangulo,
rem flechas = cruceta, Enter = START, Tab = SELECT, Q/W = L1/R1.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Probar-arranque.ps1" -Seconds 2000000 -Tag manual
pause
