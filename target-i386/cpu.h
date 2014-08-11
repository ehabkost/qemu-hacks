/*
 * i386 virtual CPU header
 *
 *  Copyright (c) 2003 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#ifndef CPU_I386_H
#define CPU_I386_H

#include "config.h"
#include "qemu-common.h"

#include "hw/i386/cpu.h"

#ifdef TARGET_X86_64
#define TARGET_LONG_BITS 64
#else
#define TARGET_LONG_BITS 32
#endif

/* target supports implicit self modifying code */
#define TARGET_HAS_SMC
/* support for self modifying code even if the modified instruction is
   close to the modifying instruction */
#define TARGET_HAS_PRECISE_SMC

#define TARGET_HAS_ICE 1

#ifdef TARGET_X86_64
#define ELF_MACHINE     EM_X86_64
#define ELF_MACHINE_UNAME "x86_64"
#else
#define ELF_MACHINE     EM_386
#define ELF_MACHINE_UNAME "i686"
#endif

#define CPUArchState struct CPUX86State

#include "exec/cpu-defs.h"

#include "fpu/softfloat.h"

/* i386-specific interrupt pending bits.  */
#define CPU_INTERRUPT_POLL      CPU_INTERRUPT_TGT_EXT_1
#define CPU_INTERRUPT_SMI       CPU_INTERRUPT_TGT_EXT_2
#define CPU_INTERRUPT_NMI       CPU_INTERRUPT_TGT_EXT_3
#define CPU_INTERRUPT_MCE       CPU_INTERRUPT_TGT_EXT_4
#define CPU_INTERRUPT_VIRQ      CPU_INTERRUPT_TGT_INT_0
#define CPU_INTERRUPT_SIPI      CPU_INTERRUPT_TGT_INT_1
#define CPU_INTERRUPT_TPR       CPU_INTERRUPT_TGT_INT_2

/* Use a clearer name for this.  */
#define CPU_INTERRUPT_INIT      CPU_INTERRUPT_RESET

typedef enum {
    CC_OP_DYNAMIC, /* must use dynamic code to get cc_op */
    CC_OP_EFLAGS,  /* all cc are explicitly computed, CC_SRC = flags */

    CC_OP_MULB, /* modify all flags, C, O = (CC_SRC != 0) */
    CC_OP_MULW,
    CC_OP_MULL,
    CC_OP_MULQ,

    CC_OP_ADDB, /* modify all flags, CC_DST = res, CC_SRC = src1 */
    CC_OP_ADDW,
    CC_OP_ADDL,
    CC_OP_ADDQ,

    CC_OP_ADCB, /* modify all flags, CC_DST = res, CC_SRC = src1 */
    CC_OP_ADCW,
    CC_OP_ADCL,
    CC_OP_ADCQ,

    CC_OP_SUBB, /* modify all flags, CC_DST = res, CC_SRC = src1 */
    CC_OP_SUBW,
    CC_OP_SUBL,
    CC_OP_SUBQ,

    CC_OP_SBBB, /* modify all flags, CC_DST = res, CC_SRC = src1 */
    CC_OP_SBBW,
    CC_OP_SBBL,
    CC_OP_SBBQ,

    CC_OP_LOGICB, /* modify all flags, CC_DST = res */
    CC_OP_LOGICW,
    CC_OP_LOGICL,
    CC_OP_LOGICQ,

    CC_OP_INCB, /* modify all flags except, CC_DST = res, CC_SRC = C */
    CC_OP_INCW,
    CC_OP_INCL,
    CC_OP_INCQ,

    CC_OP_DECB, /* modify all flags except, CC_DST = res, CC_SRC = C  */
    CC_OP_DECW,
    CC_OP_DECL,
    CC_OP_DECQ,

    CC_OP_SHLB, /* modify all flags, CC_DST = res, CC_SRC.msb = C */
    CC_OP_SHLW,
    CC_OP_SHLL,
    CC_OP_SHLQ,

    CC_OP_SARB, /* modify all flags, CC_DST = res, CC_SRC.lsb = C */
    CC_OP_SARW,
    CC_OP_SARL,
    CC_OP_SARQ,

    CC_OP_BMILGB, /* Z,S via CC_DST, C = SRC==0; O=0; P,A undefined */
    CC_OP_BMILGW,
    CC_OP_BMILGL,
    CC_OP_BMILGQ,

    CC_OP_ADCX, /* CC_DST = C, CC_SRC = rest.  */
    CC_OP_ADOX, /* CC_DST = O, CC_SRC = rest.  */
    CC_OP_ADCOX, /* CC_DST = C, CC_SRC2 = O, CC_SRC = rest.  */

    CC_OP_CLR, /* Z set, all other flags clear.  */

    CC_OP_NB,
} CCOp;

