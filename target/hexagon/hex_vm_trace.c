/*
 * Copyright (c) 2024 Taylor Simpson All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "hex_vm_trace.h"

void hex_vm_trace_push(CPUHexagonState *env, HexTraceEvent event,
                       target_ulong PC)
{
    HexVMTraceStackEntry *entry;

    /* Don't push if we are replaying the instruction on top of the stack */
    if (env->trace_stack->idx > 0) {
        entry = &env->trace_stack->entries[env->trace_stack->idx - 1];
        if (entry->event == event && entry->PC == PC) {
            return;
        }
    }
    g_assert(env->trace_stack->idx < HEX_VM_TRACE_STACK_SIZE);
    entry = &env->trace_stack->entries[env->trace_stack->idx];
    entry->event = event;
    entry->PC = PC;
    for (int i = 0; i < HEX_VM_TRACE_STACK_REGS; i++) {
        entry->regs[i] = env->gpr[i];
    }
    env->trace_stack->idx++;
}

static const uint32_t hex_vm_trace_stack_pop_first_reg[HEX_VM_TRACE_INVALID] = {
    [HEX_VM_TRACE_VMVERSION] = 1,
    [HEX_VM_TRACE_VMRTE] = 0,
    [HEX_VM_TRACE_VMSETVEC] = 1,
    [HEX_VM_TRACE_VMSETIE] = 1,
    [HEX_VM_TRACE_VMGETIE] = 1,
    [HEX_VM_TRACE_VMINTOP] = 1,
    [HEX_VM_TRACE_VMCLRMAP] = 1,
    [HEX_VM_TRACE_VMNEWMAP] = 1,
    [HEX_VM_TRACE_VMCACHE] = 0,
    [HEX_VM_TRACE_VMGETTIME] = 2,
    [HEX_VM_TRACE_VMSETTIME] = 0,
    [HEX_VM_TRACE_VMWAIT] = 1,
    [HEX_VM_TRACE_VMYIELD] = 0,
    [HEX_VM_TRACE_VMSTART] = 1,
    [HEX_VM_TRACE_VMSTOP] = 0,
    [HEX_VM_TRACE_VMPID] = 1,
    [HEX_VM_TRACE_VMSETREGS] = 0,
    [HEX_VM_TRACE_VMGETREGS] = 0,
    [HEX_VM_TRACE_TRAP1_UNKNOWN] = 29,
    [HEX_VM_TRACE_TRAP0_ANGEL] = 2,
    [HEX_VM_TRACE_TRAP0] = 0,
    [HEX_VM_TRACE_MACHINE_CHECK] = 0,
    [HEX_VM_TRACE_GEN_EXCEPTION] = 0,
    [HEX_VM_TRACE_GUEST_TRAP0] = 0,
    [HEX_VM_TRACE_INTERRUPT] = 0,
    [HEX_VM_TRACE_RESERVED] = 0,
    [HEX_VM_TRACE_TLB_MISSRW] = 0,
    [HEX_VM_TRACE_TLB_MISSX] = 0,
};

HexTraceEvent hex_vm_trace_pop(CPUHexagonState *env, target_ulong PC)
{
    HexVMTraceStackEntry *entry;
    uint32_t first;

    /* Sometimes an rte can happen when there is nothing to pop */
    if (env->trace_stack->idx == 0) {
        return HEX_VM_TRACE_INVALID;
    }

    env->trace_stack->idx--;
    entry = &env->trace_stack->entries[env->trace_stack->idx];
    first = hex_vm_trace_stack_pop_first_reg[entry->event];
    for (uint32_t i = first; i < HEX_VM_TRACE_STACK_REGS; i++) {
        target_ulong pop = entry->regs[i];
        target_ulong gpr = env->gpr[i];
        if (pop != gpr) {
            GString *msg = hex_vm_trace_str(PC);
            g_string_append_printf(msg, "WARNING: register %" PRId32
                                   " modified: ", i);
            g_string_append_printf(msg, "0x%08" PRIx32 " != 0x%08" PRIx32,
                                   pop, gpr);
            hex_vm_trace_end(msg);
        }
    }
    return entry->event;
}

