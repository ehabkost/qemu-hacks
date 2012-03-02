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

#include "bitops.h"

/** Return the bit width needed for 'count' IDs
 */
static unsigned bits_for_count(unsigned count)
{
    g_assert(count >= 1);
    if (count == 1)
        return 0;
    return bitops_flsl(count - 1) + 1;
}

/** Bit width of the SMT_ID (thread ID) field
 */
static inline unsigned topo_smt_width(unsigned nr_cores, unsigned nr_threads)
{
        return bits_for_count(nr_threads);
}

/** Bit width of the Core_ID field
 */
static inline unsigned topo_core_width(unsigned nr_cores, unsigned nr_threads)
{
        return bits_for_count(nr_cores);
}

/** Bit offset of the Core_ID field
 */
static inline unsigned topo_core_offset(unsigned nr_cores, unsigned nr_threads)
{
        return topo_smt_width(nr_cores, nr_threads);
}

/** Bit offset of the Pkg_ID (socket ID) field
 */
static inline unsigned topo_pkg_offset(unsigned nr_cores, unsigned nr_threads)
{
        return topo_core_offset(nr_cores, nr_threads) + \
               topo_core_width(nr_cores, nr_threads);
}

/** Make APIC ID for the CPU based on Pkg_ID, Core_ID, SMT_ID
 *
 * The caller must make sure core_id < nr_cores and smt_id < nr_threads.
 */
static inline uint8_t __topo_make_apicid(unsigned nr_cores, unsigned nr_threads,
                                         unsigned pkg_id, unsigned core_id,
                                         unsigned smt_id)
{
        return (pkg_id  << topo_pkg_offset(nr_cores, nr_threads)) | \
               (core_id << topo_core_offset(nr_cores, nr_threads)) | \
               smt_id;
}

/** Calculate thread/core/package IDs for a specific topology based on CPU index
 */
static inline void topo_ids_from_idx(unsigned nr_cores, unsigned nr_threads,
                                     unsigned index,
                                     unsigned *pkg_id, unsigned *core_id,
                                     unsigned *smt_id)
{
    unsigned core_index = index / nr_threads;
    *smt_id = index % nr_threads;
    *core_id = core_index % nr_cores;
    *pkg_id = core_index / nr_cores;
}

/** Make APIC ID for the CPU on 'index'
 *
 * 'index' is a sequential, contiguous ID for the CPU.
 */
static inline uint8_t topo_make_apicid(unsigned nr_cores, unsigned nr_threads,
                                       unsigned cpu_index)
{
    unsigned pkg_id, core_id, smt_id;
    topo_ids_from_idx(nr_cores, nr_threads, cpu_index, &pkg_id, &core_id, &smt_id);
    return __topo_make_apicid(nr_cores, nr_threads, pkg_id, core_id, smt_id);
}

#endif /* __QMU_X86_TOPOLOGY_H__ */
