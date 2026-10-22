# Arranque de prueba de God of War III recompilado (Gow3Recomp.exe).
# - Usa la ISO montada (la monta si hace falta; no extrae nada).
# - Guardados y caché en gamedata\ junto al .exe (la ISO es de solo lectura).
# - Reutiliza de RPCS3 el firmware (dev_flash) y las DLL de FFmpeg para los videos.
# - Lo cierra solo al cabo de -Seconds y deja el registro en logs\.
param(
    [string]$Iso    = 'C:\Games\God_Of_War_3\God of War III (Europe) (En,Fr,De,Es,It,Nl,Pt,Pl,Ru).iso',
    [string]$Rpcs3  = 'C:\Games\Emulador',
    [int]$Seconds   = 180,
    [string]$Tag    = '',
    [int]$ParkWaitMs = 30000,
    [string]$MaxCont = '7=3,8=3,9=3,10=3'
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
# Registro reducido: con PS3_VERBOSE=1 todos los hilos escriben por un mismo
# cerrojo del archivo de registro, y eso puede trabar el proceso entero (Ascension
# lo apaga por defecto). El perfilador también queda apagado.
$env:PS3_VERBOSE = '0'
Remove-Item Env:PS3_SAMPLE -ErrorAction SilentlyContinue
# Cuánto espera el runtime a que el juego libere la GPU aparcada antes de saltar
# adelante (por defecto del juego: 2000 ms). Más largo = distinguir 'lento' de 'nunca'.
$env:GCM_PARK_WAIT_MS = "$ParkWaitMs"
# Diagnóstico SPU: cuánto código corre en el intérprete lento (cada 5 s) y copia
# de la memoria local de la SPU cuando la lista de trabajos 206 salta a código
# que se carga en tiempo de ejecución (para poder traducirlo también).
$env:SPU_INTERP_HIST = '1'
$LsDump = Join-Path $Logs 'lsdump'; New-Item -ItemType Directory -Force $LsDump | Out-Null
$env:SPU_SMC_LSDUMP = ''   # ya se copiaron (logs\lsdump)
# Foto de todos los hilos del juego (también los del PS3: en qué función y qué
# esperan) a los 120 y 170 s: si se colgó antes, muestra dónde.
$env:WATCHDOG_AT = '120,170'
$env:GCM_HOLD_LOG = '1'      # registra cada vez que la GPU sale de un aparcamiento
# Cuántas SPU puede usar a la vez cada lista de trabajos Wws (por defecto 1).
# wid 7 = RenderJobs, 8-10 = EDGE0-2 (geometría). En Ascension, repartir así sus
# listas llevó el menú de 14 a 33 fps; ponerlo en todas a la vez lo colgaba.
if ($MaxCont) { $env:SPURS_MAXCONT = $MaxCont } else { Remove-Item Env:SPURS_MAXCONT -ErrorAction SilentlyContinue }

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

# Vigilancia: si el registro deja de crecer 25 s con el juego abierto, el proceso
# está colgado; se guarda un volcado (pila de cada hilo) para ver dónde.
Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices; using Microsoft.Win32.SafeHandles;
public static class Volcado {
  [DllImport("dbghelp.dll", SetLastError=true)]
  static extern bool MiniDumpWriteDump(IntPtr h, uint pid, SafeFileHandle f, uint type, IntPtr e, IntPtr u, IntPtr c);
  public static bool Escribir(System.Diagnostics.Process p, string ruta) {
    using (var fs = new System.IO.FileStream(ruta, System.IO.FileMode.Create)) {
      // MiniDumpNormal | WithThreadInfo | WithUnloadedModules: pilas de todos los hilos, pocos MB
      return MiniDumpWriteDump(p.Handle, (uint)p.Id, fs.SafeFileHandle, 0x1020, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);
    }
  }
}
"@ -ErrorAction SilentlyContinue
$errLog = "$Log.err"; $ultimo = -1; $quieto = 0; $volcado = $false
$fin = (Get-Date).AddSeconds($Seconds)
while (-not $p.HasExited -and (Get-Date) -lt $fin) {
    Start-Sleep -Seconds 2
    $tam = if (Test-Path $errLog) { (Get-Item $errLog).Length } else { 0 }
    if ($tam -eq $ultimo) { $quieto += 2 } else { $quieto = 0; $ultimo = $tam }
    if (-not $volcado -and $quieto -ge 25) {
        $dmp = Join-Path $Logs "cuelgue_$Stamp.dmp"
        try { if ([Volcado]::Escribir($p, $dmp)) { Write-Host "Cuelgue detectado: volcado guardado en $dmp" } } catch { Write-Host "No se pudo guardar el volcado: $_" }
        $volcado = $true
    }
}
if (-not $p.HasExited) {
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    if (-not $p.HasExited) { & taskkill.exe /PID $p.Id /T /F | Out-Null }
    $estado = "cerrado tras $Seconds s"
} else { $estado = "terminó solo, código $($p.ExitCode)" }
"$estado | disco=$Disc | tag=$Tag | SPU_OVL_OFF=$env:SPU_OVL_OFF | park=$ParkWaitMs | maxcont=$MaxCont | $(Get-Date -Format s)" | Out-File (Join-Path $Logs "arranque_$Stamp.txt") -Encoding utf8
Write-Host "Listo: $estado."