typedef struct SegmentCache {
    uint32_t selector;
    target_ulong base;
    uint32_t limit;
    uint32_t flags;
} SegmentCache;

typedef union {
    uint8_t _b[16];
    uint16_t _w[8];
    uint32_t _l[4];
    uint64_t _q[2];
    float32 _s[4];
    float64 _d[2];
} XMMReg;

typedef union {
    uint8_t _b[8];
    uint16_t _w[4];
    uint32_t _l[2];
    float32 _s[2];
    uint64_t q;
} MMXReg;

typedef struct BNDReg {
    uint64_t lb;
    uint64_t ub;
} BNDReg;

typedef struct BNDCSReg {
    uint64_t cfgu;
    uint64_t sts;
} BNDCSReg;

#ifdef HOST_WORDS_BIGENDIAN
#define XMM_B(n) _b[15 - (n)]
#define XMM_W(n) _w[7 - (n)]
#define XMM_L(n) _l[3 - (n)]
#define XMM_S(n) _s[3 - (n)]
#define XMM_Q(n) _q[1 - (n)]
#define XMM_D(n) _d[1 - (n)]

#define MMX_B(n) _b[7 - (n)]
#define MMX_W(n) _w[3 - (n)]
#define MMX_L(n) _l[1 - (n)]
#define MMX_S(n) _s[1 - (n)]
#else
#define XMM_B(n) _b[n]
#define XMM_W(n) _w[n]
#define XMM_L(n) _l[n]
#define XMM_S(n) _s[n]
#define XMM_Q(n) _q[n]
#define XMM_D(n) _d[n]

#define MMX_B(n) _b[n]
#define MMX_W(n) _w[n]
#define MMX_L(n) _l[n]
#define MMX_S(n) _s[n]
#endif
#define MMX_Q(n) q

typedef union {
    floatx80 d __attribute__((aligned(16)));
    MMXReg mmx;
} FPReg;

typedef struct {
    uint64_t base;
    uint64_t mask;
} MTRRVar;

#define CPU_NB_REGS64 16
#define CPU_NB_REGS32 8

#ifdef TARGET_X86_64
#define CPU_NB_REGS CPU_NB_REGS64
#else
#define CPU_NB_REGS CPU_NB_REGS32
#endif

#define MAX_FIXED_COUNTERS 3
#define MAX_GP_COUNTERS    (MSR_IA32_PERF_STATUS - MSR_P6_EVNTSEL0)

#define NB_MMU_MODES 3

typedef enum TPRAccess {
    TPR_ACCESS_READ,
    TPR_ACCESS_WRITE,
} TPRAccess;

