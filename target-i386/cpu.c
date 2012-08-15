/*
 *  i386 CPUID helper functions
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "cpu.h"
#include "kvm.h"

#include "qemu-option.h"
#include "qemu-config.h"

#include "qapi/qapi-visit-core.h"
#include "qmp-commands.h"

#include "hyperv.h"

#include "hw/hw.h"

/* feature flags taken from "Intel Processor Identification and the CPUID
 * Instruction" and AMD's "CPUID Specification".  In cases of disagreement
 * between feature naming conventions, aliases may be added.
 */
static const char *feature_name[] = {
    "fpu", "vme", "de", "pse",
    "tsc", "msr", "pae", "mce",
    "cx8", "apic", NULL, "sep",
    "mtrr", "pge", "mca", "cmov",
    "pat", "pse36", "pn" /* Intel psn */, "clflush" /* Intel clfsh */,
    NULL, "ds" /* Intel dts */, "acpi", "mmx",
    "fxsr", "sse", "sse2", "ss",
    "ht" /* Intel htt */, "tm", "ia64", "pbe",
};
static const char *ext_feature_name[] = {
    "pni|sse3" /* Intel,AMD sse3 */, "pclmulqdq|pclmuldq", "dtes64", "monitor",
    "ds_cpl", "vmx", "smx", "est",
    "tm2", "ssse3", "cid", NULL,
    "fma", "cx16", "xtpr", "pdcm",
    NULL, "pcid", "dca", "sse4.1|sse4_1",
    "sse4.2|sse4_2", "x2apic", "movbe", "popcnt",
    "tsc-deadline", "aes", "xsave", "osxsave",
    "avx", NULL, NULL, "hypervisor",
};
static const char *ext2_feature_name[] = {
    "fpu", "vme", "de", "pse",
    "tsc", "msr", "pae", "mce",
    "cx8" /* AMD CMPXCHG8B */, "apic", NULL, "syscall",
    "mtrr", "pge", "mca", "cmov",
    "pat", "pse36", NULL, NULL /* Linux mp */,
    "nx|xd", NULL, "mmxext", "mmx",
    "fxsr", "fxsr_opt|ffxsr", "pdpe1gb" /* AMD Page1GB */, "rdtscp",
    NULL, "lm|i64", "3dnowext", "3dnow",
};
static const char *ext3_feature_name[] = {
    "lahf_lm" /* AMD LahfSahf */, "cmp_legacy", "svm", "extapic" /* AMD ExtApicSpace */,
    "cr8legacy" /* AMD AltMovCr8 */, "abm", "sse4a", "misalignsse",
    "3dnowprefetch", "osvw", "ibs", "xop",
    "skinit", "wdt", NULL, NULL,
    "fma4", NULL, "cvt16", "nodeid_msr",
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
};

static const char *kvm_feature_name[] = {
    "kvmclock", "kvm_nopiodelay", "kvm_mmu", "kvmclock", "kvm_asyncpf", NULL, "kvm_pv_eoi", NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};

static const char *svm_feature_name[] = {
    "npt", "lbrv", "svm_lock", "nrip_save",
    "tsc_scale", "vmcb_clean",  "flushbyasid", "decodeassists",
    NULL, NULL, "pause_filter", NULL,
    "pfthreshold", NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
};

/* collects per-function cpuid data
 */
typedef struct model_features_t {
    uint32_t *guest_feat;
    uint32_t *host_feat;
    uint32_t check_feat;
    const char **flag_names;
    uint32_t cpuid;
    } model_features_t;

bool check_cpuid;
bool enforce_cpuid;

void host_cpuid(uint32_t function, uint32_t count,
                uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
#if defined(CONFIG_KVM)
    uint32_t vec[4];

#ifdef __x86_64__
    asm volatile("cpuid"
                 : "=a"(vec[0]), "=b"(vec[1]),
                   "=c"(vec[2]), "=d"(vec[3])
                 : "0"(function), "c"(count) : "cc");
#else
    asm volatile("pusha \n\t"
                 "cpuid \n\t"
                 "mov %%eax, 0(%2) \n\t"
                 "mov %%ebx, 4(%2) \n\t"
                 "mov %%ecx, 8(%2) \n\t"
                 "mov %%edx, 12(%2) \n\t"
                 "popa"
                 : : "a"(function), "c"(count), "S"(vec)
                 : "memory", "cc");
#endif

    if (eax)
        *eax = vec[0];
    if (ebx)
        *ebx = vec[1];
    if (ecx)
        *ecx = vec[2];
    if (edx)
        *edx = vec[3];
#endif
}

#define iswhite(c) ((c) && ((c) <= ' ' || '~' < (c)))

/* general substring compare of *[s1..e1) and *[s2..e2).  sx is start of
 * a substring.  ex if !NULL points to the first char after a substring,
 * otherwise the string is assumed to sized by a terminating nul.
 * Return lexical ordering of *s1:*s2.
 */
static int sstrcmp(const char *s1, const char *e1, const char *s2,
    const char *e2)
{
    for (;;) {
        if (!*s1 || !*s2 || *s1 != *s2)
            return (*s1 - *s2);
        ++s1, ++s2;
        if (s1 == e1 && s2 == e2)
            return (0);
        else if (s1 == e1)
            return (*s2);
        else if (s2 == e2)
            return (*s1);
    }
}

/* compare *[s..e) to *altstr.  *altstr may be a simple string or multiple
 * '|' delimited (possibly empty) strings in which case search for a match
 * within the alternatives proceeds left to right.  Return 0 for success,
 * non-zero otherwise.
 */
static int altcmp(const char *s, const char *e, const char *altstr)
{
    const char *p, *q;

    for (q = p = altstr; ; ) {
        while (*p && *p != '|')
            ++p;
        if ((q == p && !*s) || (q != p && !sstrcmp(s, e, q, p)))
            return (0);
        if (!*p)
            return (1);
        else
            q = ++p;
    }
}

/* search featureset for flag *[s..e), if found set corresponding bit in
 * *pval and return true, otherwise return false
 */
static bool lookup_feature(uint32_t *pval, const char *s, const char *e,
                           const char **featureset)
{
    uint32_t mask;
    const char **ppc;
    bool found = false;

    for (mask = 1, ppc = featureset; mask; mask <<= 1, ++ppc) {
        if (*ppc && !altcmp(s, e, *ppc)) {
            *pval |= mask;
            found = true;
        }
    }
    return found;
}


#define I486_FEATURES (CPUID_FP87 | CPUID_VME | CPUID_PSE)
#define PENTIUM_FEATURES (I486_FEATURES | CPUID_DE | CPUID_TSC | \
          CPUID_MSR | CPUID_MCE | CPUID_CX8 | CPUID_MMX | CPUID_APIC)
#define PENTIUM2_FEATURES (PENTIUM_FEATURES | CPUID_PAE | CPUID_SEP | \
          CPUID_MTRR | CPUID_PGE | CPUID_MCA | CPUID_CMOV | CPUID_PAT | \
          CPUID_PSE36 | CPUID_FXSR)
#define PENTIUM3_FEATURES (PENTIUM2_FEATURES | CPUID_SSE)
#define PPRO_FEATURES (CPUID_FP87 | CPUID_DE | CPUID_PSE | CPUID_TSC | \
          CPUID_MSR | CPUID_MCE | CPUID_CX8 | CPUID_PGE | CPUID_CMOV | \
          CPUID_PAT | CPUID_FXSR | CPUID_MMX | CPUID_SSE | CPUID_SSE2 | \
          CPUID_PAE | CPUID_SEP | CPUID_APIC)
#define EXT2_FEATURE_MASK 0x0183F3FF

#define TCG_FEATURES (CPUID_FP87 | CPUID_PSE | CPUID_TSC | CPUID_MSR | \
          CPUID_PAE | CPUID_MCE | CPUID_CX8 | CPUID_APIC | CPUID_SEP | \
          CPUID_MTRR | CPUID_PGE | CPUID_MCA | CPUID_CMOV | CPUID_PAT | \
          CPUID_PSE36 | CPUID_CLFLUSH | CPUID_ACPI | CPUID_MMX | \
          CPUID_FXSR | CPUID_SSE | CPUID_SSE2 | CPUID_SS)
          /* partly implemented:
          CPUID_MTRR, CPUID_MCA, CPUID_CLFLUSH (needed for Win64)
          CPUID_PSE36 (needed for Solaris) */
          /* missing:
          CPUID_VME, CPUID_DTS, CPUID_SS, CPUID_HT, CPUID_TM, CPUID_PBE */
#define TCG_EXT_FEATURES (CPUID_EXT_SSE3 | CPUID_EXT_MONITOR | \
          CPUID_EXT_CX16 | CPUID_EXT_POPCNT | \
          CPUID_EXT_HYPERVISOR)
          /* missing:
          CPUID_EXT_DTES64, CPUID_EXT_DSCPL, CPUID_EXT_VMX, CPUID_EXT_EST,
          CPUID_EXT_TM2, CPUID_EXT_XTPR, CPUID_EXT_PDCM, CPUID_EXT_XSAVE */
#define TCG_EXT2_FEATURES ((TCG_FEATURES & EXT2_FEATURE_MASK) | \
          CPUID_EXT2_NX | CPUID_EXT2_MMXEXT | CPUID_EXT2_RDTSCP | \
          CPUID_EXT2_3DNOW | CPUID_EXT2_3DNOWEXT)
          /* missing:
          CPUID_EXT2_PDPE1GB */
#define TCG_EXT3_FEATURES (CPUID_EXT3_LAHF_LM | CPUID_EXT3_SVM | \
          CPUID_EXT3_CR8LEG | CPUID_EXT3_ABM | CPUID_EXT3_SSE4A)
#define TCG_SVM_FEATURES 0


typedef struct X86CPUModelTableEntry {
    const char *name;
    struct X86CPUModelTableEntry *next;
    bool is_builtin;
    X86CPUDefinition cpudef;
} X86CPUModelTableEntry;

/* maintains list of cpu model definitions
 */
static X86CPUModelTableEntry *x86_defs = {NULL};

/* built-in cpu model definitions (deprecated)
 */