void hex_vm_trace_trap0(CPUHexagonState *env, int imm, target_ulong PC)
{
  if (TRACE_HEX_VM) {
    GString *msg = hex_vm_trace_str(PC);
    g_string_append_printf(msg, "trap0(#%" PRId32 ")", imm);
    if (imm == 0) {
        hex_vm_trace_push(env, HEX_VM_TRACE_TRAP0_ANGEL, PC);
    } else {
        hex_vm_trace_push(env, HEX_VM_TRACE_TRAP0, PC);
    }
    hex_vm_trace_end(msg);
  }
}

static const char *guest_event_name[] = {
    [HEX_VM_TRACE_GUEST_TRAP0] = "guest trap0",
    [HEX_VM_TRACE_MACHINE_CHECK] = "machine check",
    [HEX_VM_TRACE_GEN_EXCEPTION] = "general exception",
    [HEX_VM_TRACE_INTERRUPT] = "interrupt",
    [HEX_VM_TRACE_RESERVED] = "*reserved*",
};

static void hex_vm_trace_vmrte(CPUHexagonState *env, GString *msg,
                               target_ulong PC)
{
    HexTraceEvent event = hex_vm_trace_pop(env, PC);
    const char *name = event < ARRAY_SIZE(guest_event_name) &&
                       guest_event_name[event] ?
                           guest_event_name[event] :
                           "unknown";
    g_string_append_printf(msg, "vmrte from %s: GELR = 0x%08" PRIx32,
                           name,
                           env->greg[HEX_GREG_GELR]);
    hex_vm_trace_push(env, HEX_VM_TRACE_VMRTE, PC);
}

