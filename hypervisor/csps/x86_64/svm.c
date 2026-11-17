/*
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: moved from hypervisor/src/ to
 * arch/x86_64/; svm_enable_cpu(), svm_vcpu_init() and svm_vcpu_run() are
 * now hv_arch_cpu_enable(), hv_arch_vcpu_init() and hv_arch_vcpu_run(),
 * called by the common code, and the SVM state is in the arch parts of the
 * structures. Doxygen headers for the file and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * hv_action_e (was hv_action_t); the macros and the _Static_asserts are in
 * arch/svm.h, documented there.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * hypervisor/arch/x86_64/svm.c (the code of the processor architecture is in
 * hypervisor/csps); memset() of the library shared with the kernel replaces
 * hv_memset().
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: requirements with more than one
 * "shall" are split into requirements of one "shall" each, and the high level
 * requirements trace to the system requirements split in the same way.
 */

/**
 * @file
 * AMD SVM back end: enabling SVM on a CPU, the initial guest state (as a
 * Multiboot loader leaves it), the run loop of a vCPU and the handling of
 * \#VMEXITs. QEMU's TCG emulates SVM with nested paging, but not the next-RIP
 * save or decode assists: the intercepted instructions are skipped by their
 * fixed lengths (IOIO exits give the next RIP), and nothing is decoded.
 */

#include <arch/internal.h>

static uint8_t msrpm[SVM_MSRPM_SIZE] HV_ALIGNED(4096);

/**
 * @funcname `svm_pa`
 *
 * @description
 * Returns the physical address of a VMCB, save area or permission map
 * (identity-mapped), as the CPU wants it.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `p` | `const void *` | [in] The structure. |
 *
 * @return Its physical address.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_SVM_SVM_PA_01}: `svm_pa()` shall give the physical address of a
 *   structure of SVM, which the hypervisor maps at the same address.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_005}
 *
 * @llr
 * - \llrid{LLR_SVM_SVM_PA_01}: `svm_pa()` shall return the address of the
 *   structure.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_SVM_PA_01}
 *
 * @design
 * 1. Return `(uint64_t) (uintptr_t) p`. \llrref{LLR_SVM_SVM_PA_01}
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Rule 11.4 and Rule 11.6 deviation (svm_pa()): the VMCB,
 * the host save area and the permission maps are given to the CPU by
 * their physical addresses (identity-mapped), converted from pointers.
 */
static uint64_t svm_pa(const void *p)
{
    return (uint64_t) (uintptr_t) p;
}

/**
 * @funcname `svm_supported`
 *
 * @description
 * Checks that the CPU has AMD SVM with nested paging, not disabled by the
 * firmware, and reports what is missing.
 *
 * @return true if SVM with nested paging can be used.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `regs` | `uint32_t[4]` | The CPUID results |
 * | `svm` | `bool` | Whether SVM is there and enabled |
 * | `npt` | `bool` | Whether nested paging is there |
 *
 * @hlr
 * - \hlrid{HLR_SVM_SVM_SUPPORTED_01}: `svm_supported()` shall tell whether the
 *   processor has AMD SVM with nested paging, not disabled by the firmware, and
 *   report what is missing.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_BOOT_003}
 *   - \srqref{SRQ_HV_BOOT_008}
 *   - \srqref{SRQ_HV_GEN_002}
 *
 * @llr
 * - \llrid{LLR_SVM_SVM_SUPPORTED_01}: If `CPUID_EXT_ECX_SVM` is not set in ECX
 *   of `x86_cpuid()` for `CPUID_EXT_FEATURES`, `svm_supported()` shall call
 *   `hv_printf()` once with `"[hv] the CPU has no AMD SVM\n"` and return false,
 *   without reading `MSR_VM_CR`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_SVM_SUPPORTED_01}
 * - \llrid{LLR_SVM_SVM_SUPPORTED_02}: If `CPUID_EXT_ECX_SVM` is set in ECX of
 *   `x86_cpuid()` for `CPUID_EXT_FEATURES` and `VM_CR_SVMDIS` is set in
 *   `MSR_VM_CR` (`x86_rdmsr()`), `svm_supported()` shall call `hv_printf()`
 *   with `"[hv] SVM is disabled by the firmware\n"` and then with `"[hv] the
 *   CPU has no AMD SVM\n"`, and return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_SVM_SUPPORTED_01}
 * - \llrid{LLR_SVM_SVM_SUPPORTED_03}: If `CPUID_EXT_ECX_SVM` is set,
 *   `VM_CR_SVMDIS` is not set in `MSR_VM_CR` and `CPUID_SVM_EDX_NPT` is not set
 *   in EDX of `x86_cpuid()` for `CPUID_SVM_FEATURES`, `svm_supported()` shall
 *   call `hv_printf()` once with `"[hv] the CPU has no nested paging (NPT)\n"`
 *   and return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_SVM_SUPPORTED_01}
 * - \llrid{LLR_SVM_SVM_SUPPORTED_04}: If `CPUID_EXT_ECX_SVM` is set,
 *   `VM_CR_SVMDIS` is not set in `MSR_VM_CR` and `CPUID_SVM_EDX_NPT` is set in
 *   EDX of `x86_cpuid()` for `CPUID_SVM_FEATURES`, `svm_supported()` shall
 *   return true without calling `hv_printf()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_SVM_SUPPORTED_01}
 *
 * @design
 * 1. Set `npt` to `false`. \llrref{LLR_SVM_SVM_SUPPORTED_01}
 * 2. Call `x86_cpuid()` with the arguments `CPUID_EXT_FEATURES`, `0U` and
 *    `regs`. \llrref{LLR_SVM_SVM_SUPPORTED_01},
 *    \llrref{LLR_SVM_SVM_SUPPORTED_02}, \llrref{LLR_SVM_SVM_SUPPORTED_03},
 *    \llrref{LLR_SVM_SVM_SUPPORTED_04}
 * 3. Set `svm` to `(regs[2] & CPUID_EXT_ECX_SVM) != 0U`.
 *    \llrref{LLR_SVM_SVM_SUPPORTED_01}, \llrref{LLR_SVM_SVM_SUPPORTED_02},
 *    \llrref{LLR_SVM_SVM_SUPPORTED_03}, \llrref{LLR_SVM_SVM_SUPPORTED_04}
 * 4. If `svm` is true: \llrref{LLR_SVM_SVM_SUPPORTED_02},
 *    \llrref{LLR_SVM_SVM_SUPPORTED_03}, \llrref{LLR_SVM_SVM_SUPPORTED_04}
 *    1. Call `x86_cpuid()` with the arguments `CPUID_SVM_FEATURES`, `0U` and
 *       `regs`.
 *    2. Set `npt` to `(regs[3] & CPUID_SVM_EDX_NPT) != 0U`.
 *    3. If `(x86_rdmsr(MSR_VM_CR) & VM_CR_SVMDIS)` is not equal to `0U`:
 *       \llrref{LLR_SVM_SVM_SUPPORTED_02}
 *       1. Call `hv_printf()` with the argument
 *          `"[hv] SVM is disabled by the firmware\n"`.
 *       2. Set `svm` to `false`.
 * 5. If `svm` is false: \llrref{LLR_SVM_SVM_SUPPORTED_01},
 *    \llrref{LLR_SVM_SVM_SUPPORTED_02}
 *    1. Call `hv_printf()` with the argument `"[hv] the CPU has no AMD SVM\n"`.
 * 6. Otherwise, if `npt` is false: \llrref{LLR_SVM_SVM_SUPPORTED_03},
 *    \llrref{LLR_SVM_SVM_SUPPORTED_04}
 *    1. Call `hv_printf()` with the argument
 *       `"[hv] the CPU has no nested paging (NPT)\n"`.
 * 7. Return `svm && npt`. \llrref{LLR_SVM_SVM_SUPPORTED_01},
 *    \llrref{LLR_SVM_SVM_SUPPORTED_02}, \llrref{LLR_SVM_SVM_SUPPORTED_03},
 *    \llrref{LLR_SVM_SVM_SUPPORTED_04}
 *
 * @calls
 * - `x86_cpuid()`
 * - `x86_rdmsr()`
 * - `hv_printf()`
 */
bool svm_supported(void)
{
    uint32_t regs[4];
    bool svm;
    bool npt = false;

    // SVM in the extended features; NPT and VM_CR only exist with SVM.
    x86_cpuid(CPUID_EXT_FEATURES, 0U, regs);
    svm = (regs[2] & CPUID_EXT_ECX_SVM) != 0U;
    if (svm) {
        x86_cpuid(CPUID_SVM_FEATURES, 0U, regs);
        npt = (regs[3] & CPUID_SVM_EDX_NPT) != 0U;
        if ((x86_rdmsr(MSR_VM_CR) & VM_CR_SVMDIS) != 0U) {
            hv_printf("[hv] SVM is disabled by the firmware\n");
            svm = false;
        }
    }
    if (!svm) {
        hv_printf("[hv] the CPU has no AMD SVM\n");
    } else if (!npt) {
        hv_printf("[hv] the CPU has no nested paging (NPT)\n");
    } else {
        // Supported.
    }
    return svm && npt;
}

/**
 * @funcname `svm_init`
 *
 * @description
 * Sets up what all vCPUs share: the MSR permission map, with every MSR
 * access exiting (see handle_msr()).
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `msrpm` | `uint8_t[SVM_MSRPM_SIZE]` | The MSR permission map |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_SVM_SVM_INIT_01}: `svm_init()` shall make every access of a
 *   guest to a model specific register exit to the hypervisor.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_010}
 *   - \srqref{SRQ_HV_CPU_006}
 *
 * @llr
 * - \llrid{LLR_SVM_SVM_INIT_01}: `svm_init()` shall set every byte of `msrpm`
 *   to `0xFF` with `memset()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_SVM_INIT_01}
 *
 * @design
 * 1. Call `memset()` with the arguments `msrpm`, `0xFF` and
 *    `sizeof(msrpm)`. \llrref{LLR_SVM_SVM_INIT_01}
 *
 * @calls
 * - `memset()`
 */
void svm_init(void)
{
    // Every MSR access exits (see handle_msr()).
    (void) memset(msrpm, 0xFF, sizeof(msrpm));
}

/**
 * @funcname `hv_arch_cpu_enable`
 *
 * @description
 * Enables SVM on this CPU and gives its vCPU a VMCB and an ASID.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cpu` | `hv_cpu_t *` | [in,out] The CPU. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vmcbs` | `static vmcb_t[HV_MAX_CPUS]` | The VMCB of each vCPU |
 * | `host_save_areas` | `static uint8_t[HV_MAX_CPUS][HV_PAGE_SIZE]` | The host save area of each CPU |
 * | `vcpu` | `hv_vcpu_t *` | The CPU's vCPU |
 *
 * @hlr
 * - \hlrid{HLR_SVM_HV_ARCH_CPU_ENABLE_01}: `hv_arch_cpu_enable()` shall enable
 *   SVM on the calling CPU and give its vCPU a control block and an address
 *   space identifier.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_005}
 *
 * @llr
 * - \llrid{LLR_SVM_HV_ARCH_CPU_ENABLE_01}: `hv_arch_cpu_enable()` shall set
 *   `EFER_SVME` in `MSR_EFER` (`x86_rdmsr()`, `x86_wrmsr()`), write the address
 *   of the host save area of the index of the CPU (`svm_pa()`) to
 *   `MSR_VM_HSAVE_PA`, and set, in the vCPU of the CPU, `vmcb` to the VMCB of
 *   the index of the CPU, `vmcb_pa` to the address of that VMCB (`svm_pa()`)
 *   and `asid` to the index of the CPU plus 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HV_ARCH_CPU_ENABLE_01}
 *
 * @design
 * 1. Set `vcpu` to the address of `cpu->vcpu`.
 *    \llrref{LLR_SVM_HV_ARCH_CPU_ENABLE_01}
 * 2. Call `x86_wrmsr()` with the arguments `MSR_EFER` and
 *    `x86_rdmsr(MSR_EFER) | EFER_SVME`. \llrref{LLR_SVM_HV_ARCH_CPU_ENABLE_01}
 * 3. Call `x86_wrmsr()` with the arguments `MSR_VM_HSAVE_PA` and
 *    `svm_pa(host_save_areas[cpu->index])`.
 *    \llrref{LLR_SVM_HV_ARCH_CPU_ENABLE_01}
 * 4. Set `vcpu->arch.vmcb` to the address of `vmcbs[cpu->index]`.
 *    \llrref{LLR_SVM_HV_ARCH_CPU_ENABLE_01}
 * 5. Call `svm_pa()` with the argument `vcpu->arch.vmcb`, and store the result
 *    in `vcpu->arch.vmcb_pa`. \llrref{LLR_SVM_HV_ARCH_CPU_ENABLE_01}
 * 6. Set `vcpu->arch.asid` to `cpu->index + 1U`.
 *    \llrref{LLR_SVM_HV_ARCH_CPU_ENABLE_01}
 *
 * @calls
 * - `x86_wrmsr()`
 * - `x86_rdmsr()`
 * - `svm_pa()`
 */
void hv_arch_cpu_enable(hv_cpu_t *cpu)
{
    static vmcb_t vmcbs[HV_MAX_CPUS] HV_ALIGNED(4096);
    static uint8_t host_save_areas[HV_MAX_CPUS][HV_PAGE_SIZE]
        HV_ALIGNED(4096);
    hv_vcpu_t *vcpu = &cpu->vcpu;

    // VMRUN keeps the host state in this CPU's save area.
    x86_wrmsr(MSR_EFER, x86_rdmsr(MSR_EFER) | EFER_SVME);
    x86_wrmsr(MSR_VM_HSAVE_PA, svm_pa(host_save_areas[cpu->index]));
    vcpu->arch.vmcb = &vmcbs[cpu->index];
    vcpu->arch.vmcb_pa = svm_pa(vcpu->arch.vmcb);
    vcpu->arch.asid = cpu->index + 1U;   // ASID 0 is the host's
}

/**
 * @funcname `set_segment`
 *
 * @description
 * Sets a segment register of the VMCB save area, with base 0.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `seg` | `vmcb_segment_t *` | [out] The segment register. |
 * | `selector` | `uint16_t` | [in] The selector. |
 * | `attrib` | `uint16_t` | [in] The attributes, in the VMCB format. |
 * | `limit` | `uint32_t` | [in] The limit. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_SVM_SET_SEGMENT_01}: `set_segment()` shall set a segment
 *   register of the initial state of a guest, with base 0.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_VM_005}
 *
 * @llr
 * - \llrid{LLR_SVM_SET_SEGMENT_01}: `set_segment()` shall set `selector`,
 *   `attrib` and `limit` of the segment to the arguments of the same names, and
 *   its `base` to 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_SET_SEGMENT_01}
 *
 * @design
 * 1. Set `seg->selector` to `selector`. \llrref{LLR_SVM_SET_SEGMENT_01}
 * 2. Set `seg->attrib` to `attrib`. \llrref{LLR_SVM_SET_SEGMENT_01}
 * 3. Set `seg->limit` to `limit`. \llrref{LLR_SVM_SET_SEGMENT_01}
 * 4. Set `seg->base` to `0U`. \llrref{LLR_SVM_SET_SEGMENT_01}
 *
 * @calls
 * None.
 */
