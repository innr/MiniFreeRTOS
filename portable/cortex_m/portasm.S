.syntax unified
.cpu cortex-m3
.thumb

.text
.align 2
.thumb_func
.global SVC_Handler
.type SVC_Handler, %function
SVC_Handler:
    bl      pxPortGetCurrentTaskStack
    ldmia   r0!, {r4-r11}
    msr     psp, r0
    movs    r0, #2                  /* thread mode uses PSP, privileged */
    msr     control, r0
    isb
    cpsie   i
    ldr     lr, =0xFFFFFFFD        /* exception return, use PSP */
    bx      lr
.size SVC_Handler, .-SVC_Handler

.align 2
.thumb_func
.global PendSV_Handler
.type PendSV_Handler, %function
PendSV_Handler:
    mrs     r0, psp
    stmdb   r0!, {r4-r11}
    mov     r4, lr                  /* preserve EXC_RETURN across C calls */
    bl      vPortSaveCurrentTaskStack
    bl      vTaskSwitchContext
    bl      pxPortGetCurrentTaskStack
    mov     r12, r4
    ldmia   r0!, {r4-r11}
    msr     psp, r0
    mov     lr, r12
    bx      lr
.size PendSV_Handler, .-PendSV_Handler

.align 2
.thumb_func
.global SysTick_Handler
.type SysTick_Handler, %function
SysTick_Handler:
    push    {r0, lr}
    bl      vTaskTickISR
    pop     {r0, lr}
    bx      lr
.size SysTick_Handler, .-SysTick_Handler
