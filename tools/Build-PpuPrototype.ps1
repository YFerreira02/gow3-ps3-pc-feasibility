param(
    [switch]$BuildOnly
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$zig = Join-Path $repoRoot 'work\zig\zig-x86_64-windows-0.17.0\zig.exe'
$outDir = Join-Path $repoRoot 'work\PpuPrototype'
$exe = Join-Path $outDir 'Gow3PpuPrototype.exe'
$source = Join-Path $repoRoot 'src\Gow3Runtime\ppu'

if (-not (Test-Path -LiteralPath $zig)) {
    throw "Zig not found at $zig. Install Zig 0.17.0 into the ignored work folder or update this script."
}
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
& $zig cc -target x86_64-windows-gnu -O2 -std=c11 -nostdlib `
    (Join-Path $source 'ppu_core.c') (Join-Path $source 'ppu_prototype_main.c') `
    '-Wl,-e,entry' -lkernel32 `
    -o $exe
if ($LASTEXITCODE -ne 0) { throw "Zig compilation failed with exit code $LASTEXITCODE" }
Write-Host "Built native Windows prototype: $exe"
if (-not $BuildOnly) {
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "Prototype checks failed with exit code $LASTEXITCODE" }
}