typedef struct CPUX86State {
    /* standard registers */
    target_ulong regs[CPU_NB_REGS];
    target_ulong eip;
    target_ulong eflags; /* eflags register. During CPU emulation, CC
                        flags and DF are set to zero because they are
                        stored elsewhere */

    /* emulator internal eflags handling */
    target_ulong cc_dst;
    target_ulong cc_src;
    target_ulong cc_src2;
    uint32_t cc_op;
    int32_t df; /* D flag : 1 if D = 0, -1 if D = 1 */
    uint32_t hflags; /* TB flags, see HF_xxx constants. These flags
                        are known at translation time. */
    uint32_t hflags2; /* various other flags, see HF2_xxx constants. */

    /* segments */
    SegmentCache segs[6]; /* selector values */
    SegmentCache ldt;
    SegmentCache tr;
    SegmentCache gdt; /* only base and limit are used */
    SegmentCache idt; /* only base and limit are used */

    target_ulong cr[5]; /* NOTE: cr1 is unused */
    int32_t a20_mask;

    BNDReg bnd_regs[4];
    BNDCSReg bndcs_regs;
    uint64_t msr_bndcfgs;

    /* Beginning of state preserved by INIT (dummy marker).  */
    struct {} start_init_save;

    /* FPU state */
    unsigned int fpstt; /* top of stack index */
    uint16_t fpus;
    uint16_t fpuc;
    uint8_t fptags[8];   /* 0 = valid, 1 = empty */
    FPReg fpregs[8];
    /* KVM-only so far */
    uint16_t fpop;
    uint64_t fpip;
    uint64_t fpdp;

    /* emulator internal variables */
    float_status fp_status;
    floatx80 ft0;

    float_status mmx_status; /* for 3DNow! float ops */
    float_status sse_status;
    uint32_t mxcsr;
    XMMReg xmm_regs[CPU_NB_REGS];
    XMMReg xmm_t0;
    MMXReg mmx_t0;

    XMMReg ymmh_regs[CPU_NB_REGS];

    /* sysenter registers */
    uint32_t sysenter_cs;
    target_ulong sysenter_esp;
    target_ulong sysenter_eip;
    uint64_t efer;
    uint64_t star;

    uint64_t vm_hsave;

#ifdef TARGET_X86_64
    target_ulong lstar;
    target_ulong cstar;
    target_ulong fmask;
    target_ulong kernelgsbase;
#endif

    uint64_t tsc;
    uint64_t tsc_adjust;
    uint64_t tsc_deadline;

    uint64_t mcg_status;
    uint64_t msr_ia32_misc_enable;
    uint64_t msr_ia32_feature_control;

    uint64_t msr_fixed_ctr_ctrl;
    uint64_t msr_global_ctrl;
    uint64_t msr_global_status;
    uint64_t msr_global_ovf_ctrl;
    uint64_t msr_fixed_counters[MAX_FIXED_COUNTERS];
    uint64_t msr_gp_counters[MAX_GP_COUNTERS];
    uint64_t msr_gp_evtsel[MAX_GP_COUNTERS];

    uint64_t pat;
    uint32_t smbase;

    /* End of state preserved by INIT (dummy marker).  */
    struct {} end_init_save;

    uint64_t system_time_msr;
    uint64_t wall_clock_msr;
    uint64_t steal_time_msr;
    uint64_t async_pf_en_msr;
    uint64_t pv_eoi_en_msr;

    uint64_t msr_hv_hypercall;
    uint64_t msr_hv_guest_os_id;
    uint64_t msr_hv_vapic;
    uint64_t msr_hv_tsc;

    /* exception/interrupt handling */
    int error_code;
    int exception_is_int;
    target_ulong exception_next_eip;
    target_ulong dr[8]; /* debug registers */
    union {
        struct CPUBreakpoint *cpu_breakpoint[4];
        struct CPUWatchpoint *cpu_watchpoint[4];
    }; /* break/watchpoints for dr[0..3] */
    int old_exception;  /* exception in flight */

    uint64_t vm_vmcb;
    uint64_t tsc_offset;
    uint64_t intercept;
    uint16_t intercept_cr_read;
    uint16_t intercept_cr_write;
    uint16_t intercept_dr_read;
    uint16_t intercept_dr_write;
    uint32_t intercept_exceptions;
    uint8_t v_tpr;

    /* KVM states, automatically cleared on reset */
    uint8_t nmi_injected;
    uint8_t nmi_pending;

    CPU_COMMON

    /* Fields from here on are preserved across CPU reset. */

    /* processor features (e.g. for CPUID insn) */
    uint32_t cpuid_level;
    uint32_t cpuid_xlevel;
    uint32_t cpuid_xlevel2;
    uint32_t cpuid_vendor1;
    uint32_t cpuid_vendor2;
    uint32_t cpuid_vendor3;
    uint32_t cpuid_version;
    FeatureWordArray features;
    uint32_t cpuid_model[12];
    uint32_t cpuid_apic_id;

    /* MTRRs */
    uint64_t mtrr_fixed[11];
    uint64_t mtrr_deftype;
    MTRRVar mtrr_var[8];

    /* For KVM */
    uint32_t mp_state;
    int32_t exception_injected;
    int32_t interrupt_injected;
    uint8_t soft_interrupt;
    uint8_t has_error_code;
    uint32_t sipi_vector;
    bool tsc_valid;
    int tsc_khz;
    void *kvm_xsave_buf;

    uint64_t mcg_cap;
    uint64_t mcg_ctl;
    uint64_t mce_banks[MCE_BANKS_DEF*4];

    uint64_t tsc_aux;

    /* vmstate */
    uint16_t fpus_vmstate;
    uint16_t fptag_vmstate;
    uint16_t fpregs_format_vmstate;
    uint64_t xstate_bv;

    uint64_t xcr0;

    TPRAccess tpr_access_type;
} CPUX86State;

