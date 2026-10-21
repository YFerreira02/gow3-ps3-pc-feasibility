# Arranque de prueba de God of War III recompilado (Gow3Recomp.exe).
# - Usa la ISO montada (la monta si hace falta; no extrae nada).
# - Guardados y caché en gamedata\ junto al .exe (la ISO es de solo lectura).
# - Reutiliza de RPCS3 el firmware (dev_flash) y las DLL de FFmpeg para los videos.
# - Lo cierra solo al cabo de -Seconds y deja el registro en logs\.
param(
    [string]$Iso    = 'C:\Games\God_Of_War_3\God of War III (Europe) (En,Fr,De,Es,It,Nl,Pt,Pl,Ru).iso',
    [string]$Rpcs3  = 'C:\Games\Emulador',
    [int]$Seconds   = 180
)
$ErrorActionPreference = 'Stop'
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Exe  = Join-Path $Here 'Gow3Recomp.exe'
$Elf  = Join-Path $Here 'EBOOT.elf'
# Primera vez (o versión nueva): descomprimir Gow3Recomp.zip junto al script.
$Zip = Join-Path $Here 'Gow3Recomp.zip'
if ((Test-Path $Zip) -and (-not (Test-Path $Exe) -or (Get-Item $Zip).LastWriteTime -gt (Get-Item $Exe).LastWriteTime)) {
    Expand-Archive -Path $Zip -DestinationPath $Here -Force
    (Get-Item $Exe).LastWriteTime = Get-Date
}
foreach ($f in $Exe, $Elf) { if (-not (Test-Path $f)) { throw "Falta $f" } }

# 1. Disco: la ISO montada como unidad.
$img = Get-DiskImage -ImagePath $Iso
if (-not $img.Attached) { $img = Mount-DiskImage -ImagePath $Iso -Access ReadOnly -PassThru }
$letter = ($img | Get-Volume).DriveLetter
if (-not $letter) { throw 'No se pudo montar la ISO.' }
$Disc = "${letter}:\"
if (-not (Test-Path (Join-Path $Disc 'PS3_GAME\USRDIR\GAME.PSARC'))) { throw "La unidad $Disc no tiene el juego." }

# 2. Carpetas escribibles y registro.
$Data = Join-Path $Here 'gamedata'
$Hdd0 = Join-Path $Data 'dev_hdd0'; $Hdd1 = Join-Path $Data 'dev_hdd1'
$Logs = Join-Path $Here 'logs'
foreach ($d in $Hdd0, $Hdd1, $Logs) { New-Item -ItemType Directory -Force $d | Out-Null }
# Caché de disco (dev_hdd1) borrada en cada arranque: si el juego se cerró sin
# terminar de escribirla, su índice queda en blanco y fios reintenta abrirlo sin
# fin. Es solo una caché de velocidad; la consola también la vacía al cambiar de juego.
Remove-Item -Recurse -Force (Join-Path $Hdd1 'cache') -ErrorAction SilentlyContinue
$Stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$Log   = Join-Path $Logs "arranque_$Stamp.log"

# 3. Entorno del runtime.
$env:PS3_VFS_ROOT  = $Disc
$env:PS3_HDD0_ROOT = $Hdd0
$env:PS3_HDD1_ROOT = $Hdd1
if (Test-Path (Join-Path $Rpcs3 'dev_flash')) { $env:PS3_DEV_FLASH = Join-Path $Rpcs3 'dev_flash' }
if (Test-Path (Join-Path $Rpcs3 'avcodec-63.dll')) { $env:PATH = "$Rpcs3;$env:PATH" }   # FFmpeg de RPCS3
$env:PS3_VERBOSE = '1'
$env:PS3_SAMPLE  = '20'      # perfilador: cada 20 ms anota qué función ejecuta cada hilo

# 4. Lanzar y esperar.
$p = Start-Process -FilePath $Exe -ArgumentList "`"$Elf`"" -WorkingDirectory $Here `
     -RedirectStandardOutput $Log -RedirectStandardError "$Log.err" -PassThru
Write-Host "Gow3Recomp en marcha (PID $($p.Id)). Disco $Disc. Registro: $Log"
Write-Host ''
Write-Host 'Teclado = mando: Z = X (cruz), X = circulo, A = cuadrado, S = triangulo,'
Write-Host '                 flechas = cruceta, Enter = START, Tab = SELECT, Q/W = L1/R1.'
Write-Host '(Haz clic en la ventana del juego para que reciba las teclas.)'
Write-Host ''
Write-Host "Se cerrará solo en $Seconds s (puedes cerrarlo antes)."
if (-not $p.WaitForExit($Seconds * 1000)) {
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    if (-not $p.HasExited) { & taskkill.exe /PID $p.Id /T /F | Out-Null }
    $estado = "cerrado tras $Seconds s"
} else { $estado = "terminó solo, código $($p.ExitCode)" }
"$estado | disco=$Disc | $(Get-Date -Format s)" | Out-File (Join-Path $Logs "arranque_$Stamp.txt") -Encoding utf8
Write-Host "Listo: $estado."