static X86CPUModelTableEntry builtin_x86_defs[] = {
    {
      .name = "qemu64",
      .cpudef = {
        .level = 4,
        .vendor = CPUID_VENDOR_AMD,
        .family = 6,
        .model = 2,
        .stepping = 3,
        .features = PPRO_FEATURES |
            CPUID_MTRR | CPUID_CLFLUSH | CPUID_MCA |
            CPUID_PSE36,
        .ext_features = CPUID_EXT_SSE3 | CPUID_EXT_CX16 | CPUID_EXT_POPCNT,
        .ext2_features = (PPRO_FEATURES & EXT2_FEATURE_MASK) |
            CPUID_EXT2_LM | CPUID_EXT2_SYSCALL | CPUID_EXT2_NX,
        .ext3_features = CPUID_EXT3_LAHF_LM | CPUID_EXT3_SVM |
            CPUID_EXT3_ABM | CPUID_EXT3_SSE4A,
        .xlevel = 0x8000000A,
      },
    },
    {
      .name = "phenom",
      .cpudef = {
        .level = 5,
        .vendor = CPUID_VENDOR_AMD,
        .family = 16,
        .model = 2,
        .stepping = 3,
        .features = PPRO_FEATURES |
            CPUID_MTRR | CPUID_CLFLUSH | CPUID_MCA |
            CPUID_PSE36 | CPUID_VME | CPUID_HT,
        .ext_features = CPUID_EXT_SSE3 | CPUID_EXT_MONITOR | CPUID_EXT_CX16 |
            CPUID_EXT_POPCNT,
        .ext2_features = (PPRO_FEATURES & EXT2_FEATURE_MASK) |
            CPUID_EXT2_LM | CPUID_EXT2_SYSCALL | CPUID_EXT2_NX |
            CPUID_EXT2_3DNOW | CPUID_EXT2_3DNOWEXT | CPUID_EXT2_MMXEXT |
            CPUID_EXT2_FFXSR | CPUID_EXT2_PDPE1GB | CPUID_EXT2_RDTSCP,
        /* Missing: CPUID_EXT3_CMP_LEG, CPUID_EXT3_EXTAPIC,
                    CPUID_EXT3_CR8LEG,
                    CPUID_EXT3_MISALIGNSSE, CPUID_EXT3_3DNOWPREFETCH,
                    CPUID_EXT3_OSVW, CPUID_EXT3_IBS */
        .ext3_features = CPUID_EXT3_LAHF_LM | CPUID_EXT3_SVM |
            CPUID_EXT3_ABM | CPUID_EXT3_SSE4A,
        .svm_features = CPUID_SVM_NPT | CPUID_SVM_LBRV,
        .xlevel = 0x8000001A,
        .model_id = "AMD Phenom(tm) 9550 Quad-Core Processor"
      },
    },
    {
      .name = "core2duo",
      .cpudef = {
        .level = 10,
        .family = 6,
        .model = 15,
        .stepping = 11,
        .features = PPRO_FEATURES |
            CPUID_MTRR | CPUID_CLFLUSH | CPUID_MCA |
            CPUID_PSE36 | CPUID_VME | CPUID_DTS | CPUID_ACPI | CPUID_SS |
            CPUID_HT | CPUID_TM | CPUID_PBE,
        .ext_features = CPUID_EXT_SSE3 | CPUID_EXT_MONITOR | CPUID_EXT_SSSE3 |
            CPUID_EXT_DTES64 | CPUID_EXT_DSCPL | CPUID_EXT_VMX | CPUID_EXT_EST |
            CPUID_EXT_TM2 | CPUID_EXT_CX16 | CPUID_EXT_XTPR | CPUID_EXT_PDCM,
        .ext2_features = CPUID_EXT2_LM | CPUID_EXT2_SYSCALL | CPUID_EXT2_NX,
        .ext3_features = CPUID_EXT3_LAHF_LM,
        .xlevel = 0x80000008,
        .model_id = "Intel(R) Core(TM)2 Duo CPU     T7700  @ 2.40GHz",
      },
    },
    {
      .name = "kvm64",
      .cpudef = {
        .level = 5,
        .vendor = CPUID_VENDOR_INTEL,
        .family = 15,
        .model = 6,
        .stepping = 1,
        /* Missing: CPUID_VME, CPUID_HT */
        .features = PPRO_FEATURES |
            CPUID_MTRR | CPUID_CLFLUSH | CPUID_MCA |
            CPUID_PSE36,
        /* Missing: CPUID_EXT_POPCNT, CPUID_EXT_MONITOR */
        .ext_features = CPUID_EXT_SSE3 | CPUID_EXT_CX16,
        /* Missing: CPUID_EXT2_PDPE1GB, CPUID_EXT2_RDTSCP */
        .ext2_features = (PPRO_FEATURES & EXT2_FEATURE_MASK) |
            CPUID_EXT2_LM | CPUID_EXT2_SYSCALL | CPUID_EXT2_NX,
        /* Missing: CPUID_EXT3_LAHF_LM, CPUID_EXT3_CMP_LEG, CPUID_EXT3_EXTAPIC,
                    CPUID_EXT3_CR8LEG, CPUID_EXT3_ABM, CPUID_EXT3_SSE4A,
                    CPUID_EXT3_MISALIGNSSE, CPUID_EXT3_3DNOWPREFETCH,
                    CPUID_EXT3_OSVW, CPUID_EXT3_IBS, CPUID_EXT3_SVM */
        .ext3_features = 0,
        .xlevel = 0x80000008,
        .model_id = "Common KVM processor"
      },
    },
    {
      .name = "qemu32",
      .cpudef = {
        .level = 4,
        .family = 6,
        .model = 3,
        .stepping = 3,
        .features = PPRO_FEATURES,
        .ext_features = CPUID_EXT_SSE3 | CPUID_EXT_POPCNT,
        .xlevel = 0x80000004,
      },
    },
    {
      .name = "kvm32",
      .cpudef = {
        .level = 5,
        .family = 15,
        .model = 6,
        .stepping = 1,
        .features = PPRO_FEATURES |
            CPUID_MTRR | CPUID_CLFLUSH | CPUID_MCA | CPUID_PSE36,
        .ext_features = CPUID_EXT_SSE3,
        .ext2_features = PPRO_FEATURES & EXT2_FEATURE_MASK,
        .ext3_features = 0,
        .xlevel = 0x80000008,
        .model_id = "Common 32-bit KVM processor"
      },
    },
    {
      .name = "coreduo",
      .cpudef = {
        .level = 10,
        .family = 6,
        .model = 14,
        .stepping = 8,
        .features = PPRO_FEATURES | CPUID_VME |
            CPUID_MTRR | CPUID_CLFLUSH | CPUID_MCA | CPUID_DTS | CPUID_ACPI |
            CPUID_SS | CPUID_HT | CPUID_TM | CPUID_PBE,
        .ext_features = CPUID_EXT_SSE3 | CPUID_EXT_MONITOR | CPUID_EXT_VMX |
            CPUID_EXT_EST | CPUID_EXT_TM2 | CPUID_EXT_XTPR | CPUID_EXT_PDCM,
        .ext2_features = CPUID_EXT2_NX,
        .xlevel = 0x80000008,
        .model_id = "Genuine Intel(R) CPU           T2600  @ 2.16GHz",
      },
    },
    {
      .name = "486",
      .cpudef = {
        .level = 1,
        .family = 4,
        .model = 0,
        .stepping = 0,
        .features = I486_FEATURES,
        .xlevel = 0,
      },
    },
    {
      .name = "pentium",
      .cpudef = {
        .level = 1,
        .family = 5,
        .model = 4,
        .stepping = 3,
        .features = PENTIUM_FEATURES,
        .xlevel = 0,
      },
    },
    {
      .name = "pentium2",
      .cpudef = {
        .level = 2,
        .family = 6,
        .model = 5,
        .stepping = 2,
        .features = PENTIUM2_FEATURES,
        .xlevel = 0,
      },
    },
    {
      .name = "pentium3",
      .cpudef = {
        .level = 2,
        .family = 6,
        .model = 7,
        .stepping = 3,
        .features = PENTIUM3_FEATURES,
        .xlevel = 0,
      },
    },
    {
      .name = "athlon",
      .cpudef = {
        .level = 2,
        .vendor = CPUID_VENDOR_AMD,
        .family = 6,
        .model = 2,
        .stepping = 3,
        .features = PPRO_FEATURES | CPUID_PSE36 | CPUID_VME | CPUID_MTRR | CPUID_MCA,
        .ext2_features = (PPRO_FEATURES & EXT2_FEATURE_MASK) | CPUID_EXT2_MMXEXT | CPUID_EXT2_3DNOW | CPUID_EXT2_3DNOWEXT,
        .xlevel = 0x80000008,
      },
    },
    {
      .name = "n270",
      .cpudef = {
        /* original is on level 10 */
        .level = 5,
        .family = 6,
        .model = 28,
        .stepping = 2,
        .features = PPRO_FEATURES |
            CPUID_MTRR | CPUID_CLFLUSH | CPUID_MCA | CPUID_VME | CPUID_DTS |
            CPUID_ACPI | CPUID_SS | CPUID_HT | CPUID_TM | CPUID_PBE,
            /* Some CPUs got no CPUID_SEP */
        .ext_features = CPUID_EXT_SSE3 | CPUID_EXT_MONITOR | CPUID_EXT_SSSE3 |
            CPUID_EXT_DSCPL | CPUID_EXT_EST | CPUID_EXT_TM2 | CPUID_EXT_XTPR,
        .ext2_features = (PPRO_FEATURES & EXT2_FEATURE_MASK) | CPUID_EXT2_NX,
        .ext3_features = CPUID_EXT3_LAHF_LM,
        .xlevel = 0x8000000A,
        .model_id = "Intel(R) Atom(TM) CPU N270   @ 1.60GHz",
      },
    },
    {
      .name = "Conroe",
      .cpudef = {
        .level = 2,
        .vendor = CPUID_VENDOR_INTEL,
        .family = 6,
        .model = 2,
        .stepping = 3,
        .features = CPUID_SSE2 | CPUID_SSE | CPUID_FXSR | CPUID_MMX |
             CPUID_CLFLUSH | CPUID_PSE36 | CPUID_PAT | CPUID_CMOV | CPUID_MCA |
             CPUID_PGE | CPUID_MTRR | CPUID_SEP | CPUID_APIC | CPUID_CX8 |
             CPUID_MCE | CPUID_PAE | CPUID_MSR | CPUID_TSC | CPUID_PSE |
             CPUID_DE | CPUID_FP87,
        .ext_features = CPUID_EXT_SSSE3 | CPUID_EXT_SSE3,
        .ext2_features = CPUID_EXT2_LM | CPUID_EXT2_NX | CPUID_EXT2_SYSCALL,
        .ext3_features = CPUID_EXT3_LAHF_LM,
        .xlevel = 0x8000000A,
        .model_id = "Intel Celeron_4x0 (Conroe/Merom Class Core 2)",
      },
    },
    {
      .name = "Penryn",
      .cpudef = {
        .level = 2,
        .vendor = CPUID_VENDOR_INTEL,
        .family = 6,
        .model = 2,
        .stepping = 3,
        .features = CPUID_SSE2 | CPUID_SSE | CPUID_FXSR | CPUID_MMX |
             CPUID_CLFLUSH | CPUID_PSE36 | CPUID_PAT | CPUID_CMOV | CPUID_MCA |
             CPUID_PGE | CPUID_MTRR | CPUID_SEP | CPUID_APIC | CPUID_CX8 |
             CPUID_MCE | CPUID_PAE | CPUID_MSR | CPUID_TSC | CPUID_PSE |
             CPUID_DE | CPUID_FP87,
        .ext_features = CPUID_EXT_SSE41 | CPUID_EXT_CX16 | CPUID_EXT_SSSE3 |
             CPUID_EXT_SSE3,
        .ext2_features = CPUID_EXT2_LM | CPUID_EXT2_NX | CPUID_EXT2_SYSCALL,
        .ext3_features = CPUID_EXT3_LAHF_LM,
        .xlevel = 0x8000000A,
        .model_id = "Intel Core 2 Duo P9xxx (Penryn Class Core 2)",
      },
    },
    {
      .name = "Nehalem",
      .cpudef = {
        .level = 2,
        .vendor = CPUID_VENDOR_INTEL,
        .family = 6,
        .model = 2,
        .stepping = 3,
        .features = CPUID_SSE2 | CPUID_SSE | CPUID_FXSR | CPUID_MMX |
             CPUID_CLFLUSH | CPUID_PSE36 | CPUID_PAT | CPUID_CMOV | CPUID_MCA |
             CPUID_PGE | CPUID_MTRR | CPUID_SEP | CPUID_APIC | CPUID_CX8 |
             CPUID_MCE | CPUID_PAE | CPUID_MSR | CPUID_TSC | CPUID_PSE |
             CPUID_DE | CPUID_FP87,
        .ext_features = CPUID_EXT_POPCNT | CPUID_EXT_SSE42 | CPUID_EXT_SSE41 |
             CPUID_EXT_CX16 | CPUID_EXT_SSSE3 | CPUID_EXT_SSE3,
        .ext2_features = CPUID_EXT2_LM | CPUID_EXT2_SYSCALL | CPUID_EXT2_NX,
        .ext3_features = CPUID_EXT3_LAHF_LM,
        .xlevel = 0x8000000A,
        .model_id = "Intel Core i7 9xx (Nehalem Class Core i7)",
      },
    },
    {
      .name = "Westmere",
      .cpudef = {
        .level = 11,
        .vendor = CPUID_VENDOR_INTEL,
        .family = 6,
        .model = 44,
        .stepping = 1,
        .features = CPUID_SSE2 | CPUID_SSE | CPUID_FXSR | CPUID_MMX |
             CPUID_CLFLUSH | CPUID_PSE36 | CPUID_PAT | CPUID_CMOV | CPUID_MCA |
             CPUID_PGE | CPUID_MTRR | CPUID_SEP | CPUID_APIC | CPUID_CX8 |
             CPUID_MCE | CPUID_PAE | CPUID_MSR | CPUID_TSC | CPUID_PSE |
             CPUID_DE | CPUID_FP87,
        .ext_features = CPUID_EXT_AES | CPUID_EXT_POPCNT | CPUID_EXT_SSE42 |
             CPUID_EXT_SSE41 | CPUID_EXT_CX16 | CPUID_EXT_SSSE3 |
             CPUID_EXT_SSE3,
        .ext2_features = CPUID_EXT2_LM | CPUID_EXT2_SYSCALL | CPUID_EXT2_NX,
        .ext3_features = CPUID_EXT3_LAHF_LM,
        .xlevel = 0x8000000A,
        .model_id = "Westmere E56xx/L56xx/X56xx (Nehalem-C)",
      },
    },
    {
      .name = "SandyBridge",
      .cpudef = {
        .level = 0xd,
        .vendor = CPUID_VENDOR_INTEL,
        .family = 6,
        .model = 42,
        .stepping = 1,
        .features = CPUID_SSE2 | CPUID_SSE | CPUID_FXSR | CPUID_MMX |
             CPUID_CLFLUSH | CPUID_PSE36 | CPUID_PAT | CPUID_CMOV | CPUID_MCA |
             CPUID_PGE | CPUID_MTRR | CPUID_SEP | CPUID_APIC | CPUID_CX8 |
             CPUID_MCE | CPUID_PAE | CPUID_MSR | CPUID_TSC | CPUID_PSE |
             CPUID_DE | CPUID_FP87,
        .ext_features = CPUID_EXT_AVX | CPUID_EXT_XSAVE | CPUID_EXT_AES |
             CPUID_EXT_TSC_DEADLINE_TIMER | CPUID_EXT_POPCNT |
             CPUID_EXT_X2APIC | CPUID_EXT_SSE42 | CPUID_EXT_SSE41 |
             CPUID_EXT_CX16 | CPUID_EXT_SSSE3 | CPUID_EXT_PCLMULQDQ |
             CPUID_EXT_SSE3,
        .ext2_features = CPUID_EXT2_LM | CPUID_EXT2_RDTSCP | CPUID_EXT2_NX |
             CPUID_EXT2_SYSCALL,
        .ext3_features = CPUID_EXT3_LAHF_LM,
        .xlevel = 0x8000000A,
        .model_id = "Intel Xeon E312xx (Sandy Bridge)",
      },
    },
    {
      .name = "Opteron_G1",
      .cpudef = {
        .level = 5,
        .vendor = CPUID_VENDOR_AMD,
        .family = 15,
        .model = 6,
        .stepping = 1,
        .features = CPUID_SSE2 | CPUID_SSE | CPUID_FXSR | CPUID_MMX |
             CPUID_CLFLUSH | CPUID_PSE36 | CPUID_PAT | CPUID_CMOV | CPUID_MCA |
             CPUID_PGE | CPUID_MTRR | CPUID_SEP | CPUID_APIC | CPUID_CX8 |
             CPUID_MCE | CPUID_PAE | CPUID_MSR | CPUID_TSC | CPUID_PSE |
             CPUID_DE | CPUID_FP87,
        .ext_features = CPUID_EXT_SSE3,
        .ext2_features = CPUID_EXT2_LM | CPUID_EXT2_FXSR | CPUID_EXT2_MMX |
             CPUID_EXT2_NX | CPUID_EXT2_PSE36 | CPUID_EXT2_PAT |
             CPUID_EXT2_CMOV | CPUID_EXT2_MCA | CPUID_EXT2_PGE |
             CPUID_EXT2_MTRR | CPUID_EXT2_SYSCALL | CPUID_EXT2_APIC |
             CPUID_EXT2_CX8 | CPUID_EXT2_MCE | CPUID_EXT2_PAE | CPUID_EXT2_MSR |
             CPUID_EXT2_TSC | CPUID_EXT2_PSE | CPUID_EXT2_DE | CPUID_EXT2_FPU,
        .xlevel = 0x80000008,
        .model_id = "AMD Opteron 240 (Gen 1 Class Opteron)",
      },
    },
    {
      .name = "Opteron_G2",
      .cpudef = {
        .level = 5,
        .vendor = CPUID_VENDOR_AMD,
        .family = 15,
        .model = 6,
        .stepping = 1,
        .features = CPUID_SSE2 | CPUID_SSE | CPUID_FXSR | CPUID_MMX |
             CPUID_CLFLUSH | CPUID_PSE36 | CPUID_PAT | CPUID_CMOV | CPUID_MCA |
             CPUID_PGE | CPUID_MTRR | CPUID_SEP | CPUID_APIC | CPUID_CX8 |
             CPUID_MCE | CPUID_PAE | CPUID_MSR | CPUID_TSC | CPUID_PSE |
             CPUID_DE | CPUID_FP87,
        .ext_features = CPUID_EXT_CX16 | CPUID_EXT_SSE3,
        .ext2_features = CPUID_EXT2_LM | CPUID_EXT2_RDTSCP | CPUID_EXT2_FXSR |
             CPUID_EXT2_MMX | CPUID_EXT2_NX | CPUID_EXT2_PSE36 |
             CPUID_EXT2_PAT | CPUID_EXT2_CMOV | CPUID_EXT2_MCA |
             CPUID_EXT2_PGE | CPUID_EXT2_MTRR | CPUID_EXT2_SYSCALL |
             CPUID_EXT2_APIC | CPUID_EXT2_CX8 | CPUID_EXT2_MCE |
             CPUID_EXT2_PAE | CPUID_EXT2_MSR | CPUID_EXT2_TSC | CPUID_EXT2_PSE |
             CPUID_EXT2_DE | CPUID_EXT2_FPU,
        .ext3_features = CPUID_EXT3_SVM | CPUID_EXT3_LAHF_LM,
        .xlevel = 0x80000008,
        .model_id = "AMD Opteron 22xx (Gen 2 Class Opteron)",
      },
    },
    {
      .name = "Opteron_G3",
      .cpudef = {
        .level = 5,
        .vendor = CPUID_VENDOR_AMD,
        .family = 15,
        .model = 6,
        .stepping = 1,
        .features = CPUID_SSE2 | CPUID_SSE | CPUID_FXSR | CPUID_MMX |
             CPUID_CLFLUSH | CPUID_PSE36 | CPUID_PAT | CPUID_CMOV | CPUID_MCA |
             CPUID_PGE | CPUID_MTRR | CPUID_SEP | CPUID_APIC | CPUID_CX8 |
             CPUID_MCE | CPUID_PAE | CPUID_MSR | CPUID_TSC | CPUID_PSE |
             CPUID_DE | CPUID_FP87,
        .ext_features = CPUID_EXT_POPCNT | CPUID_EXT_CX16 | CPUID_EXT_MONITOR |
             CPUID_EXT_SSE3,
        .ext2_features = CPUID_EXT2_LM | CPUID_EXT2_RDTSCP | CPUID_EXT2_FXSR |
             CPUID_EXT2_MMX | CPUID_EXT2_NX | CPUID_EXT2_PSE36 |
             CPUID_EXT2_PAT | CPUID_EXT2_CMOV | CPUID_EXT2_MCA |
             CPUID_EXT2_PGE | CPUID_EXT2_MTRR | CPUID_EXT2_SYSCALL |
             CPUID_EXT2_APIC | CPUID_EXT2_CX8 | CPUID_EXT2_MCE |
             CPUID_EXT2_PAE | CPUID_EXT2_MSR | CPUID_EXT2_TSC | CPUID_EXT2_PSE |
             CPUID_EXT2_DE | CPUID_EXT2_FPU,
        .ext3_features = CPUID_EXT3_MISALIGNSSE | CPUID_EXT3_SSE4A |
             CPUID_EXT3_ABM | CPUID_EXT3_SVM | CPUID_EXT3_LAHF_LM,
        .xlevel = 0x80000008,
        .model_id = "AMD Opteron 23xx (Gen 3 Class Opteron)",
      },
    },
    {
      .name = "Opteron_G4",
      .cpudef = {
        .level = 0xd,
        .vendor = CPUID_VENDOR_AMD,
        .family = 21,
        .model = 1,
        .stepping = 2,
        .features = CPUID_SSE2 | CPUID_SSE | CPUID_FXSR | CPUID_MMX |
             CPUID_CLFLUSH | CPUID_PSE36 | CPUID_PAT | CPUID_CMOV | CPUID_MCA |
             CPUID_PGE | CPUID_MTRR | CPUID_SEP | CPUID_APIC | CPUID_CX8 |
             CPUID_MCE | CPUID_PAE | CPUID_MSR | CPUID_TSC | CPUID_PSE |
             CPUID_DE | CPUID_FP87,
        .ext_features = CPUID_EXT_AVX | CPUID_EXT_XSAVE | CPUID_EXT_AES |
             CPUID_EXT_POPCNT | CPUID_EXT_SSE42 | CPUID_EXT_SSE41 |
             CPUID_EXT_CX16 | CPUID_EXT_SSSE3 | CPUID_EXT_PCLMULQDQ |
             CPUID_EXT_SSE3,
        .ext2_features = CPUID_EXT2_LM | CPUID_EXT2_RDTSCP |
             CPUID_EXT2_PDPE1GB | CPUID_EXT2_FXSR | CPUID_EXT2_MMX |
             CPUID_EXT2_NX | CPUID_EXT2_PSE36 | CPUID_EXT2_PAT |
             CPUID_EXT2_CMOV | CPUID_EXT2_MCA | CPUID_EXT2_PGE |
             CPUID_EXT2_MTRR | CPUID_EXT2_SYSCALL | CPUID_EXT2_APIC |
             CPUID_EXT2_CX8 | CPUID_EXT2_MCE | CPUID_EXT2_PAE | CPUID_EXT2_MSR |
             CPUID_EXT2_TSC | CPUID_EXT2_PSE | CPUID_EXT2_DE | CPUID_EXT2_FPU,
        .ext3_features = CPUID_EXT3_FMA4 | CPUID_EXT3_XOP |
             CPUID_EXT3_3DNOWPREFETCH | CPUID_EXT3_MISALIGNSSE |
             CPUID_EXT3_SSE4A | CPUID_EXT3_ABM | CPUID_EXT3_SVM |
             CPUID_EXT3_LAHF_LM,
        .xlevel = 0x8000001A,
        .model_id = "AMD Opteron 62xx class CPU",
      },
    },
};