#include "cpu-qom.h"

X86CPU *cpu_x86_init(const char *cpu_model);
X86CPU *cpu_x86_create(const char *cpu_model, DeviceState *icc_bridge,
                       Error **errp);
int cpu_x86_exec(CPUX86State *s);
void x86_cpu_list(FILE *f, fprintf_function cpu_fprintf);
void x86_cpudef_setup(void);
int cpu_x86_support_mca_broadcast(CPUX86State *env);

int cpu_get_pic_interrupt(CPUX86State *s);
/* MSDOS compatibility mode FPU exception support */
void cpu_set_ferr(CPUX86State *s);

/* this function must always be used to load data in the segment
   cache: it synchronizes the hflags with the segment cache values */
static inline void cpu_x86_load_seg_cache(CPUX86State *env,
                                          int seg_reg, unsigned int selector,
                                          target_ulong base,
                                          unsigned int limit,
                                          unsigned int flags)
{
    SegmentCache *sc;
    unsigned int new_hflags;

    sc = &env->segs[seg_reg];
    sc->selector = selector;
    sc->base = base;
    sc->limit = limit;
    sc->flags = flags;

    /* update the hidden flags */
    {
        if (seg_reg == R_CS) {
#ifdef TARGET_X86_64
            if ((env->hflags & HF_LMA_MASK) && (flags & DESC_L_MASK)) {
                /* long mode */
                env->hflags |= HF_CS32_MASK | HF_SS32_MASK | HF_CS64_MASK;
                env->hflags &= ~(HF_ADDSEG_MASK);
            } else
#endif
            {
                /* legacy / compatibility case */
                new_hflags = (env->segs[R_CS].flags & DESC_B_MASK)
                    >> (DESC_B_SHIFT - HF_CS32_SHIFT);
                env->hflags = (env->hflags & ~(HF_CS32_MASK | HF_CS64_MASK)) |
                    new_hflags;
            }
        }
        if (seg_reg == R_SS) {
            int cpl = (flags >> DESC_DPL_SHIFT) & 3;
#if HF_CPL_MASK != 3
#error HF_CPL_MASK is hardcoded
#endif
            env->hflags = (env->hflags & ~HF_CPL_MASK) | cpl;
        }
        new_hflags = (env->segs[R_SS].flags & DESC_B_MASK)
            >> (DESC_B_SHIFT - HF_SS32_SHIFT);
        if (env->hflags & HF_CS64_MASK) {
            /* zero base assumed for DS, ES and SS in long mode */
        } else if (!(env->cr[0] & CR0_PE_MASK) ||
                   (env->eflags & VM_MASK) ||
                   !(env->hflags & HF_CS32_MASK)) {
            /* XXX: try to avoid this test. The problem comes from the
               fact that is real mode or vm86 mode we only modify the
               'base' and 'selector' fields of the segment cache to go
               faster. A solution may be to force addseg to one in
               translate-i386.c. */
            new_hflags |= HF_ADDSEG_MASK;
        } else {
            new_hflags |= ((env->segs[R_DS].base |
                            env->segs[R_ES].base |
                            env->segs[R_SS].base) != 0) <<
                HF_ADDSEG_SHIFT;
        }
        env->hflags = (env->hflags &
                       ~(HF_SS32_MASK | HF_ADDSEG_MASK)) | new_hflags;
    }
}