static void set_segment(vmcb_segment_t *seg, uint16_t selector,
                        uint16_t attrib, uint32_t limit)
{
    seg->selector = selector;
    seg->attrib = attrib;
    seg->limit = limit;
    seg->base = 0U;
}

/**
 * @funcname `hv_arch_vcpu_init`
 *
 * @description
 * Sets the initial state of a vCPU, at the VM's first start and each reset,
 * as a Multiboot loader leaves it: 32-bit protected mode with flat
 * segments, paging off, interrupts off, EAX the Multiboot magic and EBX the
 * address of the Multiboot information.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vcpu` | `hv_vcpu_t *` | [in,out] The vCPU. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `msrpm` | `uint8_t[SVM_MSRPM_SIZE]` | The MSR permission map |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vmcb` | `vmcb_t *` | The vCPU's VMCB |
 * | `c` | `vmcb_control_t *` | Its control area |
 * | `s` | `vmcb_save_t *` | Its save area |
 * | `vm` | `const hv_vm_t *` | The vCPU's VM |
 *
 * @hlr
 * - \hlrid{HLR_SVM_HV_ARCH_VCPU_INIT_01}: `hv_arch_vcpu_init()` shall set the
 *   state of a vCPU at the first start of its VM and at each reset as a
 *   Multiboot boot loader leaves it: 32-bit protected mode with flat segments,
 *   paging and interrupts off, the Multiboot magic in EAX and the address of
 *   the Multiboot information in EBX, at the entry point of the image.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_VM_005}
 * - \hlrid{HLR_SVM_HV_ARCH_VCPU_INIT_02}: `hv_arch_vcpu_init()` shall make the
 *   guest exit to the hypervisor for physical interrupts, CPUID, HLT, port and
 *   MSR access, a triple fault, the virtualization instructions, INVLPGA,
 *   MONITOR and MWAIT, and run it with the nested page tables of its VM.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_010}
 * - \hlrid{HLR_SVM_HV_ARCH_VCPU_INIT_03}: `hv_arch_vcpu_init()` shall make the
 *   time stamp counter of the guest start at 0.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_001}
 *
 * @llr
 * - \llrid{LLR_SVM_HV_ARCH_VCPU_INIT_01}: `hv_arch_vcpu_init()` shall set every
 *   byte of the VMCB of the vCPU to 0 with `memset()`, and then set, in its
 *   control area, `intercept_misc1` to `SVM_INTERCEPT_INTR |
 *   SVM_INTERCEPT_CPUID | SVM_INTERCEPT_HLT | SVM_INTERCEPT_INVLPGA |
 *   SVM_INTERCEPT_IOIO | SVM_INTERCEPT_MSR | SVM_INTERCEPT_SHUTDOWN` and
 *   `intercept_misc2` to `SVM_INTERCEPT_VMRUN | SVM_INTERCEPT_VMMCALL |
 *   SVM_INTERCEPT_VMLOAD | SVM_INTERCEPT_VMSAVE | SVM_INTERCEPT_STGI |
 *   SVM_INTERCEPT_CLGI | SVM_INTERCEPT_SKINIT | SVM_INTERCEPT_MONITOR |
 *   SVM_INTERCEPT_MWAIT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HV_ARCH_VCPU_INIT_02}
 * - \llrid{LLR_SVM_HV_ARCH_VCPU_INIT_02}: `hv_arch_vcpu_init()` shall set, in
 *   the control area of the VMCB, `iopm_base_pa` to `iopm_pa` of the VM,
 *   `msrpm_base_pa` to the address of `msrpm` (`svm_pa()`), `guest_asid` to
 *   `asid` of the vCPU, `int_ctl` to `SVM_INT_CTL_V_INTR_MASKING |
 *   SVM_INT_CTL_V_INTR_PRIO | SVM_INT_CTL_V_IGN_TPR`, `nested_ctl` to
 *   `SVM_NESTED_NP_ENABLE` and `nested_cr3` to `npt_root` of the VM, and set
 *   `flush_tlb` of the vCPU to true.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HV_ARCH_VCPU_INIT_02}
 * - \llrid{LLR_SVM_HV_ARCH_VCPU_INIT_03}: `hv_arch_vcpu_init()` shall set
 *   `tsc_offset` of the control area of the VMCB to 0 minus the result of
 *   `x86_rdtsc()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HV_ARCH_VCPU_INIT_03}
 * - \llrid{LLR_SVM_HV_ARCH_VCPU_INIT_04}: `hv_arch_vcpu_init()` shall set with
 *   `set_segment()`, in the save area of the VMCB, `cs` to
 *   `GUEST_CODE_SELECTOR`, `SEG_ATTRIB_CODE32` and the limit `0xFFFFFFFF`, each
 *   of `ds`, `es`, `fs`, `gs` and `ss` to `GUEST_DATA_SELECTOR`,
 *   `SEG_ATTRIB_DATA32` and the limit `0xFFFFFFFF`, `gdtr` to 0, 0 and the
 *   limit 0, `idtr` to 0, 0 and the limit `0x3FF`, `ldtr` to 0,
 *   `SEG_ATTRIB_LDT` and the limit `0xFFFF`, and `tr` to 0,
 *   `SEG_ATTRIB_TSS_BUSY` and the limit `0xFFFF`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HV_ARCH_VCPU_INIT_01}
 * - \llrid{LLR_SVM_HV_ARCH_VCPU_INIT_05}: `hv_arch_vcpu_init()` shall set, in
 *   the save area of the VMCB, `cpl` to 0, `efer` to `EFER_SVME`, `cr0` to
 *   `X86_CR0_PE | X86_CR0_ET`, `cr3` and `cr4` to 0, `dr6` to `GUEST_DR6`,
 *   `dr7` to `GUEST_DR7`, `rflags` to `GUEST_RFLAGS`, `rip` to the entry of the
 *   configuration of the VM, `rsp` to 0, `rax` to `MULTIBOOT_BOOT_MAGIC` and
 *   `g_pat` to `GUEST_PAT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HV_ARCH_VCPU_INIT_01}
 * - \llrid{LLR_SVM_HV_ARCH_VCPU_INIT_06}: `hv_arch_vcpu_init()` shall set every
 *   byte of the saved registers `regs` of the vCPU to 0 with `memset()`, and
 *   then set their `rbx` to `HV_GUEST_MBI`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HV_ARCH_VCPU_INIT_01}
 *
 * @design
 * 1. Set `vmcb` to `vcpu->arch.vmcb`. \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_01},
 *    \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_02},
 *    \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_03},
 *    \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_04},
 *    \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_05}
 * 2. Set `c` to the address of `vmcb->control`.
 *    \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_01},
 *    \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_02},
 *    \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_03}
 * 3. Set `s` to the address of `vmcb->save`.
 *    \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_04},
 *    \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_05}
 * 4. Set `vm` to `vcpu->vm`. \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_02},
 *    \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_05}
 * 5. Call `memset()` with the arguments `vmcb`, `0` and `sizeof(*vmcb)`.
 *    \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_01}
 * 6. Set `c->intercept_misc1` to
 *    `SVM_INTERCEPT_INTR | SVM_INTERCEPT_CPUID | SVM_INTERCEPT_HLT |`
 *    `SVM_INTERCEPT_INVLPGA | SVM_INTERCEPT_IOIO | SVM_INTERCEPT_MSR |`
 *    `SVM_INTERCEPT_SHUTDOWN`. \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_01}
 * 7. Set `c->intercept_misc2` to
 *    `SVM_INTERCEPT_VMRUN | SVM_INTERCEPT_VMMCALL | SVM_INTERCEPT_VMLOAD |`
 *    `SVM_INTERCEPT_VMSAVE | SVM_INTERCEPT_STGI | SVM_INTERCEPT_CLGI |`
 *    `SVM_INTERCEPT_SKINIT | SVM_INTERCEPT_MONITOR | SVM_INTERCEPT_MWAIT`.
 *    \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_01}
 * 8. Set `c->iopm_base_pa` to `vm->arch.iopm_pa`.
 *    \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_02}
 * 9. Call `svm_pa()` with the argument `msrpm`, and store the result in
 *    `c->msrpm_base_pa`. \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_02}
 * 10. Set `c->tsc_offset` to `0U - x86_rdtsc()`.
 *     \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_03}
 * 11. Set `c->guest_asid` to `vcpu->arch.asid`.
 *     \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_02}
 * 12. Set `c->int_ctl` to
 *     `SVM_INT_CTL_V_INTR_MASKING | SVM_INT_CTL_V_INTR_PRIO |`
 *     `SVM_INT_CTL_V_IGN_TPR`. \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_02}
 * 13. Set `c->nested_ctl` to `SVM_NESTED_NP_ENABLE`.
 *     \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_02}
 * 14. Set `c->nested_cr3` to `vm->arch.npt_root`.
 *     \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_02}
 * 15. Call `set_segment()` with the arguments the address of `s->cs`,
 *     `GUEST_CODE_SELECTOR`, `SEG_ATTRIB_CODE32` and `0xFFFFFFFFU`.
 *     \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_04}
 * 16. Call `set_segment()` with the arguments the address of `s->ds`,
 *     `GUEST_DATA_SELECTOR`, `SEG_ATTRIB_DATA32` and `0xFFFFFFFFU`.
 *     \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_04}
 * 17. Call `set_segment()` with the arguments the address of `s->es`,
 *     `GUEST_DATA_SELECTOR`, `SEG_ATTRIB_DATA32` and `0xFFFFFFFFU`.
 *     \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_04}
 * 18. Call `set_segment()` with the arguments the address of `s->fs`,
 *     `GUEST_DATA_SELECTOR`, `SEG_ATTRIB_DATA32` and `0xFFFFFFFFU`.
 *     \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_04}
 * 19. Call `set_segment()` with the arguments the address of `s->gs`,
 *     `GUEST_DATA_SELECTOR`, `SEG_ATTRIB_DATA32` and `0xFFFFFFFFU`.
 *     \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_04}
 * 20. Call `set_segment()` with the arguments the address of `s->ss`,
 *     `GUEST_DATA_SELECTOR`, `SEG_ATTRIB_DATA32` and `0xFFFFFFFFU`.
 *     \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_04}
 * 21. Call `set_segment()` with the arguments the address of `s->gdtr`, `0U`,
 *     `0U` and `0U`. \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_04}
 * 22. Call `set_segment()` with the arguments the address of `s->idtr`, `0U`,
 *     `0U` and `0x3FFU`. \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_04}
 * 23. Call `set_segment()` with the arguments the address of `s->ldtr`, `0U`,
 *     `SEG_ATTRIB_LDT` and `0xFFFFU`. \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_04}
 * 24. Call `set_segment()` with the arguments the address of `s->tr`, `0U`,
 *     `SEG_ATTRIB_TSS_BUSY` and `0xFFFFU`.
 *     \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_04}
 * 25. Set `s->cpl` to `0U`. \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_05}
 * 26. Set `s->efer` to `EFER_SVME`. \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_05}
 * 27. Set `s->cr0` to `X86_CR0_PE | X86_CR0_ET`.
 *     \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_05}
 * 28. Set `s->cr3` to `0U`. \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_05}
 * 29. Set `s->cr4` to `0U`. \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_05}
 * 30. Set `s->dr6` to `GUEST_DR6`. \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_05}
 * 31. Set `s->dr7` to `GUEST_DR7`. \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_05}
 * 32. Set `s->rflags` to `GUEST_RFLAGS`. \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_05}
 * 33. Set `s->rip` to `vm->config->entry`.
 *     \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_05}
 * 34. Set `s->rsp` to `0U`. \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_05}
 * 35. Set `s->rax` to `MULTIBOOT_BOOT_MAGIC`.
 *     \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_05}
 * 36. Set `s->g_pat` to `GUEST_PAT`. \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_05}
 * 37. Call `memset()` with the arguments the address of `vcpu->arch.regs`,
 *     `0` and `sizeof(vcpu->arch.regs)`. \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_06}
 * 38. Set `vcpu->arch.regs.rbx` to `HV_GUEST_MBI`.
 *     \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_06}
 * 39. Set `vcpu->arch.flush_tlb` to `true`.
 *     \llrref{LLR_SVM_HV_ARCH_VCPU_INIT_02}
 *
 * @calls
 * - `memset()`
 * - `svm_pa()`
 * - `x86_rdtsc()`
 * - `set_segment()`
 */
void hv_arch_vcpu_init(hv_vcpu_t *vcpu)
{
    vmcb_t *vmcb = vcpu->arch.vmcb;
    vmcb_control_t *c = &vmcb->control;
    vmcb_save_t *s = &vmcb->save;
    const hv_vm_t *vm = vcpu->vm;

    (void) memset(vmcb, 0, sizeof(*vmcb));

    /*
     * Exits: physical interrupts, the instructions the hypervisor
     * emulates (CPUID, HLT, port I/O, MSRs), triple faults, and the SVM
     * and MONITOR/MWAIT instructions, which the guest doesn't get.
     */
    c->intercept_misc1 = SVM_INTERCEPT_INTR | SVM_INTERCEPT_CPUID
                         | SVM_INTERCEPT_HLT | SVM_INTERCEPT_INVLPGA
                         | SVM_INTERCEPT_IOIO | SVM_INTERCEPT_MSR
                         | SVM_INTERCEPT_SHUTDOWN;
    c->intercept_misc2 = SVM_INTERCEPT_VMRUN | SVM_INTERCEPT_VMMCALL
                         | SVM_INTERCEPT_VMLOAD | SVM_INTERCEPT_VMSAVE
                         | SVM_INTERCEPT_STGI | SVM_INTERCEPT_CLGI
                         | SVM_INTERCEPT_SKINIT | SVM_INTERCEPT_MONITOR
                         | SVM_INTERCEPT_MWAIT;
    c->iopm_base_pa = vm->arch.iopm_pa;     // Per VM: its serial ports differ
    c->msrpm_base_pa = svm_pa(msrpm);
    c->tsc_offset = 0U - x86_rdtsc();   // The guest's TSC starts at 0
    c->guest_asid = vcpu->arch.asid;
    // The guest's IF masks only virtual interrupts (V_IRQ, any priority).
    c->int_ctl = SVM_INT_CTL_V_INTR_MASKING | SVM_INT_CTL_V_INTR_PRIO
                 | SVM_INT_CTL_V_IGN_TPR;
    // Guest-physical memory is the VM's RAM (npt.c).
    c->nested_ctl = SVM_NESTED_NP_ENABLE;
    c->nested_cr3 = vm->arch.npt_root;

    // Flat 4 GiB segments, as a Multiboot loader leaves them.
    set_segment(&s->cs, GUEST_CODE_SELECTOR, SEG_ATTRIB_CODE32,
                0xFFFFFFFFU);
    set_segment(&s->ds, GUEST_DATA_SELECTOR, SEG_ATTRIB_DATA32,
                0xFFFFFFFFU);
    set_segment(&s->es, GUEST_DATA_SELECTOR, SEG_ATTRIB_DATA32,
                0xFFFFFFFFU);
    set_segment(&s->fs, GUEST_DATA_SELECTOR, SEG_ATTRIB_DATA32,
                0xFFFFFFFFU);
    set_segment(&s->gs, GUEST_DATA_SELECTOR, SEG_ATTRIB_DATA32,
                0xFFFFFFFFU);
    set_segment(&s->ss, GUEST_DATA_SELECTOR, SEG_ATTRIB_DATA32,
                0xFFFFFFFFU);
    // No GDT yet: the guest loads its own before it reloads a segment.
    set_segment(&s->gdtr, 0U, 0U, 0U);
    set_segment(&s->idtr, 0U, 0U, 0x3FFU);
    set_segment(&s->ldtr, 0U, SEG_ATTRIB_LDT, 0xFFFFU);
    set_segment(&s->tr, 0U, SEG_ATTRIB_TSS_BUSY, 0xFFFFU);
    // Ring 0, protected mode without paging; VMRUN requires EFER.SVME.
    s->cpl = 0U;
    s->efer = EFER_SVME;
    s->cr0 = X86_CR0_PE | X86_CR0_ET;
    s->cr3 = 0U;
    s->cr4 = 0U;
    s->dr6 = GUEST_DR6;
    s->dr7 = GUEST_DR7;
    s->rflags = GUEST_RFLAGS;
    s->rip = vm->config->entry;
    s->rsp = 0U;
    s->rax = MULTIBOOT_BOOT_MAGIC;
    s->g_pat = GUEST_PAT;           // Power-on PAT

    // EBX: the Multiboot information written by vm_load().
    (void) memset(&vcpu->arch.regs, 0, sizeof(vcpu->arch.regs));
    vcpu->arch.regs.rbx = HV_GUEST_MBI;
    vcpu->arch.flush_tlb = true;
}

