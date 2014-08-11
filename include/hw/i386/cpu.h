/* i386 CPU types and declarations that don't depend on target-i386/cpu.h
 */
#ifndef HW_I386_CPU_H
#define HW_I386_CPU_H

#define R_EAX 0
#define R_ECX 1
#define R_EDX 2
#define R_EBX 3
#define R_ESP 4
#define R_EBP 5
#define R_ESI 6
#define R_EDI 7

#define R_AL 0
#define R_CL 1
#define R_DL 2
#define R_BL 3
#define R_AH 4
#define R_CH 5
#define R_DH 6
#define R_BH 7

#define R_ES 0
#define R_CS 1
#define R_SS 2
#define R_DS 3
#define R_FS 4
#define R_GS 5

/* segment descriptor fields */
#define DESC_G_MASK     (1 << 23)
#define DESC_B_SHIFT    22
#define DESC_B_MASK     (1 << DESC_B_SHIFT)
#define DESC_L_SHIFT    21 /* x86_64 only : 64 bit code segment */
#define DESC_L_MASK     (1 << DESC_L_SHIFT)
#define DESC_AVL_MASK   (1 << 20)
#define DESC_P_MASK     (1 << 15)
#define DESC_DPL_SHIFT  13
#define DESC_DPL_MASK   (3 << DESC_DPL_SHIFT)
#define DESC_S_MASK     (1 << 12)
#define DESC_TYPE_SHIFT 8
#define DESC_TYPE_MASK  (15 << DESC_TYPE_SHIFT)
#define DESC_A_MASK     (1 << 8)

#define DESC_CS_MASK    (1 << 11) /* 1=code segment 0=data segment */
#define DESC_C_MASK     (1 << 10) /* code: conforming */
#define DESC_R_MASK     (1 << 9)  /* code: readable */

#define DESC_E_MASK     (1 << 10) /* data: expansion direction */
#define DESC_W_MASK     (1 << 9)  /* data: writable */

#define DESC_TSS_BUSY_MASK (1 << 9)

/* eflags masks */
#define CC_C    0x0001
#define CC_P    0x0004
#define CC_A    0x0010
#define CC_Z    0x0040
#define CC_S    0x0080
#define CC_O    0x0800

#define TF_SHIFT   8
#define IOPL_SHIFT 12
#define VM_SHIFT   17

#define TF_MASK                 0x00000100
#define IF_MASK                 0x00000200
#define DF_MASK                 0x00000400
#define IOPL_MASK               0x00003000
#define NT_MASK                 0x00004000
#define RF_MASK                 0x00010000
#define VM_MASK                 0x00020000
#define AC_MASK                 0x00040000
#define VIF_MASK                0x00080000
#define VIP_MASK                0x00100000
#define ID_MASK                 0x00200000

/* hidden flags - used internally by qemu to represent additional cpu
   states. Only the INHIBIT_IRQ, SMM and SVMI are not redundant. We
   avoid using the IOPL_MASK, TF_MASK, VM_MASK and AC_MASK bit
   positions to ease oring with eflags. */
/* current cpl */
#define HF_CPL_SHIFT         0
/* true if soft mmu is being used */
#define HF_SOFTMMU_SHIFT     2
/* true if hardware interrupts must be disabled for next instruction */
#define HF_INHIBIT_IRQ_SHIFT 3
/* 16 or 32 segments */
#define HF_CS32_SHIFT        4
#define HF_SS32_SHIFT        5
/* zero base for DS, ES and SS : can be '0' only in 32 bit CS segment */
#define HF_ADDSEG_SHIFT      6
/* copy of CR0.PE (protected mode) */
#define HF_PE_SHIFT          7
#define HF_TF_SHIFT          8 /* must be same as eflags */
#define HF_MP_SHIFT          9 /* the order must be MP, EM, TS */
#define HF_EM_SHIFT         10
#define HF_TS_SHIFT         11
#define HF_IOPL_SHIFT       12 /* must be same as eflags */
#define HF_LMA_SHIFT        14 /* only used on x86_64: long mode active */
#define HF_CS64_SHIFT       15 /* only used on x86_64: 64 bit code segment  */
#define HF_RF_SHIFT         16 /* must be same as eflags */
#define HF_VM_SHIFT         17 /* must be same as eflags */
#define HF_AC_SHIFT         18 /* must be same as eflags */
#define HF_SMM_SHIFT        19 /* CPU in SMM mode */
#define HF_SVME_SHIFT       20 /* SVME enabled (copy of EFER.SVME) */
#define HF_SVMI_SHIFT       21 /* SVM intercepts are active */
#define HF_OSFXSR_SHIFT     22 /* CR4.OSFXSR */
#define HF_SMAP_SHIFT       23 /* CR4.SMAP */

