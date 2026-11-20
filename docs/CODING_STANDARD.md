# JetOS coding standard

This document states how the code of JetOS and of its hypervisor is
written. It was derived from the code itself (the conventions it follows,
measured across the tree) and from the rules that `README.md`, the system
requirements (`docs/SRQs`) and the build already impose. Where the code is
not yet consistent, it says which form the newest code uses; new and
rewritten code uses that form.

Contents:

1. [Scope](#1-scope)
2. [Language and build](#2-language-and-build)
3. [Structure and dependencies](#3-structure-and-dependencies)
4. [Files](#4-files)
5. [Licence and change records](#5-licence-and-change-records)
6. [Naming](#6-naming)
7. [Formatting](#7-formatting)
8. [Types, constants and declarations](#8-types-constants-and-declarations)
9. [Expressions and control flow](#9-expressions-and-control-flow)
10. [Errors, assertions and memory](#10-errors-assertions-and-memory)
11. [Hardware, concurrency and the preprocessor](#11-hardware-concurrency-and-the-preprocessor)
12. [MISRA C:2025 deviations](#12-misra-c2025-deviations)
13. [Documentation and traceability](#13-documentation-and-traceability)
14. [Assembly and linker scripts](#14-assembly-and-linker-scripts)
15. [Python and SCons](#15-python-and-scons)
16. [Tests and examples](#16-tests-and-examples)
17. [Commits](#17-commits)
18. [Legacy code](#18-legacy-code)
19. [Review checklist](#19-review-checklist)

## 1. Scope

The standard applies to:

- the C and assembly code of `kernel/` (with `kernel/csps` and
  `kernel/bsps`), `common/`, `include/`, `hypervisor/`, `libpok/` and
  `syspart/`;
- the C templates that generate code (`misc/templates/*.tpl`,
  `hypervisor/templates/*.tpl`), which follow it in the code they
  generate;
- the build and configuration tools (Python, SCons): section 15;
- the tests and examples: section 16.

Third-party code imported as it is, `libpok/libm` and `libpok/protocols`,
keeps its upstream layout. It still gets the Doxygen headers (section 13)
and the MISRA deviation comments (section 12) that the README asks for.
The ported benchmarks in `benchmark/` are out of scope.

Words used:

- **must**: a rule that the system requirements, MISRA C:2025, the build
  (`-Wall -Wextra`) or the documentation checks (`make docs`, `make
  docs-trace`) impose. A change that breaks it is not accepted.
- **should**: the convention of the newest code (`hypervisor/`, `common/`,
  `include/` and the rewritten parts of `kernel/`). New and rewritten code
  follows it; old code is brought to it when it is rewritten (section 18).

`README.md` stays the reference for the Doxygen headers, the requirement
identifiers and the code layout ("Function documentation", "Requirements
traceability", "Code layout", "Types", "Macros"). This document summarizes
those rules and refers to them; it doesn't replace them.

## 2. Language and build

- The code **must** be GNU C99 (`-std=gnu99`), freestanding
  (`-ffreestanding -nostdlib -nostdinc`): neither the kernel nor the
  hypervisor is linked with a C library. What they need is in `common/`
  (`memcpy()`, `memset()`, the printf formatter `jet_format()`) or in the
  program itself.
- The code **must** build without warnings with `-Wall -Wextra
  -Wuninitialized`, for both boards (`make`, `make BSP=arm64-qemu`,
  `make hv`, `make BSP=arm64-qemu hv`).
- The kernel and the hypervisor **must not** use floating point
  (SRQ_JET_GEN_002): they are built with `-mgeneral-regs-only`, and the
  FP state belongs to the partitions. The partitions may use floating
  point.
- The code **must** be written for 64-bit processors (SRQ_JET_GEN_003):
  addresses and sizes are held in `uintptr_t`, `size_t` or `uint64_t`,
  never in a 32-bit type.
- A service **must** behave the same way on x86_64 and on arm64
  (SRQ_JET_GEN_001, SRQ_HV_GEN_001). A change is built and tested on both
  (`make run` and `make BSP=arm64-qemu run`; `make hv-test` and
  `make BSP=arm64-qemu hv-test` for the hypervisor).
- The kernel **must** follow the constraints of its flags: no red zone on
  x86_64 (`-mno-red-zone`: interrupts use the kernel stack), no unaligned
  access on arm64 (`-mstrict-align`: early code runs with the MMU off).
- C11 keywords (`_Noreturn`, `_Static_assert`) and GCC extensions (inline
  assembly, `__attribute__`, `typeof`, statement expressions, register
  variables) are allowed only where standard C99 can't do the job, isolated
  in a macro or a small `static inline` function, with a MISRA Dir 1.2
  (and Dir 4.3 for assembly) deviation comment (section 12).

## 3. Structure and dependencies

- The code **must** respect the split of each program into its parts:

  | Part | Kernel | Hypervisor | Holds |
  |------|--------|------------|-------|
  | core | `kernel/core` | `hypervisor/core` | Code independent of the processor and of the board. |
  | CSP | `kernel/csps/<arch>` | `hypervisor/csps/<arch>` | The processor (x86_64, arm64). |
  | BSP | `kernel/bsps/<bsp>` | `hypervisor/bsps/<bsp>` | The devices of the board (x86_64-qemu, arm64-qemu). |

- The core reaches the processor and the board only through interfaces:
  in the kernel, the `ja_*` functions declared in `kernel/include/asp/`
  and the `<arch/...>` and `<bsp/...>` headers that each CSP provides; in
  the hypervisor, the `hv_arch_*` functions declared in
  `hypervisor/core/include/hv/hv.h`. Each architecture defines the same
  function under the same name (`ja_space_switch()` in
  `kernel/csps/x86_64/space.c` and `kernel/csps/arm64/space.c`).
- Code that is the same on both architectures **should** be in the core,
  not copied into each CSP.
- The hypervisor **must** include nothing from `kernel/` (SRQ_HV_GEN_006),
  and **must** consist only of code under the BSD-2-Clause or POK licence
  (SRQ_HV_GEN_003). The code the two programs share is therefore in
  `common/` (the library) and `include/` (headers: `include/common/`, the
  processor's registers and instructions in `include/csps/<arch>/cpu/`,
  the devices' registers in `include/bsps/<bsp>/dev/`), and **must** be
  under one of those two licences.
- The kernel's optional features are compiled with `#ifdef
  POK_NEEDS_<FEATURE>` (`POK_NEEDS_GDB`, `POK_NEEDS_PORTS_QUEUEING`, ...),
  which the generated `config.h` defines for each project.

## 4. Files

### 4.1 Names

- File and directory names **should** be lower case with underscores
  (`delayed_event.c`, `sched_arinc.c`, `partition_arinc_types.h`); board
  names have a hyphen (`x86_64-qemu`).
- A module's private header **should** be named after it with
  `_internal` (`kernel/core/thread_internal.h`).
- Assembly files are `.S` (passed through the C preprocessor), linker
  scripts `.lds`, templates `.tpl`, generated syspart sources
  `<COMPONENT>_gen.c` and `<COMPONENT>_gen.h`.

### 4.2 C files

A C file **must** hold, in this order:

1. the licence and change records (section 5);
2. a `/** @file ... */` comment that says what the file holds (Doxygen
   documents the functions of a file only if the file is documented);
3. the `#include` lines;
4. the deviations that apply to the whole file (`MISRA C:2025 ... (this
   file)`);
5. the declarations of its `static` functions, if any are needed, and its
   variable definitions (global and `static`);
6. its function definitions, each right after its Doxygen header.

A C file **must not** define macros or types: they go in a header of its
module (README, "Code layout"). The only exception is a macro whose name
would clash in the other files that include the header.

```c
/*
 * <licence, section 5>
 */

/**
 * @file
 * Virtual machines: their RAM, a contiguous host region after the
 * hypervisor, one VM after the other in configuration order.
 */

#include <hv/hv.h>

hv_vm_t hv_vms[HV_MAX_VMS];

/**
 * @funcname `vm_print`
 * ...
 */
void vm_print(const hv_vm_t *vm, const char *message)
{
    hv_printf("[hv] %s: %s\n", vm->config->name, message);
}
```

### 4.3 Headers

A header **must** hold, in this order:

1. the licence and change records;
2. optionally, a plain comment (`/*`, not `/**`) that says what the
   header is for: headers have no `@file` comment, so that Doxygen
   doesn't document them as files;
3. the include guard;
4. its `#include` lines;
5. its types, macros, `extern` variables, `static inline` functions and
   function prototypes (README, "Code layout").

Rules for the parts:

- The guard **must** be `#ifndef` / `#define` / `#endif` (no `#pragma
  once`), named `<PREFIX>_<PATH>_H`, where the prefix is that of the
  component (`HV_` in the hypervisor, `JET_` in `include/` and in new
  kernel headers; `POK_` and `LIBJET_` remain in old ones). It **must
  not** start with an underscore (a reserved identifier, MISRA Rule 21.2).
  The `#endif` **should** repeat the guard in a comment:
  `#endif /* JET_COMMON_STRING_H */`.
- Every type and every macro (except the guard) **must** have a Doxygen
  header (section 13).
- The prototypes **must** be enclosed in `/** \cond */` ... `/** \endcond
  */`, one pair around each run of declarations, so that a function is
  documented only at its definition.
- A `static inline` function **must** have a plain comment, not a Doxygen
  header.

```c
#ifndef POK_TIME_H
#define POK_TIME_H

#include <types.h>
#include <errno.h>

/**
 * @macroname `POK_TIMER_FREQUENCY`
 *
 * @description
 * The rate of the clock in POK, in Hz.
 */
#define POK_TIMER_FREQUENCY 10000U

/** \cond */
pok_ret_e pok_clock_gettime(clockid_t clk_id, pok_time_t* JET_USER val);
pok_ret_e jet_time(time_t* JET_USER val);
/** \endcond */

#endif /* POK_TIME_H */
```

### 4.4 Includes

- Headers on the include path **must** be included with `<...>`; `"..."`
  is only for a private header in the same directory (`"thread_internal.h"`,
  `"gdt.h"`, `"interrupt.h"`).
- A kernel C file **should** include `<config.h>` first (it defines the
  `POK_NEEDS_*` switches that the other headers test).
- Includes **should** be grouped (C library and basic types, then the
  component's headers, then the interfaces to the architecture), the
  groups separated by a blank line. A comment after an include may say why
  it is needed:

  ```c
  #include <config.h>

  #include <types.h>
  #include <errno.h>

  #include <core/time.h>
  #include <core/uaccess.h>
  #include <core/sched.h>

  #include <asp/entries.h> /* jet_on_tick() declaration. */
  ```

## 5. Licence and change records

- Every source file (C, header, assembly, linker script, template, Python,
  SCons, YAML, XML) **must** start with a comment that gives its copyright
  and its licence.
- A file **must** keep the licence it has. A file adapted from another
  keeps the licence of the original and says where it comes from
  ("adapted from ..."). The kernel's files are under GPLv3 (from ISPRAS) or
  the POK licence; those of `common/`, `include/` and `hypervisor/`
  **must** be under BSD-2-Clause or the POK licence (section 3).
- A new file written from scratch **should** take the BSD-2-Clause header
  of `hypervisor/core/vm.c`, as the arm64 CSP and the hypervisor do: it
  can be shared by both programs.
- Each change to a file **must** add a record to the end of its licence
  comment, a paragraph that says what changed and why:

  ```c
   * Modified by <Name>, 2026-11-14: the enum types are named with _e:
   * pok_ret_e (was pok_ret_t).
  ```

  The date is ISO 8601 (`YYYY-MM-DD`); the text is a sentence, or a few
  clauses separated by semicolons. The records are in date order; a blank
  comment line separates them (the old POK headers have them one after the
  other).
- A file under the POK licence ends its records with:

  ```c
   * The modifications are distributed under the POK licence
   * (BSD-2-Clause, see POK-LICENSE).
  ```

## 6. Naming

All identifiers are lower case with underscores, except macros,
enumerators and the ARINC 653 names, which are upper case.

| What | Convention | Examples |
|------|------------|----------|
| Kernel functions | `jet_` (new); `pok_` (the POK API, kept as it is) | `jet_time()`, `jet_mem_alloc()`, `pok_clock_gettime()` |
| Kernel functions of a CSP or BSP called by the core | `ja_` | `ja_space_switch()`, `ja_system_time()`, `ja_context_switch` |
| Hypervisor functions | `hv_`; `hv_arch_` for those each architecture defines | `hv_printf()`, `hv_fatal()`, `hv_arch_vcpu_run()` |
| Functions of a driver or module | the module's name | `gic_read32()`, `uart_putc()`, `gdb_*()`, `vpit_*()` |
| `static` functions | no prefix needed; a verb and its object | `align_up()`, `put_number()`, `find_semaphore()` |
| APEX services (`libpok/arinc653`) | the ARINC 653 names, upper case, parameters too | `CREATE_SEMAPHORE(SEMAPHORE_NAME, ..., RETURN_CODE)` |
| Variables | lower case; globals with the module's prefix; `nb_` for a count | `hv_vms`, `hv_nb_cpus`, `k_val` |
| Kernel copy of a user pointer | `k_` and the user name | `val` and `k_val` |
| Struct, union and other typedefs | `_t` | `hv_vm_t`, `jet_space_layout_t`, `output_t` |
| Enum typedefs | `_e` | `pok_ret_e`, `hv_action_e`, `gdb_step_state_e` |
| Function pointer typedefs | `_fn_t` | `format_emit_fn_t`, `pok_part_start_fn_t` |
| Macros and enumerators | upper case, with the component's prefix where it is shared | `POK_TIMER_FREQUENCY`, `HV_PAGE_SIZE`, `HV_CONTINUE`, `POK_ERRNO_OK` |
| Function-like macros that stand for a function | may be lower case, named like a function | `jet_system_time()`, `jet_user_to_kernel_typed()`, `barrier()` |
| Configuration switches | `POK_NEEDS_<FEATURE>` | `POK_NEEDS_GDB` |
| Requirement identifiers | README, "Requirements traceability" | `HLR_TIME_POK_CLOCK_GETTIME_01` |

Other rules:

- Identifiers **must not** start with an underscore or contain `__`
  (reserved, MISRA Rule 21.2), except the linker's symbols (`__hv_end`,
  `__bss_start`) and the standard names that the programs define
  (`memcpy()`, `memset()`), which have a deviation comment.
- A name **should** say what the object is, in words (`first_cpu`,
  `ram_base`, `memory_size`); the usual abbreviations of the domain are
  fine (`cpu`, `vm`, `vcpu`, `irq`, `pa`, `cfg`).
- A function's parameters **must** have the same names in its declaration
  and in each of its definitions (MISRA Rule 8.3).
- Don't rename a function or a file without need: the HLR and LLR
  identifiers are made from those names (section 13).

## 7. Formatting

The layout of `hypervisor/`, `common/` and `include/` is the reference.

### 7.1 Indentation and lines

- Indentation **should** be 4 spaces per level; tabs **should not** be
  used in C. (Makefiles need tabs, and the linker scripts are indented
  with them; for assembly see section 14.)
- Lines **should** be at most 80 columns. In a Doxygen header this is a
  **must** (README); the only line allowed to be longer is a Markdown table
  row of a header.
- Lines **should** have no trailing whitespace, and a file ends with a
  newline.
- One statement per line. A blank line **should** separate the local
  declarations of a block from its statements, and the logical steps of a
  function from each other. Two blank lines in a row **should not** be
  used in C.

### 7.2 Braces

- A function's opening brace **should** be on a line of its own; the
  braces of everything else (`if`, `else`, loops, `switch`, `struct`,
  `enum`, initializers) **should** be at the end of the line that opens
  them:

  ```c
  static void put_repeated(const format_out_t *out, char c, uint32_t count)
  {
      for (uint32_t i = 0U; i < count; i++) {
          put(out, c);
      }
  }
  ```

- `else` **should** be on the line of the closing brace: `} else {`,
  `} else if (...) {`.
- The body of every `if`, `else`, `for`, `while` and `do` **must** be a
  compound statement, even when it is one statement (MISRA Rule 15.6).
- A `do ... while` loop's `while` is on the line of its closing brace:
  `} while (rest != 0U);`.

### 7.3 Spaces

- One space **should** follow the keywords `if`, `for`, `while`, `switch`
  and `return`: `if (k_val == NULL) {`.
- No space **should** come between a function's name and its `(`, in
  definitions, declarations and calls: `hv_printf("...")`,
  `void jet_on_tick(void)`. `sizeof` is written like a call:
  `sizeof(*(addr))`.
- Binary and ternary operators **should** have a space on each side; unary
  operators (`!`, `~`, `-`, `*`, `&`, `++`) have none. A comma and the `;`
  of a `for` are followed by a space.
- The `*` of a pointer **should** go with the name, not the type:
  `const char *s`, `hv_vm_t *vm`, `hv_cpu_t *hv_this_cpu(void)`. The
  exception is a pointer annotated `JET_USER` or `JET_KUSER`: the
  annotation comes between the `*` and the name, which the code writes
  `const void* JET_USER data`.
- A cast **should** be followed by a space: `(uint64_t) value`,
  `(void) printf(...)`.

### 7.4 Breaking lines

- A call's arguments that don't fit **should** continue under the first
  argument; a function's parameters likewise under the first parameter:

  ```c
  static void put_number(const format_out_t *out, uint64_t value, uint32_t base,
                         bool negative, const format_conv_t *conv)
  ```

- A long expression **should** be broken before a binary operator (`&&`,
  `||`, `+`, ...), which starts the next line, aligned with the start of
  the expression that it continues:

  ```c
      if ((size == 0U) || (size > HV_VM_MAX_MEMORY)
          || ((size % HV_LARGE_PAGE_SIZE) != 0U)) {
          hv_fatal("bad VM memory size in the configuration");
      }

      bool is_id = (offset >= ID_FIRST)
                   && (offset < (ID_FIRST + (4U * ID_COUNT)))
                   && ((offset & 3U) == 0U);
  ```

- A long string literal **should** be split into adjacent literals, each
  on its line, rather than exceed 80 columns.

### 7.5 switch

- `case` and `default` **should** be indented one level inside `switch`,
  their statements one more level. A clause that needs local variables
  puts them in a block at the level of its statements:

  ```c
      switch (c) {
          case 'd':
          case 'i':
              {
                  int64_t value = signed_argument(conv, args);
                  ...
              }
              break;
          case 'u':
              put_number(out, unsigned_argument(conv, args), 10U, false,
                         conv);
              break;
          default:
              ...
              break;
      }
  ```

### 7.6 Comments

- A comment **should** say why the code does what it does, or what it
  stands for, in full sentences; it doesn't repeat the code.
- Block comments are `/* ... */`, with a ` * ` at the start of each line
  inside. Short comments in a function body may be `//` (C99, which
  MISRA C:2025 allows) or `/* ... */`.
- A comment **must not** contain `/*` or `//` (MISRA Rule 3.1).
- `/**` opens only the Doxygen comments of section 13: the `@file`
  comment, function, type and macro headers, member comments (`/**<`),
  `@name` groups and `\cond`. Any other comment is a plain comment, so
  that it stays out of the PDFs.
- `TODO:` marks known work left to do; it says what is missing.

## 8. Types, constants and declarations

- The built-in integer types (`int`, `unsigned`, `long`, `short`, `char`
  as a number) **must not** be used: the fixed-width types of
  `<stdint.h>` (`uint32_t`, `int64_t`, ...), `size_t` and `uintptr_t` are
  (MISRA Dir 4.6). The only exceptions are the signatures that a standard
  fixes (`void *memset(void *dest, int val, size_t count)`, the `va_arg`
  of the printf formatter). `char` is for characters.
- Booleans **should** be `bool` with `true` and `false` from
  `<stdbool.h>`. `pok_bool_t`, `TRUE` and `FALSE` are kept in the kernel's
  and libpok's interfaces (`uapi/types.h`), where they are `bool`, `true`
  and `false`.
- Unsigned integer constants **must** have a `U` suffix (`0U`, `256U`,
  `0xFFU`, MISRA Rule 7.2); hexadecimal digits **should** be upper case
  in C (`0xEDB88320U`; the old kernel code has both).
  A constant shifted into the upper bits is cast first:
  `((uint32_t) 1U << HV_IRQ_VTIMER)`.
- Magic numbers **should** be named: a constant used more than once, or
  whose meaning isn't plain, is a macro in a header
  (`CRC32_POLYNOMIAL`, `BITS_PER_BYTE`).
- Structures, unions and named enums **must** be typedefs without a tag,
  used by their typedef name, never as `struct x` (README, "Code layout"):

  ```c
  typedef struct {
      hv_vm_t *vm; /**< The VM the vCPU belongs to. */
      uint32_t index; /**< VCPU number within its VM. */
      hv_arch_vcpu_t arch; /**< The state that depends on the architecture. */
  } hv_vcpu_t;
  ```

  The tag stays only for a structure that refers to itself
  (`typedef struct delayed_event { struct delayed_event *next_event; ... }
  delayed_event_t;`), for a type that the shared code uses by pointer and
  each architecture defines (`typedef struct jet_context jet_context_t;`),
  and for the POSIX structures (`struct tm`, `struct timespec`). A
  function pointer field has a typedef of its own (`_fn_t`).
- Every member of a type **must** have a member comment (section 13).
- An object that the function doesn't change through a pointer parameter
  **should** be `const`: `const hv_vm_t *vm`, `const char *message`.
- `static` **must** be given to functions and file-scope objects that are
  used only in their file (MISRA Rule 8.7, 8.8); an object used by one
  function only **should** be defined inside it.
- A local variable **should** be declared at the start of the block that
  uses it, and initialized there when its first value is known; a loop
  counter is declared in its `for`: `for (uint32_t i = 0U; i < n; i++)`.
- Layouts that assembly or another program depends on **should** be
  checked at compile time with `_Static_assert` (with its Dir 1.2
  deviation), as in `hypervisor/csps/arm64/include/arch/internal.h`.

## 9. Expressions and control flow

- The condition of an `if`, a loop or `?:` **must** be essentially Boolean
  (MISRA Rule 14.4): a comparison or a `bool`. An integer or a pointer is
  compared explicitly: `if (count != 0U)`, `if (k_val == NULL)`, never
  `if (count)` or `if (!ptr)`.
- The operands of `&&`, `||` and of a mixed expression **should** be in
  parentheses (MISRA Rule 12.1): `if ((port >= HV_PORT_COM1) && (port <=
  HV_NB_PORTS))`.
- The value of a function that is not used **must** be discarded with a
  `(void)` cast (MISRA Rule 17.7): `(void) printf("...")`. An unused
  parameter is marked with `(void) x;`.
- Assignments **must not** be used as values (inside conditions or other
  expressions), and `++` / `--` **should** be statements of their own
  (MISRA Rules 13.3, 13.4).
- Unsigned arithmetic that may wrap **should** be checked or saturated
  where the values come from a partition or the configuration
  (`pok_time_add()` saturates instead of overflowing).
- A `switch` **must** end every clause with `break`, `return` or a call
  to a `_Noreturn` function (MISRA Rule 16.3), and **must** have a
  `default` clause (Rule 16.4).
- Early `return`s are used, mostly to refuse bad arguments at the start of
  a function (section 10).
- `goto` **should not** be used. Where it is, it **must** only jump
  forward to the function's common exit path (`out:`, `err:`), which
  releases what the function took or restores the scheduler's state, and
  the file **must** have a Rule 15.1 deviation that says so
  (`kernel/core/port.c`, `kernel/core/thread.c`).
- Functions **should not** be recursive (MISRA Rule 17.2): the stacks are
  small and fixed.

## 10. Errors, assertions and memory

### 10.1 Errors returned

- A kernel service (a syscall) **must** return a `pok_ret_e`:
  `POK_ERRNO_OK`, or the reason why it did nothing (`POK_ERRNO_EINVAL`,
  `POK_ERRNO_EFAULT`, `POK_ERRNO_MODE`, ...). The APEX services in libpok
  return their `RETURN_CODE_TYPE` through their `RETURN_CODE` parameter,
  with the codes of ARINC 653.
- A service **should** check its arguments first and return at once on a
  bad one, without changing any state; then do its work.
- The result of every function that can fail **must** be checked (or
  discarded with `(void)`, section 9).

### 10.2 User memory

- A pointer that comes from a partition **must** be annotated `JET_USER`
  and **must not** be dereferenced by the kernel. It is converted with
  `jet_user_to_kernel()`, `jet_user_to_kernel_ro()` or their typed forms,
  which check that the partition may access that memory; the result
  (`JET_KUSER`) is checked for `NULL`, and the service then returns
  `POK_ERRNO_EFAULT` without writing anything:

  ```c
  pok_ret_e jet_time(time_t* JET_USER val)
  {
      time_t* JET_KUSER k_val = jet_user_to_kernel_typed(val);

      if (k_val == NULL) {
          return POK_ERRNO_EFAULT;
      }
      *k_val = jet_calendar_time();
      return POK_ERRNO_OK;
  }
  ```

- A value that the kernel reads from user memory and then relies on (a
  time-out, a length) **should** be copied into a kernel variable first,
  and only the copy used (`kernel_timeout` in `pok_port_queuing_send()`).

### 10.3 Fatal errors and assertions

- A state the software can't continue from **must** stop it with
  `pok_fatal()` (kernel) or `hv_fatal()` (hypervisor), with a message that
  says what is wrong ("bad VM memory size in the configuration"). In the
  hypervisor, a fault of a guest stops that VM only, not the hypervisor.
- `assert()` (kernel) and `assert_os()` (libpok) check conditions that
  can't be false if the code is right. An assertion **must not** have side
  effects: it compiles to nothing with `NDEBUG`, and the requirements and
  the design describe the code as built with `NDEBUG`. A check that must
  stay in the built code is an `if` with `pok_fatal()`.
- A path that can't be taken (a `default` that can't happen) calls
  `unreachable()`, which stops the kernel even with `NDEBUG`.

### 10.4 Memory

- The kernel **must** allocate its memory only at initialization
  (`jet_mem_alloc()`); nothing is freed. The sizes of tables come from the
  configuration (generated `deployment.c`) or from constants
  (`HV_MAX_VMS`, `HV_MAX_CPUS`).
- The hypervisor **must not** allocate memory dynamically: its tables are
  static arrays sized by constants.
- Copies and fills of memory use `memcpy()` and `memset()` from
  `common/`.

## 11. Hardware, concurrency and the preprocessor

### 11.1 Hardware access

- Device registers **should** be accessed through small accessor
  functions of the driver (`gic_read32()`, `reg_read32()`,
  `config_read()`), the only place where an address is converted to a
  pointer, with its Rule 11.4 deviation. The register layouts and values
  are macros in `include/bsps/<bsp>/dev/` (devices) or
  `include/csps/<arch>/cpu/` (processor).
- Inline assembly **must** be isolated in a macro or a small `static
  inline` function of a header (`include/csps/<arch>/cpu/`,
  `hypervisor/csps/<arch>/include/arch/x86.h` or `arm64.h`), with its
  Dir 1.2 and Dir 4.3 deviation; the C code calls those.

### 11.2 Concurrency

- An object that an interrupt handler or another CPU changes **must** be
  accessed through `volatile`, `ACCESS_ONCE()` or the flag macros of
  `kernel/include/compiler.h` (`flag_set()`, `flag_reset()`,
  `flag_test_and_reset()`); `barrier()` keeps the compiler from moving
  accesses across it.
- The kernel's critical sections **should** use
  `pok_preemption_local_disable()` / `pok_preemption_local_enable()`
  (or the `ja_preempt_*()` of the architecture where the core can't),
  enabling again on every path out (hence the common exit path of
  section 9).
- The hypervisor gives each vCPU its own physical CPU, so a VM's devices
  need no locks; what the CPUs share (the console) has its lock
  (`hv_console_lock()`).

### 11.3 Macros

- A function **should** be preferred to a function-like macro (MISRA
  Dir 4.9); a macro is used where a function can't do the job (a constant
  expression, `sizeof` of an argument, `__FILE__`) or to give a name of
  the kernel to a function of the architecture
  (`#define jet_system_time() ja_system_time()`).
- Every parameter of a macro **must** be in parentheses in its body, and
  the body too when it is an expression:
  `#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))`.
- A macro with statements **must** be `do { ... } while (false)`.
- `#undef` **should not** be used: a macro that a function needs for
  itself has a name of its own (README, "Code layout").
- `#` and `##` **should** be avoided (MISRA Rule 20.10); where they are
  needed (`assert()` prints its expression), the macro has a deviation.
- Conditional compilation is for the configuration switches
  (`POK_NEEDS_*`), the architecture and `NDEBUG`; code **should not** be
  disabled with `#if 0`.

## 12. MISRA C:2025 deviations

The kernel (with its CSPs and BSPs), `common/`, libpok, syspart and the
hypervisor **must** follow MISRA C:2025, each deviation documented where
it is made (SRQ_JET_GEN_005, SRQ_HV_GEN_005).

- A deviation **must** be a plain comment right before what it covers (a
  function, a macro, a statement, or the code after the includes for the
  whole file), in this form:

  ```c
  /*
   * MISRA C:2025 Rule 11.4 deviation (gic_read32(), gic_write32()): the
   * GIC's registers are used by physical address (identity-mapped),
   * converted to pointers.
   */
  ```

  - It starts with `MISRA C:2025`, so that `grep -rn "MISRA C:2025"`
    finds every one.
  - It names each guideline: `Rule 11.4`, `Dir 1.2`, `Rule 11.4 and
    Rule 11.6 deviation`, `Dir 1.2 and Dir 4.3 deviation`. "(advisory)"
    may be added for an advisory one.
  - The scope is in parentheses when it is more than the next construct:
    `(this file)`, `(the register accessors below)`, or the names of the
    functions.
  - The reason says why the guideline can't be followed there and what
    keeps the construct safe.
- A deviation **must** be as narrow as it can: one function or accessor
  rather than the file, when only it needs it.
- The usual deviations, and where they come from:

  | Guideline | Why |
  |-----------|-----|
  | Rule 11.4, 11.6 | Physical addresses and device registers converted to pointers; `%p` prints an address. |
  | Rule 11.5 | `void *` from the user memory helpers or the allocators converted to the object's type. |
  | Rule 11.3 | Byte images (ELF headers, device trees) read through their structure types. |
  | Rule 18.4 | IDs computed as the difference between an element's address and its array's start. |
  | Rule 18.7 | Flexible array members sized by the configuration. |
  | Dir 1.2, Dir 4.3 | GNU extensions: inline assembly, attributes, `typeof`, `_Noreturn`, `_Static_assert`. |
  | Rule 5.10, 20.15, 21.2 | The standard names that the freestanding programs define (`memcpy()`, `memset()`). |
  | Rule 17.1 | `<stdarg.h>` for the printf family. |
  | Rule 21.6, 21.10 | libpok's C library implements `<stdio.h>` and `<time.h>`. |
  | Rule 15.1 | Forward `goto` to a common exit path (section 9). |
  | Rule 20.10 | `#` in `assert()` and in the assembly macros. |

## 13. Documentation and traceability

The rules are in `README.md` ("Function documentation", "Requirements
traceability", "Types", "Macros"); in short:

- Every C function **must** have a Doxygen header right before its
  definition, with the fields `@funcname`, `@description`, `@params`
  (unless it has none), `@return` (unless it returns `void`), `@globals`,
  `@locals`, `@hlr`, `@llr`, `@design` and `@calls`, in that order. A
  `static inline` function of a header has a plain comment instead.
- In `kernel/`, `common/` and `hypervisor/`, every function **must** have
  HLRs and LLRs, and every step of its `@design` **must** be linked to the
  LLRs that it implements (`\llrref{...}`); `make docs-trace` **must**
  pass. Each requirement has exactly one "shall". In the other components
  the two fields are `None.`.
- `@design` **must** describe every statement, in order, with the phrases
  that the README lists ("Set `x` to `v`.", "If ...:", "Call `f()`."),
  except the code that compiles to nothing (`assert()`, `(void) x;`, an
  empty branch). When the code of a function changes, its `@design`,
  `@locals`, `@globals`, `@calls` and requirements **must** change with
  it, and the `Traceability` of the SRQs it traces to if an HLR changes.
- Every type and every macro of a header **must** have a header
  (`@typename` or `@macroname`, then `@description`, and `@params` for a
  function-like macro), and every member of a type a `/**< ... */`
  comment (or `/** ... */` before it when it doesn't fit in 80 columns).
  Related constants form a `@name` group.
- A header (`.h`) **must not** have an `@file` comment; a C file **must**.
- The escapes of the README apply: `%%` before a letter, `\#` before a
  word, names in code font, `\|` in table cells.
- `make docs` **must** leave no warning in
  `build/docs/<name>/warnings.log`.
- Requirement identifiers are made from the file and the function names,
  in capitals: `HLR_<FILE>_<FUNCTION>_<NN>`, `LLR_<FILE>_<FUNCTION>_<NN>`
  (with the architecture after the file where both architectures have a
  function of that name). An identifier is never reused.

## 14. Assembly and linker scripts

- Assembly files **should** be `.S`, so that they can use `#define` and
  `#include`; on x86_64 they use the AT&T syntax of GNU as.
- Instructions and directives **should** be indented with spaces, the
  same amount throughout a file: 4 in the arm64 files, 2 in the x86_64
  ones. Labels are at the left margin. Local branches use numeric labels
  (`1:`, `b 1b`, `b.hs 2f`).
- A global symbol **should** have `.globl` and, for a function, a `.size`
  directive after it: `.size ja_context_switch, . - ja_context_switch`.
- Comments are `/* ... */`, or `//` at the end of a line (the file is
  preprocessed, so `#` is not a comment). A
  function's comment **should** say what it expects in which register and
  what it leaves (`/* rdi: old_sp_p, rsi: new_sp */`); the file's comment
  says how it is entered.
- A constant that must agree with C (a structure's size or offset)
  **should** say where its C definition is (`/* Size of 'struct
  jet_context' (thread.h). */`), and be checked where possible
  (`_Static_assert` on the C side, `misc/asm_offsets`).
- Linker scripts (`.lds`) have the licence header, a comment that says
  how the image is laid out, and a comment for each symbol that the C code
  uses (`__hv_end`, `hv_image_crc`).

## 15. Python and SCons

- Python **must** be Python 3, and **should** follow PEP 8: 4-space
  indentation, at most 79 columns, two blank lines between top-level
  definitions, `snake_case` functions and variables, `CapWords` classes,
  `UPPER_CASE` constants.
- A tool **should** use only the standard library and the packages that
  the build image installs (`misc/docker/Dockerfile`: `lxml`, `jinja2`,
  `yaml`, `ipaddr`); a new dependency is added to the Dockerfile.
- A script starts with `#!/usr/bin/env python3` if it is run directly,
  then the licence comment, then a comment that says what it does and how
  it is used (`hypervisor/hvcrc.py`).
- Errors of the input **should** raise an exception of the tool
  (`ConfigError`, `ImageError`) whose message names the file, the element
  and the problem (`'%s: Memory must be a multiple of 2 MiB'`); the
  caller prints it and exits with status 1.
- SCons files follow the same style; compiler flags are built from
  strings with a comment for each flag that isn't obvious (`misc/SConscript`,
  `hypervisor/SConstruct`).

## 16. Tests and examples

- Tests and examples are C code of partitions and **should** follow the
  formatting of section 7. They don't need Doxygen headers or
  requirements.
- A test **must** follow the rules of `tests/README`: each check prints
  `TEST <description>: PASS` or `TEST <description>: FAIL ...`, no other
  line contains `FAIL`, and each partition ends with
  `<partition> DONE: <n> failure(s)`.
- A check **should** print what it got and what it wanted
  (`FAIL (got %lld, want %lld)`).
- A change of behaviour **should** come with a test, in `tests/` (APEX
  services, the C library, the GDB server) or `tests/hypervisor/`.

## 17. Commits

- The subject line **should** be a short summary in plain English, without
  a final period; it may start with the component it touches and a colon
  (`hypervisor:`, `docs:`, `arm64-qemu:`, `README:`). It says what the
  change does: "Share CPU, device and boot definitions between kernel and
  hypervisor (include/)", "Don't read a NULL thread's FP store when
  switching to idle".
- The body, after a blank line, **should** say what changed and why,
  wrapped at about 72 columns, with a list for several points.
- A commit **should** do one thing; a commit that reformats code doesn't
  change its behaviour.

## 18. Legacy code

Much of `kernel/` and `libpok/` comes from POK and from the first JetOS
code. Its layout differs from section 7 in places (3-space or tab
indentation, `if(`, `type* p`, a space before the `(` of a declaration,
braces on their own line after a condition). Measured on the tree:
4-space indentation and `if (` are the majority in every component;
`type* p` is still the majority in `kernel/`.

- Whole files **should not** be reformatted in a commit that changes
  behaviour: it hides the change from review.
- A function that is rewritten **should** be brought to this standard;
  a few lines changed inside an old function keep the layout of that
  function.
- The rules marked **must** (MISRA, the headers, the requirements, the
  licence and change records) apply to legacy code as to new code, and
  the code has been brought to them.
- The `pok_` names and the POK API **should** be kept: renaming changes
  the interface and the requirement identifiers.

## 19. Review checklist

Before a change is merged:

- [ ] It builds without warnings for `x86_64-qemu` and `arm64-qemu`, and
      `make run` (and `make hv-test` for the hypervisor) pass on both.
- [ ] No floating point in the kernel or the hypervisor; no dynamic
      allocation after initialization; no 32-bit addresses or sizes.
- [ ] The core reaches the architecture only through `ja_*` / `hv_arch_*`;
      the hypervisor includes nothing from `kernel/`; shared code is
      BSD-2-Clause or POK.
- [ ] Each changed file has a new `Modified by` record; new files have a
      licence header.
- [ ] No macros or types in C files; prototypes inside `\cond`; types
      without tags; `_t`, `_e`, `_fn_t` suffixes.
- [ ] Fixed-width types, `U` suffixes, essentially Boolean conditions,
      braces everywhere, `(void)` on discarded results, `default` in each
      `switch`.
- [ ] User pointers are `JET_USER` and go through
      `jet_user_to_kernel*()`, with `POK_ERRNO_EFAULT` on failure.
- [ ] Every new deviation from MISRA C:2025 has a `MISRA C:2025 ...
      deviation` comment with its scope and reason.
- [ ] Every new or changed function, type and macro has an up-to-date
      Doxygen header; `@design`, `@calls`, `@locals`, `@globals`, HLRs,
      LLRs and SRQ `Traceability` match the code; `make docs` has no
      warning and `make docs-trace` passes.
- [ ] 4-space indentation, 80 columns, the brace and space rules of
      section 7 in new and rewritten code.
- [ ] A test covers the change of behaviour.