static int cpu_x86_fill_model_id(char *str)
{
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    int i;

    for (i = 0; i < 3; i++) {
        host_cpuid(0x80000002 + i, 0, &eax, &ebx, &ecx, &edx);
        memcpy(str + i * 16 +  0, &eax, 4);
        memcpy(str + i * 16 +  4, &ebx, 4);
        memcpy(str + i * 16 +  8, &ecx, 4);
        memcpy(str + i * 16 + 12, &edx, 4);
    }
    return 0;
}

/* Fill X86CPUDefinition struct with host CPU features */
static int cpu_x86_fill_host(X86CPUDefinition *x86_cpu_def)
{
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    int i;

    host_cpuid(0x0, 0, &eax, &ebx, &ecx, &edx);
    x86_cpu_def->level = eax;
    for (i = 0; i < 4; i++) {
        x86_cpu_def->vendor[i] = ebx >> (8 * i);
        x86_cpu_def->vendor[i + 4] = edx >> (8 * i);
        x86_cpu_def->vendor[i + 8] = ecx >> (8 * i);
    }

    host_cpuid(0x1, 0, &eax, &ebx, &ecx, &edx);
    x86_cpu_def->family = ((eax >> 8) & 0x0F) + ((eax >> 20) & 0xFF);
    x86_cpu_def->model = ((eax >> 4) & 0x0F) | ((eax & 0xF0000) >> 12);
    x86_cpu_def->stepping = eax & 0x0F;
    x86_cpu_def->ext_features = ecx;
    x86_cpu_def->features = edx;

    if (kvm_enabled() && x86_cpu_def->level >= 7) {
        x86_cpu_def->cpuid_7_0_ebx_features = kvm_arch_get_supported_cpuid(kvm_state, 0x7, 0, R_EBX);
    } else {
        x86_cpu_def->cpuid_7_0_ebx_features = 0;
    }

    host_cpuid(0x80000000, 0, &eax, &ebx, &ecx, &edx);
    x86_cpu_def->xlevel = eax;

    host_cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx);
    x86_cpu_def->ext2_features = edx;
    x86_cpu_def->ext3_features = ecx;
    cpu_x86_fill_model_id(x86_cpu_def->model_id);
    x86_cpu_def->vendor_override = 0;

    /* Call Centaur's CPUID instruction. */
    if (!strcmp(x86_cpu_def->vendor, CPUID_VENDOR_VIA)) {
        host_cpuid(0xC0000000, 0, &eax, &ebx, &ecx, &edx);
        if (eax >= 0xC0000001) {
            /* Support VIA max extended level */
            x86_cpu_def->xlevel2 = eax;
            host_cpuid(0xC0000001, 0, &eax, &ebx, &ecx, &edx);
            x86_cpu_def->ext4_features = edx;
        }
    }

    /*
     * Every SVM feature requires emulation support in KVM - so we can't just
     * read the host features here. KVM might even support SVM features not
     * available on the host hardware. Just set all bits and mask out the
     * unsupported ones later.
     */
    x86_cpu_def->svm_features = -1;

    return 0;
}