/**
 * @funcname `skip_instruction`
 *
 * @description
 * Skips the intercepted instruction of the guest and ends its interrupt
 * shadow (QEMU gives no next RIP, so the length is fixed per exit).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vmcb` | `vmcb_t *` | [in,out] The vCPU's VMCB. |
 * | `length` | `uint64_t` | [in] The length of the instruction. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_SVM_SKIP_INSTRUCTION_01}: `skip_instruction()` shall make the
 *   guest go on after the instruction that the hypervisor handled.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_010}
 *
 * @llr
 * - \llrid{LLR_SVM_SKIP_INSTRUCTION_01}: `skip_instruction()` shall add
 *   `length` to `rip` of the save area of the VMCB and clear
 *   `SVM_INT_STATE_SHADOW` in `int_state` of its control area.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_SKIP_INSTRUCTION_01}
 *
 * @design
 * 1. Add `length` to `vmcb->save.rip`. \llrref{LLR_SVM_SKIP_INSTRUCTION_01}
 * 2. Keep only the bits `~SVM_INT_STATE_SHADOW` of `vmcb->control.int_state`.
 *    \llrref{LLR_SVM_SKIP_INSTRUCTION_01}
 *
 * @calls
 * None.
 */
static void skip_instruction(vmcb_t *vmcb, uint64_t length)
{
    vmcb->save.rip += length;
    vmcb->control.int_state &= ~SVM_INT_STATE_SHADOW;
}

/**
 * @funcname `inject_exception`
 *
 * @description
 * Makes the guest take an exception (without an error code) at the next
 * VMRUN.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vmcb` | `vmcb_t *` | [out] The vCPU's VMCB. |
 * | `vector` | `uint32_t` | [in] The exception vector. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_SVM_INJECT_EXCEPTION_01}: `inject_exception()` shall make the
 *   guest take an exception at its next run.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_010}
 *
 * @llr
 * - \llrid{LLR_SVM_INJECT_EXCEPTION_01}: `inject_exception()` shall set
 *   `event_inj` of the control area of the VMCB to `SVM_EVENT_VALID |
 *   SVM_EVENT_TYPE_EXCEPT` with `vector` in its low bits.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_INJECT_EXCEPTION_01}
 *
 * @design
 * 1. Set `vmcb->control.event_inj` to
 *    `SVM_EVENT_VALID | SVM_EVENT_TYPE_EXCEPT | (uint64_t) vector`.
 *    \llrref{LLR_SVM_INJECT_EXCEPTION_01}
 *
 * @calls
 * None.
 */
static void inject_exception(vmcb_t *vmcb, uint32_t vector)
{
    vmcb->control.event_inj = SVM_EVENT_VALID | SVM_EVENT_TYPE_EXCEPT
                              | (uint64_t) vector;
}

/**
 * @funcname `deliver_ticks`
 *
 * @description
 * Turns the local APIC timer ticks counted on this CPU into IRQ 0 of the
 * VM's virtual PIC, if the virtual PIT is running. Missed ticks are dropped:
 * JetOS keeps its time with the TSC.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cpu` | `hv_cpu_t *` | [in,out] The CPU. |
 * | `vm` | `hv_vm_t *` | [in,out] The CPU's VM. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_SVM_DELIVER_TICKS_01}: `deliver_ticks()` shall turn the ticks of
 *   the local APIC timer of the CPU into IRQ 0 of the VM's virtual PIC while
 *   channel 0 of its virtual timer runs periodically, dropping ticks that the
 *   guest missed.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_006}
 *
 * @llr
 * - \llrid{LLR_SVM_DELIVER_TICKS_01}: If `timer_ticks` of the CPU is not 0 and
 *   `vpit_timer_running()` returns true for the PIT of the VM,
 *   `deliver_ticks()` shall set `timer_ticks` to 0 and call `vpic_raise()` once
 *   with the PIC of the VM and 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_DELIVER_TICKS_01}
 * - \llrid{LLR_SVM_DELIVER_TICKS_02}: If `timer_ticks` of the CPU is not 0 and
 *   `vpit_timer_running()` returns false for the PIT of the VM,
 *   `deliver_ticks()` shall set `timer_ticks` to 0 without calling
 *   `vpic_raise()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_DELIVER_TICKS_01}
 * - \llrid{LLR_SVM_DELIVER_TICKS_03}: If `timer_ticks` of the CPU is 0,
 *   `deliver_ticks()` shall return without calling `vpit_timer_running()` or
 *   `vpic_raise()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_DELIVER_TICKS_01}
 *
 * @design
 * 1. If `cpu->arch.timer_ticks` is not equal to `0U`:
 *    \llrref{LLR_SVM_DELIVER_TICKS_01}, \llrref{LLR_SVM_DELIVER_TICKS_02},
 *    \llrref{LLR_SVM_DELIVER_TICKS_03}
 *    1. Set `cpu->arch.timer_ticks` to `0U`.
 *    2. If `vpit_timer_running(&vm->arch.pit)` is true:
 *       \llrref{LLR_SVM_DELIVER_TICKS_01}, \llrref{LLR_SVM_DELIVER_TICKS_02}
 *       1. Call `vpic_raise()` with the arguments the address of `vm->arch.pic`
 *          and `0U`.
 *
 * @calls
 * - `vpit_timer_running()`
 * - `vpic_raise()`
 */
static void deliver_ticks(hv_cpu_t *cpu, hv_vm_t *vm)
{
    // Interrupts are disabled here, so the counter doesn't change meanwhile.
    if (cpu->arch.timer_ticks != 0U) {
        // Missed ticks are dropped: JetOS keeps its time with the TSC.
        cpu->arch.timer_ticks = 0U;
        if (vpit_timer_running(&vm->arch.pit)) {
            vpic_raise(&vm->arch.pic, 0U);
        }
    }
}

/**
 * @funcname `inject_interrupt`
 *
 * @description
 * Requests the next virtual PIC interrupt with V_IRQ: the CPU delivers it
 * when the guest can take it (RFLAGS.IF set, no interrupt shadow).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vcpu` | `hv_vcpu_t *` | [in] The vCPU. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `vmcb_control_t *` | The VMCB's control area |
 * | `vector` | `uint8_t` | The vector of the interrupt |
 *
 * @hlr
 * - \hlrid{HLR_SVM_INJECT_INTERRUPT_01}: `inject_interrupt()` shall request the
 *   next interrupt of the virtual PIC for the guest, which the processor
 *   delivers when the guest can take it.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_002}
 *
 * @llr
 * - \llrid{LLR_SVM_INJECT_INTERRUPT_01}: If `SVM_INT_CTL_V_IRQ` is not set in
 *   `int_ctl` of the control area of the VMCB of the vCPU and `vpic_next()`
 *   returns true for the PIC of the VM, `inject_interrupt()` shall set
 *   `int_vector` of the control area to the vector that `vpic_next()` gives and
 *   set `SVM_INT_CTL_V_IRQ` in `int_ctl`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_INJECT_INTERRUPT_01}
 * - \llrid{LLR_SVM_INJECT_INTERRUPT_02}: If `SVM_INT_CTL_V_IRQ` is set in
 *   `int_ctl` of the control area of the VMCB of the vCPU, `inject_interrupt()`
 *   shall leave the control area as it is, without calling `vpic_next()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_INJECT_INTERRUPT_01}
 * - \llrid{LLR_SVM_INJECT_INTERRUPT_03}: If `SVM_INT_CTL_V_IRQ` is not set in
 *   `int_ctl` of the control area and `vpic_next()` returns false for the PIC
 *   of the VM, `inject_interrupt()` shall leave the control area as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_INJECT_INTERRUPT_01}
 *
 * @design
 * 1. Set `c` to the address of `vcpu->arch.vmcb->control`.
 *    \llrref{LLR_SVM_INJECT_INTERRUPT_01},
 *    \llrref{LLR_SVM_INJECT_INTERRUPT_02}, \llrref{LLR_SVM_INJECT_INTERRUPT_03}
 * 2. If `(c->int_ctl & SVM_INT_CTL_V_IRQ)` is equal to `0U` and
 *    `vpic_next(&vcpu->vm->arch.pic, &vector)` is true:
 *    \llrref{LLR_SVM_INJECT_INTERRUPT_01},
 *    \llrref{LLR_SVM_INJECT_INTERRUPT_02}, \llrref{LLR_SVM_INJECT_INTERRUPT_03}
 *    1. Set `c->int_vector` to `vector`.
 *    2. Set the bits `SVM_INT_CTL_V_IRQ` in `c->int_ctl`.
 *
 * @calls
 * - `vpic_next()`
 */
static void inject_interrupt(hv_vcpu_t *vcpu)
{
    vmcb_control_t *c = &vcpu->arch.vmcb->control;
    uint8_t vector;

    // One interrupt at a time: the next one after this one is taken.
    if (((c->int_ctl & SVM_INT_CTL_V_IRQ) == 0U)
        && vpic_next(&vcpu->vm->arch.pic, &vector)) {
        c->int_vector = vector;
        c->int_ctl |= SVM_INT_CTL_V_IRQ;
    }
}

/**
 * @funcname `interrupt_ready`
 *
 * @description
 * Tells whether the guest has an interrupt to take: requested with V_IRQ or
 * pending in the virtual PIC.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vcpu` | `const hv_vcpu_t *` | [in] The vCPU. |
 *
 * @return true if an interrupt is there.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_SVM_INTERRUPT_READY_01}: `interrupt_ready()` shall tell whether
 *   the guest has an interrupt to take.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_002}
 *   - \srqref{SRQ_HV_INT_007}
 *
 * @llr
 * - \llrid{LLR_SVM_INTERRUPT_READY_01}: If `SVM_INT_CTL_V_IRQ` is set in
 *   `int_ctl` of the control area of the VMCB of the vCPU, or `vpic_pending()`
 *   returns true for the PIC of the VM, `interrupt_ready()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_INTERRUPT_READY_01}
 * - \llrid{LLR_SVM_INTERRUPT_READY_02}: If `SVM_INT_CTL_V_IRQ` is not set in
 *   `int_ctl` of the control area of the VMCB of the vCPU and `vpic_pending()`
 *   returns false for the PIC of the VM, `interrupt_ready()` shall return
 *   false.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_INTERRUPT_READY_01}
 *
 * @design
 * 1. Return `((vcpu->arch.vmcb->control.int_ctl & SVM_INT_CTL_V_IRQ) != 0U) ||`
 *    `vpic_pending(&vcpu->vm->arch.pic)`. \llrref{LLR_SVM_INTERRUPT_READY_01},
 *    \llrref{LLR_SVM_INTERRUPT_READY_02}
 *
 * @calls
 * - `vpic_pending()`
 */
static bool interrupt_ready(const hv_vcpu_t *vcpu)
{
    return ((vcpu->arch.vmcb->control.int_ctl & SVM_INT_CTL_V_IRQ) != 0U)
           || vpic_pending(&vcpu->vm->arch.pic);
}

/**
 * @funcname `handle_hlt`
 *
 * @description
 * Handles a HLT exit: waits until the guest has an interrupt to take. A
 * halt with interrupts disabled stops the VM.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cpu` | `hv_cpu_t *` | [in,out] The CPU. |
 *
 * @return HV_STOP if the guest halted with interrupts disabled,
 *         HV_CONTINUE otherwise.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vcpu` | `hv_vcpu_t *` | The CPU's vCPU |
 * | `vmcb` | `vmcb_t *` | Its VMCB |
 * | `action` | `hv_action_e` | What the run loop does next |
 *
 * @hlr
 * - \hlrid{HLR_SVM_HANDLE_HLT_01}: `handle_hlt()` shall let a guest that halted
 *   with interrupts enabled wait until it has an interrupt to take.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_007}
 * - \hlrid{HLR_SVM_HANDLE_HLT_02}: `handle_hlt()` shall stop a VM whose guest
 *   halts with interrupts disabled, with a message.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_ERR_001}
 *
 * @llr
 * - \llrid{LLR_SVM_HANDLE_HLT_01}: `handle_hlt()` shall call
 *   `skip_instruction()` once with the VMCB of the vCPU of the CPU and
 *   `INSN_LEN_HLT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_HLT_01}
 * - \llrid{LLR_SVM_HANDLE_HLT_02}: If `X86_RFLAGS_IF` is not set in `rflags` of
 *   the save area of the VMCB, `handle_hlt()` shall call `vm_print()` once with
 *   the VM and `"halted with interrupts disabled"`, and return `HV_STOP`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_HLT_02}
 * - \llrid{LLR_SVM_HANDLE_HLT_03}: If `X86_RFLAGS_IF` is set in `rflags` of the
 *   save area of the VMCB, `handle_hlt()` shall call `x86_wait_interrupt()` and
 *   then `deliver_ticks()` with `cpu` and the VM, while `interrupt_ready()`
 *   returns false for the vCPU, and then return `HV_CONTINUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_HLT_01}
 *
 * @design
 * 1. Set `vcpu` to the address of `cpu->vcpu`. \llrref{LLR_SVM_HANDLE_HLT_01},
 *    \llrref{LLR_SVM_HANDLE_HLT_02}, \llrref{LLR_SVM_HANDLE_HLT_03}
 * 2. Set `vmcb` to `vcpu->arch.vmcb`. \llrref{LLR_SVM_HANDLE_HLT_01},
 *    \llrref{LLR_SVM_HANDLE_HLT_02}, \llrref{LLR_SVM_HANDLE_HLT_03}
 * 3. Set `action` to `HV_CONTINUE`. \llrref{LLR_SVM_HANDLE_HLT_03}
 * 4. Call `skip_instruction()` with the arguments `vmcb` and `INSN_LEN_HLT`.
 *    \llrref{LLR_SVM_HANDLE_HLT_01}
 * 5. If `(vmcb->save.rflags & X86_RFLAGS_IF)` is equal to `0U`:
 *    \llrref{LLR_SVM_HANDLE_HLT_02}
 *    1. Call `vm_print()` with the arguments `vcpu->vm` and
 *       `"halted with interrupts disabled"`.
 *    2. Set `action` to `HV_STOP`.
 * 6. Otherwise: \llrref{LLR_SVM_HANDLE_HLT_03}
 *    1. While `interrupt_ready(vcpu)` is false:
 *       1. Call `x86_wait_interrupt()`.
 *       2. Call `deliver_ticks()` with the arguments `cpu` and `vcpu->vm`.
 * 7. Return `action`. \llrref{LLR_SVM_HANDLE_HLT_02},
 *    \llrref{LLR_SVM_HANDLE_HLT_03}
 *
 * @calls
 * - `skip_instruction()`
 * - `vm_print()`
 * - `interrupt_ready()`
 * - `x86_wait_interrupt()`
 * - `deliver_ticks()`
 */
