# Prepara ps3kit: ps3recomp v0.17 con los parches de ../../patches.
# Requisitos: Git y Python 3.9+ en el PATH.
# Uso: .\scripts\windows\setup.ps1
$ErrorActionPreference = 'Stop'
$Root    = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$Parent  = Split-Path $Root -Parent
$Kit     = Join-Path $Parent 'ps3kit'
$Base    = 'a6d76c8'   # ps3recomp v0.17.0

foreach ($t in 'git', 'python') {
    if (-not (Get-Command $t -ErrorAction SilentlyContinue)) {
        throw "Falta '$t' en el PATH. Instálalo y vuelve a ejecutar."
    }
}

if (-not (Test-Path $Kit)) {
    git clone https://github.com/sp00nznet/ps3recomp $Kit
}
Push-Location $Kit
try {
    if (-not (git branch --list gow3-kit)) {
        git checkout -q -b gow3-kit $Base
        git -c user.name=gow3 -c user.email=gow3@localhost am -q --3way (Get-ChildItem (Join-Path $Root 'patches\*.patch') | Sort-Object Name | ForEach-Object FullName)
    } else {
        git checkout -q gow3-kit
    }
    python -m pip install -q -r requirements.txt
} finally { Pop-Location }

New-Item -ItemType Directory -Force (Join-Path $Parent 'work') | Out-Null
Write-Host "Listo. Kit en $Kit; deja el EBOOT descifrado en $(Join-Path $Parent 'work')."
