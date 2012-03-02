/*
 *  x86 CPU topology data structures and functions
 *
 *  Copyright (c) 2012 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef __QEMU_X86_TOPOLOGY_H__
#define __QEMU_X86_TOPOLOGY_H__

#include <stdint.h>
#include <string.h>

struct CPUTopology
{
        unsigned int nr_cores;   /* cores per socket */
        unsigned int nr_threads; /* thread per core  */

        /* Bit widths for SMT_ID and Core_ID, calculated
         * from nr_threads & nr_cores
         */
        uint8_t smt_width, core_width;
};

void topo_init(struct CPUTopology *t, unsigned int nr_cores, unsigned int nr_threads);

/** Return the bit width needed for 'count' IDs
 */
unsigned int bits_for_count(unsigned int count);

/** Bit width of the SMT_ID (thread ID) field
 */
static inline unsigned int topo_smt_width(const struct CPUTopology *t)
{
        return t->smt_width;
}

/** Bit width of the Core_ID field
 */
static inline unsigned int topo_core_width(const struct CPUTopology *t)
{
        return t->core_width;
}

/** Bit offset of the Core_ID field
 */
static inline unsigned int topo_core_offset(const struct CPUTopology *t)
{
        return topo_smt_width(t);
}

/** Bit offset of the Pkg_ID (socket ID) field
 */
static inline unsigned int topo_pkg_offset(const struct CPUTopology *t)
{
        return topo_core_offset(t)+topo_core_width(t);
}

/** Make APIC ID for the CPU based on Pkg_ID, Core_ID, SMT_ID
 */
static inline uint8_t __topo_make_apicid(const struct CPUTopology *t,
                                       unsigned int pkg_id,
                                       unsigned int core_id,
                                       unsigned int smt_id)
{
        return (pkg_id << topo_pkg_offset(t)) | (core_id << topo_core_offset(t)) | smt_id;
}

/** Make APIC ID for the CPU on 'index'
 *
 * 'index' is a sequential, contiguous ID for the CPU.
 */
static inline uint8_t topo_make_apicid(const struct CPUTopology *t, unsigned int index)
{
        unsigned int smt_id = index % t->nr_threads;
        unsigned int core_index = index / t->nr_threads;
        unsigned int core_id = core_index % t->nr_cores;
        unsigned int pkg_id = core_index / t->nr_cores;

        return __topo_make_apicid(t, pkg_id, core_id, smt_id);
}

#endif /* __QEMU_X86_TOPOLOGY_H__ */
