# P6 Design Delta: Deterministic Heap Allocators

Status: **Frozen for implementation**  
Target: POSIX PC teaching port  
Depends on: P0 task lifetime, P4 kernel-object allocation, and P5 mutex tests

## 1. Scope

P6 replaces the host `malloc`/`calloc` calls used by the kernel with a small,
selectable MiniFreeRTOS heap layer. The phase implements three allocator
policies that are intentionally easy to compare:

- `heap_1`: bump-pointer allocation; freeing never makes memory available
  again;
- `heap_2`: first-fit variable-size allocation with free blocks that may be
  returned but are never coalesced;
- `heap_4`: first-fit variable-size allocation with an address-ordered free
  list and adjacent-block coalescing.

Only one heap implementation is linked into an executable at a time. The
default build selects `heap_4`; `make HEAP_SCHEME=1 ...` and
`make HEAP_SCHEME=2 ...` select the other implementations.

P6 does not add a host-backed fallback allocator. The POSIX simulation still
uses a statically reserved byte array, so exhaustion and fragmentation are
deterministic and visible in a debugger.

## 2. Public API and configuration

Add `include/portable.h` with the common memory API:

```c
void *pvPortMalloc(size_t size);
void vPortFree(void *pointer);
size_t xPortGetFreeHeapSize(void);
```

Configuration additions in `include/FreeRTOSConfig.h` are guarded so a build
can override them with `-D` flags:

```c
#ifndef configTOTAL_HEAP_SIZE
#define configTOTAL_HEAP_SIZE (1024U * 1024U)
#endif

#ifndef configHEAP_SCHEME
#define configHEAP_SCHEME 4U
#endif

#ifndef portBYTE_ALIGNMENT
#define portBYTE_ALIGNMENT 8U
#endif
```

`HEAP_SCHEME` is the Makefile selection and is passed to the compiler as
`configHEAP_SCHEME`. A heap source checks that it is the selected source; this
prevents accidentally linking a file for the wrong policy. `configTOTAL_HEAP_SIZE`
is the size of the static backing array and must be large enough for the
metadata and the configured application tasks.

### Common API contract

- `pvPortMalloc(0)` returns `NULL`.
- Requests are rounded up to `portBYTE_ALIGNMENT`; overflow or exhaustion
  returns `NULL`.
- Successful results are aligned to `portBYTE_ALIGNMENT`.
- `vPortFree(NULL)` is a no-op.
- `xPortGetFreeHeapSize()` reports the sum of currently free heap regions,
  including allocator metadata. It is a capacity indicator, not a promise that
  one contiguous request of that size will succeed.
- Allocator metadata changes are protected by the existing nested critical
  section. No allocator function calls POSIX or libc allocation routines.
- There is no `realloc`, `calloc`, heap deletion, or per-task heap in P6.

For `heap_1`, `vPortFree` is deliberately a no-op, matching the teaching point
that an allocate-only heap trades reclamation for simple deterministic behavior.
For `heap_2` and `heap_4`, a pointer not returned by the active heap, or a
double free, calls `configASSERT`.

## 3. Allocator data structures

Each implementation owns a statically aligned array:

```c
_Alignas(portBYTE_ALIGNMENT) static uint8_t ucHeap[configTOTAL_HEAP_SIZE];
```

The usable heap size is rounded down to the alignment boundary. The allocator
must assert that the resulting array is large enough for one aligned request.

### heap_1

```text
ucHeap[]
next_free       /* offset of the next bump allocation */
free_bytes
```

The allocator aligns `next_free`, checks the remaining bytes, advances it, and
never creates a free list. `xPortGetFreeHeapSize()` is the remaining tail.

### heap_2 and heap_4

Both variable-size heaps use an aligned header immediately before each returned
payload:

```c
typedef struct HeapBlock {
    size_t size;              /* header plus payload, aligned */
    struct HeapBlock *next;   /* meaningful while the block is free */
    uint32_t magic;           /* allocated/free validation marker */
    size_t alignment_padding; /* keeps the header aligned on 32-bit ports */
} HeapBlock_t;
```

`size` includes the header. A split is made only when the remainder can hold a
header and at least one aligned payload unit. Allocated blocks have
`next == NULL`; free blocks are linked in address order. The free list itself
is stored inside free blocks, so no host allocation is needed.

The padding field is not part of the allocator state; it makes the header size
an alignment multiple when `size_t` and pointers are 32-bit (for example, the
Cortex-M3 build uses 8-byte payload alignment). This keeps the payload aligned
without weakening the compile-time layout check.

`heap_2` removes a fitting free block and optionally splits it. On free it
inserts the block back in address order without merging neighboring blocks.

`heap_4` uses the same allocation path, then merges a newly freed block with an
immediately adjacent successor and/or predecessor. Merging changes list links
and block sizes but does not double-count free bytes.

The high bit of `magic` is not used as a size flag; size and validation remain
separate so the layout is straightforward to inspect in a debugger. Each heap
source has compile-time assertions that the header size and heap base satisfy
the selected alignment.

## 4. Kernel integration

