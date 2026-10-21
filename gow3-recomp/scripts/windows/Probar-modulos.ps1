# Prueba automática: arranca el juego varias veces, cada una con un grupo de
# módulos de trabajo SPU (imágenes 200-219) en el intérprete en vez de traducidos.
# Sirve para encontrar qué módulo traducido hace que el juego se bloquee antes
# de la pantalla de brillo. Unos 15 minutos; no hace falta tocar nada.
$ErrorActionPreference = 'Stop'
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$pruebas = @(
    @{ Tag = 'todos-interpretados'; Off = 'all' },
    @{ Tag = 'grupo-200-203';       Off = '200-203' },
    @{ Tag = 'grupo-204-207';       Off = '204-207' },
    @{ Tag = 'grupo-208-211';       Off = '208-211' },
    @{ Tag = 'grupo-212-215';       Off = '212-215' },
    @{ Tag = 'grupo-216-219';       Off = '216-219' },
    @{ Tag = 'todos-traducidos';    Off = '' }
)
$i = 0
foreach ($t in $pruebas) {
    $i++
    Write-Host ""
    Write-Host "=== Prueba $i de $($pruebas.Count): $($t.Tag) ==="
    $env:SPU_OVL_OFF = $t.Off
    & (Join-Path $Here 'Probar-arranque.ps1') -Seconds 120 -Tag $t.Tag
    Start-Sleep -Seconds 3
}
Remove-Item Env:SPU_OVL_OFF -ErrorAction SilentlyContinue
Write-Host ""
Write-Host "Terminado. Avísale a Claude: los registros están en logs\."