#define HF_CPL_MASK          (3 << HF_CPL_SHIFT)
#define HF_SOFTMMU_MASK      (1 << HF_SOFTMMU_SHIFT)
#define HF_INHIBIT_IRQ_MASK  (1 << HF_INHIBIT_IRQ_SHIFT)
#define HF_CS32_MASK         (1 << HF_CS32_SHIFT)
#define HF_SS32_MASK         (1 << HF_SS32_SHIFT)
#define HF_ADDSEG_MASK       (1 << HF_ADDSEG_SHIFT)
#define HF_PE_MASK           (1 << HF_PE_SHIFT)
#define HF_TF_MASK           (1 << HF_TF_SHIFT)
#define HF_MP_MASK           (1 << HF_MP_SHIFT)
#define HF_EM_MASK           (1 << HF_EM_SHIFT)
#define HF_TS_MASK           (1 << HF_TS_SHIFT)
#define HF_IOPL_MASK         (3 << HF_IOPL_SHIFT)
#define HF_LMA_MASK          (1 << HF_LMA_SHIFT)
#define HF_CS64_MASK         (1 << HF_CS64_SHIFT)
#define HF_RF_MASK           (1 << HF_RF_SHIFT)
#define HF_VM_MASK           (1 << HF_VM_SHIFT)
#define HF_AC_MASK           (1 << HF_AC_SHIFT)
#define HF_SMM_MASK          (1 << HF_SMM_SHIFT)
#define HF_SVME_MASK         (1 << HF_SVME_SHIFT)
#define HF_SVMI_MASK         (1 << HF_SVMI_SHIFT)
#define HF_OSFXSR_MASK       (1 << HF_OSFXSR_SHIFT)
#define HF_SMAP_MASK         (1 << HF_SMAP_SHIFT)

/* hflags2 */

#define HF2_GIF_SHIFT        0 /* if set CPU takes interrupts */
#define HF2_HIF_SHIFT        1 /* value of IF_MASK when entering SVM */
#define HF2_NMI_SHIFT        2 /* CPU serving NMI */
#define HF2_VINTR_SHIFT      3 /* value of V_INTR_MASKING bit */

#define HF2_GIF_MASK          (1 << HF2_GIF_SHIFT)
#define HF2_HIF_MASK          (1 << HF2_HIF_SHIFT)
#define HF2_NMI_MASK          (1 << HF2_NMI_SHIFT)
#define HF2_VINTR_MASK        (1 << HF2_VINTR_SHIFT)

#define CR0_PE_SHIFT 0
#define CR0_MP_SHIFT 1

#define CR0_PE_MASK  (1U << 0)
#define CR0_MP_MASK  (1U << 1)
#define CR0_EM_MASK  (1U << 2)
#define CR0_TS_MASK  (1U << 3)
#define CR0_ET_MASK  (1U << 4)
#define CR0_NE_MASK  (1U << 5)
#define CR0_WP_MASK  (1U << 16)
#define CR0_AM_MASK  (1U << 18)
#define CR0_PG_MASK  (1U << 31)

#define CR4_VME_MASK  (1U << 0)
#define CR4_PVI_MASK  (1U << 1)
#define CR4_TSD_MASK  (1U << 2)
#define CR4_DE_MASK   (1U << 3)
#define CR4_PSE_MASK  (1U << 4)
#define CR4_PAE_MASK  (1U << 5)
#define CR4_MCE_MASK  (1U << 6)
#define CR4_PGE_MASK  (1U << 7)
#define CR4_PCE_MASK  (1U << 8)
#define CR4_OSFXSR_SHIFT 9
#define CR4_OSFXSR_MASK (1U << CR4_OSFXSR_SHIFT)
#define CR4_OSXMMEXCPT_MASK  (1U << 10)
#define CR4_VMXE_MASK   (1U << 13)
#define CR4_SMXE_MASK   (1U << 14)
#define CR4_FSGSBASE_MASK (1U << 16)
#define CR4_PCIDE_MASK  (1U << 17)
#define CR4_OSXSAVE_MASK (1U << 18)
#define CR4_SMEP_MASK   (1U << 20)
#define CR4_SMAP_MASK   (1U << 21)

