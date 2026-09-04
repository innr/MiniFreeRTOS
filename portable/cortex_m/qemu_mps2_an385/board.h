#ifndef MINI_CORTEX_M_QEMU_AN385_BOARD_H
#define MINI_CORTEX_M_QEMU_AN385_BOARD_H

#include <stddef.h>

void vBoardEarlyInit(void);
void vBoardConsoleWrite(const char *text);
void vBoardAssertFailed(const char *file, int line);
void vBoardExit(int status);

#endif