static int unavailable_host_feature(struct model_features_t *f, uint32_t mask)
{
    int i;

    for (i = 0; i < 32; ++i)
        if (1 << i & mask) {
            fprintf(stderr, "warning: host cpuid %04x_%04x lacks requested"
                " flag '%s' [0x%08x]\n",
                f->cpuid >> 16, f->cpuid & 0xffff,
                f->flag_names[i] ? f->flag_names[i] : "[reserved]", mask);
            break;
        }
    return 0;
}

/* best effort attempt to inform user requested cpu flags aren't making
 * their way to the guest.  Note: ft[].check_feat ideally should be
 * specified via a guest_def field to suppress report of extraneous flags.
 */
static int check_features_against_host(X86CPU *cpu)
{
    CPUX86State *env = &cpu->env;
    X86CPUDefinition host_def;
    uint32_t mask;
    int rv, i;
    struct model_features_t ft[] = {
        {&env->cpuid_features, &host_def.features,
            ~0, feature_name, 0x00000000},
        {&env->cpuid_ext_features, &host_def.ext_features,
            ~CPUID_EXT_HYPERVISOR, ext_feature_name, 0x00000001},
        {&env->cpuid_ext2_features, &host_def.ext2_features,
            ~PPRO_FEATURES, ext2_feature_name, 0x80000000},
        {&env->cpuid_ext3_features, &host_def.ext3_features,
            ~CPUID_EXT3_SVM, ext3_feature_name, 0x80000001}};

    cpu_x86_fill_host(&host_def);
    for (rv = 0, i = 0; i < ARRAY_SIZE(ft); ++i)
        for (mask = 1; mask; mask <<= 1)
            if (ft[i].check_feat & mask && *ft[i].guest_feat & mask &&
                !(*ft[i].host_feat & mask)) {
                    unavailable_host_feature(&ft[i], mask);
                    rv = 1;
                }
    return rv;
}

static bool is_feature_set(const char *name, const uint32_t featbitmap,
                                  const char **featureset)
{
    uint32_t mask;

    for (mask = 1; mask; mask <<= 1) {
        if (featureset[ffs(mask) - 1] &&
            !altcmp(name, NULL, featureset[ffs(mask) - 1])) {
            break;
    }
    }
    if (featbitmap & mask) {
        return true;
    }
    return false;
}

static void x86_cpuid_get_feature(Object *obj, Visitor *v, void *opaque,
                                         const char *name, Error **errp)
{
    X86CPU *cpu = X86_CPU(obj);
    CPUX86State *env = &cpu->env;
    bool value = true;

    if (!is_feature_set(name, env->cpuid_features, feature_name) &&
       !is_feature_set(name, env->cpuid_ext_features, ext_feature_name) &&
       !is_feature_set(name, env->cpuid_ext2_features, ext2_feature_name) &&
       !is_feature_set(name, env->cpuid_ext3_features, ext3_feature_name) &&
       !is_feature_set(name, env->cpuid_kvm_features, kvm_feature_name) &&
       !is_feature_set(name, env->cpuid_svm_features, svm_feature_name)) {
        value = false;
    }

    visit_type_bool(v, &value, name, errp);
}

static void x86_cpuid_set_feature(Object *obj, Visitor *v, void *opaque,
                                         const char *name, Error **errp)
{
    X86CPU *cpu = X86_CPU(obj);
    CPUX86State *env = &cpu->env;
    uint32_t mask = 0;
    uint32_t *dst_features;
    bool value;

    visit_type_bool(v, &value, name, errp);
    if (error_is_set(errp)) {
        return;
    }

    if (lookup_feature(&mask, name, NULL, feature_name)) {
        dst_features = &env->cpuid_features;
    } else if (lookup_feature(&mask, name, NULL, ext_feature_name)) {
        dst_features = &env->cpuid_ext_features;
    } else if (lookup_feature(&mask, name, NULL, ext2_feature_name)) {
        dst_features = &env->cpuid_ext2_features;
    } else if (lookup_feature(&mask, name, NULL, ext3_feature_name)) {
        dst_features = &env->cpuid_ext3_features;
    } else if (lookup_feature(&mask, name, NULL, kvm_feature_name)) {
        dst_features = &env->cpuid_kvm_features;
    } else if (lookup_feature(&mask, name, NULL, svm_feature_name)) {
        dst_features = &env->cpuid_svm_features;
    } else {
        error_set(errp, QERR_PROPERTY_NOT_FOUND, "", name);
        return;
    }

    if (value) {
        *dst_features |= mask;
    } else {
        *dst_features &= ~mask;
    }
}

static void x86_register_cpuid_properties(Object *obj, const char **featureset)
{
    uint32_t mask;

    for (mask = 1; mask; mask <<= 1) {
        if (featureset[ffs(mask) - 1]) {
            char *feature_name, *save_ptr;
            char buf[32];
            if (strlen(featureset[ffs(mask) - 1]) > sizeof(buf) - 1) {
                abort();
            }
            pstrcpy(buf, sizeof(buf), featureset[ffs(mask) - 1]);
            feature_name = strtok_r(buf, "|", &save_ptr);
            while (feature_name) {
                object_property_add(obj, feature_name, "bool",
                                x86_cpuid_get_feature,
                                x86_cpuid_set_feature, NULL, NULL, NULL);
                feature_name = strtok_r(NULL, "|", &save_ptr);
            }
        }
    }
}

static void x86_cpuid_version_get_family(Object *obj, Visitor *v, void *opaque,
                                         const char *name, Error **errp)
{
    X86CPU *cpu = X86_CPU(obj);
    CPUX86State *env = &cpu->env;
    int64_t value;

    value = (env->cpuid_version >> 8) & 0xf;
    if (value == 0xf) {
        value += (env->cpuid_version >> 20) & 0xff;
    }
    visit_type_int(v, &value, name, errp);
}

static void x86_cpuid_version_set_family(Object *obj, Visitor *v, void *opaque,
                                         const char *name, Error **errp)
{
    X86CPU *cpu = X86_CPU(obj);
    CPUX86State *env = &cpu->env;
    const int64_t min = 0;
    const int64_t max = 0xff + 0xf;
    int64_t value;

    visit_type_int(v, &value, name, errp);
    if (error_is_set(errp)) {
        return;
    }
    if (value < min || value > max) {
        error_set(errp, QERR_PROPERTY_VALUE_OUT_OF_RANGE, "",
                  name ? name : "null", value, min, max);
        return;
    }

    env->cpuid_version &= ~0xff00f00;
    if (value > 0x0f) {
        env->cpuid_version |= 0xf00 | ((value - 0x0f) << 20);
    } else {
        env->cpuid_version |= value << 8;
    }
}

static void x86_cpuid_version_get_model(Object *obj, Visitor *v, void *opaque,
                                        const char *name, Error **errp)
{
    X86CPU *cpu = X86_CPU(obj);
    CPUX86State *env = &cpu->env;
    int64_t value;

    value = (env->cpuid_version >> 4) & 0xf;
    value |= ((env->cpuid_version >> 16) & 0xf) << 4;
    visit_type_int(v, &value, name, errp);
}

static void x86_cpuid_version_set_model(Object *obj, Visitor *v, void *opaque,
                                        const char *name, Error **errp)
{
    X86CPU *cpu = X86_CPU(obj);
    CPUX86State *env = &cpu->env;
    const int64_t min = 0;
    const int64_t max = 0xff;
    int64_t value;

    visit_type_int(v, &value, name, errp);
    if (error_is_set(errp)) {
        return;
    }
    if (value < min || value > max) {
        error_set(errp, QERR_PROPERTY_VALUE_OUT_OF_RANGE, "",
                  name ? name : "null", value, min, max);
        return;
    }

    env->cpuid_version &= ~0xf00f0;
    env->cpuid_version |= ((value & 0xf) << 4) | ((value >> 4) << 16);
}