static hv_action_e handle_hlt(hv_cpu_t *cpu)
{
    hv_vcpu_t *vcpu = &cpu->vcpu;
    vmcb_t *vmcb = vcpu->arch.vmcb;
    hv_action_e action = HV_CONTINUE;

    skip_instruction(vmcb, INSN_LEN_HLT);
    if ((vmcb->save.rflags & X86_RFLAGS_IF) == 0U) {
        vm_print(vcpu->vm, "halted with interrupts disabled");
        action = HV_STOP;
    } else {
        // Sleep until a timer tick gives the guest an interrupt.
        while (!interrupt_ready(vcpu)) {
            x86_wait_interrupt();
            deliver_ticks(cpu, vcpu->vm);
        }
    }
    return action;
}

/**
 * @funcname `io_mask`
 *
 * @description
 * Returns the mask of the bits of an I/O access.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `size` | `uint32_t` | [in] The access size: 1, 2 or 4 bytes. |
 *
 * @return 0xFF, 0xFFFF or 0xFFFFFFFF.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mask` | `uint64_t` | The mask |
 *
 * @hlr
 * - \hlrid{HLR_SVM_IO_MASK_01}: `io_mask()` shall give the mask of the bits of
 *   a port access of 1, 2 or 4 bytes.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_DEV_001}
 *
 * @llr
 * - \llrid{LLR_SVM_IO_MASK_01}: If `size` is 1, `io_mask()` shall return
 *   `0xFF`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_IO_MASK_01}
 * - \llrid{LLR_SVM_IO_MASK_02}: If `size` is 2, `io_mask()` shall return
 *   `0xFFFF`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_IO_MASK_01}
 * - \llrid{LLR_SVM_IO_MASK_03}: If `size` is neither 1 nor 2, `io_mask()` shall
 *   return `0xFFFFFFFF`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_IO_MASK_01}
 *
 * @design
 * 1. If `size` is equal to `1U`: \llrref{LLR_SVM_IO_MASK_01}
 *    1. Set `mask` to `0xFFU`.
 * 2. Otherwise, if `size` is equal to `2U`: \llrref{LLR_SVM_IO_MASK_02}
 *    1. Set `mask` to `0xFFFFU`.
 * 3. Otherwise: \llrref{LLR_SVM_IO_MASK_03}
 *    1. Set `mask` to `0xFFFFFFFFU`.
 * 4. Return `mask`. \llrref{LLR_SVM_IO_MASK_01}, \llrref{LLR_SVM_IO_MASK_02},
 *    \llrref{LLR_SVM_IO_MASK_03}
 *
 * @calls
 * None.
 */
static uint64_t io_mask(uint32_t size)
{
    uint64_t mask;

    if (size == 1U) {
        mask = 0xFFU;
    } else if (size == 2U) {
        mask = 0xFFFFU;
    } else {
        mask = 0xFFFFFFFFU;
    }
    return mask;
}

/**
 * @funcname `handle_io`
 *
 * @description
 * Handles an IOIO exit: performs the port access (ioport.c), puts the value
 * read into RAX and goes on after the instruction. String I/O stops the VM.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vcpu` | `hv_vcpu_t *` | [in] The vCPU. |
 *
 * @return What the run loop does next (HV_RESET for a reset port).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vmcb` | `vmcb_t *` | The vCPU's VMCB |
 * | `info` | `uint64_t` | EXITINFO1: port, direction, size, string, REP |
 * | `port` | `uint16_t` | The port |
 * | `is_in` | `bool` | Whether it is a read |
 * | `size` | `uint32_t` | The access size in bytes |
 * | `value` | `uint32_t` | The value written or read |
 * | `action` | `hv_action_e` | What the run loop does next |
 * | `mask` | `uint64_t` | The bits of a read |
 *
 * @hlr
 * - \hlrid{HLR_SVM_HANDLE_IO_01}: `handle_io()` shall perform a port access of
 *   the guest with the device of the port, give a value read in EAX, and make
 *   the guest go on after the instruction.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_DEV_001}
 * - \hlrid{HLR_SVM_HANDLE_IO_02}: `handle_io()` shall stop a VM whose guest
 *   uses string I/O, with a message.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_ERR_001}
 *
 * @llr
 * - \llrid{LLR_SVM_HANDLE_IO_01}: If `SVM_IOIO_STR` or `SVM_IOIO_REP` is set in
 *   `exit_info1` of the control area of the VMCB, `handle_io()` shall call
 *   `hv_printf()` once with `"[hv] %%s: string I/O on port 0x%%x is not
 *   supported\n"`, the name of the VM and the port (the 16 bits of `exit_info1`
 *   from `SVM_IOIO_PORT_SHIFT`), and return `HV_STOP`, without calling
 *   `io_access()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_IO_02}
 * - \llrid{LLR_SVM_HANDLE_IO_02}: If neither `SVM_IOIO_STR` nor `SVM_IOIO_REP`
 *   is set in `exit_info1` and `SVM_IOIO_SZ8` is set, `handle_io()` shall call
 *   `io_access()` once with the VM, the port, the size 1, whether `SVM_IOIO_IN`
 *   is set, and the low 8 bits of `rax` of the save area, and return its
 *   result.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_IO_01}
 * - \llrid{LLR_SVM_HANDLE_IO_03}: If neither `SVM_IOIO_STR` nor `SVM_IOIO_REP`
 *   is set in `exit_info1`, `SVM_IOIO_SZ8` is not set and `SVM_IOIO_SZ16` is
 *   set, `handle_io()` shall call `io_access()` once with the VM, the port, the
 *   size 2, whether `SVM_IOIO_IN` is set, and the low 16 bits of `rax` of the
 *   save area, and return its result.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_IO_01}
 * - \llrid{LLR_SVM_HANDLE_IO_04}: If neither `SVM_IOIO_STR` nor `SVM_IOIO_REP`
 *   is set in `exit_info1`, and neither `SVM_IOIO_SZ8` nor `SVM_IOIO_SZ16` is
 *   set, `handle_io()` shall call `io_access()` once with the VM, the port, the
 *   size 4, whether `SVM_IOIO_IN` is set, and the low 32 bits of `rax` of the
 *   save area, and return its result.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_IO_01}
 * - \llrid{LLR_SVM_HANDLE_IO_05}: If neither `SVM_IOIO_STR` nor `SVM_IOIO_REP`
 *   is set in `exit_info1`, `SVM_IOIO_IN` is set and the size is 4,
 *   `handle_io()` shall set `rax` of the save area to the 32-bit value that
 *   `io_access()` gives, with its bits 63 to 32 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_IO_01}
 * - \llrid{LLR_SVM_HANDLE_IO_06}: If neither `SVM_IOIO_STR` nor `SVM_IOIO_REP`
 *   is set in `exit_info1`, `SVM_IOIO_IN` is set and the size is 1 or 2,
 *   `handle_io()` shall replace the low 8 or 16 bits of `rax` of the save area
 *   (`io_mask()`) with the value that `io_access()` gives, and keep the other
 *   bits of `rax`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_IO_01}
 * - \llrid{LLR_SVM_HANDLE_IO_07}: If neither `SVM_IOIO_STR` nor `SVM_IOIO_REP`
 *   is set in `exit_info1` and `SVM_IOIO_IN` is not set, `handle_io()` shall
 *   leave `rax` of the save area as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_IO_01}
 * - \llrid{LLR_SVM_HANDLE_IO_08}: If neither `SVM_IOIO_STR` nor `SVM_IOIO_REP`
 *   is set in `exit_info1`, `handle_io()` shall set `rip` of the save area to
 *   `exit_info2` and clear `SVM_INT_STATE_SHADOW` in `int_state` of the control
 *   area.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_IO_01}
 *
 * @design
 * 1. Set `vmcb` to `vcpu->arch.vmcb`. \llrref{LLR_SVM_HANDLE_IO_01},
 *    \llrref{LLR_SVM_HANDLE_IO_02}, \llrref{LLR_SVM_HANDLE_IO_03},
 *    \llrref{LLR_SVM_HANDLE_IO_04}, \llrref{LLR_SVM_HANDLE_IO_05},
 *    \llrref{LLR_SVM_HANDLE_IO_06}, \llrref{LLR_SVM_HANDLE_IO_07},
 *    \llrref{LLR_SVM_HANDLE_IO_08}
 * 2. Set `info` to `vmcb->control.exit_info1`. \llrref{LLR_SVM_HANDLE_IO_01},
 *    \llrref{LLR_SVM_HANDLE_IO_02}, \llrref{LLR_SVM_HANDLE_IO_03},
 *    \llrref{LLR_SVM_HANDLE_IO_04}, \llrref{LLR_SVM_HANDLE_IO_05},
 *    \llrref{LLR_SVM_HANDLE_IO_06}, \llrref{LLR_SVM_HANDLE_IO_07},
 *    \llrref{LLR_SVM_HANDLE_IO_08}
 * 3. Set `port` to `(uint16_t) (info >> SVM_IOIO_PORT_SHIFT)`.
 *    \llrref{LLR_SVM_HANDLE_IO_01}, \llrref{LLR_SVM_HANDLE_IO_02},
 *    \llrref{LLR_SVM_HANDLE_IO_03}, \llrref{LLR_SVM_HANDLE_IO_04}
 * 4. Set `is_in` to `(info & SVM_IOIO_IN) != 0U`.
 *    \llrref{LLR_SVM_HANDLE_IO_02}, \llrref{LLR_SVM_HANDLE_IO_03},
 *    \llrref{LLR_SVM_HANDLE_IO_04}, \llrref{LLR_SVM_HANDLE_IO_05},
 *    \llrref{LLR_SVM_HANDLE_IO_06}, \llrref{LLR_SVM_HANDLE_IO_07}
 * 5. Set `size` to `4U`. \llrref{LLR_SVM_HANDLE_IO_04}
 * 6. Set `value` to `(uint32_t) vmcb->save.rax`. \llrref{LLR_SVM_HANDLE_IO_02},
 *    \llrref{LLR_SVM_HANDLE_IO_03}, \llrref{LLR_SVM_HANDLE_IO_04}
 * 7. If `(info & SVM_IOIO_SZ8)` is not equal to `0U`:
 *    \llrref{LLR_SVM_HANDLE_IO_02}
 *    1. Set `size` to `1U`.
 * 8. Otherwise, if `(info & SVM_IOIO_SZ16)` is not equal to `0U`:
 *    \llrref{LLR_SVM_HANDLE_IO_03}, \llrref{LLR_SVM_HANDLE_IO_04}
 *    1. Set `size` to `2U`.
 * 9. If `(info & (SVM_IOIO_STR | SVM_IOIO_REP))` is not equal to `0U`:
 *    \llrref{LLR_SVM_HANDLE_IO_01}
 *    1. Call `hv_printf()` with the arguments
 *       `"[hv] %%s: string I/O on port 0x%%x is not supported\n"`,
 *       `vcpu->vm->config->name` and `(uint32_t) port`.
 *    2. Set `action` to `HV_STOP`.
 * 10. Otherwise: \llrref{LLR_SVM_HANDLE_IO_02}, \llrref{LLR_SVM_HANDLE_IO_03},
 *     \llrref{LLR_SVM_HANDLE_IO_04}, \llrref{LLR_SVM_HANDLE_IO_05},
 *     \llrref{LLR_SVM_HANDLE_IO_06}, \llrref{LLR_SVM_HANDLE_IO_07},
 *     \llrref{LLR_SVM_HANDLE_IO_08}
 *     1. Keep only the bits `(uint32_t) io_mask(size)` of `value`.
 *        \llrref{LLR_SVM_HANDLE_IO_02}, \llrref{LLR_SVM_HANDLE_IO_03},
 *        \llrref{LLR_SVM_HANDLE_IO_04}
 *     2. Call `io_access()` with the arguments `vcpu->vm`, `port`, `size`,
 *        `is_in` and the address of `value`, and store the result in `action`.
 *        \llrref{LLR_SVM_HANDLE_IO_02}, \llrref{LLR_SVM_HANDLE_IO_03},
 *        \llrref{LLR_SVM_HANDLE_IO_04}
 *     3. If `is_in` is true: \llrref{LLR_SVM_HANDLE_IO_05},
 *        \llrref{LLR_SVM_HANDLE_IO_06}, \llrref{LLR_SVM_HANDLE_IO_07}
 *        1. Call `io_mask()` with the argument `size`, and store the result in
 *           `mask`.
 *        2. If `size` is equal to `4U`: \llrref{LLR_SVM_HANDLE_IO_05}
 *           1. Set `vmcb->save.rax` to `(uint64_t) value`.
 *        3. Otherwise: \llrref{LLR_SVM_HANDLE_IO_06}
 *           1. Set `vmcb->save.rax` to
 *              `(vmcb->save.rax & ~mask) | ((uint64_t) value & mask)`.
 *     4. Set `vmcb->save.rip` to `vmcb->control.exit_info2`.
 *        \llrref{LLR_SVM_HANDLE_IO_08}
 *     5. Keep only the bits `~SVM_INT_STATE_SHADOW` of
 *        `vmcb->control.int_state`. \llrref{LLR_SVM_HANDLE_IO_08}
 * 11. Return `action`. \llrref{LLR_SVM_HANDLE_IO_01},
 *     \llrref{LLR_SVM_HANDLE_IO_02}, \llrref{LLR_SVM_HANDLE_IO_03},
 *     \llrref{LLR_SVM_HANDLE_IO_04}
 *
 * @calls
 * - `hv_printf()`
 * - `io_mask()`
 * - `io_access()`
 */
static hv_action_e handle_io(hv_vcpu_t *vcpu)
{
    vmcb_t *vmcb = vcpu->arch.vmcb;
    uint64_t info = vmcb->control.exit_info1;
    uint16_t port = (uint16_t) (info >> SVM_IOIO_PORT_SHIFT);
    bool is_in = (info & SVM_IOIO_IN) != 0U;
    uint32_t size = 4U;
    uint32_t value = (uint32_t) vmcb->save.rax;
    hv_action_e action;

    // EXITINFO1: port, direction, size; EXITINFO2: the next RIP.
    if ((info & SVM_IOIO_SZ8) != 0U) {
        size = 1U;
    } else if ((info & SVM_IOIO_SZ16) != 0U) {
        size = 2U;
    } else {
        // 32-bit access.
    }

    if ((info & (SVM_IOIO_STR | SVM_IOIO_REP)) != 0U) {
        hv_printf("[hv] %s: string I/O on port 0x%x is not supported\n",
                  vcpu->vm->config->name, (uint32_t) port);
        action = HV_STOP;
    } else {
        value &= (uint32_t) io_mask(size);
        action = io_access(vcpu->vm, port, size, is_in, &value);
        if (is_in) {
            uint64_t mask = io_mask(size);

            if (size == 4U) {
                // 32-bit results are zero-extended.
                vmcb->save.rax = (uint64_t) value;
            } else {
                // 8- and 16-bit results replace only AL or AX.
                vmcb->save.rax = (vmcb->save.rax & ~mask)
                                 | ((uint64_t) value & mask);
            }
        }
        vmcb->save.rip = vmcb->control.exit_info2;
        vmcb->control.int_state &= ~SVM_INT_STATE_SHADOW;
    }
    return action;
}