static inline void cpu_x86_load_seg_cache_sipi(X86CPU *cpu,
                                               int sipi_vector)
{
    CPUState *cs = CPU(cpu);
    CPUX86State *env = &cpu->env;

    env->eip = 0;
    cpu_x86_load_seg_cache(env, R_CS, sipi_vector << 8,
                           sipi_vector << 12,
                           env->segs[R_CS].limit,
                           env->segs[R_CS].flags);
    cs->halted = 0;
}

int cpu_x86_get_descr_debug(CPUX86State *env, unsigned int selector,
                            target_ulong *base, unsigned int *limit,
                            unsigned int *flags);

/* op_helper.c */
/* used for debug or cpu save/restore */
void cpu_get_fp80(uint64_t *pmant, uint16_t *pexp, floatx80 f);
floatx80 cpu_set_fp80(uint64_t mant, uint16_t upper);

/* cpu-exec.c */
/* the following helpers are only usable in user mode simulation as
   they can trigger unexpected exceptions */
void cpu_x86_load_seg(CPUX86State *s, int seg_reg, int selector);
void cpu_x86_fsave(CPUX86State *s, target_ulong ptr, int data32);
void cpu_x86_frstor(CPUX86State *s, target_ulong ptr, int data32);

/* you can call this signal handler from your SIGBUS and SIGSEGV
   signal handlers to inform the virtual CPU of exceptions. non zero
   is returned if the signal was handled by the virtual CPU.  */
int cpu_x86_signal_handler(int host_signum, void *pinfo,
                           void *puc);

/* cpuid.c */
void cpu_x86_cpuid(CPUX86State *env, uint32_t index, uint32_t count,
                   uint32_t *eax, uint32_t *ebx,
                   uint32_t *ecx, uint32_t *edx);
void cpu_clear_apic_feature(CPUX86State *env);
void host_cpuid(uint32_t function, uint32_t count,
                uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);

/* helper.c */
int x86_cpu_handle_mmu_fault(CPUState *cpu, vaddr addr,
                             int is_write, int mmu_idx);
void x86_cpu_set_a20(X86CPU *cpu, int a20_state);

static inline bool hw_local_breakpoint_enabled(unsigned long dr7, int index)
{
    return (dr7 >> (index * 2)) & 1;
}

static inline bool hw_global_breakpoint_enabled(unsigned long dr7, int index)
{
    return (dr7 >> (index * 2)) & 2;

}
static inline bool hw_breakpoint_enabled(unsigned long dr7, int index)
{
    return hw_global_breakpoint_enabled(dr7, index) ||
           hw_local_breakpoint_enabled(dr7, index);
}

static inline int hw_breakpoint_type(unsigned long dr7, int index)
{
    return (dr7 >> (DR7_TYPE_SHIFT + (index * 4))) & 3;
}