static void x86_cpuid_version_get_stepping(Object *obj, Visitor *v,
                                           void *opaque, const char *name,
                                           Error **errp)
{
    X86CPU *cpu = X86_CPU(obj);
    CPUX86State *env = &cpu->env;
    int64_t value;

    value = env->cpuid_version & 0xf;
    visit_type_int(v, &value, name, errp);
}

static void x86_cpuid_version_set_stepping(Object *obj, Visitor *v,
                                           void *opaque, const char *name,
                                           Error **errp)
{
    X86CPU *cpu = X86_CPU(obj);
    CPUX86State *env = &cpu->env;
    const int64_t min = 0;
    const int64_t max = 0xf;
    int64_t value;

    visit_type_int(v, &value, name, errp);
    if (error_is_set(errp)) {
        return;
    }
    if (value < min || value > max) {
        error_set(errp, QERR_PROPERTY_VALUE_OUT_OF_RANGE, "",
                  name ? name : "null", value, min, max);
        return;
    }

    env->cpuid_version &= ~0xf;
    env->cpuid_version |= value & 0xf;
}

static void x86_cpuid_get_level(Object *obj, Visitor *v, void *opaque,
                                const char *name, Error **errp)
{
    X86CPU *cpu = X86_CPU(obj);

    visit_type_uint32(v, &cpu->env.cpuid_level, name, errp);
}

static void x86_cpuid_set_level(Object *obj, Visitor *v, void *opaque,
                                const char *name, Error **errp)
{
    X86CPU *cpu = X86_CPU(obj);

    visit_type_uint32(v, &cpu->env.cpuid_level, name, errp);
}

static void x86_cpuid_get_xlevel(Object *obj, Visitor *v, void *opaque,
                                 const char *name, Error **errp)
{
    X86CPU *cpu = X86_CPU(obj);

    visit_type_uint32(v, &cpu->env.cpuid_xlevel, name, errp);
}

static void x86_cpuid_set_xlevel(Object *obj, Visitor *v, void *opaque,
                                 const char *name, Error **errp)
{
    X86CPU *cpu = X86_CPU(obj);
    uint32_t value;

    visit_type_uint32(v, &value, name, errp);
    if (error_is_set(errp)) {
        return;
    }

    if (value < 0x80000000) {
        value += 0x80000000;
    }
    cpu->env.cpuid_xlevel = value;
}

static char *x86_cpuid_get_vendor(Object *obj, Error **errp)
{
    X86CPU *cpu = X86_CPU(obj);
    CPUX86State *env = &cpu->env;
    char *value;
    int i;

    value = (char *)g_malloc(CPUID_VENDOR_SZ + 1);
    for (i = 0; i < 4; i++) {
        value[i    ] = env->cpuid_vendor1 >> (8 * i);
        value[i + 4] = env->cpuid_vendor2 >> (8 * i);
        value[i + 8] = env->cpuid_vendor3 >> (8 * i);
    }
    value[CPUID_VENDOR_SZ] = '\0';
    return value;
}

static void x86_cpuid_set_vendor(Object *obj, const char *value,
                                 Error **errp)
{
    X86CPU *cpu = X86_CPU(obj);
    CPUX86State *env = &cpu->env;
    int i;

    if (strlen(value) != CPUID_VENDOR_SZ) {
        error_set(errp, QERR_PROPERTY_VALUE_BAD, "",
                  "vendor", value);
        return;
    }

    env->cpuid_vendor1 = 0;
    env->cpuid_vendor2 = 0;
    env->cpuid_vendor3 = 0;
    for (i = 0; i < 4; i++) {
        env->cpuid_vendor1 |= ((uint8_t)value[i    ]) << (8 * i);
        env->cpuid_vendor2 |= ((uint8_t)value[i + 4]) << (8 * i);
        env->cpuid_vendor3 |= ((uint8_t)value[i + 8]) << (8 * i);
    }
}

static char *x86_cpuid_get_model_id(Object *obj, Error **errp)
{
    X86CPU *cpu = X86_CPU(obj);
    CPUX86State *env = &cpu->env;
    char *value;
    int i;

    value = g_malloc(48 + 1);
    for (i = 0; i < 48; i++) {
        value[i] = env->cpuid_model[i >> 2] >> (8 * (i & 3));
    }
    value[48] = '\0';
    return value;
}

static void x86_cpuid_set_model_id(Object *obj, const char *model_id,
                                   Error **errp)
{
    X86CPU *cpu = X86_CPU(obj);
    CPUX86State *env = &cpu->env;
    int c, len, i;

    if (model_id == NULL) {
        model_id = "";
    }
    len = strlen(model_id);
    memset(env->cpuid_model, 0, 48);
    for (i = 0; i < 48; i++) {
        if (i >= len) {
            c = '\0';
        } else {
            c = (uint8_t)model_id[i];
        }
        env->cpuid_model[i >> 2] |= c << (8 * (i & 3));
    }
}

static void x86_cpuid_get_tsc_freq(Object *obj, Visitor *v, void *opaque,
                                   const char *name, Error **errp)
{
    X86CPU *cpu = X86_CPU(obj);
    int64_t value;

    value = cpu->env.tsc_khz * 1000;
    visit_type_int(v, &value, name, errp);
}

static void x86_cpuid_set_tsc_freq(Object *obj, Visitor *v, void *opaque,
                                   const char *name, Error **errp)
{
    X86CPU *cpu = X86_CPU(obj);
    const int64_t min = 0;
    const int64_t max = INT_MAX;
    int64_t value;

    visit_type_hz(v, &value, name, errp);
    if (error_is_set(errp)) {
        return;
    }
    if (value < min || value > max) {
        error_set(errp, QERR_PROPERTY_VALUE_OUT_OF_RANGE, "",
                  name ? name : "null", value, min, max);
        return;
    }

    cpu->env.tsc_khz = value / 1000;
}

#if !defined(CONFIG_USER_ONLY)
static void x86_get_hv_spinlocks(Object *obj, Visitor *v, void *opaque,
                                 const char *name, Error **errp)
{
    int64_t value = hyperv_get_spinlock_retries();

    visit_type_int(v, &value, name, errp);
}

static void x86_set_hv_spinlocks(Object *obj, Visitor *v, void *opaque,
                                 const char *name, Error **errp)
{
    int64_t value;

    visit_type_int(v, &value, name, errp);
    if (error_is_set(errp)) {
        return;
    }
    if (!value) {
        error_set(errp, QERR_PROPERTY_VALUE_BAD, "", name, "0");
        return;
    }
    hyperv_set_spinlock_retries(value);
}

static void x86_get_hv_relaxed(Object *obj, Visitor *v, void *opaque,
                                 const char *name, Error **errp)
{
    bool value = hyperv_relaxed_timing_enabled();

    visit_type_bool(v, &value, name, errp);
}

static void x86_set_hv_relaxed(Object *obj, Visitor *v, void *opaque,
                                 const char *name, Error **errp)
{
    bool value;

    visit_type_bool(v, &value, name, errp);
    if (error_is_set(errp)) {
        return;
    }
    hyperv_enable_relaxed_timing(value);
}

static void x86_get_hv_vapic(Object *obj, Visitor *v, void *opaque,
                                 const char *name, Error **errp)
{
    bool value = hyperv_vapic_recommended();

    visit_type_bool(v, &value, name, errp);
}

static void x86_set_hv_vapic(Object *obj, Visitor *v, void *opaque,
                                 const char *name, Error **errp)
{
    bool value;

    visit_type_bool(v, &value, name, errp);
    if (error_is_set(errp)) {
        return;
    }
    hyperv_enable_vapic_recommended(value);
}
#endif

static void x86_cpuid_get_check(Object *obj, Visitor *v, void *opaque,
                                         const char *name, Error **errp)
{
    visit_type_bool(v, &check_cpuid, name, errp);
}

static void x86_cpuid_set_check(Object *obj, Visitor *v, void *opaque,
                                         const char *name, Error **errp)
{
    bool value;

    visit_type_bool(v, &value, name, errp);
    if (error_is_set(errp)) {
        return;
    }
    check_cpuid = value;
}

static void x86_cpuid_get_enforce(Object *obj, Visitor *v, void *opaque,
                                         const char *name, Error **errp)
{
    visit_type_bool(v, &enforce_cpuid, name, errp);
}

static void x86_cpuid_set_enforce(Object *obj, Visitor *v, void *opaque,
                                         const char *name, Error **errp)
{
    bool value;

    visit_type_bool(v, &value, name, errp);
    if (error_is_set(errp)) {
        return;
    }
    enforce_cpuid = value;
    object_property_set_bool(obj, value, "check", errp);
}

static void
x86_cpuid_get_vendor_override(Object *obj, Visitor *v, void *opaque,
                                         const char *name, Error **errp)
{
    X86CPU *cpu = X86_CPU(obj);
    CPUX86State *env = &cpu->env;

    visit_type_bool(v, &env->cpuid_vendor_override, name, errp);
}

static void
x86_cpuid_set_vendor_override(Object *obj, Visitor *v, void *opaque,
                                         const char *name, Error **errp)
{
    X86CPU *cpu = X86_CPU(obj);
    CPUX86State *env = &cpu->env;
    bool value;

    visit_type_bool(v, &value, name, errp);
    if (error_is_set(errp)) {
        return;
    }
    env->cpuid_vendor_override = value;
}

static void cpudef_2_x86_cpu(X86CPU *cpu, X86CPUDefinition *def, Error **errp)
{
    CPUX86State *env = &cpu->env;

    object_property_set_str(OBJECT(cpu), def->vendor[0] ?
                            def->vendor : CPUID_VENDOR_INTEL, "vendor", errp);
    object_property_set_bool(OBJECT(cpu), true, "vendor-override", errp);
    object_property_set_int(OBJECT(cpu), def->level, "level", errp);
    object_property_set_int(OBJECT(cpu), def->family, "family", errp);
    object_property_set_int(OBJECT(cpu), def->model, "model", errp);
    object_property_set_int(OBJECT(cpu), def->stepping, "stepping", errp);
    object_property_set_int(OBJECT(cpu), def->xlevel, "xlevel", errp);
    object_property_set_str(OBJECT(cpu), def->model_id, "model-id", errp);
    object_property_set_int(OBJECT(cpu), (int64_t)def->tsc_khz * 1000,
                            "tsc-frequency", errp);
    env->cpuid_features = def->features;
    env->cpuid_ext_features = def->ext_features;
    env->cpuid_ext2_features = def->ext2_features;
    env->cpuid_ext3_features = def->ext3_features;
    env->cpuid_svm_features = def->svm_features;
    env->cpuid_ext4_features = def->ext4_features;
    env->cpuid_7_0_ebx = def->cpuid_7_0_ebx_features;
    env->cpuid_xlevel2 = def->xlevel2;

    /* not supported bits will be filtered out later */
    env->cpuid_kvm_features = ~0;

    object_property_set_bool(OBJECT(cpu), true, "hypervisor", errp);
}