/**
 * @funcname `handle_cpuid`
 *
 * @description
 * Handles a CPUID exit: the host's values, without SVM, the APIC, MONITOR,
 * x2APIC and the topology, with one CPU and the hypervisor's vendor leaf.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vcpu` | `hv_vcpu_t *` | [in,out] The vCPU. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vmcb` | `vmcb_t *` | The vCPU's VMCB |
 * | `leaf` | `uint32_t` | The leaf (EAX) |
 * | `subleaf` | `uint32_t` | The subleaf (ECX) |
 * | `r` | `uint32_t[4]` | The results: EAX, EBX, ECX, EDX |
 *
 * @hlr
 * - \hlrid{HLR_SVM_HANDLE_CPUID_01}: `handle_cpuid()` shall give the guest the
 *   CPUID values of the host without SVM, the local APIC, x2APIC, MONITOR, VMX,
 *   the TSC deadline and the topology, with one CPU, the bit that tells that a
 *   hypervisor is present, and a vendor leaf of the hypervisor.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_004}
 *   - \srqref{SRQ_HV_DEV_002}
 *
 * @llr
 * - \llrid{LLR_SVM_HANDLE_CPUID_01}: `handle_cpuid()` shall take the four
 *   values of `x86_cpuid()` for the leaf (the low 32 bits of `rax` of the save
 *   area of the VMCB) and the subleaf (the low 32 bits of `rcx` of the saved
 *   registers of the vCPU), change them as the leaf requires, store them in
 *   `rax` of the save area and in `rbx`, `rcx` and `rdx` of the saved
 *   registers, with their bits 63 to 32 0, and call `skip_instruction()` once
 *   with the VMCB and `INSN_LEN_CPUID`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_CPUID_01}
 * - \llrid{LLR_SVM_HANDLE_CPUID_02}: If the leaf is 1, `handle_cpuid()` shall
 *   keep the low 16 bits of EBX, set its bits 23 to 16 to 1 and its bits 31 to
 *   24 to the index of the vCPU, clear `CPUID_1_ECX_MONITOR`,
 *   `CPUID_1_ECX_VMX`, `CPUID_1_ECX_X2APIC` and `CPUID_1_ECX_DEADLINE` and set
 *   `CPUID_1_ECX_HYPERVISOR` in ECX, and clear `CPUID_1_EDX_APIC` in EDX.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_CPUID_01}
 * - \llrid{LLR_SVM_HANDLE_CPUID_03}: If the leaf is 4, `handle_cpuid()` shall
 *   keep only the low 14 bits of EAX.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_CPUID_01}
 * - \llrid{LLR_SVM_HANDLE_CPUID_04}: If the leaf is `0xB`, `0x1F` or
 *   `CPUID_SVM_FEATURES`, `handle_cpuid()` shall set the four values to 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_CPUID_01}
 * - \llrid{LLR_SVM_HANDLE_CPUID_05}: If the leaf is `CPUID_EXT_FEATURES`,
 *   `handle_cpuid()` shall clear `CPUID_EXT_ECX_SVM` and `CPUID_EXT_ECX_SKINIT`
 *   in ECX.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_CPUID_01}
 * - \llrid{LLR_SVM_HANDLE_CPUID_06}: If the leaf is `CPUID_HV_BASE`,
 *   `handle_cpuid()` shall set EAX to `CPUID_HV_BASE`, EBX to `0x4F74654A`, ECX
 *   to `0x56482053` and EDX to `0x20202020`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_CPUID_01}
 * - \llrid{LLR_SVM_HANDLE_CPUID_07}: If the leaf is more than `CPUID_HV_BASE`
 *   and is not more than `CPUID_HV_LAST`, `handle_cpuid()` shall set the four
 *   values to 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_CPUID_01}
 * - \llrid{LLR_SVM_HANDLE_CPUID_08}: If the leaf is none of 1, 4, `0xB`,
 *   `0x1F`, `CPUID_SVM_FEATURES` and `CPUID_EXT_FEATURES`, and is not from
 *   `CPUID_HV_BASE` to `CPUID_HV_LAST`, `handle_cpuid()` shall leave the four
 *   values of `x86_cpuid()` as they are.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_CPUID_01}
 *
 * @design
 * 1. Set `vmcb` to `vcpu->arch.vmcb`. \llrref{LLR_SVM_HANDLE_CPUID_01}
 * 2. Set `leaf` to `(uint32_t) vmcb->save.rax`.
 *    \llrref{LLR_SVM_HANDLE_CPUID_01}
 * 3. Set `subleaf` to `(uint32_t) vcpu->arch.regs.rcx`.
 *    \llrref{LLR_SVM_HANDLE_CPUID_01}
 * 4. Call `x86_cpuid()` with the arguments `leaf`, `subleaf` and `r`.
 *    \llrref{LLR_SVM_HANDLE_CPUID_01}
 * 5. If `leaf` is equal to `1U`: \llrref{LLR_SVM_HANDLE_CPUID_02}
 *    1. Set `r[1]` to
 *       `(r[1] & 0x0000FFFFU) | ((uint32_t) 1U << 16) | (vcpu->index << 24)`.
 *    2. Keep only the bits
 *       `~(CPUID_1_ECX_MONITOR | CPUID_1_ECX_VMX | CPUID_1_ECX_X2APIC |`
 *       `CPUID_1_ECX_DEADLINE)` of `r[2]`.
 *    3. Set the bits `CPUID_1_ECX_HYPERVISOR` in `r[2]`.
 *    4. Keep only the bits `~CPUID_1_EDX_APIC` of `r[3]`.
 * 6. Otherwise, if `leaf` is equal to `4U`: \llrref{LLR_SVM_HANDLE_CPUID_03}
 *    1. Keep only the bits `0x00003FFFU` of `r[0]`.
 * 7. Otherwise, if `leaf` is equal to `0xBU` or `leaf` is equal to `0x1FU` or
 *    `leaf` is equal to `CPUID_SVM_FEATURES`: \llrref{LLR_SVM_HANDLE_CPUID_04}
 *    1. Set `r[0]` to `0U`.
 *    2. Set `r[1]` to `0U`.
 *    3. Set `r[2]` to `0U`.
 *    4. Set `r[3]` to `0U`.
 * 8. Otherwise, if `leaf` is equal to `CPUID_EXT_FEATURES`:
 *    \llrref{LLR_SVM_HANDLE_CPUID_05}
 *    1. Keep only the bits `~(CPUID_EXT_ECX_SVM | CPUID_EXT_ECX_SKINIT)` of
 *       `r[2]`.
 * 9. Otherwise, if `leaf` is equal to `CPUID_HV_BASE`:
 *    \llrref{LLR_SVM_HANDLE_CPUID_06}
 *    1. Set `r[0]` to `CPUID_HV_BASE`.
 *    2. Set `r[1]` to `0x4F74654AU`.
 *    3. Set `r[2]` to `0x56482053U`.
 *    4. Set `r[3]` to `0x20202020U`.
 * 10. Otherwise, if `leaf` is greater than `CPUID_HV_BASE` and `leaf` is less
 *     than or equal to `CPUID_HV_LAST`: \llrref{LLR_SVM_HANDLE_CPUID_07},
 *     \llrref{LLR_SVM_HANDLE_CPUID_08}
 *     1. Set `r[0]` to `0U`.
 *     2. Set `r[1]` to `0U`.
 *     3. Set `r[2]` to `0U`.
 *     4. Set `r[3]` to `0U`.
 * 11. Set `vmcb->save.rax` to `r[0]`. \llrref{LLR_SVM_HANDLE_CPUID_01}
 * 12. Set `vcpu->arch.regs.rbx` to `r[1]`. \llrref{LLR_SVM_HANDLE_CPUID_01}
 * 13. Set `vcpu->arch.regs.rcx` to `r[2]`. \llrref{LLR_SVM_HANDLE_CPUID_01}
 * 14. Set `vcpu->arch.regs.rdx` to `r[3]`. \llrref{LLR_SVM_HANDLE_CPUID_01}
 * 15. Call `skip_instruction()` with the arguments `vmcb` and `INSN_LEN_CPUID`.
 *     \llrref{LLR_SVM_HANDLE_CPUID_01}
 *
 * @calls
 * - `x86_cpuid()`
 * - `skip_instruction()`
 */
static void handle_cpuid(hv_vcpu_t *vcpu)
{
    vmcb_t *vmcb = vcpu->arch.vmcb;
    uint32_t leaf = (uint32_t) vmcb->save.rax;
    uint32_t subleaf = (uint32_t) vcpu->arch.regs.rcx;
    uint32_t r[4];

    // Start from the host's values and change what the guest must not see.
    x86_cpuid(leaf, subleaf, r);
    if (leaf == 1U) {
        // One logical CPU, APIC ID = vCPU number; no APIC, no VMX.
        r[1] = (r[1] & 0x0000FFFFU) | ((uint32_t) 1U << 16)
               | (vcpu->index << 24);
        r[2] &= ~(CPUID_1_ECX_MONITOR | CPUID_1_ECX_VMX
                  | CPUID_1_ECX_X2APIC | CPUID_1_ECX_DEADLINE);
        r[2] |= CPUID_1_ECX_HYPERVISOR;
        r[3] &= ~CPUID_1_EDX_APIC;
    } else if (leaf == 4U) {
        r[0] &= 0x00003FFFU;    // One core, no shared threads
    } else if ((leaf == 0xBU) || (leaf == 0x1FU)
               || (leaf == CPUID_SVM_FEATURES)) {
        r[0] = 0U;              // No topology, no SVM
        r[1] = 0U;
        r[2] = 0U;
        r[3] = 0U;
    } else if (leaf == CPUID_EXT_FEATURES) {
        r[2] &= ~(CPUID_EXT_ECX_SVM | CPUID_EXT_ECX_SKINIT);   // No nesting
    } else if (leaf == CPUID_HV_BASE) {
        r[0] = CPUID_HV_BASE;   // Vendor "JetOS HV    "
        r[1] = 0x4F74654AU;
        r[2] = 0x56482053U;
        r[3] = 0x20202020U;
    } else if ((leaf > CPUID_HV_BASE) && (leaf <= CPUID_HV_LAST)) {
        // No other hypervisor leaves.
        r[0] = 0U;
        r[1] = 0U;
        r[2] = 0U;
        r[3] = 0U;
    } else {
        // Other leaves are passed through.
    }
    // The results go to EAX-EDX (zero-extended to 64 bits).
    vmcb->save.rax = r[0];
    vcpu->arch.regs.rbx = r[1];
    vcpu->arch.regs.rcx = r[2];
    vcpu->arch.regs.rdx = r[3];
    skip_instruction(vmcb, INSN_LEN_CPUID);
}

/**
 * @funcname `log_msr`
 *
 * @description
 * Reports an MSR access that is ignored, once per MSR (up to 16 MSRs).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vcpu` | `const hv_vcpu_t *` | [in] The vCPU. |
 * | `msr` | `uint32_t` | [in] The MSR. |
 * | `write` | `bool` | [in] true for WRMSR. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `logged` | `static uint32_t[NB_LOGGED_MSRS]` | The MSRs reported |
 * | `nb_logged` | `static uint32_t` | The number of MSRs reported |
 * | `found` | `bool` | Whether the MSR was reported |
 * | `i` | `uint32_t` | The index in logged |
 *
 * @hlr
 * - \hlrid{HLR_SVM_LOG_MSR_01}: `log_msr()` shall report the first access to a
 *   model specific register that is ignored, for the first 16 such registers.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_006}
 *   - \srqref{SRQ_HV_CON_003}
 *
 * @llr
 * - \llrid{LLR_SVM_LOG_MSR_01}: If `msr` is not one of the `nb_logged`
 *   registers of `logged`, `nb_logged` is less than `NB_LOGGED_MSRS` and
 *   `write` is true, `log_msr()` shall store `msr` in `logged[nb_logged]`,
 *   increment `nb_logged` and call `hv_printf()` once with `"[hv] %%s: %%s of
 *   MSR 0x%%x ignored\n"`, the name of the VM, `"write"` and `msr`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_LOG_MSR_01}
 * - \llrid{LLR_SVM_LOG_MSR_02}: If `msr` is not one of the `nb_logged`
 *   registers of `logged`, `nb_logged` is less than `NB_LOGGED_MSRS` and
 *   `write` is false, `log_msr()` shall store `msr` in `logged[nb_logged]`,
 *   increment `nb_logged` and call `hv_printf()` once with `"[hv] %%s: %%s of
 *   MSR 0x%%x ignored\n"`, the name of the VM, `"read"` and `msr`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_LOG_MSR_01}
 * - \llrid{LLR_SVM_LOG_MSR_03}: If `msr` is one of the `nb_logged` registers of
 *   `logged`, or `nb_logged` is `NB_LOGGED_MSRS`, `log_msr()` shall leave
 *   `logged` and `nb_logged` as they are and not call `hv_printf()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_LOG_MSR_01}
 *
 * @design
 * 1. Set `found` to `false`. \llrref{LLR_SVM_LOG_MSR_01},
 *    \llrref{LLR_SVM_LOG_MSR_02}, \llrref{LLR_SVM_LOG_MSR_03}
 * 2. Set `i` to `0U`; then while `i` is less than `nb_logged` and `found` is
 *    false, repeat (after each pass: increment `i`):
 *    \llrref{LLR_SVM_LOG_MSR_01}, \llrref{LLR_SVM_LOG_MSR_02},
 *    \llrref{LLR_SVM_LOG_MSR_03}
 *    1. Set `found` to `(logged[i] == msr)`.
 * 3. If `found` is false and `nb_logged` is less than `NB_LOGGED_MSRS`:
 *    \llrref{LLR_SVM_LOG_MSR_01}, \llrref{LLR_SVM_LOG_MSR_02},
 *    \llrref{LLR_SVM_LOG_MSR_03}
 *    1. Set `logged[nb_logged]` to `msr`.
 *    2. Increment `nb_logged`.
 *    3. Call `hv_printf()` with the arguments
 *       `"[hv] %%s: %%s of MSR 0x%%x ignored\n"`, `vcpu->vm->config->name`,
 *       `"write"` if `write` is true, otherwise `"read"` and `msr`.
 *
 * @calls
 * - `hv_printf()`
 */
static void log_msr(const hv_vcpu_t *vcpu, uint32_t msr, bool write)
{
    static uint32_t logged[NB_LOGGED_MSRS];
    static uint32_t nb_logged = 0U;
    bool found = false;

    for (uint32_t i = 0U; (i < nb_logged) && !found; i++) {
        found = (logged[i] == msr);
    }
    // Report each MSR once; beyond the table, nothing more is reported.
    if (!found && (nb_logged < NB_LOGGED_MSRS)) {
        logged[nb_logged] = msr;
        nb_logged++;
        hv_printf("[hv] %s: %s of MSR 0x%x ignored\n",
                  vcpu->vm->config->name, write ? "write" : "read", msr);
    }
}

