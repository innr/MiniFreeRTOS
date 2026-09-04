#include "portmacro.h"

void vPortBuildInitialStackFrame(PortContext_t *context,
                                 uint32_t *stack,
                                 size_t stack_words,
                                 uintptr_t task_argument,
                                 uintptr_t task_entry,
                                 uintptr_t task_exit)
{
    uintptr_t stack_start;
    uintptr_t stack_end;
    uint32_t *hardware_frame;
    uint32_t *software_frame;

    if ((context == NULL) || (stack == NULL) ||
        (stack_words < MINI_CORTEX_M_STACK_FRAME_WORDS)) {
        if (context != NULL) {
            context->saved_psp = NULL;
        }
        return;
    }

    stack_start = (uintptr_t)stack;
    if (stack_words > ((UINTPTR_MAX - stack_start) / sizeof(uint32_t))) {
        context->saved_psp = NULL;
        return;
    }
    stack_end = stack_start + (stack_words * sizeof(uint32_t));
    stack_end &= ~(uintptr_t)0x7U;
    if ((stack_end < stack_start) ||
        ((stack_end - stack_start) <
         (MINI_CORTEX_M_STACK_FRAME_WORDS * sizeof(uint32_t)))) {
        context->saved_psp = NULL;
        return;
    }

    hardware_frame = (uint32_t *)stack_end - 8U;
    hardware_frame[0] = (uint32_t)task_argument; /* R0 */
    hardware_frame[1] = 0U;                      /* R1 */
    hardware_frame[2] = 0U;                      /* R2 */
    hardware_frame[3] = 0U;                      /* R3 */
    hardware_frame[4] = 0U;                      /* R12 */
    hardware_frame[5] = (uint32_t)task_exit | 1U; /* LR */
    hardware_frame[6] = (uint32_t)task_entry | 1U; /* PC */
    hardware_frame[7] = MINI_CORTEX_M_INITIAL_XPSR;

    software_frame = hardware_frame - 8U;
    for (size_t index = 0U; index < 8U; ++index) {
        software_frame[index] = 0U; /* R4-R11 */
    }
    context->saved_psp = software_frame;
}