`kernel/tasks.c` and `kernel/queue.c` include `portable.h` and replace direct
host allocations as follows:

| Existing allocation | P6 behavior |
|---|---|
| task TCB | `pvPortMalloc(sizeof(TCB_t))`, then `memset` |
| task stack | `pvPortMalloc(stack_depth)` |
| idle TCB and stack | selected heap, same as an application task |
| queue/semaphore/mutex object | selected heap, then `memset` |
| queue byte storage | selected heap |
| partial-create rollback | `vPortFree` (a no-op for `heap_1`) |

The project has no public task/queue deletion API yet, so live task stacks and
IPC objects remain allocated for the lifetime of their teaching process. P6
does not silently reclaim a task's own stack when it returns. This keeps task
context lifetime separate from allocator policy and avoids freeing a stack
while it is still executing.

After this migration, no direct `malloc`, `calloc`, or `free` call remains in
`kernel/`; only the selected heap implementation owns the backing storage.
Public headers contain no POSIX types.

## 5. Critical-section interaction

`pvPortMalloc`, `vPortFree`, and `xPortGetFreeHeapSize` enter and leave the
existing nested critical section around heap metadata access. This permits a
task to allocate while the POSIX tick is active without allowing a signal to
observe a partially linked free list. The functions may also be called before
the scheduler starts; the POSIX port's zero-initialized signal set and nested
critical-depth handling support that existing setup path.

The heap layer never yields and never performs a context switch. Kernel callers
remain responsible for any higher-level scheduling effects after an allocation
failure.

## 6. Error policy

- Invalid configuration (`portBYTE_ALIGNMENT == 0`, non-power-of-two alignment,
  or a heap too small for metadata) calls `configASSERT` during compilation or
  first use.
- Zero-size, overflowed, over-aligned, or exhausted requests return `NULL`.
- `vPortFree(NULL)` succeeds as a no-op.
- `heap_1` ignores non-NULL frees because it cannot reclaim storage.
- `heap_2` and `heap_4` assert on a pointer outside the active heap, a pointer
  with a bad header marker, or a double free.
- No errno values or host allocator fallback are introduced.

## 7. Directory changes

```text
include/FreeRTOSConfig.h       # heap size, alignment, selected scheme
include/portable.h             # pvPortMalloc/vPortFree/statistics API
kernel/heap_1.c                # allocate-only policy
kernel/heap_2.c                # free-list policy without coalescing
kernel/heap_4.c                # ordered free-list policy with coalescing
kernel/heap_internal.h         # shared alignment and range helpers
kernel/tasks.c                 # TCB/stack/idle allocation migration
kernel/queue.c                 # queue/semaphore/mutex allocation migration
tests/test_heap_support.c      # standalone critical/assert stubs
tests/test_p6_heap.c           # same behavior test compiled three ways
examples/06_heap/main.c        # observable allocation lesson
docs/P6_DESIGN.md
docs/ROADMAP.md
README.md
Makefile                      # selected heap and three-variant tests
```

The POSIX context-switch and tick code remain unchanged.

## 8. Verification plan

`tests/test_p6_heap.c` is compiled once against each heap source with a small
`configTOTAL_HEAP_SIZE` so fragmentation behavior is deterministic. It must
cover:

1. zero-size and overflow requests return `NULL`;
2. successful allocations are aligned and writable without overlap;
3. allocation eventually reports exhaustion;
4. `vPortFree(NULL)` is harmless;
5. `heap_1` does not reuse a freed region;
6. `heap_2` reuses a freed block when it fits but cannot satisfy a request that
   requires coalescing adjacent blocks;
7. `heap_4` coalesces adjacent freed blocks and satisfies that larger request;
8. free-byte statistics move in the expected direction.

The normal P0-P5 regression binaries and examples use the selected default
`heap_4` backend. The build must also pass with `HEAP_SCHEME=1` and
`HEAP_SCHEME=2`, proving that kernel allocation does not depend on heap_4-only
behavior. All variants are run with:

```sh
make clean && make test
make clean && make HEAP_SCHEME=1 test
make clean && make HEAP_SCHEME=2 test
make clean && make test \
  CFLAGS='-std=c11 -O0 -g -Wall -Wextra -Wpedantic -Werror \
          -fsanitize=undefined -fno-sanitize-recover=undefined'
```

## 9. Acceptance criteria

1. `pvPortMalloc`, `vPortFree`, and `xPortGetFreeHeapSize` are documented and
   callable through a public header.
2. Exactly one of `heap_1`, `heap_2`, or `heap_4` is linked per executable.
3. Kernel TCBs, stacks, queues, semaphores, and mutexes use the selected heap.
4. No direct libc allocation remains in `kernel/`.
5. The three allocator tests demonstrate their distinct free/reuse/coalescing
   behavior.
6. P0-P5 tests and examples remain green for all three selected schemes.
7. Default warnings-as-errors and UBSan builds pass.
8. `examples/06_heap` prints aligned addresses and free-byte changes so a
   learner can observe the allocator policy without reading every line first.

Implementation starts only after this document is reviewed and marked
**Frozen for implementation**.
