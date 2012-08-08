/*
 * QEMU x86 CPU
 *
 * Copyright (c) 2012 SUSE LINUX Products GmbH
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/lgpl-2.1.html>
 */
#ifndef QEMU_I386_CPU_QOM_H
#define QEMU_I386_CPU_QOM_H

#include "qemu/cpu.h"
#include "cpu.h"
#include "error.h"

#ifdef TARGET_X86_64
#define TYPE_X86_CPU "x86_64-cpu"
#else
#define TYPE_X86_CPU "i386-cpu"
#endif

#define X86_CPU_CLASS(klass) \
    OBJECT_CLASS_CHECK(X86CPUClass, (klass), TYPE_X86_CPU)
#define X86_CPU(obj) \
    OBJECT_CHECK(X86CPU, (obj), TYPE_X86_CPU)
#define X86_CPU_GET_CLASS(obj) \
    OBJECT_GET_CLASS(X86CPUClass, (obj), TYPE_X86_CPU)

typedef struct X86CPUDefinition {
    uint32_t level;
    uint32_t vendor1, vendor2, vendor3;
    int family;
    int model;
    int stepping;
    int tsc_khz;
    uint32_t xlevel;
    char model_id[48];
    int vendor_override;
    /* Store the results of Centaur's CPUID instructions */
    uint32_t xlevel2;
    /* Raw feature words. this field is used only for the bits that represent
     * a single feature, that are taken directly from CPUID leaves.
     * Not every bit inside each word is configurable, but each configurable
     * bit may be directly mapped to a CPUID (sub)leaf+register.
     */
    uint32_t feature_words[FEATURE_WORDS];
} X86CPUDefinition;

/**
 * X86CPUClass:
 * @parent_reset: The parent class' reset handler.
 *
 * An x86 CPU model or family.
 */
typedef struct X86CPUClass {
    /*< private >*/
    CPUClass parent_class;
    /*< public >*/

    void (*parent_reset)(CPUState *cpu);

    X86CPUDefinition cpudef;
} X86CPUClass;

/**
 * X86CPU:
 * @env: #CPUX86State
 *
 * An x86 CPU.
 */
typedef struct X86CPU {
    /*< private >*/
    CPUState parent_obj;
    /*< public >*/

    CPUX86State env;
} X86CPU;

static inline X86CPU *x86_env_get_cpu(CPUX86State *env)
{
    return X86_CPU(container_of(env, X86CPU, env));
}

#define ENV_GET_CPU(e) CPU(x86_env_get_cpu(e))

/* TODO Drop once ObjectClass::realize is available */
void x86_cpu_realize(Object *obj, Error **errp);


#endif
