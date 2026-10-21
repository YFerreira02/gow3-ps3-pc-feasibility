@echo off
if not exist "%~dp0backup\config_BCES00510.original.yml" (
  echo No hay copia de seguridad todavia: la configuracion original no se ha cambiado.
  pause
  exit /b 0
)
copy /Y "%~dp0backup\config_BCES00510.original.yml" "C:\Games\Emulador\config\custom_configs\config_BCES00510.yml"
echo Configuracion original de God of War III restaurada.
pause
