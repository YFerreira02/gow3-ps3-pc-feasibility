/* Minimal Win32 stand-ins so the asset-free PPU test harness
 * (ppu_prototype_main.c) can run on Linux and in CI.
 * Used only by tools/run-ppu-tests.sh; the Windows build is unchanged. */
#include <stdlib.h>
#include <unistd.h>

typedef void *HANDLE;

HANDLE GetStdHandle(unsigned long handle)
{
    (void)handle;
    return (HANDLE)1;
}

int WriteFile(HANDLE handle, const void *buffer, unsigned long length,
              unsigned long *written, void *overlapped)
{
    (void)handle;
    (void)overlapped;
    const ssize_t result = write(1, buffer, length);
    if (written)
        *written = result < 0 ? 0ul : (unsigned long)result;
    return result >= 0;
}

void ExitProcess(unsigned int code)
{
    exit((int)code);
}

void entry(void);

int main(void)
{
    entry();
    return 0;
}
