#include "board.h"
#include <stdint.h>

enum {
    eSemihostWrite0 = 0x04U,
    eSemihostWrite = 0x05U,
    eSemihostExit = 0x18U,
    eSemihostExitOk = 0x20026U,
    eSemihostExitError = 0x20023U
};

static uintptr_t prvSemihostCall(uint32_t operation, uintptr_t argument)
{
    register uintptr_t r0 __asm("r0") = (uintptr_t)operation;
    register uintptr_t r1 __asm("r1") = argument;

    __asm volatile("bkpt 0xAB"
                   : "+r"(r0)
                   : "r"(r1)
                   : "memory");
    return r0;
}

void vBoardEarlyInit(void)
{
}

void vBoardConsoleWrite(const char *text)
{
    if (text != NULL) {
        (void)prvSemihostCall(eSemihostWrite0,
                              (uintptr_t)text);
    }
}

void vBoardAssertFailed(const char *file, int line)
{
    (void)file;
    (void)line;
    vBoardConsoleWrite("MiniFreeRTOS assertion failed\n");
}

void vBoardExit(int status)
{
    uintptr_t reason = (status == 0) ? eSemihostExitOk : eSemihostExitError;

    (void)prvSemihostCall(eSemihostExit, reason);
    for (;;) {
        __asm volatile("wfi" ::: "memory");
    }
}

int _write(int file_descriptor, const void *buffer, size_t length)
{
    uint32_t block[3];
    uintptr_t not_written;

    block[0] = (uint32_t)file_descriptor;
    block[1] = (uint32_t)(uintptr_t)buffer;
    block[2] = (uint32_t)length;
    not_written = prvSemihostCall(eSemihostWrite, (uintptr_t)block);
    if (not_written > length) {
        return -1;
    }
    return (int)(length - (size_t)not_written);
}

void _exit(int status)
{
    vBoardExit(status);
}
