param(
    [Parameter(Mandatory = $true)]
    [string]$Executable
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$fixturePath = Join-Path $repoRoot 'work\PpuHost\synthetic_syscall.elf'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $fixturePath) | Out-Null
$elf = [byte[]]::new(124)

function Set-BeValue([byte[]]$Bytes, [int]$Offset, [uint64]$Value, [int]$Width) {
    for ($index = 0; $index -lt $Width; $index++) {
        $shift = 8 * ($Width - 1 - $index)
        $Bytes[$Offset + $index] = [byte](($Value -shr $shift) -band 0xff)
    }
}

$elf[0] = 0x7f; $elf[1] = 0x45; $elf[2] = 0x4c; $elf[3] = 0x46
$elf[4] = 2; $elf[5] = 2; $elf[6] = 1
Set-BeValue $elf 16 2 2       # ET_EXEC
Set-BeValue $elf 18 21 2      # EM_PPC64
Set-BeValue $elf 20 1 4
Set-BeValue $elf 24 0x10000 8
Set-BeValue $elf 32 64 8      # program-header offset
Set-BeValue $elf 52 64 2      # ELF header size
Set-BeValue $elf 54 56 2      # program-header size
Set-BeValue $elf 56 1 2       # one program header
Set-BeValue $elf 64 1 4       # PT_LOAD
Set-BeValue $elf 68 5 4       # PF_R | PF_X
Set-BeValue $elf 72 120 8     # file offset
Set-BeValue $elf 80 0x10000 8 # virtual address
Set-BeValue $elf 88 0x10000 8 # physical address
Set-BeValue $elf 96 4 8       # file size
Set-BeValue $elf 104 4 8      # memory size
Set-BeValue $elf 112 4 8      # alignment
Set-BeValue $elf 120 0x44000002 4 # sc: controlled host-dispatcher handoff
[IO.File]::WriteAllBytes($fixturePath, $elf)

$output = & $Executable $fixturePath 1000 2>&1
$exitCode = $LASTEXITCODE
$output | ForEach-Object { Write-Host $_ }
if ($exitCode -ne 0 -or ($output -join "`n") -notmatch 'status 4 after 1 instructions') {
    throw "Synthetic ELF host run failed (exit code $exitCode)."
}
Write-Host 'Synthetic ELF was loaded and executed to the syscall handoff.'
