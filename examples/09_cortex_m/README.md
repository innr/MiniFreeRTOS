# P9 Cortex-M QEMU lesson

This lesson runs the MiniFreeRTOS kernel on the Cortex-M3 MPS2 AN385 model.
The consumer blocks on a queue, the producer wakes it, and SysTick/PendSV
perform the task hand-off. The final line is emitted through the board's
semihosting console adapter.

## Prerequisites

Install and put these commands on `PATH`:

```text
arm-none-eabi-gcc
arm-none-eabi-objcopy
arm-none-eabi-size
arm-none-eabi-objdump
qemu-system-arm
```

The host `make test` target does not require these tools. An ARM target must
fail with a prerequisite error when one is absent; it must never run a host
binary as a substitute.

## Build and run

```sh
make -f Makefile.cortex_m all
make -f Makefile.cortex_m size
make -f Makefile.cortex_m inspect
make -f Makefile.cortex_m qemu
```

The QEMU command uses `-M mps2-an385 -cpu cortex-m3 -nographic` and enables
semihosting for the lesson console and exit status. Expected output is:

```text
MiniFreeRTOS P9 Cortex-M lesson
P9 PASS: SVC/PendSV/SysTick/queue/trace
```

This is a teaching port. It deliberately uses PRIMASK, does not save floating
point registers, and has no vendor HAL. QEMU's MPS2 model and real AN385 board
do not have identical peripheral behavior; see the QEMU MPS2 documentation for
the emulation differences.
