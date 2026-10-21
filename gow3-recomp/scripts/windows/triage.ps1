# Triaje del EBOOT descifrado: tamaño de código, importaciones, uso de SPU.
# Uso: .\scripts\windows\triage.ps1 [-Eboot ruta\EBOOT.elf]
param([string]$Eboot)
$ErrorActionPreference = 'Stop'
$Root   = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$Parent = Split-Path $Root -Parent
$Kit    = Join-Path $Parent 'ps3kit'
$Work   = Join-Path $Parent 'work'
if (-not $Eboot) { $Eboot = Join-Path $Work 'EBOOT.elf' }
if (-not (Test-Path $Eboot)) { throw "No encuentro $Eboot. Descífralo con RPCS3 (PLAN.md, fase 1)." }
if (-not (Test-Path $Kit))   { throw "Falta ps3kit: ejecuta antes setup.ps1." }

$Out = Join-Path $Work 'triage'
New-Item -ItemType Directory -Force $Out | Out-Null
$Tools = Join-Path $Kit 'tools'

Write-Host '1/3 Resumen del ejecutable'
python (Join-Path $Tools 'triage_elf.py') $Eboot | Tee-Object (Join-Path $Out 'resumen.txt')

Write-Host '2/3 Importaciones y cobertura de ps3recomp'
python (Join-Path $Tools 'prx_analyzer.py') $Eboot --stubs *> (Join-Path $Out 'importaciones.txt')

Write-Host '3/3 Imágenes SPU incrustadas'
python (Join-Path $Tools 'extract_spu_images.py') $Eboot -o (Join-Path $Out 'spu_images') *> (Join-Path $Out 'spu.txt')

Write-Host "Resultados en $Out (resumen.txt, importaciones.txt, spu.txt)."
Write-Host 'Los tres .txt son solo listas y tamaños: compártelos con Claude.'
Write-Host 'La carpeta spu_images sí contiene código del juego: queda solo en tu PC.'