/**
 * @funcname `vmcb_msr`
 *
 * @description
 * Returns the VMCB field that holds a guest MSR (PAT, the SYSCALL and
 * SYSENTER MSRs, FS/GS bases), or NULL for the others.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `vmcb_save_t *` | [in] The VMCB save area. |
 * | `msr` | `uint32_t` | [in] The MSR. |
 *
 * @return The field, or NULL.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `field` | `uint64_t *` | The field |
 *
 * @hlr
 * - \hlrid{HLR_SVM_VMCB_MSR_01}: `vmcb_msr()` shall give the place in the
 *   control block of a model specific register that the guest may use: the page
 *   attribute table, the system call registers and the segment base registers.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_006}
 *
 * @llr
 * - \llrid{LLR_SVM_VMCB_MSR_01}: If `msr` is `MSR_PAT`, `MSR_STAR`,
 *   `MSR_LSTAR`, `MSR_CSTAR`, `MSR_SFMASK`, `MSR_KERNEL_GS_BASE`,
 *   `MSR_FS_BASE`, `MSR_GS_BASE`, `MSR_SYSENTER_CS`, `MSR_SYSENTER_ESP` or
 *   `MSR_SYSENTER_EIP`, `vmcb_msr()` shall return the address of `g_pat`,
 *   `star`, `lstar`, `cstar`, `sfmask`, `kernel_gs_base`, `fs.base`, `gs.base`,
 *   `sysenter_cs`, `sysenter_esp` or `sysenter_eip` of the save area, in this
 *   order.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_VMCB_MSR_01}
 * - \llrid{LLR_SVM_VMCB_MSR_02}: If `msr` is none of `MSR_PAT`, `MSR_STAR`,
 *   `MSR_LSTAR`, `MSR_CSTAR`, `MSR_SFMASK`, `MSR_KERNEL_GS_BASE`,
 *   `MSR_FS_BASE`, `MSR_GS_BASE`, `MSR_SYSENTER_CS`, `MSR_SYSENTER_ESP` and
 *   `MSR_SYSENTER_EIP`, `vmcb_msr()` shall return NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_VMCB_MSR_01}
 *
 * @design
 * 1. Depending on `msr`: \llrref{LLR_SVM_VMCB_MSR_01},
 *    \llrref{LLR_SVM_VMCB_MSR_02}
 *    1. Case `MSR_PAT`: \llrref{LLR_SVM_VMCB_MSR_01}
 *       1. Set `field` to the address of `s->g_pat`.
 *       2. Leave the switch.
 *    2. Case `MSR_STAR`: \llrref{LLR_SVM_VMCB_MSR_01}
 *       1. Set `field` to the address of `s->star`.
 *       2. Leave the switch.
 *    3. Case `MSR_LSTAR`: \llrref{LLR_SVM_VMCB_MSR_01}
 *       1. Set `field` to the address of `s->lstar`.
 *       2. Leave the switch.
 *    4. Case `MSR_CSTAR`: \llrref{LLR_SVM_VMCB_MSR_01}
 *       1. Set `field` to the address of `s->cstar`.
 *       2. Leave the switch.
 *    5. Case `MSR_SFMASK`: \llrref{LLR_SVM_VMCB_MSR_01}
 *       1. Set `field` to the address of `s->sfmask`.
 *       2. Leave the switch.
 *    6. Case `MSR_KERNEL_GS_BASE`: \llrref{LLR_SVM_VMCB_MSR_01}
 *       1. Set `field` to the address of `s->kernel_gs_base`.
 *       2. Leave the switch.
 *    7. Case `MSR_FS_BASE`: \llrref{LLR_SVM_VMCB_MSR_01}
 *       1. Set `field` to the address of `s->fs.base`.
 *       2. Leave the switch.
 *    8. Case `MSR_GS_BASE`: \llrref{LLR_SVM_VMCB_MSR_01}
 *       1. Set `field` to the address of `s->gs.base`.
 *       2. Leave the switch.
 *    9. Case `MSR_SYSENTER_CS`: \llrref{LLR_SVM_VMCB_MSR_01}
 *       1. Set `field` to the address of `s->sysenter_cs`.
 *       2. Leave the switch.
 *    10. Case `MSR_SYSENTER_ESP`: \llrref{LLR_SVM_VMCB_MSR_01}
 *        1. Set `field` to the address of `s->sysenter_esp`.
 *        2. Leave the switch.
 *    11. Case `MSR_SYSENTER_EIP`: \llrref{LLR_SVM_VMCB_MSR_01}
 *        1. Set `field` to the address of `s->sysenter_eip`.
 *        2. Leave the switch.
 *    12. Otherwise (default): \llrref{LLR_SVM_VMCB_MSR_02}
 *        1. Set `field` to `NULL`.
 *        2. Leave the switch.
 * 2. Return `field`. \llrref{LLR_SVM_VMCB_MSR_01}, \llrref{LLR_SVM_VMCB_MSR_02}
 *
 * @calls
 * None.
 */
static uint64_t *vmcb_msr(vmcb_save_t *s, uint32_t msr)
{
    uint64_t *field;

    switch (msr) {
        case MSR_PAT:
            field = &s->g_pat;
            break;
        case MSR_STAR:
            field = &s->star;
            break;
        case MSR_LSTAR:
            field = &s->lstar;
            break;
        case MSR_CSTAR:
            field = &s->cstar;
            break;
        case MSR_SFMASK:
            field = &s->sfmask;
            break;
        case MSR_KERNEL_GS_BASE:
            field = &s->kernel_gs_base;
            break;
        case MSR_FS_BASE:
            field = &s->fs.base;
            break;
        case MSR_GS_BASE:
            field = &s->gs.base;
            break;
        case MSR_SYSENTER_CS:
            field = &s->sysenter_cs;
            break;
        case MSR_SYSENTER_ESP:
            field = &s->sysenter_esp;
            break;
        case MSR_SYSENTER_EIP:
            field = &s->sysenter_eip;
            break;
        default:
            field = NULL;
            break;
    }
    return field;
}

/**
 * @funcname `handle_msr`
 *
 * @description
 * Handles an RDMSR or WRMSR exit: EFER (with SVME hidden), the TSC (through
 * the TSC offset) and the MSRs kept in the VMCB; others read 0 and ignore
 * writes.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vcpu` | `hv_vcpu_t *` | [in,out] The vCPU. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vmcb` | `vmcb_t *` | The vCPU's VMCB |
 * | `msr` | `uint32_t` | The MSR (ECX) |
 * | `write` | `bool` | Whether it is WRMSR |
 * | `value` | `uint64_t` | The value written (EDX:EAX) or read |
 * | `field` | `uint64_t *` | The VMCB field of the MSR, or NULL |
 *
 * @hlr
 * - \hlrid{HLR_SVM_HANDLE_MSR_01}: `handle_msr()` shall emulate a read or write
 *   of a model specific register: EFER without SVM visible, the time stamp
 *   counter through the offset, and the registers kept in the control block.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_006}
 * - \hlrid{HLR_SVM_HANDLE_MSR_02}: `handle_msr()` shall give zero for a read of
 *   another model specific register and ignore a write of it.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_006}
 *
 * @llr
 * - \llrid{LLR_SVM_HANDLE_MSR_01}: If the register (the low 32 bits of `rcx` of
 *   the saved registers of the vCPU) is `MSR_EFER` and `exit_info1` of the
 *   control area of the VMCB is 1 (a write), `handle_msr()` shall set `efer` of
 *   the save area to the value written (the low 32 bits of `rdx` of the saved
 *   registers as its high half and the low 32 bits of `rax` of the save area as
 *   its low half), with `EFER_SVME` set.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_MSR_01}
 * - \llrid{LLR_SVM_HANDLE_MSR_02}: If the register is `MSR_EFER` and
 *   `exit_info1` is not 1 (a read), `handle_msr()` shall take as the value read
 *   `efer` of the save area with `EFER_SVME` cleared.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_MSR_01}
 * - \llrid{LLR_SVM_HANDLE_MSR_03}: If the register is `MSR_TSC` and
 *   `exit_info1` is 1, `handle_msr()` shall set `tsc_offset` of the control
 *   area to the value written minus the result of `x86_rdtsc()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_MSR_01}
 * - \llrid{LLR_SVM_HANDLE_MSR_04}: If the register is `MSR_TSC` and
 *   `exit_info1` is not 1, `handle_msr()` shall take as the value read the
 *   result of `x86_rdtsc()` plus `tsc_offset` of the control area.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_MSR_01}
 * - \llrid{LLR_SVM_HANDLE_MSR_05}: If the register is neither `MSR_EFER` nor
 *   `MSR_TSC`, `vmcb_msr()` returns a field for it and `exit_info1` is 1,
 *   `handle_msr()` shall store the value written in that field.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_MSR_01}
 * - \llrid{LLR_SVM_HANDLE_MSR_06}: If the register is neither `MSR_EFER` nor
 *   `MSR_TSC`, `vmcb_msr()` returns a field for it and `exit_info1` is not 1,
 *   `handle_msr()` shall take that field as the value read.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_MSR_01}
 * - \llrid{LLR_SVM_HANDLE_MSR_07}: If the register is neither `MSR_EFER` nor
 *   `MSR_TSC` and `vmcb_msr()` returns NULL for it, `handle_msr()` shall call
 *   `log_msr()` once with `vcpu`, the register and whether `exit_info1` is 1,
 *   and take 0 as the value read.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_MSR_02}
 * - \llrid{LLR_SVM_HANDLE_MSR_08}: If and only if `exit_info1` is not 1,
 *   `handle_msr()` shall store the low 32 bits of the value read in `rax` of
 *   the save area and its high 32 bits in `rdx` of the saved registers, with
 *   the bits 63 to 32 of both 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_MSR_01}
 *   - \hlrref{HLR_SVM_HANDLE_MSR_02}
 * - \llrid{LLR_SVM_HANDLE_MSR_09}: `handle_msr()` shall call
 *   `skip_instruction()` once with the VMCB and `INSN_LEN_MSR`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_MSR_01}
 *   - \hlrref{HLR_SVM_HANDLE_MSR_02}
 *
 * @design
 * 1. Set `vmcb` to `vcpu->arch.vmcb`. \llrref{LLR_SVM_HANDLE_MSR_01},
 *    \llrref{LLR_SVM_HANDLE_MSR_02}, \llrref{LLR_SVM_HANDLE_MSR_03},
 *    \llrref{LLR_SVM_HANDLE_MSR_04}, \llrref{LLR_SVM_HANDLE_MSR_05},
 *    \llrref{LLR_SVM_HANDLE_MSR_06}, \llrref{LLR_SVM_HANDLE_MSR_07},
 *    \llrref{LLR_SVM_HANDLE_MSR_08}, \llrref{LLR_SVM_HANDLE_MSR_09}
 * 2. Set `msr` to `(uint32_t) vcpu->arch.regs.rcx`.
 *    \llrref{LLR_SVM_HANDLE_MSR_01}, \llrref{LLR_SVM_HANDLE_MSR_02},
 *    \llrref{LLR_SVM_HANDLE_MSR_03}, \llrref{LLR_SVM_HANDLE_MSR_04},
 *    \llrref{LLR_SVM_HANDLE_MSR_05}, \llrref{LLR_SVM_HANDLE_MSR_06},
 *    \llrref{LLR_SVM_HANDLE_MSR_07}
 * 3. Set `write` to `vmcb->control.exit_info1 == 1U`.
 *    \llrref{LLR_SVM_HANDLE_MSR_01}, \llrref{LLR_SVM_HANDLE_MSR_02},
 *    \llrref{LLR_SVM_HANDLE_MSR_03}, \llrref{LLR_SVM_HANDLE_MSR_04},
 *    \llrref{LLR_SVM_HANDLE_MSR_05}, \llrref{LLR_SVM_HANDLE_MSR_06},
 *    \llrref{LLR_SVM_HANDLE_MSR_07}, \llrref{LLR_SVM_HANDLE_MSR_08}
 * 4. Set `value` to
 *    `(vcpu->arch.regs.rdx << 32) | (vmcb->save.rax & 0xFFFFFFFFU)`.
 *    \llrref{LLR_SVM_HANDLE_MSR_01}, \llrref{LLR_SVM_HANDLE_MSR_03},
 *    \llrref{LLR_SVM_HANDLE_MSR_05}
 * 5. Call `vmcb_msr()` with the arguments the address of `vmcb->save` and
 *    `msr`, and store the result in `field`. \llrref{LLR_SVM_HANDLE_MSR_05},
 *    \llrref{LLR_SVM_HANDLE_MSR_06}, \llrref{LLR_SVM_HANDLE_MSR_07}
 * 6. If `msr` is equal to `MSR_EFER`: \llrref{LLR_SVM_HANDLE_MSR_01},
 *    \llrref{LLR_SVM_HANDLE_MSR_02}
 *    1. If `write` is true: \llrref{LLR_SVM_HANDLE_MSR_01}
 *       1. Set `vmcb->save.efer` to `value | EFER_SVME`.
 *    2. Otherwise: \llrref{LLR_SVM_HANDLE_MSR_02}
 *       1. Set `value` to `vmcb->save.efer & ~EFER_SVME`.
 * 7. Otherwise, if `msr` is equal to `MSR_TSC`: \llrref{LLR_SVM_HANDLE_MSR_03},
 *    \llrref{LLR_SVM_HANDLE_MSR_04}
 *    1. If `write` is true: \llrref{LLR_SVM_HANDLE_MSR_03}
 *       1. Set `vmcb->control.tsc_offset` to `value - x86_rdtsc()`.
 *    2. Otherwise: \llrref{LLR_SVM_HANDLE_MSR_04}
 *       1. Set `value` to `x86_rdtsc() + vmcb->control.tsc_offset`.
 * 8. Otherwise, if `field` is not NULL: \llrref{LLR_SVM_HANDLE_MSR_05},
 *    \llrref{LLR_SVM_HANDLE_MSR_06}
 *    1. If `write` is true: \llrref{LLR_SVM_HANDLE_MSR_05}
 *       1. Set `*field` to `value`.
 *    2. Otherwise: \llrref{LLR_SVM_HANDLE_MSR_06}
 *       1. Set `value` to the value at `field`.
 * 9. Otherwise: \llrref{LLR_SVM_HANDLE_MSR_07}
 *    1. Call `log_msr()` with the arguments `vcpu`, `msr` and `write`.
 *    2. Set `value` to `0U`.
 * 10. If `write` is false: \llrref{LLR_SVM_HANDLE_MSR_08}
 *     1. Set `vmcb->save.rax` to `value & 0xFFFFFFFFU`.
 *     2. Set `vcpu->arch.regs.rdx` to `value >> 32`.
 * 11. Call `skip_instruction()` with the arguments `vmcb` and `INSN_LEN_MSR`.
 *     \llrref{LLR_SVM_HANDLE_MSR_09}
 *
 * @calls
 * - `vmcb_msr()`
 * - `x86_rdtsc()`
 * - `log_msr()`
 * - `skip_instruction()`
 */