void hex_vm_trace_trap1(CPUHexagonState *env, int imm, target_ulong PC)
{
    if (TRACE_HEX_VM) {
        GString *msg = hex_vm_trace_str(PC);
        switch (imm) {
        case HEX_VM_TRAP1_VMVERSION:
            g_string_append_printf(msg, "vmversion(0x%08" PRIx32 ")",
                                   env->gpr[HEX_REG_R00]);
            hex_vm_trace_push(env, HEX_VM_TRACE_VMVERSION, PC);
            break;
        case HEX_VM_TRAP1_VMRTE:
            hex_vm_trace_vmrte(env, msg, PC);
            break;
        case HEX_VM_TRAP1_VMSETVEC:
            g_string_append_printf(msg, "vmsetvec(0x%08" PRIx32 ")",
                                   env->gpr[HEX_REG_R00]);
            hex_vm_trace_push(env, HEX_VM_TRACE_VMSETVEC, PC);
            /* Record the event vector for future tracing */
            env->event_vectors = env->gpr[HEX_REG_R00];
            break;
        case HEX_VM_TRAP1_VMSETIE:
            g_string_append_printf(msg, "vmsetie(%s)",
                                   env->gpr[HEX_REG_R00] == 1 ?
                                       "enable" :
                                       "disable");
            hex_vm_trace_push(env, HEX_VM_TRACE_VMSETIE, PC);
            break;
        case HEX_VM_TRAP1_VMGETIE:
            g_string_append_printf(msg, "vmgetie");
            hex_vm_trace_push(env, HEX_VM_TRACE_VMGETIE, PC);
            break;
        case HEX_VM_TRAP1_VMINTOP:
            g_string_append_printf(msg, "vmintop(%" PRIu32 ")",
                            env->gpr[HEX_REG_R00]);
            hex_vm_trace_push(env, HEX_VM_TRACE_VMINTOP, PC);
            break;
        case HEX_VM_TRAP1_VMCLRMAP:
            g_string_append_printf(msg, "vmclrmap(0x%08" PRIx32
                                   ", %" PRIu32 ")",
                                   env->gpr[HEX_REG_R00],
                                   env->gpr[HEX_REG_R01]);
            hex_vm_trace_push(env, HEX_VM_TRACE_VMCLRMAP, PC);
            break;
        case HEX_VM_TRAP1_VMNEWMAP:
            g_string_append_printf(msg, "vmnewmap(0x%08" PRIx32 ")",
                                   env->gpr[HEX_REG_R00]);
            hex_vm_trace_push(env, HEX_VM_TRACE_VMNEWMAP, PC);
            break;
        case HEX_VM_TRAP1_VMCACHE:
            g_string_append_printf(msg, "vmcache(%" PRId32 ", 0x%08" PRIx32
                                   ", %" PRId32 ")",
                                   env->gpr[HEX_REG_R00],
                                   env->gpr[HEX_REG_R01],
                                   env->gpr[HEX_REG_R02]);
            hex_vm_trace_push(env, HEX_VM_TRACE_VMCACHE, PC);
            break;
        case HEX_VM_TRAP1_VMGETTIME:
            g_string_append_printf(msg, "vmgettime");
            hex_vm_trace_push(env, HEX_VM_TRACE_VMGETTIME, PC);
            break;
        case HEX_VM_TRAP1_VMSETTIME:
            g_string_append_printf(msg, "vmsettime(0x%08" PRIx32
                                   ", 0x%08" PRIx32 ")",
                                   env->gpr[HEX_REG_R00],
                                   env->gpr[HEX_REG_R01]);
            hex_vm_trace_push(env, HEX_VM_TRACE_VMSETTIME, PC);
            break;
        case HEX_VM_TRAP1_VMWAIT:
            g_string_append_printf(msg, "vmwait");
            hex_vm_trace_push(env, HEX_VM_TRACE_VMWAIT, PC);
            break;
        case HEX_VM_TRAP1_VMYIELD:
            g_string_append_printf(msg, "vmyield");
            hex_vm_trace_push(env, HEX_VM_TRACE_VMYIELD, PC);
            break;
        case HEX_VM_TRAP1_VMSTART:
            g_string_append_printf(msg, "vmstart(0x%08" PRIx32
                                   ", 0x%08" PRIx32 ")",
                                   env->gpr[HEX_REG_R00],
                                   env->gpr[HEX_REG_R01]);
            hex_vm_trace_push(env, HEX_VM_TRACE_VMSTART, PC);
            break;
        case HEX_VM_TRAP1_VMSTOP:
            g_string_append_printf(msg, "vmstop");
            /*
             * We should never return from vmstop, so there's a
             * g_assert_not_reached() in hex_vm_trace_rte in case we ever do.
             */
            hex_vm_trace_push(env, HEX_VM_TRACE_VMSTOP, PC);
            break;
        case HEX_VM_TRAP1_VMVPID:
            g_string_append_printf(msg, "vmpid");
            hex_vm_trace_push(env, HEX_VM_TRACE_VMPID, PC);
            break;
        case HEX_VM_TRAP1_VMSETREGS:
            g_string_append_printf(msg, "vmsetregs(0x%08" PRIx32
                                   ", 0x%08" PRIx32 ", 0x%08" PRIx32
                                   ", 0x%08" PRIx32 ")",
                                   env->gpr[HEX_REG_R00],
                                   env->gpr[HEX_REG_R01],
                                   env->gpr[HEX_REG_R02],
                                   env->gpr[HEX_REG_R03]);
            hex_vm_trace_push(env, HEX_VM_TRACE_VMSETREGS, PC);
            break;
        case HEX_VM_TRAP1_VMGETREGS:
            g_string_append_printf(msg, "vmgetregs");
            hex_vm_trace_push(env, HEX_VM_TRACE_VMGETREGS, PC);
            break;
        default:
            g_string_append_printf(msg, "trap1(%" PRId32 ")", imm);
            hex_vm_trace_push(env, HEX_VM_TRACE_TRAP1_UNKNOWN, PC);
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
            hex_vm_trace_push(env, HEX_VM_TRACE_RESERVED, PC);
            break;
        case HEX_VM_EVENT_MACHINE_CHECK:
            g_string_append_printf(msg, "machine check");
            hex_vm_trace_push(env, HEX_VM_TRACE_MACHINE_CHECK, PC);
            break;
        case HEX_VM_EVENT_GEN_EXCEPTION:
            g_string_append_printf(msg, "general exception: ");
            {
                target_ulong gsr = hexagon_greg_read(env, HEX_GREG_GSR);
                target_ulong cause = gsr & 0xffff;
                const char *name = cause < ARRAY_SIZE(gen_exception_name) &&
                                   gen_exception_name[cause] ?
                                       gen_exception_name[cause] :
                                       "unknown cause";
                g_string_append(msg, name);
            }
            hex_vm_trace_push(env, HEX_VM_TRACE_GEN_EXCEPTION, PC);
            break;
        case HEX_VM_EVENT_TRAP0:
            g_string_append_printf(msg, "trap0");
            hex_vm_trace_push(env, HEX_VM_TRACE_GUEST_TRAP0, PC);
            break;
        case HEX_VM_EVENT_INTERRUPT:
            g_string_append_printf(msg, "interrupt");
            hex_vm_trace_push(env, HEX_VM_TRACE_INTERRUPT, PC);
            break;
        default:
            g_string_append_printf(msg, "unknown");
            break;
        }
        g_string_append_printf(msg, ": GELR = 0x%08" PRIx32,
                               env->greg[HEX_GREG_GELR]);
        hex_vm_trace_end(msg);
    }
}