static inline int hw_breakpoint_len(unsigned long dr7, int index)
{
    int len = ((dr7 >> (DR7_LEN_SHIFT + (index * 4))) & 3);
    return (len == 2) ? 8 : len + 1;
}

void hw_breakpoint_insert(CPUX86State *env, int index);
void hw_breakpoint_remove(CPUX86State *env, int index);
bool check_hw_breakpoints(CPUX86State *env, bool force_dr6_update);
void breakpoint_handler(CPUX86State *env);

/* will be suppressed */
void cpu_x86_update_cr0(CPUX86State *env, uint32_t new_cr0);
void cpu_x86_update_cr3(CPUX86State *env, target_ulong new_cr3);
void cpu_x86_update_cr4(CPUX86State *env, uint32_t new_cr4);

/* hw/pc.c */
void cpu_smm_update(CPUX86State *env);
uint64_t cpu_get_tsc(CPUX86State *env);

#define TARGET_PAGE_BITS 12

#ifdef TARGET_X86_64
#define TARGET_PHYS_ADDR_SPACE_BITS 52
/* ??? This is really 48 bits, sign-extended, but the only thing
   accessible to userland with bit 48 set is the VSYSCALL, and that
   is handled via other mechanisms.  */
#define TARGET_VIRT_ADDR_SPACE_BITS 47
#else
#define TARGET_PHYS_ADDR_SPACE_BITS 36
#define TARGET_VIRT_ADDR_SPACE_BITS 32
#endif

/* XXX: This value should match the one returned by CPUID
 * and in exec.c */
# if defined(TARGET_X86_64)
# define PHYS_ADDR_MASK 0xffffffffffLL
# else
# define PHYS_ADDR_MASK 0xfffffffffLL
# endif

static inline CPUX86State *cpu_init(const char *cpu_model)
{
    X86CPU *cpu = cpu_x86_init(cpu_model);
    if (cpu == NULL) {
        return NULL;
    }
    return &cpu->env;
}

#define cpu_exec cpu_x86_exec
#define cpu_gen_code cpu_x86_gen_code
#define cpu_signal_handler cpu_x86_signal_handler
#define cpu_list x86_cpu_list
#define cpudef_setup x86_cpudef_setup

/* MMU modes definitions */
#define MMU_MODE0_SUFFIX _ksmap
#define MMU_MODE1_SUFFIX _user
#define MMU_MODE2_SUFFIX _knosmap /* SMAP disabled or CPL<3 && AC=1 */
#define MMU_KSMAP_IDX   0
#define MMU_USER_IDX    1
#define MMU_KNOSMAP_IDX 2
static inline int cpu_mmu_index(CPUX86State *env)
{
    return (env->hflags & HF_CPL_MASK) == 3 ? MMU_USER_IDX :
        (!(env->hflags & HF_SMAP_MASK) || (env->eflags & AC_MASK))
        ? MMU_KNOSMAP_IDX : MMU_KSMAP_IDX;
}

static inline int cpu_mmu_index_kernel(CPUX86State *env)
{
    return !(env->hflags & HF_SMAP_MASK) ? MMU_KNOSMAP_IDX :
        ((env->hflags & HF_CPL_MASK) < 3 && (env->eflags & AC_MASK))
        ? MMU_KNOSMAP_IDX : MMU_KSMAP_IDX;
}

#define CC_DST  (env->cc_dst)
#define CC_SRC  (env->cc_src)
#define CC_SRC2 (env->cc_src2)
#define CC_OP   (env->cc_op)

/* n must be a constant to be efficient */
static inline target_long lshift(target_long x, int n)
{
    if (n >= 0) {
        return x << n;
    } else {
        return x >> (-n);
    }
}

/* float macros */
#define FT0    (env->ft0)
#define ST0    (env->fpregs[env->fpstt].d)
#define ST(n)  (env->fpregs[(env->fpstt + (n)) & 7].d)
#define ST1    ST(1)

/* translate.c */
void optimize_flags_init(void);

