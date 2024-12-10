/*
 * Copyright (c) 2024 Taylor Simpson All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "hex_vm_trace.h"

void hex_vm_trace_trap1(CPUHexagonState *env, int imm, target_ulong PC)
{
    if (TRACE_HEX_VM) {
        GString *msg = hex_vm_trace_str(PC);
        switch (imm) {
        case HEX_VM_TRAP1_VMVERSION:
            g_string_append_printf(msg, "vmversion(0x%08" PRIx32 ")",
                                   env->gpr[HEX_REG_R00]);
            break;
        case HEX_VM_TRAP1_VMRTE:
            g_string_append_printf(msg, "vmrte: GELR = 0x%08" PRIx32,
                                   env->greg[HEX_GREG_GELR]);
            break;
        case HEX_VM_TRAP1_VMSETVEC:
            g_string_append_printf(msg, "vmsetvec(0x%08" PRIx32 ")",
                                   env->gpr[HEX_REG_R00]);
            /* Record the event vector for future tracing */
            env->event_vectors = env->gpr[HEX_REG_R00];
            break;
        case HEX_VM_TRAP1_VMSETIE:
            g_string_append_printf(msg, "vmsetie(%s)",
                                   env->gpr[HEX_REG_R00] == 1 ?
                                       "enable" :
                                       "disable");
            break;
        case HEX_VM_TRAP1_VMGETIE:
            g_string_append_printf(msg, "vmgetie");
            break;
        case HEX_VM_TRAP1_VMINTOP:
            g_string_append_printf(msg, "vmintop(%" PRIu32 ")",
                            env->gpr[HEX_REG_R00]);
            break;
        case HEX_VM_TRAP1_VMCLRMAP:
            g_string_append_printf(msg, "vmclrmap(0x%08" PRIx32
                                   ", %" PRIu32 ")",
                                   env->gpr[HEX_REG_R00],
                                   env->gpr[HEX_REG_R01]);
            break;
        case HEX_VM_TRAP1_VMNEWMAP:
            g_string_append_printf(msg, "vmnewmap(0x%08" PRIx32 ")",
                                   env->gpr[HEX_REG_R00]);
            break;
        case HEX_VM_TRAP1_VMCACHE:
            g_string_append_printf(msg, "vmcache(%" PRId32 ", 0x%08" PRIx32
                                   ", %" PRId32 ")",
                                   env->gpr[HEX_REG_R00],
                                   env->gpr[HEX_REG_R01],
                                   env->gpr[HEX_REG_R02]);
            break;
        case HEX_VM_TRAP1_VMGETTIME:
            g_string_append_printf(msg, "vmgettime");
            break;
        case HEX_VM_TRAP1_VMSETTIME:
            g_string_append_printf(msg, "vmsettime(0x%08" PRIx32
                                   ", 0x%08" PRIx32 ")",
                                   env->gpr[HEX_REG_R00],
                                   env->gpr[HEX_REG_R01]);
            break;
        case HEX_VM_TRAP1_VMWAIT:
            g_string_append_printf(msg, "vmwait");
            break;
        case HEX_VM_TRAP1_VMYIELD:
            g_string_append_printf(msg, "vmyield");
            break;
        case HEX_VM_TRAP1_VMSTART:
            g_string_append_printf(msg, "vmstart(0x%08" PRIx32
                                   ", 0x%08" PRIx32 ")",
                                   env->gpr[HEX_REG_R00],
                                   env->gpr[HEX_REG_R01]);
            break;
        case HEX_VM_TRAP1_VMSTOP:
            g_string_append_printf(msg, "vmstop");
            break;
        case HEX_VM_TRAP1_VMVPID:
            g_string_append_printf(msg, "vmpid");
            break;
        case HEX_VM_TRAP1_VMSETREGS:
            g_string_append_printf(msg, "vmsetregs(0x%08" PRIx32
                                   ", 0x%08" PRIx32 ", 0x%08" PRIx32
                                   ", 0x%08" PRIx32 ")",
                                   env->gpr[HEX_REG_R00],
                                   env->gpr[HEX_REG_R01],
                                   env->gpr[HEX_REG_R02],
                                   env->gpr[HEX_REG_R03]);
            break;
        case HEX_VM_TRAP1_VMGETREGS:
            g_string_append_printf(msg, "vmgetregs");
            break;
        default:
            g_string_append_printf(msg, "trap1(%" PRId32 ")", imm);
            break;
        }
        hex_vm_trace_end(msg);
    }
}

