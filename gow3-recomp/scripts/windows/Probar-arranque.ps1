# Primer arranque de God of War III recompilado.
# Monta la ISO (sin extraerla), ejecuta Gow3Recomp.exe con registro y lo cierra
# solo al cabo de -Seconds. El registro queda en logs\ junto al .exe.
param(
    [string]$Iso = 'C:\Games\God_Of_War_3\God of War III (Europe) (En,Fr,De,Es,It,Nl,Pt,Pl,Ru).iso',
    [int]$Seconds = 90
)
$ErrorActionPreference = 'Stop'
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Exe  = Join-Path $Here 'Gow3Recomp.exe'
$Logs = Join-Path $Here 'logs'
New-Item -ItemType Directory -Force $Logs | Out-Null
$Stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$Log   = Join-Path $Logs "arranque_$Stamp.log"

# 1. Montar la ISO, o reutilizarla si ya está montada.
$img = Get-DiskImage -ImagePath $Iso
if (-not $img.Attached) { $img = Mount-DiskImage -ImagePath $Iso -Access ReadOnly -PassThru }
$letter = ($img | Get-Volume).DriveLetter
if (-not $letter) { throw 'No se pudo montar la ISO.' }
$Disc = "${letter}:\"
if (-not (Test-Path (Join-Path $Disc 'PS3_GAME\USRDIR\EBOOT.BIN'))) { throw "La ISO montada en $Disc no tiene PS3_GAME\USRDIR." }

# 2. Lanzar el juego. El EBOOT descifrado va al lado del .exe.
$env:PS3_VFS_ROOT = $Disc
$env:PS3_VERBOSE  = '1'
$elf = Join-Path $Here 'EBOOT.elf'
$p = Start-Process -FilePath $Exe -ArgumentList "`"$elf`"" -WorkingDirectory $Here `
     -RedirectStandardOutput $Log -RedirectStandardError "$Log.err" -PassThru
Write-Host "Gow3Recomp en marcha (PID $($p.Id)), disco en $Disc, registro en $Log"

# 3. Cerrar al cabo de $Seconds si sigue vivo.
if (-not $p.WaitForExit($Seconds * 1000)) { Stop-Process -Id $p.Id -Force; Write-Host "Cerrado tras $Seconds s." }
else { Write-Host "Terminó solo (código $($p.ExitCode))." }
"exit=$($p.ExitCode) seconds=$Seconds disc=$Disc" | Out-File (Join-Path $Logs "arranque_$Stamp.txt")
