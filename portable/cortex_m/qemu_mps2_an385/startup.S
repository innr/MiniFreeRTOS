.syntax unified
.cpu cortex-m3
.thumb

.section .isr_vector, "a", %progbits
.align 2
.global g_pfnVectors
.type g_pfnVectors, %object
g_pfnVectors:
    .word _estack
    .word Reset_Handler
    .word NMI_Handler
    .word HardFault_Handler
    .word MemManage_Handler
    .word BusFault_Handler
    .word UsageFault_Handler
    .word 0
    .word 0
    .word 0
    .word 0
    .word SVC_Handler
    .word DebugMon_Handler
    .word 0
    .word PendSV_Handler
    .word SysTick_Handler
    .rept 45
    .word Default_Handler
    .endr
.size g_pfnVectors, .-g_pfnVectors

.section .text.Reset_Handler, "ax", %progbits
.align 2
.thumb_func
.global Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
    ldr     r0, =_estack
    mov     sp, r0

    ldr     r0, =_sidata
    ldr     r1, =_sdata
    ldr     r2, =_edata
1:
    cmp     r1, r2
    bcc     2f
    b       3f
2:
    ldr     r3, [r0], #4
    str     r3, [r1], #4
    b       1b
3:
    ldr     r1, =_sbss
    ldr     r2, =_ebss
    movs    r3, #0
4:
    cmp     r1, r2
    bcc     5f
    b       6f
5:
    str     r3, [r1], #4
    adds    r1, #4
    b       4b
6:
    bl      vBoardEarlyInit
    bl      main
    b       Default_Handler
.size Reset_Handler, .-Reset_Handler

.section .text.Default_Handler, "ax", %progbits
.align 2
.thumb_func
.global Default_Handler
.type Default_Handler, %function
Default_Handler:
    ldr     r0, =0
    movs    r1, #0
    bl      vBoardAssertFailed
7:
    wfi
    b       7b
.size Default_Handler, .-Default_Handler

.weak NMI_Handler
.thumb_set NMI_Handler, Default_Handler
.weak HardFault_Handler
.thumb_set HardFault_Handler, Default_Handler
.weak MemManage_Handler
.thumb_set MemManage_Handler, Default_Handler
.weak BusFault_Handler
.thumb_set BusFault_Handler, Default_Handler
.weak UsageFault_Handler
.thumb_set UsageFault_Handler, Default_Handler
.weak DebugMon_Handler
.thumb_set DebugMon_Handler, Default_Handler