#define DR6_BD          (1 << 13)
#define DR6_BS          (1 << 14)
#define DR6_BT          (1 << 15)
#define DR6_FIXED_1     0xffff0ff0

#define DR7_GD          (1 << 13)
#define DR7_TYPE_SHIFT  16
#define DR7_LEN_SHIFT   18
#define DR7_FIXED_1     0x00000400
#define DR7_LOCAL_BP_MASK    0x55
#define DR7_MAX_BP           4
#define DR7_TYPE_BP_INST     0x0
#define DR7_TYPE_DATA_WR     0x1
#define DR7_TYPE_IO_RW       0x2
#define DR7_TYPE_DATA_RW     0x3

#define PG_PRESENT_BIT  0
#define PG_RW_BIT       1
#define PG_USER_BIT     2
#define PG_PWT_BIT      3
#define PG_PCD_BIT      4
#define PG_ACCESSED_BIT 5
#define PG_DIRTY_BIT    6
#define PG_PSE_BIT      7
#define PG_GLOBAL_BIT   8
#define PG_PSE_PAT_BIT  12
#define PG_NX_BIT       63

#define PG_PRESENT_MASK  (1 << PG_PRESENT_BIT)
#define PG_RW_MASK       (1 << PG_RW_BIT)
#define PG_USER_MASK     (1 << PG_USER_BIT)
#define PG_PWT_MASK      (1 << PG_PWT_BIT)
#define PG_PCD_MASK      (1 << PG_PCD_BIT)
#define PG_ACCESSED_MASK (1 << PG_ACCESSED_BIT)
#define PG_DIRTY_MASK    (1 << PG_DIRTY_BIT)
#define PG_PSE_MASK      (1 << PG_PSE_BIT)
#define PG_GLOBAL_MASK   (1 << PG_GLOBAL_BIT)
#define PG_PSE_PAT_MASK  (1 << PG_PSE_PAT_BIT)
#define PG_ADDRESS_MASK  0x000ffffffffff000LL
#define PG_HI_RSVD_MASK  (PG_ADDRESS_MASK & ~PHYS_ADDR_MASK)
#define PG_HI_USER_MASK  0x7ff0000000000000LL
#define PG_NX_MASK       (1LL << PG_NX_BIT)

#define PG_ERROR_W_BIT     1

#define PG_ERROR_P_MASK    0x01
#define PG_ERROR_W_MASK    (1 << PG_ERROR_W_BIT)
#define PG_ERROR_U_MASK    0x04
#define PG_ERROR_RSVD_MASK 0x08
#define PG_ERROR_I_D_MASK  0x10

#define MCG_CTL_P       (1ULL<<8)   /* MCG_CAP register available */
#define MCG_SER_P       (1ULL<<24) /* MCA recovery/new status bits */

#define MCE_CAP_DEF     (MCG_CTL_P|MCG_SER_P)
#define MCE_BANKS_DEF   10

#define MCG_STATUS_RIPV (1ULL<<0)   /* restart ip valid */
#define MCG_STATUS_EIPV (1ULL<<1)   /* ip points to correct instruction */
#define MCG_STATUS_MCIP (1ULL<<2)   /* machine check in progress */

#define MCI_STATUS_VAL   (1ULL<<63)  /* valid error */
#define MCI_STATUS_OVER  (1ULL<<62)  /* previous errors lost */
#define MCI_STATUS_UC    (1ULL<<61)  /* uncorrected error */
#define MCI_STATUS_EN    (1ULL<<60)  /* error enabled */
#define MCI_STATUS_MISCV (1ULL<<59)  /* misc error reg. valid */
#define MCI_STATUS_ADDRV (1ULL<<58)  /* addr reg. valid */
#define MCI_STATUS_PCC   (1ULL<<57)  /* processor context corrupt */
#define MCI_STATUS_S     (1ULL<<56)  /* Signaled machine check */
#define MCI_STATUS_AR    (1ULL<<55)  /* Action required */

