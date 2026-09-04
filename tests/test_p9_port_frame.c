#include "FreeRTOS.h"
#include "portmacro.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void vCheckInitialFrame(void)
{
    uint32_t stack[32];
    PortContext_t context = {0};

    vPortBuildInitialStackFrame(&context, stack, 32U,
                                (uintptr_t)0x12345678UL,
                                (uintptr_t)0x08000100UL,
                                (uintptr_t)0x08000200UL);
    assert(context.saved_psp == &stack[16]);
    for (unsigned index = 0U; index < 8U; ++index) {
        assert(context.saved_psp[index] == 0U);
    }
    assert(context.saved_psp[8] == 0x12345678U);
    assert(context.saved_psp[9] == 0U);
    assert(context.saved_psp[10] == 0U);
    assert(context.saved_psp[11] == 0U);
    assert(context.saved_psp[12] == 0U);
    assert(context.saved_psp[13] == 0x08000201U);
    assert(context.saved_psp[14] == 0x08000101U);
    assert(context.saved_psp[15] == MINI_CORTEX_M_INITIAL_XPSR);
    assert(((uintptr_t)context.saved_psp % 8U) == 0U);
}

static void vCheckInvalidStack(void)
{
    uint32_t stack[15];
    PortContext_t context = {(uint32_t *)(uintptr_t)1U};

    vPortBuildInitialStackFrame(&context, stack, 15U, 0U, 0U, 0U);
    assert(context.saved_psp == NULL);
}

int main(void)
{
    vCheckInitialFrame();
    vCheckInvalidStack();
    puts("P9 Cortex-M initial stack-frame tests passed");
    return 0;
}