#include "exec/cpu-all.h"
#include "svm.h"

#if !defined(CONFIG_USER_ONLY)
#include "hw/i386/apic.h"
#endif

#include "exec/exec-all.h"

static inline void cpu_get_tb_cpu_state(CPUX86State *env, target_ulong *pc,
                                        target_ulong *cs_base, int *flags)
{
    *cs_base = env->segs[R_CS].base;
    *pc = *cs_base + env->eip;
    *flags = env->hflags |
        (env->eflags & (IOPL_MASK | TF_MASK | RF_MASK | VM_MASK | AC_MASK));
}

void do_cpu_init(X86CPU *cpu);
void do_cpu_sipi(X86CPU *cpu);

#define MCE_INJECT_BROADCAST    1
#define MCE_INJECT_UNCOND_AO    2

void cpu_x86_inject_mce(Monitor *mon, X86CPU *cpu, int bank,
                        uint64_t status, uint64_t mcg_status, uint64_t addr,
                        uint64_t misc, int flags);

/* excp_helper.c */
void QEMU_NORETURN raise_exception(CPUX86State *env, int exception_index);
void QEMU_NORETURN raise_exception_err(CPUX86State *env, int exception_index,
                                       int error_code);
void QEMU_NORETURN raise_interrupt(CPUX86State *nenv, int intno, int is_int,
                                   int error_code, int next_eip_addend);

/* cc_helper.c */
extern const uint8_t parity_table[256];
uint32_t cpu_cc_compute_all(CPUX86State *env1, int op);

static inline uint32_t cpu_compute_eflags(CPUX86State *env)
{
    return env->eflags | cpu_cc_compute_all(env, CC_OP) | (env->df & DF_MASK);
}

/* NOTE: the translator must set DisasContext.cc_op to CC_OP_EFLAGS
 * after generating a call to a helper that uses this.
 */
static inline void cpu_load_eflags(CPUX86State *env, int eflags,
                                   int update_mask)
{
    CC_SRC = eflags & (CC_O | CC_S | CC_Z | CC_A | CC_P | CC_C);
    CC_OP = CC_OP_EFLAGS;
    env->df = 1 - (2 * ((eflags >> 10) & 1));
    env->eflags = (env->eflags & ~update_mask) |
        (eflags & update_mask) | 0x2;
}

/* load efer and update the corresponding hflags. XXX: do consistency
   checks with cpuid bits? */
static inline void cpu_load_efer(CPUX86State *env, uint64_t val)
{
    env->efer = val;
    env->hflags &= ~(HF_LMA_MASK | HF_SVME_MASK);
    if (env->efer & MSR_EFER_LMA) {
        env->hflags |= HF_LMA_MASK;
    }
    if (env->efer & MSR_EFER_SVME) {
        env->hflags |= HF_SVME_MASK;
    }
}

/* fpu_helper.c */
void cpu_set_mxcsr(CPUX86State *env, uint32_t val);

/* svm_helper.c */
void cpu_svm_check_intercept_param(CPUX86State *env1, uint32_t type,
                                   uint64_t param);
void cpu_vmexit(CPUX86State *nenv, uint32_t exit_code, uint64_t exit_info_1);

/* seg_helper.c */
void do_interrupt_x86_hardirq(CPUX86State *env, int intno, int is_hw);

void do_smm_enter(X86CPU *cpu);

void cpu_report_tpr_access(CPUX86State *env, TPRAccess access);

void x86_cpu_compat_disable_kvm_features(FeatureWord w, uint32_t features);


/* Return name of 32-bit register, from a R_* constant */
const char *get_register_name_32(unsigned int reg);

uint32_t x86_cpu_apic_id_from_index(unsigned int cpu_index);
void enable_compat_apic_id_mode(void);

#define APIC_DEFAULT_ADDRESS 0xfee00000
#define APIC_SPACE_SIZE      0x100000

#endif /* CPU_I386_H */