/* MISC register defines */
#define MCM_ADDR_SEGOFF  0      /* segment offset */
#define MCM_ADDR_LINEAR  1      /* linear address */
#define MCM_ADDR_PHYS    2      /* physical address */
#define MCM_ADDR_MEM     3      /* memory address */
#define MCM_ADDR_GENERIC 7      /* generic */

#define MSR_IA32_TSC                    0x10
#define MSR_IA32_APICBASE               0x1b
#define MSR_IA32_APICBASE_BSP           (1<<8)
#define MSR_IA32_APICBASE_ENABLE        (1<<11)
#define MSR_IA32_APICBASE_BASE          (0xfffff<<12)
#define MSR_IA32_FEATURE_CONTROL        0x0000003a
#define MSR_TSC_ADJUST                  0x0000003b
#define MSR_IA32_TSCDEADLINE            0x6e0

#define MSR_P6_PERFCTR0                 0xc1

#define MSR_MTRRcap                     0xfe
#define MSR_MTRRcap_VCNT                8
#define MSR_MTRRcap_FIXRANGE_SUPPORT    (1 << 8)
#define MSR_MTRRcap_WC_SUPPORTED        (1 << 10)

#define MSR_IA32_SYSENTER_CS            0x174
#define MSR_IA32_SYSENTER_ESP           0x175
#define MSR_IA32_SYSENTER_EIP           0x176

#define MSR_MCG_CAP                     0x179
#define MSR_MCG_STATUS                  0x17a
#define MSR_MCG_CTL                     0x17b

#define MSR_P6_EVNTSEL0                 0x186

#define MSR_IA32_PERF_STATUS            0x198

#define MSR_IA32_MISC_ENABLE            0x1a0
/* Indicates good rep/movs microcode on some processors: */
#define MSR_IA32_MISC_ENABLE_DEFAULT    1

#define MSR_MTRRphysBase(reg)           (0x200 + 2 * (reg))
#define MSR_MTRRphysMask(reg)           (0x200 + 2 * (reg) + 1)

#define MSR_MTRRfix64K_00000            0x250
#define MSR_MTRRfix16K_80000            0x258
#define MSR_MTRRfix16K_A0000            0x259
#define MSR_MTRRfix4K_C0000             0x268
#define MSR_MTRRfix4K_C8000             0x269
#define MSR_MTRRfix4K_D0000             0x26a
#define MSR_MTRRfix4K_D8000             0x26b
#define MSR_MTRRfix4K_E0000             0x26c
#define MSR_MTRRfix4K_E8000             0x26d
#define MSR_MTRRfix4K_F0000             0x26e
#define MSR_MTRRfix4K_F8000             0x26f

#define MSR_PAT                         0x277

#define MSR_MTRRdefType                 0x2ff

#define MSR_CORE_PERF_FIXED_CTR0        0x309
#define MSR_CORE_PERF_FIXED_CTR1        0x30a
#define MSR_CORE_PERF_FIXED_CTR2        0x30b
#define MSR_CORE_PERF_FIXED_CTR_CTRL    0x38d
#define MSR_CORE_PERF_GLOBAL_STATUS     0x38e
#define MSR_CORE_PERF_GLOBAL_CTRL       0x38f
#define MSR_CORE_PERF_GLOBAL_OVF_CTRL   0x390

#define MSR_MC0_CTL                     0x400
#define MSR_MC0_STATUS                  0x401
#define MSR_MC0_ADDR                    0x402
#define MSR_MC0_MISC                    0x403

#define MSR_EFER                        0xc0000080

#define MSR_EFER_SCE   (1 << 0)
#define MSR_EFER_LME   (1 << 8)
#define MSR_EFER_LMA   (1 << 10)
#define MSR_EFER_NXE   (1 << 11)
#define MSR_EFER_SVME  (1 << 12)
#define MSR_EFER_FFXSR (1 << 14)

