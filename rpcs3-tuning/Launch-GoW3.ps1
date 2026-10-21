<#
  Lanzador de God of War III (BCES00510) sobre RPCS3 con perfiles y medicion de FPS.

  Uso:
    Launch-GoW3.ps1                      -> menu para elegir perfil, con medicion
    Launch-GoW3.ps1 -Perfil v1-spu-mega -> ese perfil, con medicion
    Launch-GoW3.ps1 -Play                -> juega con el mejor perfil (best.txt), sin medir

  Medicion: con el juego abierto, ve a la escena de prueba y pulsa F10.
  Se graban FPS, CPU y RAM durante -Seconds segundos (120 por defecto) y suena un pitido al terminar.
  Al cerrar el juego se guarda el resumen y una copia del registro de RPCS3 en results\.
  Este script no toca los archivos del juego; solo cambia la config personalizada del juego
  (con copia de seguridad en backup\).
#>
param(
    [string]$Perfil,
    [int]$Seconds = 120,
    [switch]$Play,
    [string]$EmulatorDir = 'C:\Games\Emulador',
    [string]$Serial = 'BCES00510'
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$profilesDir = Join-Path $root 'profiles'
$resultsDir = Join-Path $root 'results'
$backupDir = Join-Path $root 'backup'
$rpcs3 = Join-Path $EmulatorDir 'rpcs3.exe'
$gameConfig = Join-Path $EmulatorDir "config\custom_configs\config_$Serial.yml"
$gamesYml = Join-Path $EmulatorDir 'config\games.yml'
$rpcs3Log = Join-Path $EmulatorDir 'log\RPCS3.log'

if (-not (Test-Path -LiteralPath $rpcs3)) { throw "No se encontro rpcs3.exe en $rpcs3" }
if (Get-Process -Name rpcs3 -ErrorAction SilentlyContinue) {
    Write-Host 'RPCS3 ya esta abierto. Cierralo y vuelve a ejecutar este lanzador.' -ForegroundColor Yellow
    Read-Host 'Pulsa Enter para salir' | Out-Null
    exit 1
}

# Ruta del juego desde games.yml de RPCS3
$gameLine = Get-Content -LiteralPath $gamesYml | Where-Object { $_ -match "^$Serial\s*:" } | Select-Object -First 1
if (-not $gameLine) { throw "$Serial no aparece en $gamesYml" }
$gamePath = ($gameLine -replace "^$Serial\s*:\s*", '').Trim().Trim('"')

# Elegir perfil
if ($Play) {
    $bestFile = Join-Path $root 'best.txt'
    $Perfil = if (Test-Path -LiteralPath $bestFile) { (Get-Content -LiteralPath $bestFile -TotalCount 1).Trim() } else { 'v0-actual' }
}
$available = Get-ChildItem -LiteralPath $profilesDir -Filter 'v*.yml' | Sort-Object Name | ForEach-Object { $_.BaseName }
if (-not $Perfil) {
    Write-Host 'Perfiles disponibles:' -ForegroundColor Cyan
    for ($i = 0; $i -lt $available.Count; $i++) { Write-Host ("  [{0}] {1}" -f $i, $available[$i]) }
    $choice = Read-Host 'Numero del perfil a probar'
    $Perfil = $available[[int]$choice]
}
$profileFile = Join-Path $profilesDir "$Perfil.yml"
if (-not (Test-Path -LiteralPath $profileFile)) { throw "No existe el perfil $profileFile" }

# Copia de seguridad de la config original (solo la primera vez) y aplicar perfil
New-Item -ItemType Directory -Force -Path $backupDir, $resultsDir | Out-Null
$originalBackup = Join-Path $backupDir "config_$Serial.original.yml"
if ((Test-Path -LiteralPath $gameConfig) -and -not (Test-Path -LiteralPath $originalBackup)) {
    Copy-Item -LiteralPath $gameConfig -Destination $originalBackup
}
Copy-Item -LiteralPath $profileFile -Destination $gameConfig -Force
Write-Host "Perfil aplicado: $Perfil" -ForegroundColor Green

$process = Start-Process -FilePath $rpcs3 -ArgumentList @('--no-gui', "`"$gamePath`"") -WorkingDirectory $EmulatorDir -PassThru

if ($Play) {
    Write-Host 'Juego iniciado. Puedes cerrar esta ventana.'
    exit 0
}

Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class Gow3Win {
    delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc cb, IntPtr lParam);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowText(IntPtr hWnd, StringBuilder sb, int max);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern short GetAsyncKeyState(int vKey);
    public static string FpsTitle(uint targetPid) {
        string found = null;
        EnumWindows((h, l) => {
            uint pid; GetWindowThreadProcessId(h, out pid);
            if (pid != targetPid || !IsWindowVisible(h)) return true;
            var sb = new StringBuilder(512); GetWindowText(h, sb, sb.Capacity);
            var t = sb.ToString();
            if (t.StartsWith("FPS:")) { found = t; return false; }
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
'@

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$runDir = Join-Path $resultsDir "$Perfil`_$stamp"
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
$csv = Join-Path $runDir 'samples.csv'
'elapsed_s,fps,cpu_pct,free_ram_mb' | Set-Content -LiteralPath $csv -Encoding ASCII

Write-Host ''
Write-Host "Ve a la escena de prueba y pulsa F10 para medir $Seconds segundos (pitido al empezar y al terminar)." -ForegroundColor Cyan
Write-Host 'Cuando termine la medicion, cierra el juego normalmente.'

$culture = [System.Globalization.CultureInfo]::InvariantCulture
$measuring = $false
$done = $false
$start = $null
$lastCpu = ''
$lastRam = ''
$tick = 0
$keyWasDown = $false
$samples = New-Object System.Collections.Generic.List[double]

while (-not $process.HasExited) {
    $down = ([Gow3Win]::GetAsyncKeyState(0x79) -band 0x8000) -ne 0   # F10
    $pressed = $down -and -not $keyWasDown
    $keyWasDown = $down

    if ($pressed -and -not $measuring -and -not $done) {
        $measuring = $true
        $start = Get-Date
        [console]::Beep(880, 200)
        Write-Host 'Midiendo...' -ForegroundColor Yellow
    }

    if ($measuring) {
        $elapsed = ((Get-Date) - $start).TotalSeconds
        if ($tick % 2 -eq 0) {
            try {
                $lastCpu = (Get-CimInstance Win32_PerfFormattedData_PerfOS_Processor -Filter "Name='_Total'").PercentProcessorTime
                $lastRam = [int]((Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory / 1024)
            } catch { }
        }
        $title = [Gow3Win]::FpsTitle([uint32]$process.Id)
        if ($title -and $title -match '^FPS:\s*([0-9]+(?:[.,][0-9]+)?)') {
            $fps = [double]::Parse(($Matches[1] -replace ',', '.'), $culture)
            $samples.Add($fps)
            ('{0},{1},{2},{3}' -f $elapsed.ToString('0.0', $culture), $fps.ToString('0.00', $culture), $lastCpu, $lastRam) |
                Add-Content -LiteralPath $csv -Encoding ASCII
        }
        if ($elapsed -ge $Seconds -or ($pressed -and $elapsed -gt 2)) {
            $measuring = $false
            $done = $true
            [console]::Beep(660, 200); [console]::Beep(660, 200)
            Write-Host 'Medicion terminada. Ya puedes cerrar el juego.' -ForegroundColor Green
        }
        $tick++
    }
    Start-Sleep -Milliseconds 1000
}

# Resumen
$summaryFile = Join-Path $runDir 'summary.txt'
if ($samples.Count -gt 0) {
    $sorted = @($samples | Sort-Object)
    $avg = ($samples | Measure-Object -Average).Average
    $p5 = $sorted[[int][math]::Floor(($sorted.Count - 1) * 0.05)]
    $rows = Import-Csv -LiteralPath $csv
    $cpuVals = $rows | Where-Object { $_.cpu_pct -ne '' } | ForEach-Object { [double]$_.cpu_pct }
    $ramVals = $rows | Where-Object { $_.free_ram_mb -ne '' } | ForEach-Object { [double]$_.free_ram_mb }
    $cpuAvg = if ($cpuVals) { ($cpuVals | Measure-Object -Average).Average } else { 0 }
    $ramMin = if ($ramVals) { ($ramVals | Measure-Object -Minimum).Minimum } else { 0 }
    @(
        "profile=$Perfil"
        "date=$stamp"
        "samples=$($samples.Count)"
        ('fps_avg={0}' -f $avg.ToString('0.00', $culture))
        ('fps_p5={0}' -f $p5.ToString('0.00', $culture))
        ('fps_min={0}' -f ($sorted[0]).ToString('0.00', $culture))
        ('fps_max={0}' -f ($sorted[-1]).ToString('0.00', $culture))
        ('cpu_avg_pct={0}' -f ([double]$cpuAvg).ToString('0.0', $culture))
        ('free_ram_min_mb={0}' -f $ramMin)
    ) | Set-Content -LiteralPath $summaryFile -Encoding ASCII
    Write-Host ''
    Get-Content -LiteralPath $summaryFile | Write-Host
} else {
    "profile=$Perfil`r`ndate=$stamp`r`nsamples=0 (no se pulso F10 o no se leyeron FPS del titulo de la ventana)" |
        Set-Content -LiteralPath $summaryFile -Encoding ASCII
    Write-Host 'No se registraron muestras de FPS.' -ForegroundColor Yellow
}

Start-Sleep -Seconds 2
if (Test-Path -LiteralPath $rpcs3Log) { Copy-Item -LiteralPath $rpcs3Log -Destination (Join-Path $runDir 'RPCS3.log') }
Write-Host "Resultados guardados en $runDir"
Read-Host 'Pulsa Enter para cerrar' | Out-Null