/* convert legacy cpumodel string to string cpu_name and
 * a uniforms set of custom features that will be applied to CPU
 * using object_property_parse()
 */
static void compat_normalize_cpu_model(const char *cpu_model, char **cpu_name,
                                        QDict **features, Error **errp)
{

    char *s = g_strdup(cpu_model);
    char *featurestr, *sptr;

    *cpu_name = strtok_r(s, ",", &sptr);
    *features = qdict_new();

    featurestr = strtok_r(NULL, ",", &sptr);
    while (featurestr) {
        char *val;
        if (featurestr[0] == '+') {
            /*
             * preseve legacy behaviour, if feature was disabled once
             * do not allow to enable it again
             */
            if (!qdict_haskey(*features, featurestr + 1)) {
                qdict_put(*features, featurestr + 1, qstring_from_str("on"));
            }
        } else if (featurestr[0] == '-') {
            qdict_put(*features, featurestr + 1, qstring_from_str("off"));
        } else {
            val = strchr(featurestr, '=');
            if (val) {
                *val = 0; val++;
                if (!strcmp(featurestr, "vendor")) {
                    qdict_put(*features, "vendor-override",
                              qstring_from_str("on"));
                    qdict_put(*features, featurestr, qstring_from_str(val));
                } else if (!strcmp(featurestr, "tsc_freq")) {
                    qdict_put(*features, "tsc-frequency",
                              qstring_from_str(val));
                } else {
                    qdict_put(*features, featurestr, qstring_from_str(val));
                }
            } else {
                qdict_put(*features, featurestr, qstring_from_str("on"));
            }
        }

        featurestr = strtok_r(NULL, ",", &sptr);
    }

    return;
}

/* Set features on X86CPU object based on a QDict */
static void cpu_x86_set_props(X86CPU *cpu, QDict *features, Error **errp)
{
    const QDictEntry *ent;
    for (ent = qdict_first(features); ent; ent = qdict_next(features, ent)) {
        const QString *qval = qobject_to_qstring(qdict_entry_value(ent));
        object_property_parse(OBJECT(cpu), qstring_get_str(qval),
                              qdict_entry_key(ent), errp);
        if (error_is_set(errp)) {
            return;
        }
    }
}

/* generate a composite string into buf of all cpuid names in featureset
 * selected by fbits.  indicate truncation at bufsize in the event of overflow.
 * if flags, suppress names undefined in featureset.
 */
static void listflags(char *buf, int bufsize, uint32_t fbits,
    const char **featureset, uint32_t flags)
{
    const char **p = &featureset[31];
    char *q, *b, bit;
    int nc;

    b = 4 <= bufsize ? buf + (bufsize -= 3) - 1 : NULL;
    *buf = '\0';
    for (q = buf, bit = 31; fbits && bufsize; --p, fbits &= ~(1 << bit), --bit)
        if (fbits & 1 << bit && (*p || !flags)) {
            if (*p)
                nc = snprintf(q, bufsize, "%s%s", q == buf ? "" : " ", *p);
            else
                nc = snprintf(q, bufsize, "%s[%d]", q == buf ? "" : " ", bit);
            if (bufsize <= nc) {
                if (b) {
                    memcpy(b, "...", sizeof("..."));
                }
                return;
            }
            q += nc;
            bufsize -= nc;
        }
}

/* generate CPU information:
 * -?        list model names
 * -?model   list model names/IDs
 * -?dump    output all model (X86CPUDefinition) data
 * -?cpuid   list all recognized cpuid flag names
 */
void x86_cpu_list(FILE *f, fprintf_function cpu_fprintf, const char *optarg)
{
    unsigned char model = !strcmp("?model", optarg);
    unsigned char dump = !strcmp("?dump", optarg);
    unsigned char cpuid = !strcmp("?cpuid", optarg);
    X86CPUModelTableEntry *deft;
    char buf[256];

    if (cpuid) {
        (*cpu_fprintf)(f, "Recognized CPUID flags:\n");
        listflags(buf, sizeof(buf), (uint32_t)~0, feature_name, 1);
        (*cpu_fprintf)(f, "  f_edx: %s\n", buf);
        listflags(buf, sizeof(buf), (uint32_t)~0, ext_feature_name, 1);
        (*cpu_fprintf)(f, "  f_ecx: %s\n", buf);
        listflags(buf, sizeof(buf), (uint32_t)~0, ext2_feature_name, 1);
        (*cpu_fprintf)(f, "  extf_edx: %s\n", buf);
        listflags(buf, sizeof(buf), (uint32_t)~0, ext3_feature_name, 1);
        (*cpu_fprintf)(f, "  extf_ecx: %s\n", buf);
        return;
    }
    for (deft = x86_defs; deft; deft = deft->next) {
        X86CPUDefinition *def = &deft->cpudef;
        snprintf(buf, sizeof(buf), deft->is_builtin ? "[%s]" : "%s",
                                   deft->name);
        if (model || dump) {
            (*cpu_fprintf)(f, "x86 %16s  %-48s\n", buf, def->model_id);
        } else {
            (*cpu_fprintf)(f, "x86 %16s\n", buf);
        }
        if (dump) {
            pstrcpy(buf, sizeof(def->vendor), def->vendor);
            (*cpu_fprintf)(f,
                "  family %d model %d stepping %d level %d xlevel 0x%x"
                " vendor \"%s\"\n",
                def->family, def->model, def->stepping, def->level,
                def->xlevel, buf);
            listflags(buf, sizeof(buf), def->features, feature_name, 0);
            (*cpu_fprintf)(f, "  feature_edx %08x (%s)\n", def->features,
                buf);
            listflags(buf, sizeof(buf), def->ext_features, ext_feature_name,
                0);
            (*cpu_fprintf)(f, "  feature_ecx %08x (%s)\n", def->ext_features,
                buf);
            listflags(buf, sizeof(buf), def->ext2_features, ext2_feature_name,
                0);
            (*cpu_fprintf)(f, "  extfeature_edx %08x (%s)\n",
                def->ext2_features, buf);
            listflags(buf, sizeof(buf), def->ext3_features, ext3_feature_name,
                0);
            (*cpu_fprintf)(f, "  extfeature_ecx %08x (%s)\n",
                def->ext3_features, buf);
            (*cpu_fprintf)(f, "\n");
        }
    }
    if (kvm_enabled()) {
        (*cpu_fprintf)(f, "x86 %16s\n", "[host]");
    }
}

CpuDefinitionInfoList *qmp_query_cpu_definitions(Error **errp)
{
    CpuDefinitionInfoList *cpu_list = NULL;
    X86CPUModelTableEntry *def;

    for (def = x86_defs; def; def = def->next) {
        CpuDefinitionInfoList *entry;
        CpuDefinitionInfo *info;

        info = g_malloc0(sizeof(*info));
        info->name = g_strdup(def->name);

        entry = g_malloc0(sizeof(*entry));
        entry->value = info;
        entry->next = cpu_list;
        cpu_list = entry;
    }

    return cpu_list;
}

/* Build class name for specific CPU model */
static char *build_cpu_class_name(const char *model)
{
    return g_strdup_printf("%s.%s", TYPE_X86_CPU, model);
}

X86CPU *cpu_x86_create(const char *cpu_model)
{
    X86CPU *cpu;
    CPUX86State *env;
    X86CPUDefinition def1, *def = &def1;
    Error *error = NULL;
    QDict *features;
    char *name = NULL;
    char *class_name = NULL;

    compat_normalize_cpu_model(cpu_model, &name, &features, &error);
    if (error_is_set(&error)) {
        goto error;
    }

    class_name = build_cpu_class_name(name);
    cpu = X86_CPU(object_new(class_name));
    env = &cpu->env;
    env->cpu_model_str = cpu_model;

    *def = X86_CPU_GET_CLASS(cpu)->cpudef;

    cpudef_2_x86_cpu(cpu, def, &error);
    cpu_x86_set_props(cpu, features, &error);
    if (error_is_set(&error)) {
        goto error;
    }

    QDECREF(features);
    g_free(class_name);
    g_free(name);

    x86_cpu_realize(OBJECT(cpu), NULL);
    return cpu;

error:
    g_free(name);
    g_free(class_name);
    if (error_is_set(&error)) {
        fprintf(stderr, "%s\n", error_get_pretty(error));
        error_free(error);
    }
    object_delete(OBJECT(cpu));
    return NULL;
}

#if !defined(CONFIG_USER_ONLY)
/* interpret radix and convert from string to arbitrary scalar,
 * otherwise flag failure
 */
#define setscalar(pval, str, perr)                      \
{                                                       \
    char *pend;                                         \
    unsigned long ul;                                   \
                                                        \
    ul = strtoul(str, &pend, 0);                        \
    *str && !*pend ? (*pval = ul) : (*perr = 1);        \
}

/* map cpuid options to feature bits, otherwise return failure
 * (option tags in *str are delimited by whitespace)
 */
static void setfeatures(uint32_t *pval, const char *str,
    const char **featureset, int *perr)
{
    const char *p, *q;

    for (q = p = str; *p || *q; q = p) {
        while (iswhite(*p))
            q = ++p;
        while (*p && !iswhite(*p))
            ++p;
        if (!*q && !*p)
            return;
        if (!lookup_feature(pval, q, p, featureset)) {
            fprintf(stderr, "error: feature \"%.*s\" not available in set\n",
                (int)(p - q), q);
            *perr = 1;
            return;
        }
    }
}

/* map config file options to X86CPUDefinition form
 */
static int cpudef_setfield(const char *name, const char *str, void *opaque)
{
    X86CPUModelTableEntry *deft = opaque;
    X86CPUDefinition *def = &deft->cpudef;
    int err = 0;

    if (!strcmp(name, "name")) {
        g_free((void *)deft->name);
        deft->name = g_strdup(str);
    } else if (!strcmp(name, "model_id")) {
        strncpy(def->model_id, str, sizeof(def->model_id));
    } else if (!strcmp(name, "level")) {
        setscalar(&def->level, str, &err)
    } else if (!strcmp(name, "vendor")) {
        pstrcpy(def->vendor, sizeof(def->vendor), str);
    } else if (!strcmp(name, "family")) {
        setscalar(&def->family, str, &err)
    } else if (!strcmp(name, "model")) {
        setscalar(&def->model, str, &err)
    } else if (!strcmp(name, "stepping")) {
        setscalar(&def->stepping, str, &err)
    } else if (!strcmp(name, "feature_edx")) {
        setfeatures(&def->features, str, feature_name, &err);
    } else if (!strcmp(name, "feature_ecx")) {
        setfeatures(&def->ext_features, str, ext_feature_name, &err);
    } else if (!strcmp(name, "extfeature_edx")) {
        setfeatures(&def->ext2_features, str, ext2_feature_name, &err);
    } else if (!strcmp(name, "extfeature_ecx")) {
        setfeatures(&def->ext3_features, str, ext3_feature_name, &err);
    } else if (!strcmp(name, "xlevel")) {
        setscalar(&def->xlevel, str, &err)
    } else {
        fprintf(stderr, "error: unknown option [%s = %s]\n", name, str);
        return (1);
    }
    if (err) {
        fprintf(stderr, "error: bad option value [%s = %s]\n", name, str);
        return (1);
    }
    return (0);
}

/* register config file entry as X86CPUDefinition
 */
