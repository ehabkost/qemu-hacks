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


struct X86CPUDefinition;
typedef struct X86CPUDefinition X86CPUDefinition;

struct X86CPUClass;
typedef struct X86CPUClass X86CPUClass;

/**
 * X86CPUClass:
 * @parent_reset: The parent class' reset handler.
 * @init_cpudef: initialize X86CPUDefinition struct for CPU class
 *
 * An x86 CPU model or family.
 */
struct X86CPUClass {
    /*< private >*/
    CPUClass parent_class;
    /*< public >*/

    void (*parent_reset)(CPUState *cpu);
    int (*init_cpudef)(X86CPUClass *xcc, X86CPUDefinition *def, Error **errp);
};

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
