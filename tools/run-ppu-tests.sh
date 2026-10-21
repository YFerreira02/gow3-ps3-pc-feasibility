#!/usr/bin/env bash
# Build and run the asset-free PPU synthetic checks on Linux (also used by CI).
# Also verifies that the core and harness objects, compiled for Windows,
# do not depend on C runtime helpers (memset, memcpy, stack probes) that the
# -nostdlib Windows build in Build-PpuPrototype.ps1 cannot resolve.
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
src="$repo_root/src/Gow3Runtime/ppu"
out="${PPU_TEST_OUT:-$repo_root/work/linux}"
mkdir -p "$out"

cc="${CC:-gcc}"
"$cc" -O2 -std=c11 -Wall -Wextra -Wno-unused-function \
    '-D__declspec(x)=' -D__stdcall= \
    "$src/ppu_core.c" "$src/ppu_prototype_main.c" "$src/ppu_elf.c" \
    "$src/ppu_iso.c" "$src/ppu_self.c" "$repo_root/tools/linux_test_shim.c" \
    -o "$out/ppu_tests"

"$out/ppu_tests" | tee "$out/ppu_tests.log"
passed=$(grep -c '^PASS' "$out/ppu_tests.log" || true)
failed=$(grep -c '^FAIL' "$out/ppu_tests.log" || true)
echo "PPU synthetic checks: $passed passed, $failed failed"
[ "$failed" -eq 0 ]

if command -v clang >/dev/null 2>&1; then
    status=0
    for unit in ppu_core ppu_prototype_main ppu_elf ppu_iso ppu_self; do
        clang --target=x86_64-windows-gnu -O2 -std=c11 -c "$src/$unit.c" -o "$out/$unit.obj" 2>/dev/null
        extra=$(nm "$out/$unit.obj" | awk '$1 == "U" {print $2}' |
                grep -Ev '^(ppu_|(__imp_)?(ExitProcess|GetStdHandle|WriteFile)$)' || true)
        if [ -n "$extra" ]; then
            echo "Unresolvable symbols for the -nostdlib Windows build in $unit: $extra"
            status=1
        fi
    done
    [ "$status" -eq 0 ] && echo "Windows -nostdlib symbol check: OK"
    exit "$status"
fi