static void handle_msr(hv_vcpu_t *vcpu)
{
    vmcb_t *vmcb = vcpu->arch.vmcb;
    uint32_t msr = (uint32_t) vcpu->arch.regs.rcx;
    bool write = vmcb->control.exit_info1 == 1U;   // 0: RDMSR, 1: WRMSR
    // WRMSR writes EDX:EAX.
    uint64_t value = (vcpu->arch.regs.rdx << 32)
                     | (vmcb->save.rax & 0xFFFFFFFFU);
    uint64_t *field = vmcb_msr(&vmcb->save, msr);

    if (msr == MSR_EFER) {
        // SVME stays set in the guest's EFER, but is hidden.
        if (write) {
            vmcb->save.efer = value | EFER_SVME;
        } else {
            value = vmcb->save.efer & ~EFER_SVME;
        }
    } else if (msr == MSR_TSC) {
        // The guest's TSC is the host's plus the VMCB offset.
        if (write) {
            vmcb->control.tsc_offset = value - x86_rdtsc();
        } else {
            value = x86_rdtsc() + vmcb->control.tsc_offset;
        }
    } else if (field != NULL) {
        // VMLOAD/VMRUN load the field into the CPU at the next VMRUN.
        if (write) {
            *field = value;
        } else {
            value = *field;
        }
    } else {
        log_msr(vcpu, msr, write);
        value = 0U;
    }
    // RDMSR returns EDX:EAX.
    if (!write) {
        vmcb->save.rax = value & 0xFFFFFFFFU;
        vcpu->arch.regs.rdx = value >> 32;
    }
    skip_instruction(vmcb, INSN_LEN_MSR);
}

/**
 * @funcname `report_state`
 *
 * @description
 * Prints the main registers of a vCPU when its VM stops.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vcpu` | `const hv_vcpu_t *` | [in] The vCPU. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `const vmcb_save_t *` | The VMCB save area |
 *
 * @hlr
 * - \hlrid{HLR_SVM_REPORT_STATE_01}: `report_state()` shall report the main
 *   registers of a vCPU whose VM stops.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_003}
 *   - \srqref{SRQ_HV_ERR_001}
 *
 * @llr
 * - \llrid{LLR_SVM_REPORT_STATE_01}: `report_state()` shall call `hv_printf()`
 *   once with `"[hv] %%s: RIP 0x%%lx RSP 0x%%lx RFLAGS 0x%%lx CS 0x%%x\n[hv]
 *   %%s: CR0 0x%%lx CR3 0x%%lx CR4 0x%%lx EFER 0x%%lx\n"`, the name of the VM,
 *   `rip`, `rsp`, `rflags` and the selector of `cs` of the save area of the
 *   VMCB of the vCPU, the name of the VM again, and `cr0`, `cr3`, `cr4` and
 *   `efer` of the save area.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_REPORT_STATE_01}
 *
 * @design
 * 1. Set `s` to the address of `vcpu->arch.vmcb->save`.
 *    \llrref{LLR_SVM_REPORT_STATE_01}
 * 2. Call `hv_printf()` with the arguments
 *    `"[hv] %%s: RIP 0x%%lx RSP 0x%%lx RFLAGS 0x%%lx CS 0x%%x\n"`
 *    `"[hv] %%s: CR0 0x%%lx CR3 0x%%lx CR4 0x%%lx EFER 0x%%lx\n"`,
 *    `vcpu->vm->config->name`, `s->rip`, `s->rsp`, `s->rflags`,
 *    `(uint32_t) s->cs.selector`, `vcpu->vm->config->name`, `s->cr0`, `s->cr3`,
 *    `s->cr4` and `s->efer`. \llrref{LLR_SVM_REPORT_STATE_01}
 *
 * @calls
 * - `hv_printf()`
 */
static void report_state(const hv_vcpu_t *vcpu)
{
    const vmcb_save_t *s = &vcpu->arch.vmcb->save;

    hv_printf("[hv] %s: RIP 0x%lx RSP 0x%lx RFLAGS 0x%lx CS 0x%x\n"
              "[hv] %s: CR0 0x%lx CR3 0x%lx CR4 0x%lx EFER 0x%lx\n",
              vcpu->vm->config->name, s->rip, s->rsp, s->rflags,
              (uint32_t) s->cs.selector, vcpu->vm->config->name, s->cr0,
              s->cr3, s->cr4, s->efer);
}

/**
 * @funcname `handle_exit`
 *
 * @description
 * Handles a \#VMEXIT of a vCPU.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cpu` | `hv_cpu_t *` | [in,out] The CPU. |
 *
 * @return What the run loop does next: HV_CONTINUE, HV_RESET or HV_STOP.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vcpu` | `hv_vcpu_t *` | The CPU's vCPU |
 * | `vmcb` | `vmcb_t *` | Its VMCB |
 * | `c` | `vmcb_control_t *` | The VMCB's control area |
 * | `name` | `const char *` | The VM's name |
 * | `action` | `hv_action_e` | What the run loop does next |
 *
 * @hlr
 * - \hlrid{HLR_SVM_HANDLE_EXIT_01}: `handle_exit()` shall handle an exit of a
 *   guest and tell the run loop what to do next.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_010}
 * - \hlrid{HLR_SVM_HANDLE_EXIT_02}: `handle_exit()` shall stop the VM, with a
 *   message and its registers, for an access outside its RAM, a triple fault,
 *   an invalid state or an unexpected exit.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_ERR_001}
 *   - \srqref{SRQ_HV_MEM_002}
 *
 * @llr
 * - \llrid{LLR_SVM_HANDLE_EXIT_01}: If and only if `SVM_EVENT_VALID` is set in
 *   `exit_int_info` of the control area of the VMCB of the vCPU of the CPU,
 *   `handle_exit()` shall set `event_inj` of the control area to
 *   `exit_int_info`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_EXIT_01}
 * - \llrid{LLR_SVM_HANDLE_EXIT_02}: If `exit_code` of the control area is
 *   `SVM_EXIT_INTR`, `handle_exit()` shall return `HV_CONTINUE` without calling
 *   a handler.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_EXIT_01}
 * - \llrid{LLR_SVM_HANDLE_EXIT_03}: If `exit_code` is `SVM_EXIT_HLT`,
 *   `handle_exit()` shall return the result of `handle_hlt()` for `cpu`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_EXIT_01}
 * - \llrid{LLR_SVM_HANDLE_EXIT_04}: If `exit_code` is `SVM_EXIT_IOIO`,
 *   `handle_exit()` shall return the result of `handle_io()` for the vCPU.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_EXIT_01}
 * - \llrid{LLR_SVM_HANDLE_EXIT_05}: If `exit_code` is `SVM_EXIT_CPUID`,
 *   `handle_exit()` shall call `handle_cpuid()` once with the vCPU and return
 *   `HV_CONTINUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_EXIT_01}
 * - \llrid{LLR_SVM_HANDLE_EXIT_06}: If `exit_code` is `SVM_EXIT_MSR`,
 *   `handle_exit()` shall call `handle_msr()` once with the vCPU and return
 *   `HV_CONTINUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_EXIT_01}
 * - \llrid{LLR_SVM_HANDLE_EXIT_07}: If `exit_code` is `SVM_EXIT_VMRUN`,
 *   `SVM_EXIT_VMMCALL`, `SVM_EXIT_VMLOAD`, `SVM_EXIT_VMSAVE`, `SVM_EXIT_STGI`,
 *   `SVM_EXIT_CLGI`, `SVM_EXIT_SKINIT`, `SVM_EXIT_INVLPGA`, `SVM_EXIT_MONITOR`
 *   or `SVM_EXIT_MWAIT`, `handle_exit()` shall call `inject_exception()` once
 *   with the VMCB and `EXCEPTION_UD`, and return `HV_CONTINUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_EXIT_01}
 * - \llrid{LLR_SVM_HANDLE_EXIT_08}: If `exit_code` is `SVM_EXIT_NPF`,
 *   `handle_exit()` shall call `hv_printf()` once with `"[hv] %%s: access to
 *   guest-physical address 0x%%lx outside its RAM (error code 0x%%lx)\n"`, the
 *   name of the VM, `exit_info2` and `exit_info1` of the control area, then
 *   `report_state()` once with the vCPU, and return `HV_STOP`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_EXIT_02}
 * - \llrid{LLR_SVM_HANDLE_EXIT_09}: If `exit_code` is `SVM_EXIT_SHUTDOWN`,
 *   `handle_exit()` shall call `hv_printf()` once with `"[hv] %%s: shutdown
 *   (triple fault)\n"` and the name of the VM, then `report_state()` once with
 *   the vCPU, and return `HV_STOP`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_EXIT_02}
 * - \llrid{LLR_SVM_HANDLE_EXIT_10}: If `exit_code` is `SVM_EXIT_INVALID`,
 *   `handle_exit()` shall call `hv_printf()` once with `"[hv] %%s: invalid
 *   guest state\n"` and the name of the VM, then `report_state()` once with the
 *   vCPU, and return `HV_STOP`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_EXIT_02}
 * - \llrid{LLR_SVM_HANDLE_EXIT_11}: If `exit_code` is none of `SVM_EXIT_INTR`,
 *   `SVM_EXIT_HLT`, `SVM_EXIT_IOIO`, `SVM_EXIT_CPUID`, `SVM_EXIT_MSR`,
 *   `SVM_EXIT_VMRUN`, `SVM_EXIT_VMMCALL`, `SVM_EXIT_VMLOAD`, `SVM_EXIT_VMSAVE`,
 *   `SVM_EXIT_STGI`, `SVM_EXIT_CLGI`, `SVM_EXIT_SKINIT`, `SVM_EXIT_INVLPGA`,
 *   `SVM_EXIT_MONITOR`, `SVM_EXIT_MWAIT`, `SVM_EXIT_NPF`, `SVM_EXIT_SHUTDOWN`
 *   and `SVM_EXIT_INVALID`, `handle_exit()` shall call `hv_printf()` once with
 *   `"[hv] %%s: unexpected exit 0x%%lx (0x%%lx, 0x%%lx)\n"`, the name of the
 *   VM, `exit_code`, `exit_info1` and `exit_info2`, then `report_state()` once
 *   with the vCPU, and return `HV_STOP`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HANDLE_EXIT_02}
 *
 * @design
 * 1. Set `vcpu` to the address of `cpu->vcpu`. \llrref{LLR_SVM_HANDLE_EXIT_01},
 *    \llrref{LLR_SVM_HANDLE_EXIT_02}, \llrref{LLR_SVM_HANDLE_EXIT_03},
 *    \llrref{LLR_SVM_HANDLE_EXIT_04}, \llrref{LLR_SVM_HANDLE_EXIT_05},
 *    \llrref{LLR_SVM_HANDLE_EXIT_06}, \llrref{LLR_SVM_HANDLE_EXIT_07},
 *    \llrref{LLR_SVM_HANDLE_EXIT_08}, \llrref{LLR_SVM_HANDLE_EXIT_09},
 *    \llrref{LLR_SVM_HANDLE_EXIT_10}, \llrref{LLR_SVM_HANDLE_EXIT_11}
 * 2. Set `vmcb` to `vcpu->arch.vmcb`. \llrref{LLR_SVM_HANDLE_EXIT_01},
 *    \llrref{LLR_SVM_HANDLE_EXIT_02}, \llrref{LLR_SVM_HANDLE_EXIT_03},
 *    \llrref{LLR_SVM_HANDLE_EXIT_04}, \llrref{LLR_SVM_HANDLE_EXIT_05},
 *    \llrref{LLR_SVM_HANDLE_EXIT_06}, \llrref{LLR_SVM_HANDLE_EXIT_07},
 *    \llrref{LLR_SVM_HANDLE_EXIT_08}, \llrref{LLR_SVM_HANDLE_EXIT_09},
 *    \llrref{LLR_SVM_HANDLE_EXIT_10}, \llrref{LLR_SVM_HANDLE_EXIT_11}
 * 3. Set `c` to the address of `vmcb->control`.
 *    \llrref{LLR_SVM_HANDLE_EXIT_01}, \llrref{LLR_SVM_HANDLE_EXIT_02},
 *    \llrref{LLR_SVM_HANDLE_EXIT_03}, \llrref{LLR_SVM_HANDLE_EXIT_04},
 *    \llrref{LLR_SVM_HANDLE_EXIT_05}, \llrref{LLR_SVM_HANDLE_EXIT_06},
 *    \llrref{LLR_SVM_HANDLE_EXIT_07}, \llrref{LLR_SVM_HANDLE_EXIT_08},
 *    \llrref{LLR_SVM_HANDLE_EXIT_09}, \llrref{LLR_SVM_HANDLE_EXIT_10},
 *    \llrref{LLR_SVM_HANDLE_EXIT_11}
 * 4. Set `name` to `vcpu->vm->config->name`. \llrref{LLR_SVM_HANDLE_EXIT_08},
 *    \llrref{LLR_SVM_HANDLE_EXIT_09}, \llrref{LLR_SVM_HANDLE_EXIT_10},
 *    \llrref{LLR_SVM_HANDLE_EXIT_11}
 * 5. Set `action` to `HV_CONTINUE`. \llrref{LLR_SVM_HANDLE_EXIT_02},
 *    \llrref{LLR_SVM_HANDLE_EXIT_05}, \llrref{LLR_SVM_HANDLE_EXIT_06},
 *    \llrref{LLR_SVM_HANDLE_EXIT_07}
 * 6. If `(c->exit_int_info & SVM_EVENT_VALID)` is not equal to `0U`:
 *    \llrref{LLR_SVM_HANDLE_EXIT_01}
 *    1. Set `c->event_inj` to `c->exit_int_info`.
 * 7. Depending on `c->exit_code`: \llrref{LLR_SVM_HANDLE_EXIT_02},
 *    \llrref{LLR_SVM_HANDLE_EXIT_03}, \llrref{LLR_SVM_HANDLE_EXIT_04},
 *    \llrref{LLR_SVM_HANDLE_EXIT_05}, \llrref{LLR_SVM_HANDLE_EXIT_06},
 *    \llrref{LLR_SVM_HANDLE_EXIT_07}, \llrref{LLR_SVM_HANDLE_EXIT_08},
 *    \llrref{LLR_SVM_HANDLE_EXIT_09}, \llrref{LLR_SVM_HANDLE_EXIT_10},
 *    \llrref{LLR_SVM_HANDLE_EXIT_11}
 *    1. Case `SVM_EXIT_INTR`: \llrref{LLR_SVM_HANDLE_EXIT_02}
 *       1. Leave the switch.
 *    2. Case `SVM_EXIT_HLT`: \llrref{LLR_SVM_HANDLE_EXIT_03}
 *       1. Call `handle_hlt()` with the argument `cpu`, and store the result in
 *          `action`.
 *       2. Leave the switch.
 *    3. Case `SVM_EXIT_IOIO`: \llrref{LLR_SVM_HANDLE_EXIT_04}
 *       1. Call `handle_io()` with the argument `vcpu`, and store the result in
 *          `action`.
 *       2. Leave the switch.
 *    4. Case `SVM_EXIT_CPUID`: \llrref{LLR_SVM_HANDLE_EXIT_05}
 *       1. Call `handle_cpuid()` with the argument `vcpu`.
 *       2. Leave the switch.
 *    5. Case `SVM_EXIT_MSR`: \llrref{LLR_SVM_HANDLE_EXIT_06}
 *       1. Call `handle_msr()` with the argument `vcpu`.
 *       2. Leave the switch.
 *    6. Case `SVM_EXIT_VMRUN` or `SVM_EXIT_VMMCALL` or `SVM_EXIT_VMLOAD` or
 *       `SVM_EXIT_VMSAVE` or `SVM_EXIT_STGI` or `SVM_EXIT_CLGI` or
 *       `SVM_EXIT_SKINIT` or `SVM_EXIT_INVLPGA` or `SVM_EXIT_MONITOR` or
 *       `SVM_EXIT_MWAIT`: \llrref{LLR_SVM_HANDLE_EXIT_07}
 *       1. Call `inject_exception()` with the arguments `vmcb` and
 *          `EXCEPTION_UD`.
 *       2. Leave the switch.
 *    7. Case `SVM_EXIT_NPF`: \llrref{LLR_SVM_HANDLE_EXIT_08}
 *       1. Call `hv_printf()` with the arguments
 *          `"[hv] %%s: access to guest-physical address 0x%%lx "`
 *          `"outside its RAM (error code 0x%%lx)\n"`, `name`, `c->exit_info2`
 *          and `c->exit_info1`.
 *       2. Call `report_state()` with the argument `vcpu`.
 *       3. Set `action` to `HV_STOP`.
 *       4. Leave the switch.
 *    8. Case `SVM_EXIT_SHUTDOWN`: \llrref{LLR_SVM_HANDLE_EXIT_09}
 *       1. Call `hv_printf()` with the arguments
 *          `"[hv] %%s: shutdown (triple fault)\n"` and `name`.
 *       2. Call `report_state()` with the argument `vcpu`.
 *       3. Set `action` to `HV_STOP`.
 *       4. Leave the switch.
 *    9. Case `SVM_EXIT_INVALID`: \llrref{LLR_SVM_HANDLE_EXIT_10}
 *       1. Call `hv_printf()` with the arguments
 *          `"[hv] %%s: invalid guest state\n"` and `name`.
 *       2. Call `report_state()` with the argument `vcpu`.
 *       3. Set `action` to `HV_STOP`.
 *       4. Leave the switch.
 *    10. Otherwise (default): \llrref{LLR_SVM_HANDLE_EXIT_11}
 *        1. Call `hv_printf()` with the arguments
 *           `"[hv] %%s: unexpected exit 0x%%lx (0x%%lx, 0x%%lx)\n"`, `name`,
 *           `c->exit_code`, `c->exit_info1` and `c->exit_info2`.
 *        2. Call `report_state()` with the argument `vcpu`.
 *        3. Set `action` to `HV_STOP`.
 *        4. Leave the switch.
 * 8. Return `action`. \llrref{LLR_SVM_HANDLE_EXIT_02},
 *    \llrref{LLR_SVM_HANDLE_EXIT_03}, \llrref{LLR_SVM_HANDLE_EXIT_04},
 *    \llrref{LLR_SVM_HANDLE_EXIT_05}, \llrref{LLR_SVM_HANDLE_EXIT_06},
 *    \llrref{LLR_SVM_HANDLE_EXIT_07}, \llrref{LLR_SVM_HANDLE_EXIT_08},
 *    \llrref{LLR_SVM_HANDLE_EXIT_09}, \llrref{LLR_SVM_HANDLE_EXIT_10},
 *    \llrref{LLR_SVM_HANDLE_EXIT_11}
 *
 * @calls
 * - `handle_hlt()`
 * - `handle_io()`
 * - `handle_cpuid()`
 * - `handle_msr()`
 * - `inject_exception()`
 * - `hv_printf()`
 * - `report_state()`
 */
