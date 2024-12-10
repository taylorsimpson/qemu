/*
 * Copyright (c) 2024 Taylor Simpson All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HEX_VM_TRACE_H
#define HEX_VM_TRACE_H

#include "trace.h"

#define TRACE_HEX_VM \
    (trace_event_get_state(TRACE_HEXAGON_VM) && \
     qemu_loglevel_mask(LOG_TRACE))

#define HEX_VM_NUM_VECTORS   8

#define HEX_VM_TRAP1_VMVERSION      0
#define HEX_VM_TRAP1_VMRTE          1
#define HEX_VM_TRAP1_VMSETVEC       2
#define HEX_VM_TRAP1_VMSETIE        3
#define HEX_VM_TRAP1_VMGETIE        4
#define HEX_VM_TRAP1_VMINTOP        5
#define HEX_VM_TRAP1_VMCLRMAP      10
#define HEX_VM_TRAP1_VMNEWMAP      11
#define HEX_VM_TRAP1_VMCACHE       13
#define HEX_VM_TRAP1_VMGETTIME     14
#define HEX_VM_TRAP1_VMSETTIME     15
#define HEX_VM_TRAP1_VMWAIT        16
#define HEX_VM_TRAP1_VMYIELD       17
#define HEX_VM_TRAP1_VMSTART       18
#define HEX_VM_TRAP1_VMSTOP        19
#define HEX_VM_TRAP1_VMVPID        20
#define HEX_VM_TRAP1_VMSETREGS     21
#define HEX_VM_TRAP1_VMGETREGS     22

#define HEX_VM_EVENT_RESERVED_0     0
#define HEX_VM_EVENT_MACHINE_CHECK  1
#define HEX_VM_EVENT_GEN_EXCEPTION  2
#define HEX_VM_EVENT_RESERVED_3     3
#define HEX_VM_EVENT_RESERVED_4     4
#define HEX_VM_EVENT_TRAP0          5
#define HEX_VM_EVENT_RESERVED_6     6
#define HEX_VM_EVENT_INTERRUPT      7

#define HEX_VM_EVENT_CAUSE_BUS_ERROR               0x01
#define HEX_VM_EVENT_CAUSE_PROT_EX                 0x11
#define HEX_VM_EVENT_CAUSE_PROT_UEX                0x14
#define HEX_VM_EVENT_CAUSE_INVALID_PKT             0x15
#define HEX_VM_EVENT_CAUSE_PRIV                    0x1b
#define HEX_VM_EVENT_CAUSE_MISALIGN_PC             0x1c
#define HEX_VM_EVENT_CAUSE_MISALIGN_LD             0x20
#define HEX_VM_EVENT_CAUSE_MISALIGN_ST             0x21
#define HEX_VM_EVENT_CAUSE_PROT_RD                 0x22
#define HEX_VM_EVENT_CAUSE_PROT_WR                 0x23
#define HEX_VM_EVENT_CAUSE_PROT_URD                0x24
#define HEX_VM_EVENT_CAUSE_PROT_UWR                0x25
#define HEX_VM_EVENT_CAUSE_PROT_CACHE_CONFLICT     0x28
#define HEX_VM_EVENT_CAUSE_PROT_REG_COLLISION      0x29

static inline GString *hex_vm_trace_str(target_ulong PC)
{
    GString *msg = g_string_sized_new(128);
    g_string_printf(msg, "PC: 0x%08" PRIx32 ": ", PC);
    return msg;
}

static inline void hex_vm_trace_end(GString *msg)
{
    trace_hexagon_vm(msg->str);
    g_string_free(msg, true);
}

void hex_vm_trace_trap1(CPUHexagonState *env, int imm, target_ulong PC);
void hex_vm_trace_tb_start(CPUHexagonState *env, target_ulong PC);
void hex_vm_trace_rte(CPUHexagonState *env, target_ulong PC);

#endif

