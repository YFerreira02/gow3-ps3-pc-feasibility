param(
    [Parameter(Mandatory)] [ValidatePattern('^[A-Za-z0-9_.-]+$')] [string] $ProcessName,
    [ValidateRange(10, 86400)] [int] $DurationSeconds = 120,
    [ValidateRange(1, 30)] [int] $SampleIntervalSeconds = 1,
    [string] $OutputPath
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
if (!$OutputPath) {
    $safeName = $ProcessName -replace '[^A-Za-z0-9_.-]', '_'
    $OutputPath = Join-Path $repoRoot "work\${safeName}-metrics.csv"
}
$parent = Split-Path -Parent $OutputPath
if ($parent -and !(Test-Path -LiteralPath $parent)) {
    New-Item -ItemType Directory -Path $parent -Force | Out-Null
}

if (-not ('Gow3ProcessProbe.MemoryStatus' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace Gow3ProcessProbe {
  [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Auto)]
  public class MemoryStatus {
    public uint Length = (uint)Marshal.SizeOf(typeof(MemoryStatus));
    public uint MemoryLoad;
    public ulong TotalPhysical;
    public ulong AvailablePhysical;
    public ulong TotalPageFile;
    public ulong AvailablePageFile;
    public ulong TotalVirtual;
    public ulong AvailableVirtual;
    public ulong AvailableExtendedVirtual;
    [DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
    public static extern bool GlobalMemoryStatusEx([In, Out] MemoryStatus buffer);
  }
}
'@
}

$logicalProcessors = [Environment]::ProcessorCount
$previous = @{}
$rows = [System.Collections.Generic.List[object]]::new()
$clock = [System.Diagnostics.Stopwatch]::StartNew()
$nextSample = 0.0

Write-Output "Sampling process '$ProcessName' for $DurationSeconds seconds."
Write-Output "CSV destination: $OutputPath"
Write-Output 'Start the workload manually; this recorder does not launch or control the process.'

while ($clock.Elapsed.TotalSeconds -lt $DurationSeconds) {
    $remaining = $nextSample - $clock.Elapsed.TotalSeconds
    if ($remaining -gt 0) { Start-Sleep -Milliseconds ([int]($remaining * 1000)) }
    $sampleTime = $clock.Elapsed.TotalSeconds
    $memory = [Gow3ProcessProbe.MemoryStatus]::new()
    if (-not [Gow3ProcessProbe.MemoryStatus]::GlobalMemoryStatusEx($memory)) {
        throw 'GlobalMemoryStatusEx failed.'
    }
    $availableGB = [Math]::Round($memory.AvailablePhysical / 1GB, 3)
    $totalGB = [Math]::Round($memory.TotalPhysical / 1GB, 3)
    $processes = @(Get-Process -Name $ProcessName -ErrorAction SilentlyContinue)
    $sampleWallTime = [DateTimeOffset]::Now.ToString('o')

    foreach ($process in $processes) {
        $cpuSeconds = [double]$process.TotalProcessorTime.TotalSeconds
        $cpuOneCorePct = $null
        $cpuMachinePct = $null
        if ($previous.ContainsKey($process.Id)) {
            $old = $previous[$process.Id]
            $elapsed = $sampleTime - $old.Elapsed
            if ($elapsed -gt 0) {
                $cpuOneCorePct = [Math]::Round((($cpuSeconds - $old.CpuSeconds) / $elapsed) * 100, 2)
                $cpuMachinePct = [Math]::Round($cpuOneCorePct / $logicalProcessors, 2)
            }
        }
        $previous[$process.Id] = @{ Elapsed = $sampleTime; CpuSeconds = $cpuSeconds }
        $rows.Add([pscustomobject]@{
            TimestampLocal = $sampleWallTime
            ElapsedSeconds = [Math]::Round($sampleTime, 2)
            ProcessName = $process.ProcessName
            ProcessId = $process.Id
            CpuPercentOfOneLogicalProcessor = $cpuOneCorePct
            CpuPercentOfMachineCapacity = $cpuMachinePct
            CpuTimeSeconds = [Math]::Round($cpuSeconds, 3)
            WorkingSetMB = [Math]::Round($process.WorkingSet64 / 1MB, 1)
            PrivateMemoryMB = [Math]::Round($process.PrivateMemorySize64 / 1MB, 1)
            ThreadCount = $process.Threads.Count
            HandleCount = $process.HandleCount
            SystemAvailableMemoryGB = $availableGB
            SystemTotalMemoryGB = $totalGB
        })
    }

    $nextSample += $SampleIntervalSeconds
    if ($processes.Count -eq 0) {
        Write-Output "[$sampleWallTime] '$ProcessName' is not running."
    } else {
        $cpuText = if ($null -eq $cpuMachinePct) { 'warming up' } else { "CPU machine capacity: $cpuMachinePct%" }
        Write-Output "[$sampleWallTime] sampled $($processes.Count) process instance(s); $cpuText; available RAM: ${availableGB} GB."
    }
}

if ($rows.Count -eq 0) {
    throw "No '$ProcessName' process was observed. Start the application during the capture window and retry."
}
$rows | Export-Csv -LiteralPath $OutputPath -NoTypeInformation -Encoding utf8
Write-Output "Saved $($rows.Count) process samples to $OutputPath"
Write-Output 'CPU columns report both percent of one logical processor and percent of total machine capacity; the first sample per process has no CPU delta.'