static hv_action_e handle_exit(hv_cpu_t *cpu)
{
    hv_vcpu_t *vcpu = &cpu->vcpu;
    vmcb_t *vmcb = vcpu->arch.vmcb;
    vmcb_control_t *c = &vmcb->control;
    const char *name = vcpu->vm->config->name;
    hv_action_e action = HV_CONTINUE;

    // An event whose delivery was interrupted is delivered again.
    if ((c->exit_int_info & SVM_EVENT_VALID) != 0U) {
        c->event_inj = c->exit_int_info;
    }

    switch (c->exit_code) {
        case SVM_EXIT_INTR:
            // Handled by the host when svm_vmrun() set GIF.
            break;
        case SVM_EXIT_HLT:
            action = handle_hlt(cpu);
            break;
        case SVM_EXIT_IOIO:
            action = handle_io(vcpu);
            break;
        case SVM_EXIT_CPUID:
            handle_cpuid(vcpu);
            break;
        case SVM_EXIT_MSR:
            handle_msr(vcpu);
            break;
        case SVM_EXIT_VMRUN:
        case SVM_EXIT_VMMCALL:
        case SVM_EXIT_VMLOAD:
        case SVM_EXIT_VMSAVE:
        case SVM_EXIT_STGI:
        case SVM_EXIT_CLGI:
        case SVM_EXIT_SKINIT:
        case SVM_EXIT_INVLPGA:
        case SVM_EXIT_MONITOR:
        case SVM_EXIT_MWAIT:
            inject_exception(vmcb, EXCEPTION_UD);
            break;
        case SVM_EXIT_NPF:
            hv_printf("[hv] %s: access to guest-physical address 0x%lx "
                      "outside its RAM (error code 0x%lx)\n", name,
                      c->exit_info2, c->exit_info1);
            report_state(vcpu);
            action = HV_STOP;
            break;
        case SVM_EXIT_SHUTDOWN:
            hv_printf("[hv] %s: shutdown (triple fault)\n", name);
            report_state(vcpu);
            action = HV_STOP;
            break;
        case SVM_EXIT_INVALID:
            hv_printf("[hv] %s: invalid guest state\n", name);
            report_state(vcpu);
            action = HV_STOP;
            break;
        default:
            hv_printf("[hv] %s: unexpected exit 0x%lx (0x%lx, 0x%lx)\n",
                      name, c->exit_code, c->exit_info1, c->exit_info2);
            report_state(vcpu);
            action = HV_STOP;
            break;
    }
    return action;
}

/**
 * @funcname `hv_arch_vcpu_run`
 *
 * @description
 * The run loop of a vCPU: delivers the virtual timer ticks and interrupts,
 * runs the guest, handles its exit; a reset reloads the VM, a stop halts the
 * CPU. Doesn't return.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cpu` | `hv_cpu_t *` | [in,out] The CPU. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vcpu` | `hv_vcpu_t *` | The CPU's vCPU |
 * | `vm` | `hv_vm_t *` | Its VM |
 * | `c` | `vmcb_control_t *` | The VMCB's control area |
 * | `action` | `hv_action_e` | What to do after the exit |
 *
 * @hlr
 * - \hlrid{HLR_SVM_HV_ARCH_VCPU_RUN_01}: `hv_arch_vcpu_run()` shall run the
 *   guest of a vCPU for ever, handling its exits, with the timer ticks and
 *   interrupts delivered before each run.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_005}
 *   - \srqref{SRQ_HV_CPU_010}
 * - \hlrid{HLR_SVM_HV_ARCH_VCPU_RUN_02}: `hv_arch_vcpu_run()` shall load the VM
 *   again and restart it when its guest resets it.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_VM_007}
 * - \hlrid{HLR_SVM_HV_ARCH_VCPU_RUN_03}: `hv_arch_vcpu_run()` shall stop only
 *   this VM, with a message, halting its CPU, when it must stop.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_ERR_001}
 *   - \srqref{SRQ_HV_ERR_002}
 *   - \srqref{SRQ_HV_CPU_007}
 *
 * @llr
 * - \llrid{LLR_SVM_HV_ARCH_VCPU_RUN_01}: `hv_arch_vcpu_run()` shall, again and
 *   again without returning, call in this order `deliver_ticks()` with `cpu`
 *   and the VM of the vCPU of the CPU, `inject_interrupt()` with the vCPU,
 *   `svm_vmrun()` with the saved registers and `vmcb_pa` of the vCPU, and
 *   `handle_exit()` with `cpu`, and set `event_inj` of the control area of the
 *   VMCB of the vCPU to 0 between `svm_vmrun()` and `handle_exit()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HV_ARCH_VCPU_RUN_01}
 * - \llrid{LLR_SVM_HV_ARCH_VCPU_RUN_02}: If `flush_tlb` of the vCPU is true
 *   before `svm_vmrun()`, `hv_arch_vcpu_run()` shall set `tlb_control` of the
 *   control area to `SVM_TLB_FLUSH_ALL` and `flush_tlb` to false.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HV_ARCH_VCPU_RUN_01}
 * - \llrid{LLR_SVM_HV_ARCH_VCPU_RUN_03}: If `flush_tlb` of the vCPU is false
 *   before `svm_vmrun()`, `hv_arch_vcpu_run()` shall set `tlb_control` of the
 *   control area to `SVM_TLB_DO_NOTHING` and leave `flush_tlb` false.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HV_ARCH_VCPU_RUN_01}
 * - \llrid{LLR_SVM_HV_ARCH_VCPU_RUN_04}: If `handle_exit()` returns `HV_RESET`,
 *   `hv_arch_vcpu_run()` shall increment `resets` of the VM and call, in this
 *   order, `vm_print()` with the VM and `"reset"`, `vm_load()` with the VM and
 *   `hv_arch_vcpu_init()` with the vCPU, before it runs the guest again.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HV_ARCH_VCPU_RUN_02}
 * - \llrid{LLR_SVM_HV_ARCH_VCPU_RUN_05}: If `handle_exit()` returns `HV_STOP`,
 *   `hv_arch_vcpu_run()` shall call, in this order, `lapic_timer_stop()`,
 *   `vm_print()` with the VM and `"stopped"`, and `hv_halt_forever()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HV_ARCH_VCPU_RUN_03}
 * - \llrid{LLR_SVM_HV_ARCH_VCPU_RUN_06}: If `handle_exit()` returns neither
 *   `HV_RESET` nor `HV_STOP`, `hv_arch_vcpu_run()` shall run the guest again
 *   without calling `vm_print()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SVM_HV_ARCH_VCPU_RUN_01}
 *
 * @design
 * 1. Set `vcpu` to the address of `cpu->vcpu`.
 *    \llrref{LLR_SVM_HV_ARCH_VCPU_RUN_01},
 *    \llrref{LLR_SVM_HV_ARCH_VCPU_RUN_02},
 *    \llrref{LLR_SVM_HV_ARCH_VCPU_RUN_03}, \llrref{LLR_SVM_HV_ARCH_VCPU_RUN_04}
 * 2. Set `vm` to `vcpu->vm`. \llrref{LLR_SVM_HV_ARCH_VCPU_RUN_01},
 *    \llrref{LLR_SVM_HV_ARCH_VCPU_RUN_04}, \llrref{LLR_SVM_HV_ARCH_VCPU_RUN_05}
 * 3. While `true` is true: \llrref{LLR_SVM_HV_ARCH_VCPU_RUN_01}
 *    1. Set `c` to the address of `vcpu->arch.vmcb->control`.
 *       \llrref{LLR_SVM_HV_ARCH_VCPU_RUN_01},
 *       \llrref{LLR_SVM_HV_ARCH_VCPU_RUN_02},
 *       \llrref{LLR_SVM_HV_ARCH_VCPU_RUN_03}
 *    2. Call `deliver_ticks()` with the arguments `cpu` and `vm`.
 *       \llrref{LLR_SVM_HV_ARCH_VCPU_RUN_01}
 *    3. Call `inject_interrupt()` with the argument `vcpu`.
 *       \llrref{LLR_SVM_HV_ARCH_VCPU_RUN_01}
 *    4. Set `c->tlb_control` to `SVM_TLB_FLUSH_ALL` if `vcpu->arch.flush_tlb`
 *       is true, otherwise `SVM_TLB_DO_NOTHING`.
 *       \llrref{LLR_SVM_HV_ARCH_VCPU_RUN_02},
 *       \llrref{LLR_SVM_HV_ARCH_VCPU_RUN_03}
 *    5. Set `vcpu->arch.flush_tlb` to `false`.
 *       \llrref{LLR_SVM_HV_ARCH_VCPU_RUN_02},
 *       \llrref{LLR_SVM_HV_ARCH_VCPU_RUN_03}
 *    6. Call `svm_vmrun()` with the arguments the address of `vcpu->arch.regs`
 *       and `vcpu->arch.vmcb_pa`. \llrref{LLR_SVM_HV_ARCH_VCPU_RUN_01}
 *    7. Set `c->event_inj` to `0U`. \llrref{LLR_SVM_HV_ARCH_VCPU_RUN_01}
 *    8. Call `handle_exit()` with the argument `cpu`, and store the result in
 *       `action`. \llrref{LLR_SVM_HV_ARCH_VCPU_RUN_01}
 *    9. If `action` is equal to `HV_RESET`:
 *       \llrref{LLR_SVM_HV_ARCH_VCPU_RUN_04}
 *       1. Increment `vm->resets`.
 *       2. Call `vm_print()` with the arguments `vm` and `"reset"`.
 *       3. Call `vm_load()` with the argument `vm`.
 *       4. Call `hv_arch_vcpu_init()` with the argument `vcpu`.
 *    10. Otherwise, if `action` is equal to `HV_STOP`:
 *        \llrref{LLR_SVM_HV_ARCH_VCPU_RUN_05},
 *        \llrref{LLR_SVM_HV_ARCH_VCPU_RUN_06}
 *        1. Call `lapic_timer_stop()`.
 *        2. Call `vm_print()` with the arguments `vm` and `"stopped"`.
 *        3. Call `hv_halt_forever()`.
 *
 * @calls
 * - `deliver_ticks()`
 * - `inject_interrupt()`
 * - `svm_vmrun()`
 * - `handle_exit()`
 * - `vm_print()`
 * - `vm_load()`
 * - `hv_arch_vcpu_init()`
 * - `lapic_timer_stop()`
 * - `hv_halt_forever()`
 */
void hv_arch_vcpu_run(hv_cpu_t *cpu)
{
    hv_vcpu_t *vcpu = &cpu->vcpu;
    hv_vm_t *vm = vcpu->vm;

    while (true) {
        vmcb_control_t *c = &vcpu->arch.vmcb->control;
        hv_action_e action;

        // Timer ticks become IRQ 0, the next PIC interrupt goes to V_IRQ.
        deliver_ticks(cpu, vm);
        inject_interrupt(vcpu);
        // The guest's TLB is flushed after a (re)start only.
        c->tlb_control = vcpu->arch.flush_tlb ? SVM_TLB_FLUSH_ALL
                                         : SVM_TLB_DO_NOTHING;
        vcpu->arch.flush_tlb = false;

        svm_vmrun(&vcpu->arch.regs, vcpu->arch.vmcb_pa);

        // An injected event was delivered (or is in EXITINTINFO).
        c->event_inj = 0U;
        action = handle_exit(cpu);
        if (action == HV_RESET) {
            // As at power-on: the image is loaded again and restarts.
            vm->resets++;
            vm_print(vm, "reset");
            vm_load(vm);
            hv_arch_vcpu_init(vcpu);
        } else if (action == HV_STOP) {
            // Only this VM stops; the other CPUs go on.
            lapic_timer_stop();
            vm_print(vm, "stopped");
            hv_halt_forever();
        } else {
            // Run the guest again.
        }
    }
}
