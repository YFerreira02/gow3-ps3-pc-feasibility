# Vigilante: deja esta ventana abierta y Claude podrá lanzar las pruebas solo.
# Cada 5 s mira si existe "ejecutar.txt" junto a este script (Claude lo deja ahí
# por el puente de archivos). Si aparece, lo borra y ejecuta Probar-arranque.ps1
# (3 minutos, cierra el juego solo). Para pararlo, cierra esta ventana.
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Orden = Join-Path $Here 'ejecutar.txt'
$Script = Join-Path $Here 'Probar-arranque.ps1'
Write-Host 'Vigilante en marcha: esperando pruebas de Claude. Cierra la ventana para pararlo.'
while ($true) {
    if (Test-Path $Orden) {
        $tag = (Get-Content $Orden -Raw -ErrorAction SilentlyContinue)
        Remove-Item $Orden -Force -ErrorAction SilentlyContinue
        if ($tag) { $tag = $tag.Trim() } else { $tag = '' }
        Write-Host "$(Get-Date -Format T)  Prueba pedida por Claude ($tag)"
        try { & powershell -NoProfile -ExecutionPolicy Bypass -File $Script -Tag $tag } catch { Write-Host "Error: $_" }
        Write-Host "$(Get-Date -Format T)  Prueba terminada. Esperando la siguiente."
    }
    Start-Sleep -Seconds 5
}