static const char *gen_exception_name[] = {
    [HEX_VM_EVENT_CAUSE_BUS_ERROR] = "bus error",
    [HEX_VM_EVENT_CAUSE_PROT_EX] = "execute protection",
    [HEX_VM_EVENT_CAUSE_PROT_UEX] = "user execute protection",
    [HEX_VM_EVENT_CAUSE_INVALID_PKT] = "invalid packet",
    [HEX_VM_EVENT_CAUSE_PRIV] = "privilege violation",
    [HEX_VM_EVENT_CAUSE_MISALIGN_PC] = "misaligned PC",
    [HEX_VM_EVENT_CAUSE_MISALIGN_LD] = "misaligned load",
    [HEX_VM_EVENT_CAUSE_MISALIGN_ST] = "misaligned store",
    [HEX_VM_EVENT_CAUSE_PROT_RD] = "read protection",
    [HEX_VM_EVENT_CAUSE_PROT_WR] = "write protection",
    [HEX_VM_EVENT_CAUSE_PROT_URD] = "user read protection",
    [HEX_VM_EVENT_CAUSE_PROT_UWR] = "user write protection",
    [HEX_VM_EVENT_CAUSE_PROT_CACHE_CONFLICT] = "cache conflict",
    [HEX_VM_EVENT_CAUSE_PROT_REG_COLLISION] = "register collision",
};

void hex_vm_trace_tb_start(CPUHexagonState *env, target_ulong PC)
{
    target_ulong event_vectors = env->event_vectors;
    if (event_vectors != INVALID_REG_VAL &&
        event_vectors <= PC &&
        PC < event_vectors + HEX_VM_NUM_VECTORS * 4) {
        int event_num = (PC - env->event_vectors) / 4;
        GString *msg = hex_vm_trace_str(PC);
        g_string_append_printf(msg, "guest event: ");
        switch (event_num) {
        case HEX_VM_EVENT_RESERVED_0:
        case HEX_VM_EVENT_RESERVED_3:
        case HEX_VM_EVENT_RESERVED_4:
        case HEX_VM_EVENT_RESERVED_6:
            g_string_append_printf(msg, "reserved");
            break;
        case HEX_VM_EVENT_MACHINE_CHECK:
            g_string_append_printf(msg, "machine check");
            break;
        case HEX_VM_EVENT_GEN_EXCEPTION:
            g_string_append_printf(msg, "general exception: ");
            {
                target_ulong gsr = hexagon_greg_read(env, HEX_GREG_GSR);
                target_ulong cause = gsr & 0xffff;
                const char *name = cause < ARRAY_SIZE(gen_exception_name) ?
                                   gen_exception_name[cause] : "unknown cause";
                g_string_append(msg, name);
            }
            break;
        case HEX_VM_EVENT_TRAP0:
            g_string_append_printf(msg, "trap0");
            break;
        case HEX_VM_EVENT_INTERRUPT:
            g_string_append_printf(msg, "interrupt");
            break;
        default:
            g_string_append_printf(msg, "unknown");
            break;
        }
        hex_vm_trace_end(msg);
    }
}

void hex_vm_trace_rte(CPUHexagonState *env, target_ulong PC)
{
    GString *msg = hex_vm_trace_str(PC);
    g_string_append_printf(msg, "rte: ELR = 0x%08" PRIx32,
                           arch_get_system_reg(env, HEX_SREG_ELR));
    hex_vm_trace_end(msg);
}