static const char *rte_name[] = {
    [HEX_VM_TRACE_VMVERSION] = "vmversion",
    [HEX_VM_TRACE_VMRTE] = "vmrte",
    [HEX_VM_TRACE_VMSETVEC] = "vmsetvec",
    [HEX_VM_TRACE_VMSETIE] = "vmsetie",
    [HEX_VM_TRACE_VMGETIE] = "vmgetie",
    [HEX_VM_TRACE_VMINTOP] = "vmintop",
    [HEX_VM_TRACE_VMCLRMAP] = "vmclrmap",
    [HEX_VM_TRACE_VMNEWMAP] = "vmnewmap",
    [HEX_VM_TRACE_VMCACHE] = "vmcache",
    [HEX_VM_TRACE_VMGETTIME] = "vmgettime",
    [HEX_VM_TRACE_VMSETTIME] = "vmsettime",
    [HEX_VM_TRACE_VMWAIT] = "vmwait",
    [HEX_VM_TRACE_VMYIELD] = "vmyield",
    [HEX_VM_TRACE_VMSTART] = "vmstart",
    [HEX_VM_TRACE_VMSTOP] = "vmstop",
    [HEX_VM_TRACE_VMPID] = "vmpid",
    [HEX_VM_TRACE_VMSETREGS] = "vmsetregs",
    [HEX_VM_TRACE_VMGETREGS] = "vmgetregs",
    [HEX_VM_TRACE_TRAP1_UNKNOWN] = "unknown trap1",
    [HEX_VM_TRACE_TRAP0_ANGEL] = "trap0 angel",
    [HEX_VM_TRACE_TRAP0] = "trap0",
    [HEX_VM_TRACE_TLB_MISSRW] = "tlbmissrw",
    [HEX_VM_TRACE_TLB_MISSX] = "tlbmissx",
    [HEX_VM_TRACE_PERM_ERR] = "perm err",
};

void hex_vm_trace_rte(CPUHexagonState *env, target_ulong PC)
{
    GString *msg = hex_vm_trace_str(PC);
    HexTraceEvent event = hex_vm_trace_pop(env, PC);
    const char *name = event < ARRAY_SIZE(rte_name) ? rte_name[event] : NULL;
    if (name) {
        const char *seperator = ":";
        uint32_t first_reg = hex_vm_trace_stack_pop_first_reg[event];
        g_string_append_printf(msg, "rte from %s", rte_name[event]);
        if (first_reg <= 4) {
            for (int i = 0; i < first_reg; i++) {
                g_string_append_printf(msg, "%s r%" PRId32 " = 0x%08" PRIx32,
                                       seperator, i, env->gpr[i]);
                seperator = ",";
            }
        }
        g_string_append_printf(msg, "%s ELR = 0x%08" PRIx32,
                               seperator,
                               arch_get_system_reg(env, HEX_SREG_ELR));
    } else {
        g_string_append_printf(msg, "rte: ELR = 0x%08" PRIx32,
                               arch_get_system_reg(env, HEX_SREG_ELR));
    }
    hex_vm_trace_end(msg);
}