#define MSR_STAR                        0xc0000081
#define MSR_LSTAR                       0xc0000082
#define MSR_CSTAR                       0xc0000083
#define MSR_FMASK                       0xc0000084
#define MSR_FSBASE                      0xc0000100
#define MSR_GSBASE                      0xc0000101
#define MSR_KERNELGSBASE                0xc0000102
#define MSR_TSC_AUX                     0xc0000103

#define MSR_VM_HSAVE_PA                 0xc0010117

#define MSR_IA32_BNDCFGS                0x00000d90

#define XSTATE_FP                       (1ULL << 0)
#define XSTATE_SSE                      (1ULL << 1)
#define XSTATE_YMM                      (1ULL << 2)
#define XSTATE_BNDREGS                  (1ULL << 3)
#define XSTATE_BNDCSR                   (1ULL << 4)


/* CPUID feature words */
typedef enum FeatureWord {
    FEAT_1_EDX,         /* CPUID[1].EDX */
    FEAT_1_ECX,         /* CPUID[1].ECX */
    FEAT_7_0_EBX,       /* CPUID[EAX=7,ECX=0].EBX */
    FEAT_8000_0001_EDX, /* CPUID[8000_0001].EDX */
    FEAT_8000_0001_ECX, /* CPUID[8000_0001].ECX */
    FEAT_8000_0007_EDX, /* CPUID[8000_0007].EDX */
    FEAT_C000_0001_EDX, /* CPUID[C000_0001].EDX */
    FEAT_KVM,           /* CPUID[4000_0001].EAX (KVM_CPUID_FEATURES) */
    FEAT_SVM,           /* CPUID[8000_000A].EDX */
    FEATURE_WORDS,
} FeatureWord;

typedef uint32_t FeatureWordArray[FEATURE_WORDS];

/* cpuid_features bits */
#define CPUID_FP87 (1U << 0)
#define CPUID_VME  (1U << 1)
#define CPUID_DE   (1U << 2)
#define CPUID_PSE  (1U << 3)
#define CPUID_TSC  (1U << 4)
#define CPUID_MSR  (1U << 5)
#define CPUID_PAE  (1U << 6)
#define CPUID_MCE  (1U << 7)
#define CPUID_CX8  (1U << 8)
#define CPUID_APIC (1U << 9)
#define CPUID_SEP  (1U << 11) /* sysenter/sysexit */
#define CPUID_MTRR (1U << 12)
#define CPUID_PGE  (1U << 13)
#define CPUID_MCA  (1U << 14)
#define CPUID_CMOV (1U << 15)
#define CPUID_PAT  (1U << 16)
#define CPUID_PSE36   (1U << 17)
#define CPUID_PN   (1U << 18)
#define CPUID_CLFLUSH (1U << 19)
#define CPUID_DTS (1U << 21)
#define CPUID_ACPI (1U << 22)
#define CPUID_MMX  (1U << 23)
#define CPUID_FXSR (1U << 24)
#define CPUID_SSE  (1U << 25)
#define CPUID_SSE2 (1U << 26)
#define CPUID_SS (1U << 27)
#define CPUID_HT (1U << 28)
#define CPUID_TM (1U << 29)
#define CPUID_IA64 (1U << 30)
#define CPUID_PBE (1U << 31)

#define CPUID_EXT_SSE3     (1U << 0)
#define CPUID_EXT_PCLMULQDQ (1U << 1)
#define CPUID_EXT_DTES64   (1U << 2)
#define CPUID_EXT_MONITOR  (1U << 3)
#define CPUID_EXT_DSCPL    (1U << 4)
#define CPUID_EXT_VMX      (1U << 5)
#define CPUID_EXT_SMX      (1U << 6)
#define CPUID_EXT_EST      (1U << 7)
#define CPUID_EXT_TM2      (1U << 8)
#define CPUID_EXT_SSSE3    (1U << 9)
#define CPUID_EXT_CID      (1U << 10)
#define CPUID_EXT_FMA      (1U << 12)
#define CPUID_EXT_CX16     (1U << 13)
#define CPUID_EXT_XTPR     (1U << 14)
#define CPUID_EXT_PDCM     (1U << 15)
#define CPUID_EXT_PCID     (1U << 17)
#define CPUID_EXT_DCA      (1U << 18)
#define CPUID_EXT_SSE41    (1U << 19)
#define CPUID_EXT_SSE42    (1U << 20)
#define CPUID_EXT_X2APIC   (1U << 21)
#define CPUID_EXT_MOVBE    (1U << 22)
#define CPUID_EXT_POPCNT   (1U << 23)
#define CPUID_EXT_TSC_DEADLINE_TIMER (1U << 24)
#define CPUID_EXT_AES      (1U << 25)
#define CPUID_EXT_XSAVE    (1U << 26)
#define CPUID_EXT_OSXSAVE  (1U << 27)
#define CPUID_EXT_AVX      (1U << 28)
#define CPUID_EXT_F16C     (1U << 29)
#define CPUID_EXT_RDRAND   (1U << 30)
#define CPUID_EXT_HYPERVISOR  (1U << 31)