static int cpudef_register(QemuOpts *opts, void *opaque)
{
    X86CPUModelTableEntry *def = g_malloc0(sizeof(X86CPUModelTableEntry));

    qemu_opt_foreach(opts, cpudef_setfield, def, 1);
    def->next = x86_defs;
    x86_defs = def;
    return (0);
}

void cpu_clear_apic_feature(CPUX86State *env)
{
    env->cpuid_features &= ~CPUID_APIC;
}

#endif /* !CONFIG_USER_ONLY */

static void x86_cpu_register_types(void);

/* register "cpudef" models defined in configuration file.  Here we first
 * preload any built-in definitions
 */
void x86_cpudef_setup(void)
{
    int i, j;
    static const char *model_with_versions[] = { "qemu32", "qemu64", "athlon" };

    for (i = 0; i < ARRAY_SIZE(builtin_x86_defs); ++i) {
        X86CPUModelTableEntry *def = &builtin_x86_defs[i];
        def->next = x86_defs;
        def->is_builtin = true;

        /* Look for specific "cpudef" models that */
        /* have the QEMU version in .model_id */
        for (j = 0; j < ARRAY_SIZE(model_with_versions); j++) {
            if (strcmp(model_with_versions[j], def->name) == 0) {
                pstrcpy(def->cpudef.model_id, sizeof(def->cpudef.model_id),
                        "QEMU Virtual CPU version ");
                pstrcat(def->cpudef.model_id, sizeof(def->cpudef.model_id),
                        qemu_get_version());
                break;
            }
        }

        x86_defs = def;
    }
#if !defined(CONFIG_USER_ONLY)
    qemu_opts_foreach(qemu_find_opts("cpudef"), cpudef_register, NULL, 0);
#endif

    x86_cpu_register_types();
}

static void get_cpuid_vendor(CPUX86State *env, uint32_t *ebx,
                             uint32_t *ecx, uint32_t *edx)
{
    *ebx = env->cpuid_vendor1;
    *edx = env->cpuid_vendor2;
    *ecx = env->cpuid_vendor3;

    /* sysenter isn't supported on compatibility mode on AMD, syscall
     * isn't supported in compatibility mode on Intel.
     * Normally we advertise the actual cpu vendor, but you can override
     * this if you want to use KVM's sysenter/syscall emulation
     * in compatibility mode and when doing cross vendor migration
     */
    if (kvm_enabled() && ! env->cpuid_vendor_override) {
        host_cpuid(0, 0, NULL, ebx, ecx, edx);
    }
}

void cpu_x86_cpuid(CPUX86State *env, uint32_t index, uint32_t count,
                   uint32_t *eax, uint32_t *ebx,
                   uint32_t *ecx, uint32_t *edx)
{
    /* test if maximum index reached */
    if (index & 0x80000000) {
        if (index > env->cpuid_xlevel) {
            if (env->cpuid_xlevel2 > 0) {
                /* Handle the Centaur's CPUID instruction. */
                if (index > env->cpuid_xlevel2) {
                    index = env->cpuid_xlevel2;
                } else if (index < 0xC0000000) {
                    index = env->cpuid_xlevel;
                }
            } else {
                index =  env->cpuid_xlevel;
            }
        }
    } else {
        if (index > env->cpuid_level)
            index = env->cpuid_level;
    }

    switch(index) {
    case 0:
        *eax = env->cpuid_level;
        get_cpuid_vendor(env, ebx, ecx, edx);
        break;
    case 1:
        *eax = env->cpuid_version;
        *ebx = (env->cpuid_apic_id << 24) | 8 << 8; /* CLFLUSH size in quad words, Linux wants it. */
        *ecx = env->cpuid_ext_features;
        *edx = env->cpuid_features;
        if (env->nr_cores * env->nr_threads > 1) {
            *ebx |= (env->nr_cores * env->nr_threads) << 16;
            *edx |= 1 << 28;    /* HTT bit */
        }
        break;
    case 2:
        /* cache info: needed for Pentium Pro compatibility */
        *eax = 1;
        *ebx = 0;
        *ecx = 0;
        *edx = 0x2c307d;
        break;
    case 4:
        /* cache info: needed for Core compatibility */
        if (env->nr_cores > 1) {
            *eax = (env->nr_cores - 1) << 26;
        } else {
            *eax = 0;
        }
        switch (count) {
            case 0: /* L1 dcache info */
                *eax |= 0x0000121;
                *ebx = 0x1c0003f;
                *ecx = 0x000003f;
                *edx = 0x0000001;
                break;
            case 1: /* L1 icache info */
                *eax |= 0x0000122;
                *ebx = 0x1c0003f;
                *ecx = 0x000003f;
                *edx = 0x0000001;
                break;
            case 2: /* L2 cache info */
                *eax |= 0x0000143;
                if (env->nr_threads > 1) {
                    *eax |= (env->nr_threads - 1) << 14;
                }
                *ebx = 0x3c0003f;
                *ecx = 0x0000fff;
                *edx = 0x0000001;
                break;
            default: /* end of info */
                *eax = 0;
                *ebx = 0;
                *ecx = 0;
                *edx = 0;
                break;
        }
        break;
    case 5:
        /* mwait info: needed for Core compatibility */
        *eax = 0; /* Smallest monitor-line size in bytes */
        *ebx = 0; /* Largest monitor-line size in bytes */
        *ecx = CPUID_MWAIT_EMX | CPUID_MWAIT_IBE;
        *edx = 0;
        break;
    case 6:
        /* Thermal and Power Leaf */
        *eax = 0;
        *ebx = 0;
        *ecx = 0;
        *edx = 0;
        break;
    case 7:
        /* Structured Extended Feature Flags Enumeration Leaf */
        if (count == 0) {
            *eax = 0; /* Maximum ECX value for sub-leaves */
            *ebx = env->cpuid_7_0_ebx; /* Feature flags */
            *ecx = 0; /* Reserved */
            *edx = 0; /* Reserved */
        } else {
            *eax = 0;
            *ebx = 0;
            *ecx = 0;
            *edx = 0;
        }
        break;
    case 9:
        /* Direct Cache Access Information Leaf */
        *eax = 0; /* Bits 0-31 in DCA_CAP MSR */
        *ebx = 0;
        *ecx = 0;
        *edx = 0;
        break;
    case 0xA:
        /* Architectural Performance Monitoring Leaf */
        if (kvm_enabled()) {
            KVMState *s = env->kvm_state;

            *eax = kvm_arch_get_supported_cpuid(s, 0xA, count, R_EAX);
            *ebx = kvm_arch_get_supported_cpuid(s, 0xA, count, R_EBX);
            *ecx = kvm_arch_get_supported_cpuid(s, 0xA, count, R_ECX);
            *edx = kvm_arch_get_supported_cpuid(s, 0xA, count, R_EDX);
        } else {
            *eax = 0;
            *ebx = 0;
            *ecx = 0;
            *edx = 0;
        }
        break;
    case 0xD:
        /* Processor Extended State */
        if (!(env->cpuid_ext_features & CPUID_EXT_XSAVE)) {
            *eax = 0;
            *ebx = 0;
            *ecx = 0;
            *edx = 0;
            break;
        }
        if (kvm_enabled()) {
            KVMState *s = env->kvm_state;

            *eax = kvm_arch_get_supported_cpuid(s, 0xd, count, R_EAX);
            *ebx = kvm_arch_get_supported_cpuid(s, 0xd, count, R_EBX);
            *ecx = kvm_arch_get_supported_cpuid(s, 0xd, count, R_ECX);
            *edx = kvm_arch_get_supported_cpuid(s, 0xd, count, R_EDX);
        } else {
            *eax = 0;
            *ebx = 0;
            *ecx = 0;
            *edx = 0;
        }
        break;
    case 0x80000000:
        *eax = env->cpuid_xlevel;
        *ebx = env->cpuid_vendor1;
        *edx = env->cpuid_vendor2;
        *ecx = env->cpuid_vendor3;
        break;
    case 0x80000001:
        *eax = env->cpuid_version;
        *ebx = 0;
        *ecx = env->cpuid_ext3_features;
        *edx = env->cpuid_ext2_features;

        /* The Linux kernel checks for the CMPLegacy bit and
         * discards multiple thread information if it is set.
         * So dont set it here for Intel to make Linux guests happy.
         */
        if (env->nr_cores * env->nr_threads > 1) {
            uint32_t tebx, tecx, tedx;
            get_cpuid_vendor(env, &tebx, &tecx, &tedx);
            if (tebx != CPUID_VENDOR_INTEL_1 ||
                tedx != CPUID_VENDOR_INTEL_2 ||
                tecx != CPUID_VENDOR_INTEL_3) {
                *ecx |= 1 << 1;    /* CmpLegacy bit */
            }
        }
        break;
    case 0x80000002:
    case 0x80000003:
    case 0x80000004:
        *eax = env->cpuid_model[(index - 0x80000002) * 4 + 0];
        *ebx = env->cpuid_model[(index - 0x80000002) * 4 + 1];
        *ecx = env->cpuid_model[(index - 0x80000002) * 4 + 2];
        *edx = env->cpuid_model[(index - 0x80000002) * 4 + 3];
        break;
    case 0x80000005:
        /* cache info (L1 cache) */
        *eax = 0x01ff01ff;
        *ebx = 0x01ff01ff;
        *ecx = 0x40020140;
        *edx = 0x40020140;
        break;
    case 0x80000006:
        /* cache info (L2 cache) */
        *eax = 0;
        *ebx = 0x42004200;
        *ecx = 0x02008140;
        *edx = 0;
        break;
    case 0x80000008:
        /* virtual & phys address size in low 2 bytes. */
/* XXX: This value must match the one used in the MMU code. */
        if (env->cpuid_ext2_features & CPUID_EXT2_LM) {
            /* 64 bit processor */
/* XXX: The physical address space is limited to 42 bits in exec.c. */
            *eax = 0x00003028;	/* 48 bits virtual, 40 bits physical */
        } else {
            if (env->cpuid_features & CPUID_PSE36)
                *eax = 0x00000024; /* 36 bits physical */
            else
                *eax = 0x00000020; /* 32 bits physical */
        }
        *ebx = 0;
        *ecx = 0;
        *edx = 0;
        if (env->nr_cores * env->nr_threads > 1) {
            *ecx |= (env->nr_cores * env->nr_threads) - 1;
        }
        break;
    case 0x8000000A:
        if (env->cpuid_ext3_features & CPUID_EXT3_SVM) {
            *eax = 0x00000001; /* SVM Revision */
            *ebx = 0x00000010; /* nr of ASIDs */
            *ecx = 0;
            *edx = env->cpuid_svm_features; /* optional features */
        } else {
            *eax = 0;
            *ebx = 0;
            *ecx = 0;
            *edx = 0;
        }
        break;
    case 0xC0000000:
        *eax = env->cpuid_xlevel2;
        *ebx = 0;
        *ecx = 0;
        *edx = 0;
        break;
    case 0xC0000001:
        /* Support for VIA CPU's CPUID instruction */
        *eax = env->cpuid_version;
        *ebx = 0;
        *ecx = 0;
        *edx = env->cpuid_ext4_features;
        break;
    case 0xC0000002:
    case 0xC0000003:
    case 0xC0000004:
        /* Reserved for the future, and now filled with zero */
        *eax = 0;
        *ebx = 0;
        *ecx = 0;
        *edx = 0;
        break;
    default:
        /* reserved values: zero */
        *eax = 0;
        *ebx = 0;
        *ecx = 0;
        *edx = 0;
        break;
    }
}