#define CPUID_EXT2_FPU     (1U << 0)
#define CPUID_EXT2_VME     (1U << 1)
#define CPUID_EXT2_DE      (1U << 2)
#define CPUID_EXT2_PSE     (1U << 3)
#define CPUID_EXT2_TSC     (1U << 4)
#define CPUID_EXT2_MSR     (1U << 5)
#define CPUID_EXT2_PAE     (1U << 6)
#define CPUID_EXT2_MCE     (1U << 7)
#define CPUID_EXT2_CX8     (1U << 8)
#define CPUID_EXT2_APIC    (1U << 9)
#define CPUID_EXT2_SYSCALL (1U << 11)
#define CPUID_EXT2_MTRR    (1U << 12)
#define CPUID_EXT2_PGE     (1U << 13)
#define CPUID_EXT2_MCA     (1U << 14)
#define CPUID_EXT2_CMOV    (1U << 15)
#define CPUID_EXT2_PAT     (1U << 16)
#define CPUID_EXT2_PSE36   (1U << 17)
#define CPUID_EXT2_MP      (1U << 19)
#define CPUID_EXT2_NX      (1U << 20)
#define CPUID_EXT2_MMXEXT  (1U << 22)
#define CPUID_EXT2_MMX     (1U << 23)
#define CPUID_EXT2_FXSR    (1U << 24)
#define CPUID_EXT2_FFXSR   (1U << 25)
#define CPUID_EXT2_PDPE1GB (1U << 26)
#define CPUID_EXT2_RDTSCP  (1U << 27)
#define CPUID_EXT2_LM      (1U << 29)
#define CPUID_EXT2_3DNOWEXT (1U << 30)
#define CPUID_EXT2_3DNOW   (1U << 31)

/* CPUID[8000_0001].EDX bits that are aliase of CPUID[1].EDX bits on AMD CPUs */
#define CPUID_EXT2_AMD_ALIASES (CPUID_EXT2_FPU | CPUID_EXT2_VME | \
                                CPUID_EXT2_DE | CPUID_EXT2_PSE | \
                                CPUID_EXT2_TSC | CPUID_EXT2_MSR | \
                                CPUID_EXT2_PAE | CPUID_EXT2_MCE | \
                                CPUID_EXT2_CX8 | CPUID_EXT2_APIC | \
                                CPUID_EXT2_MTRR | CPUID_EXT2_PGE | \
                                CPUID_EXT2_MCA | CPUID_EXT2_CMOV | \
                                CPUID_EXT2_PAT | CPUID_EXT2_PSE36 | \
                                CPUID_EXT2_MMX | CPUID_EXT2_FXSR)

#define CPUID_EXT3_LAHF_LM (1U << 0)
#define CPUID_EXT3_CMP_LEG (1U << 1)
#define CPUID_EXT3_SVM     (1U << 2)
#define CPUID_EXT3_EXTAPIC (1U << 3)
#define CPUID_EXT3_CR8LEG  (1U << 4)
#define CPUID_EXT3_ABM     (1U << 5)
#define CPUID_EXT3_SSE4A   (1U << 6)
#define CPUID_EXT3_MISALIGNSSE (1U << 7)
#define CPUID_EXT3_3DNOWPREFETCH (1U << 8)
#define CPUID_EXT3_OSVW    (1U << 9)
#define CPUID_EXT3_IBS     (1U << 10)
#define CPUID_EXT3_XOP     (1U << 11)
#define CPUID_EXT3_SKINIT  (1U << 12)
#define CPUID_EXT3_WDT     (1U << 13)
#define CPUID_EXT3_LWP     (1U << 15)
#define CPUID_EXT3_FMA4    (1U << 16)
#define CPUID_EXT3_TCE     (1U << 17)
#define CPUID_EXT3_NODEID  (1U << 19)
#define CPUID_EXT3_TBM     (1U << 21)
#define CPUID_EXT3_TOPOEXT (1U << 22)
#define CPUID_EXT3_PERFCORE (1U << 23)
#define CPUID_EXT3_PERFNB  (1U << 24)

#define CPUID_SVM_NPT          (1U << 0)
#define CPUID_SVM_LBRV         (1U << 1)
#define CPUID_SVM_SVMLOCK      (1U << 2)
#define CPUID_SVM_NRIPSAVE     (1U << 3)
#define CPUID_SVM_TSCSCALE     (1U << 4)
#define CPUID_SVM_VMCBCLEAN    (1U << 5)
#define CPUID_SVM_FLUSHASID    (1U << 6)
#define CPUID_SVM_DECODEASSIST (1U << 7)
#define CPUID_SVM_PAUSEFILTER  (1U << 10)
#define CPUID_SVM_PFTHRESHOLD  (1U << 12)

#define CPUID_7_0_EBX_FSGSBASE (1U << 0)
#define CPUID_7_0_EBX_BMI1     (1U << 3)
#define CPUID_7_0_EBX_HLE      (1U << 4)
#define CPUID_7_0_EBX_AVX2     (1U << 5)
#define CPUID_7_0_EBX_SMEP     (1U << 7)
#define CPUID_7_0_EBX_BMI2     (1U << 8)
#define CPUID_7_0_EBX_ERMS     (1U << 9)
#define CPUID_7_0_EBX_INVPCID  (1U << 10)
#define CPUID_7_0_EBX_RTM      (1U << 11)
#define CPUID_7_0_EBX_MPX      (1U << 14)
#define CPUID_7_0_EBX_RDSEED   (1U << 18)
#define CPUID_7_0_EBX_ADX      (1U << 19)
#define CPUID_7_0_EBX_SMAP     (1U << 20)

/* CPUID[0x80000007].EDX flags: */
#define CPUID_APM_INVTSC       (1U << 8)

#define CPUID_VENDOR_SZ      12

#define CPUID_VENDOR_INTEL_1 0x756e6547 /* "Genu" */
#define CPUID_VENDOR_INTEL_2 0x49656e69 /* "ineI" */
#define CPUID_VENDOR_INTEL_3 0x6c65746e /* "ntel" */
#define CPUID_VENDOR_INTEL "GenuineIntel"

#define CPUID_VENDOR_AMD_1   0x68747541 /* "Auth" */
#define CPUID_VENDOR_AMD_2   0x69746e65 /* "enti" */
#define CPUID_VENDOR_AMD_3   0x444d4163 /* "cAMD" */
#define CPUID_VENDOR_AMD   "AuthenticAMD"

#define CPUID_VENDOR_VIA   "CentaurHauls"

#define CPUID_MWAIT_IBE     (1U << 1) /* Interrupts can exit capability */
#define CPUID_MWAIT_EMX     (1U << 0) /* enumeration supported */

#ifndef HYPERV_SPINLOCK_NEVER_RETRY
#define HYPERV_SPINLOCK_NEVER_RETRY             0xFFFFFFFF
#endif

#define EXCP00_DIVZ	0
#define EXCP01_DB	1
#define EXCP02_NMI	2
#define EXCP03_INT3	3
#define EXCP04_INTO	4
#define EXCP05_BOUND	5
#define EXCP06_ILLOP	6
#define EXCP07_PREX	7
#define EXCP08_DBLE	8
#define EXCP09_XERR	9
#define EXCP0A_TSS	10
#define EXCP0B_NOSEG	11
#define EXCP0C_STACK	12
#define EXCP0D_GPF	13
#define EXCP0E_PAGE	14
#define EXCP10_COPR	16
#define EXCP11_ALGN	17
#define EXCP12_MCHK	18

#define EXCP_SYSCALL    0x100 /* only happens in user only emulation
                                 for syscall instruction */

#endif