/* CPUClass::reset() */
static void x86_cpu_reset(CPUState *s)
{
    X86CPU *cpu = X86_CPU(s);
    X86CPUClass *xcc = X86_CPU_GET_CLASS(cpu);
    CPUX86State *env = &cpu->env;
    int i;

    if (qemu_loglevel_mask(CPU_LOG_RESET)) {
        qemu_log("CPU Reset (CPU %d)\n", env->cpu_index);
        log_cpu_state(env, X86_DUMP_FPU | X86_DUMP_CCOP);
    }

    xcc->parent_reset(s);


    memset(env, 0, offsetof(CPUX86State, breakpoints));

    tlb_flush(env, 1);

    env->old_exception = -1;

    /* init to reset state */

#ifdef CONFIG_SOFTMMU
    env->hflags |= HF_SOFTMMU_MASK;
#endif
    env->hflags2 |= HF2_GIF_MASK;

    cpu_x86_update_cr0(env, 0x60000010);
    env->a20_mask = ~0x0;
    env->smbase = 0x30000;

    env->idt.limit = 0xffff;
    env->gdt.limit = 0xffff;
    env->ldt.limit = 0xffff;
    env->ldt.flags = DESC_P_MASK | (2 << DESC_TYPE_SHIFT);
    env->tr.limit = 0xffff;
    env->tr.flags = DESC_P_MASK | (11 << DESC_TYPE_SHIFT);

    cpu_x86_load_seg_cache(env, R_CS, 0xf000, 0xffff0000, 0xffff,
                           DESC_P_MASK | DESC_S_MASK | DESC_CS_MASK |
                           DESC_R_MASK | DESC_A_MASK);
    cpu_x86_load_seg_cache(env, R_DS, 0, 0, 0xffff,
                           DESC_P_MASK | DESC_S_MASK | DESC_W_MASK |
                           DESC_A_MASK);
    cpu_x86_load_seg_cache(env, R_ES, 0, 0, 0xffff,
                           DESC_P_MASK | DESC_S_MASK | DESC_W_MASK |
                           DESC_A_MASK);
    cpu_x86_load_seg_cache(env, R_SS, 0, 0, 0xffff,
                           DESC_P_MASK | DESC_S_MASK | DESC_W_MASK |
                           DESC_A_MASK);
    cpu_x86_load_seg_cache(env, R_FS, 0, 0, 0xffff,
                           DESC_P_MASK | DESC_S_MASK | DESC_W_MASK |
                           DESC_A_MASK);
    cpu_x86_load_seg_cache(env, R_GS, 0, 0, 0xffff,
                           DESC_P_MASK | DESC_S_MASK | DESC_W_MASK |
                           DESC_A_MASK);

    env->eip = 0xfff0;
    env->regs[R_EDX] = env->cpuid_version;

    env->eflags = 0x2;

    /* FPU init */
    for (i = 0; i < 8; i++) {
        env->fptags[i] = 1;
    }
    env->fpuc = 0x37f;

    env->mxcsr = 0x1f80;

    env->pat = 0x0007040600070406ULL;
    env->msr_ia32_misc_enable = MSR_IA32_MISC_ENABLE_DEFAULT;

    memset(env->dr, 0, sizeof(env->dr));
    env->dr[6] = DR6_FIXED_1;
    env->dr[7] = DR7_FIXED_1;
    cpu_breakpoint_remove_all(env, BP_CPU);
    cpu_watchpoint_remove_all(env, BP_CPU);

#if !defined(CONFIG_USER_ONLY)
    /* We hard-wire the BSP to the first CPU. */
    if (env->cpu_index == 0) {
        apic_designate_bsp(env->apic_state);
    }

    env->halted = !cpu_is_bsp(cpu);
#endif
}

#ifndef CONFIG_USER_ONLY
bool cpu_is_bsp(X86CPU *cpu)
{
    return cpu_get_apic_base(cpu->env.apic_state) & MSR_IA32_APICBASE_BSP;
}

/* TODO: remove me, when reset over QOM tree is implemented */
static void x86_cpu_machine_reset_cb(void *opaque)
{
    X86CPU *cpu = opaque;
    cpu_reset(CPU(cpu));
}
#endif

static void mce_init(X86CPU *cpu)
{
    CPUX86State *cenv = &cpu->env;
    unsigned int bank;

    if (((cenv->cpuid_version >> 8) & 0xf) >= 6
        && (cenv->cpuid_features & (CPUID_MCE | CPUID_MCA)) ==
            (CPUID_MCE | CPUID_MCA)) {
        cenv->mcg_cap = MCE_CAP_DEF | MCE_BANKS_DEF;
        cenv->mcg_ctl = ~(uint64_t)0;
        for (bank = 0; bank < MCE_BANKS_DEF; bank++) {
            cenv->mce_banks[bank * 4] = ~(uint64_t)0;
        }
    }
}

void x86_cpu_realize(Object *obj, Error **errp)
{
    X86CPU *cpu = X86_CPU(obj);
    CPUX86State *env = &cpu->env;

    if (!kvm_enabled()) {
        env->cpuid_features &= TCG_FEATURES;
        env->cpuid_ext_features &= TCG_EXT_FEATURES;
        env->cpuid_ext2_features &= (TCG_EXT2_FEATURES
#ifdef TARGET_X86_64
            | CPUID_EXT2_SYSCALL | CPUID_EXT2_LM
#endif
            );
        env->cpuid_ext3_features &= TCG_EXT3_FEATURES;
        env->cpuid_svm_features &= TCG_SVM_FEATURES;
    }

    if (check_cpuid && check_features_against_host(cpu)
        && enforce_cpuid) {
        error_set(errp, QERR_PERMISSION_DENIED);
        return;
    }

#ifndef CONFIG_USER_ONLY
    qemu_register_reset(x86_cpu_machine_reset_cb, cpu);
#endif

    mce_init(cpu);
    qemu_init_vcpu(&cpu->env);
    cpu_reset(CPU(cpu));
}

static void x86_cpu_initfn(Object *obj)
{
    X86CPU *cpu = X86_CPU(obj);
    CPUX86State *env = &cpu->env;
    static int inited;

    cpu_exec_init(env);

    object_property_add(obj, "family", "int",
                        x86_cpuid_version_get_family,
                        x86_cpuid_version_set_family, NULL, NULL, NULL);
    object_property_add(obj, "model", "int",
                        x86_cpuid_version_get_model,
                        x86_cpuid_version_set_model, NULL, NULL, NULL);
    object_property_add(obj, "stepping", "int",
                        x86_cpuid_version_get_stepping,
                        x86_cpuid_version_set_stepping, NULL, NULL, NULL);
    object_property_add(obj, "level", "int",
                        x86_cpuid_get_level,
                        x86_cpuid_set_level, NULL, NULL, NULL);
    object_property_add(obj, "xlevel", "int",
                        x86_cpuid_get_xlevel,
                        x86_cpuid_set_xlevel, NULL, NULL, NULL);
    object_property_add_str(obj, "vendor",
                            x86_cpuid_get_vendor,
                            x86_cpuid_set_vendor, NULL);
    object_property_add(obj, "vendor-override", "bool",
                        x86_cpuid_get_vendor_override,
                        x86_cpuid_set_vendor_override, NULL, NULL, NULL);
    object_property_add_str(obj, "model-id",
                            x86_cpuid_get_model_id,
                            x86_cpuid_set_model_id, NULL);
    object_property_add(obj, "tsc-frequency", "int",
                        x86_cpuid_get_tsc_freq,
                        x86_cpuid_set_tsc_freq, NULL, NULL, NULL);
    object_property_add(obj, "check", "bool",
                        x86_cpuid_get_check,
                        x86_cpuid_set_check, NULL, NULL, NULL);
    object_property_add(obj, "enforce", "bool",
                        x86_cpuid_get_enforce,
                        x86_cpuid_set_enforce, NULL, NULL, NULL);
#if !defined(CONFIG_USER_ONLY)
    object_property_add(obj, "hv_spinlocks", "int",
                        x86_get_hv_spinlocks,
                        x86_set_hv_spinlocks, NULL, NULL, NULL);
    object_property_add(obj, "hv_relaxed", "bool",
                        x86_get_hv_relaxed,
                        x86_set_hv_relaxed, NULL, NULL, NULL);
    object_property_add(obj, "hv_vapic", "bool",
                        x86_set_hv_vapic,
                        x86_get_hv_vapic, NULL, NULL, NULL);
#endif
    x86_register_cpuid_properties(obj, feature_name);
    x86_register_cpuid_properties(obj, ext_feature_name);
    x86_register_cpuid_properties(obj, ext2_feature_name);
    x86_register_cpuid_properties(obj, ext3_feature_name);
    x86_register_cpuid_properties(obj, kvm_feature_name);
    x86_register_cpuid_properties(obj, svm_feature_name);

    env->cpuid_apic_id = env->cpu_index;

    /* init various static tables used in TCG mode */
    if (tcg_enabled() && !inited) {
        inited = 1;
        optimize_flags_init();
#ifndef CONFIG_USER_ONLY
        cpu_set_debug_excp_handler(breakpoint_handler);
#endif
    }
}

static void x86_cpu_common_class_init(ObjectClass *oc, void *data)
{
    X86CPUClass *xcc = X86_CPU_CLASS(oc);
    CPUClass *cc = CPU_CLASS(oc);

    xcc->parent_reset = cc->reset;
    cc->reset = x86_cpu_reset;
}

static void x86_cpu_class_init(ObjectClass *oc, void *data)
{
    X86CPUClass *xcc = X86_CPU_CLASS(oc);
    X86CPUDefinition *def = data;

    xcc->cpudef = *def;
}

static const TypeInfo x86_cpu_type_info = {
    .name = TYPE_X86_CPU,
    .parent = TYPE_CPU,
    .instance_size = sizeof(X86CPU),
    .abstract = true,
    .class_size = sizeof(X86CPUClass),
    .class_init = x86_cpu_common_class_init,
};

static void x86_cpu_register_class(const char *name, X86CPUDefinition *def)
{
    char *class_name = build_cpu_class_name(name);
    TypeInfo type = {
        .name = class_name,
        .parent = TYPE_X86_CPU,
        .instance_size = sizeof(X86CPU),
        .instance_init = x86_cpu_initfn,
        .class_size = sizeof(X86CPUClass),
        .class_init = x86_cpu_class_init,
        .class_data = def,
    };
    type_register(&type);
    g_free(class_name);
}

static void x86_cpu_register_types(void)
{
    X86CPUModelTableEntry *def;
    X86CPUDefinition host_def;

    /* Abstract CPUX86 class */
    type_register_static(&x86_cpu_type_info);

    /* One class for each CPU model: */
    for (def = x86_defs; def; def = def->next) {
        x86_cpu_register_class(def->name, &def->cpudef);
    }

    /* -cpu host class */
    cpu_x86_fill_host(&host_def);
    x86_cpu_register_class("host", &host_def);
}

//HACK: the function is being called from x86_cpudef_setup()
//type_init(x86_cpu_register_types)
