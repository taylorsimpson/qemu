/*
 *  Copyright(c) 2019-2021 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "qemu/qemu-print.h"
#include "cpu.h"
#include "sysemu/cpus.h"
#include "internal.h"
#include "exec/exec-all.h"
#include "hex_mmu.h"
#include "macros.h"
#include "sys_macros.h"
#include "reg_fields.h"

#define GET_TLB_FIELD(ENTRY, FIELD)                               \
    ((uint64_t)fEXTRACTU_BITS(ENTRY, reg_field_info[FIELD].width, \
                              reg_field_info[FIELD].offset))

/*
 * PPD (physical page descriptor) is formed by putting the PTE_PA35 field
 * in the MSB of the PPD
 */
#define GET_PPD(ENTRY) \
    ((GET_TLB_FIELD((ENTRY), PTE_PPD) | \
     (GET_TLB_FIELD((ENTRY), PTE_PA35) << reg_field_info[PTE_PPD].width)))

#define NO_ASID      (1 << 8)

typedef enum {
    PGSIZE_4K,
    PGSIZE_16K,
    PGSIZE_64K,
    PGSIZE_256K,
    PGSIZE_1M,
    PGSIZE_4M,
    PGSIZE_16M,
    PGSIZE_64M,
    PGSIZE_256M,
    PGSIZE_1G,
    NUM_PGSIZE_TYPES
} tlb_pgsize_t;

static const tlb_pgsize_t pgsize[NUM_PGSIZE_TYPES] = {
    PGSIZE_4K,
    PGSIZE_16K,
    PGSIZE_64K,
    PGSIZE_256K,
    PGSIZE_1M,
    PGSIZE_4M,
    PGSIZE_16M,
    PGSIZE_64M,
    PGSIZE_256M,
    PGSIZE_1G
};

static const char *pgsize_str[NUM_PGSIZE_TYPES] = {
    "4K",
    "16K",
    "64K",
    "256K",
    "1M",
    "4M",
    "16M",
    "64M",
    "256M",
    "1G"
};

#define INVALID_MASK 0xffffffffLL

static uint64_t encmask_2_mask[] = {
    0x0fffLL,                           /* 4k,   0000 */
    0x3fffLL,                           /* 16k,  0001 */
    0xffffLL,                           /* 64k,  0010 */
    0x3ffffLL,                          /* 256k, 0011 */
    0xfffffLL,                          /* 1m,   0100 */
    0x3fffffLL,                         /* 4m,   0101 */
    0xffffffLL,                         /* 16m,  0110 */
    0x3ffffffLL,                        /* 64m,  0111 */
    0xfffffffLL,                        /* 256m, 1000 */
    0x3fffffffLL,                       /* 1g,   1001 */
    INVALID_MASK,                       /* RSVD, 0111 */
};

static inline tlb_pgsize_t hex_tlb_pgsize(uint64_t entry)
{
    if (entry == 0) {
        qemu_log_mask(CPU_LOG_MMU, "%s: Supplied TLB entry was 0!\n", __func__);
        return pgsize[0];
    }
    int size = __builtin_ctzll(entry);
    g_assert(size < NUM_PGSIZE_TYPES);
    return pgsize[size];
}

static inline uint32_t hex_tlb_page_size(uint64_t entry)
{
    return 1 << (TARGET_PAGE_BITS + 2 * hex_tlb_pgsize(GET_PPD(entry)));
}

static inline uint64_t hex_tlb_phys_page_num(uint64_t entry)
{
    uint32_t ppd = GET_PPD(entry);
    return ppd >> 1;
}

static inline uint64_t hex_tlb_phys_addr(uint64_t entry)
{
    uint64_t pagemask = encmask_2_mask[hex_tlb_pgsize(entry)];
    uint64_t pagenum = hex_tlb_phys_page_num(entry);
    uint64_t PA = (pagenum << TARGET_PAGE_BITS) & (~pagemask);
    return PA;
}

static inline uint64_t hex_tlb_virt_addr(uint64_t entry)
{
    return GET_TLB_FIELD(entry, PTE_VPN) << TARGET_PAGE_BITS;
}

static bool hex_dump_mmu_entry(FILE *f, uint64_t entry)
{
    if (GET_TLB_FIELD(entry, PTE_V)) {
        fprintf(f, "0x%016" PRIx64 ": ", entry);
        uint64_t PA = hex_tlb_phys_addr(entry);
        uint64_t VA = hex_tlb_virt_addr(entry);
        fprintf(f, "V:%" PRId64 " G:%" PRId64 " A1:%" PRId64 " A0:%" PRId64,
                GET_TLB_FIELD(entry, PTE_V), GET_TLB_FIELD(entry, PTE_G),
                GET_TLB_FIELD(entry, PTE_ATR1), GET_TLB_FIELD(entry, PTE_ATR0));
        fprintf(f, " ASID:0x%02" PRIx64 " VA:0x%08" PRIx64,
                GET_TLB_FIELD(entry, PTE_ASID), VA);
        fprintf(f,
                " X:%" PRId64 " W:%" PRId64 " R:%" PRId64 " U:%" PRId64
                " C:%" PRId64,
                GET_TLB_FIELD(entry, PTE_X), GET_TLB_FIELD(entry, PTE_W),
                GET_TLB_FIELD(entry, PTE_R), GET_TLB_FIELD(entry, PTE_U),
                GET_TLB_FIELD(entry, PTE_C));
        fprintf(f, " PA:0x%09" PRIx64 " SZ:%s (0x%x)", PA,
                pgsize_str[hex_tlb_pgsize(entry)], hex_tlb_page_size(entry));
        fprintf(f, "\n");
        return true;
    }

    /* Not valid */
    return false;
}

void dump_mmu(CPUHexagonState *env)
{
    for (uint32_t i = 0; i < NUM_TLB_ENTRIES; i++) {
        uint64_t entry = env->hex_tlb->entries[i];
        if (GET_TLB_FIELD(entry, PTE_V)) {
            qemu_printf("[%03" PRIu32 "] ", i);
            qemu_printf("0x%016" PRIx64 ": ", entry);
            uint64_t PA = hex_tlb_phys_addr(entry);
            uint64_t VA = hex_tlb_virt_addr(entry);
            qemu_printf(
                "V:%" PRId64 " G:%" PRId64 " A1:%" PRId64 " A0:%" PRId64,
                GET_TLB_FIELD(entry, PTE_V), GET_TLB_FIELD(entry, PTE_G),
                GET_TLB_FIELD(entry, PTE_ATR1), GET_TLB_FIELD(entry, PTE_ATR0));
            qemu_printf(" ASID:0x%02" PRIx64 " VA:0x%08" PRIx64,
                        GET_TLB_FIELD(entry, PTE_ASID), VA);
            qemu_printf(
                " X:%" PRId64 " W:%" PRId64 " R:%" PRId64 " U:%" PRId64
                " C:%" PRId64,
                GET_TLB_FIELD(entry, PTE_X), GET_TLB_FIELD(entry, PTE_W),
                GET_TLB_FIELD(entry, PTE_R), GET_TLB_FIELD(entry, PTE_U),
                GET_TLB_FIELD(entry, PTE_C));
            qemu_printf(" PA:0x%09" PRIx64 " SZ:%s (0x%x)", PA,
                        pgsize_str[hex_tlb_pgsize(entry)],
                        hex_tlb_page_size(entry));
            qemu_printf("\n");
        }
    }
}

#if HEX_DEBUG
static void hex_dump_mmu(CPUHexagonState *env, FILE *f)
{
    bool valid_found = false;
    int i;
    rcu_read_lock();
    for (i = 0; i < NUM_TLB_ENTRIES; i++) {
        valid_found |= hex_dump_mmu_entry(f, env->hex_tlb->entries[i]);
    }
    if (!valid_found) {
        fprintf(f, "TLB is empty :(\n");
    }
    rcu_read_unlock();
}
#endif

static inline void hex_log_tlbw(uint32_t index, uint64_t entry)
{
    if (qemu_loglevel_mask(CPU_LOG_MMU)) {
        if (qemu_log_enabled()) {
            FILE *logfile = qemu_log_trylock();
            if (logfile) {
                fprintf(logfile, "tlbw[%03d]: ", index);
                if (!hex_dump_mmu_entry(logfile, entry)) {
                    fprintf(logfile, "invalid\n");
                }
                qemu_log_unlock(logfile);
            }
        }
    }
}

void hex_tlbw(CPUHexagonState *env, uint32_t index, uint64_t value)
{
    uint32_t myidx = fTLB_NONPOW2WRAP(fTLB_IDXMASK(index));
    bool old_entry_valid = GET_TLB_FIELD(env->hex_tlb->entries[myidx], PTE_V);
    if (old_entry_valid && hexagon_cpu_mmu_enabled(env)) {
        /* FIXME - Do we have to invalidate everything here? */
        CPUState *cs = env_cpu(env);

        tlb_flush(cs);
    }
    env->hex_tlb->entries[myidx] = (value);
    hex_log_tlbw(myidx, value);
}

void hex_mmu_realize(CPUHexagonState *env)
{
    CPUState *cs = env_cpu(env);
    if (cs->cpu_index == 0) {
        env->hex_tlb = g_malloc0(sizeof(CPUHexagonTLBContext));
    } else {
        CPUState *cpu0_s = NULL;
        CPUHexagonState *env0 = NULL;
        CPU_FOREACH(cpu0_s) {
            assert(cpu0_s->cpu_index == 0);
            env0 = &(HEXAGON_CPU(cpu0_s)->env);
            break;
        }
        env->hex_tlb = env0->hex_tlb;
    }
}

void hex_mmu_reset(CPUHexagonState *env)
{
    CPUState *cs = env_cpu(env);
    if (cs->cpu_index == 0) {
        memset(env->hex_tlb, 0, sizeof(CPUHexagonTLBContext));
    }
}

void hex_mmu_on(CPUHexagonState *env)
{
    CPUState *cs = env_cpu(env);
    qemu_log_mask(CPU_LOG_MMU, "Hexagon MMU turned on!\n");
    tlb_flush(cs);

#if HEX_DEBUG
    hex_dump_mmu(env, stderr);
#endif
}

void hex_mmu_off(CPUHexagonState *env)
{
    CPUState *cs = env_cpu(env);
    qemu_log_mask(CPU_LOG_MMU, "Hexagon MMU turned off!\n");
    tlb_flush(cs);
}

void hex_mmu_mode_change(CPUHexagonState *env)
{
    qemu_log_mask(CPU_LOG_MMU, "Hexagon mode change: new mode is %s\n",
                  get_sys_str(env));
    CPUState *cs = env_cpu(env);
    tlb_flush(cs);
}

static inline bool hex_tlb_entry_match_noperm(uint64_t entry, uint32_t asid,
                                              target_ulong VA)
{
    if (GET_TLB_FIELD(entry, PTE_V)) {
        if (GET_TLB_FIELD(entry, PTE_G)) {
            /* Global entry - ingnore ASID */
        } else if (asid != NO_ASID) {
            uint32_t tlb_asid = GET_TLB_FIELD(entry, PTE_ASID);
            if (tlb_asid != asid) {
                return false;
            }
        }

        uint64_t page_size = hex_tlb_page_size(entry);
        uint64_t page_start = hex_tlb_virt_addr(entry);
        page_start &= ~(page_size - 1);
        if (page_start <= VA && VA < page_start + page_size) {
            /* FIXME - Anything else we need to check? */
            return true;
        }
    }
    return false;
}

static inline void hex_tlb_entry_get_perm(CPUHexagonState *env, uint64_t entry,
                                          MMUAccessType access_type,
                                          int mmu_idx, int *prot,
                                          int32_t *excp)
{
    bool perm_x = GET_TLB_FIELD(entry, PTE_X);
    bool perm_w = GET_TLB_FIELD(entry, PTE_W);
    bool perm_r = GET_TLB_FIELD(entry, PTE_R);
    bool perm_u = GET_TLB_FIELD(entry, PTE_U);
    bool user_idx = mmu_idx == MMU_USER_IDX;

    if (mmu_idx == MMU_KERNEL_IDX) {
        *prot = PAGE_VALID | PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        return;
    }

    *prot = PAGE_VALID;
    switch (access_type) {
    case MMU_INST_FETCH:
        if (user_idx && !perm_u) {
            *excp = HEX_EVENT_PRECISE;
            env->cause_code = HEX_CAUSE_FETCH_NO_UPAGE;
        } else if (!perm_x) {
            *excp = HEX_EVENT_PRECISE;
            env->cause_code = HEX_CAUSE_FETCH_NO_XPAGE;
        }
        break;
    case MMU_DATA_LOAD:
        if (user_idx && !perm_u) {
            *excp = HEX_EVENT_PRECISE;
            env->cause_code = HEX_CAUSE_PRIV_NO_UREAD;
        } else if (!perm_r) {
            *excp = HEX_EVENT_PRECISE;
            env->cause_code = HEX_CAUSE_PRIV_NO_READ;
        }
        break;
    case MMU_DATA_STORE:
        if (user_idx && !perm_u) {
            *excp = HEX_EVENT_PRECISE;
            env->cause_code = HEX_CAUSE_PRIV_NO_UWRITE;
        } else if (!perm_w) {
            *excp = HEX_EVENT_PRECISE;
            env->cause_code = HEX_CAUSE_PRIV_NO_WRITE;
        }
        break;
    }

    if (!user_idx || perm_u) {
        if (perm_x) {
            *prot |= PAGE_EXEC;
        }
        if (perm_r) {
            *prot |= PAGE_READ;
        }
        if (perm_w) {
            *prot |= PAGE_WRITE;
        }
    }
}

static inline bool hex_tlb_entry_match(CPUHexagonState *env, uint64_t entry,
                                       uint8_t asid, target_ulong VA,
                                       MMUAccessType access_type, hwaddr *PA,
                                       int *prot, int *size, int32_t *excp,
                                       int mmu_idx)
{
    if (hex_tlb_entry_match_noperm(entry, asid, VA)) {
        hex_tlb_entry_get_perm(env, entry, access_type, mmu_idx, prot, excp);
        *PA = hex_tlb_phys_addr(entry);
        *size = hex_tlb_page_size(entry);
        return true;
    }
    return false;
}

bool hex_tlb_find_match(CPUHexagonState *env, target_ulong VA,
                        MMUAccessType access_type, hwaddr *PA, int *prot,
                        int *size, int32_t *excp, int mmu_idx)
{
    uint32_t ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
    uint8_t asid = GET_SSR_FIELD(SSR_ASID, ssr);
    int i;
    for (i = 0; i < NUM_TLB_ENTRIES; i++) {
        uint64_t entry = env->hex_tlb->entries[i];
        if (hex_tlb_entry_match(env, entry, asid, VA, access_type, PA, prot,
                                size, excp, mmu_idx)) {
            return true;
        }
    }
    return false;
}

static uint32_t hex_tlb_lookup_by_asid(CPUHexagonState *env, uint32_t asid,
                                       uint32_t VA)
{
    uint32_t not_found = 0x80000000;
    uint32_t idx = not_found;
    int i;

    env->imprecise_exception = 0;
    for (i = 0; i < NUM_TLB_ENTRIES; i++) {
        uint64_t entry = env->hex_tlb->entries[i];
        if (hex_tlb_entry_match_noperm(entry, asid, VA)) {
            if (idx != not_found) {
                env->imprecise_exception = HEX_EVENT_IMPRECISE;
                env->cause_code = HEX_CAUSE_IMPRECISE_MULTI_TLB_MATCH;
                break;
            }
            idx = i;
        }
    }

    if (idx == not_found) {
        qemu_log_mask(CPU_LOG_MMU, "%s: 0x%x, 0x%08x => NOT FOUND\n",
                      __func__, asid, VA);
    } else {
        qemu_log_mask(CPU_LOG_MMU, "%s: 0x%x, 0x%08x => %d\n",
                      __func__, asid, VA, idx);
    }

    return idx;
}

/* Called from tlbp instruction */
uint32_t hex_tlb_lookup(CPUHexagonState *env, uint32_t ssr, uint32_t VA)
{
    return hex_tlb_lookup_by_asid(env, GET_SSR_FIELD(SSR_ASID, ssr), VA);
}

static bool hex_tlb_is_match(CPUHexagonState *env,
                             uint64_t entry1, uint64_t entry2,
                             bool consider_gbit)
{
    bool valid1 = GET_TLB_FIELD(entry1, PTE_V);
    bool valid2 = GET_TLB_FIELD(entry2, PTE_V);
    uint64_t size1 = hex_tlb_page_size(entry1);
    uint64_t vaddr1 = hex_tlb_virt_addr(entry1);
    vaddr1 &= ~(size1 - 1);
    uint64_t size2 = hex_tlb_page_size(entry2);
    uint64_t vaddr2 = hex_tlb_virt_addr(entry2);
    vaddr2 &= ~(size2 - 1);
    int asid1 = GET_TLB_FIELD(entry1, PTE_ASID);
    int asid2 = GET_TLB_FIELD(entry2, PTE_ASID);
    bool gbit1 = GET_TLB_FIELD(entry1, PTE_G);
    bool gbit2 = GET_TLB_FIELD(entry2, PTE_G);

    if (!valid1 || !valid2) {
        return false;
    }

    if (((vaddr1 <= vaddr2) && (vaddr2 < (vaddr1 + size1))) ||
        ((vaddr2 <= vaddr1) && (vaddr1 < (vaddr2 + size2)))) {
        if (asid1 == asid2) {
            return true;
        }
        if ((consider_gbit && gbit1) || gbit2) {
            return true;
        }
    }
    return false;
}

/*
 * Return codes:
 * 0 or positive             index of match
 * -1                        multiple matches
 * -2                        no match
 */
int hex_tlb_check_overlap(CPUHexagonState *env, uint64_t entry, uint64_t index)
{
    int matches = 0;
    int last_match = 0;
    int i;

    for (i = 0; i < NUM_TLB_ENTRIES; i++) {
        if (hex_tlb_is_match(env, entry, env->hex_tlb->entries[i], false)) {
            matches++;
            last_match = i;
        }
    }

    if (matches == 1) {
        return last_match;
    }
    if (matches == 0) {
        return -2;
    }
    return -1;
}

static inline void print_thread(const char *str, CPUState *cs)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *thread = &cpu->env;
    bool is_stopped = cpu_is_stopped(cs);
    int exe_mode = get_exe_mode(thread);
    hex_lock_state_t lock_state = ATOMIC_LOAD(thread->tlb_lock_state);
    qemu_log_mask(CPU_LOG_MMU,
           "%s: threadId = %d: %s, exe_mode = %s, tlb_lock_state = %s\n",
           str,
           thread->threadId,
           is_stopped ? "stopped" : "running",
           exe_mode == HEX_EXE_MODE_OFF ? "off" :
           exe_mode == HEX_EXE_MODE_RUN ? "run" :
           exe_mode == HEX_EXE_MODE_WAIT ? "wait" :
           exe_mode == HEX_EXE_MODE_DEBUG ? "debug" :
           "unknown",
           lock_state == HEX_LOCK_UNLOCKED ? "unlocked" :
           lock_state == HEX_LOCK_WAITING ? "waiting" :
           lock_state == HEX_LOCK_OWNER ? "owner" :
           "unknown");
}

static inline void print_thread_states(const char *str)
{
    CPUState *cs;
    CPU_FOREACH(cs) {
        print_thread(str, cs);
    }
}

void hex_tlb_lock(CPUHexagonState *env)
{
    qemu_log_mask(CPU_LOG_MMU, "hex_tlb_lock: %d\n", env->threadId);
    const bool exception_context = qemu_mutex_iothread_locked();
    LOCK_IOTHREAD(exception_context);

    uint32_t syscfg = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SYSCFG);
    uint8_t tlb_lock = GET_SYSCFG_FIELD(SYSCFG_TLBLOCK, syscfg);
    if (tlb_lock) {
        if (ATOMIC_LOAD(env->tlb_lock_state) == HEX_LOCK_OWNER) {
            qemu_log_mask(CPU_LOG_MMU, "Already the owner\n");
            UNLOCK_IOTHREAD(exception_context);
            return;
        }
        qemu_log_mask(CPU_LOG_MMU, "\tWaiting\n");
        ATOMIC_STORE(env->tlb_lock_state, HEX_LOCK_WAITING);
        cpu_exit(env_cpu(env));
    } else {
        qemu_log_mask(CPU_LOG_MMU, "\tAcquired\n");
        ATOMIC_STORE(env->tlb_lock_state, HEX_LOCK_OWNER);
        SET_SYSCFG_FIELD(env, SYSCFG_TLBLOCK, 1);
    }

    if (qemu_loglevel_mask(CPU_LOG_MMU)) {
        qemu_log_mask(CPU_LOG_MMU, "Threads after hex_tlb_lock:\n");
        print_thread_states("\tThread");
    }
    UNLOCK_IOTHREAD(exception_context);
}

void hex_tlb_unlock(CPUHexagonState *env)
{
    qemu_log_mask(CPU_LOG_MMU, "hex_tlb_unlock: %d\n", env->threadId);
    const bool exception_context = qemu_mutex_iothread_locked();
    LOCK_IOTHREAD(exception_context);

    /* Nothing to do if the TLB isn't locked by this thread */
    uint32_t syscfg = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SYSCFG);
    uint8_t tlb_lock = GET_SYSCFG_FIELD(SYSCFG_TLBLOCK, syscfg);
    if ((tlb_lock == 0) ||
        (ATOMIC_LOAD(env->tlb_lock_state) != HEX_LOCK_OWNER)) {
        qemu_log_mask(CPU_LOG_MMU, "\tNot owner\n");
        UNLOCK_IOTHREAD(exception_context);
        return;
    }

    ATOMIC_STORE(env->tlb_lock_state, HEX_LOCK_UNLOCKED);
    SET_SYSCFG_FIELD(env, SYSCFG_TLBLOCK, 0);

    /* Look for a thread to unlock */
    unsigned int this_threadId = env->threadId;
    CPUHexagonState *unlock_thread = NULL;
    CPUState *cs;
    CPU_FOREACH(cs) {
        HexagonCPU *cpu = HEXAGON_CPU(cs);
        CPUHexagonState *thread = &cpu->env;

        /*
         * The hardware implements round-robin fairness, so we look for threads
         * starting at env->threadId + 1 and incrementing modulo the number of
         * threads.
         *
         * To implement this, we check if thread is a earlier in the modulo
         * sequence than unlock_thread.
         *     if unlock thread is higher than this thread
         *         thread must be between this thread and unlock_thread
         *     else
         *         thread higher than this thread is ahead of unlock_thread
         *         thread must be lower then unlock thread
         */
        if (ATOMIC_LOAD(thread->tlb_lock_state) == HEX_LOCK_WAITING) {
            if (!unlock_thread) {
                unlock_thread = thread;
            } else if (unlock_thread->threadId > this_threadId) {
                if (this_threadId < thread->threadId &&
                    thread->threadId < unlock_thread->threadId) {
                    unlock_thread = thread;
                }
            } else {
                if (thread->threadId > this_threadId) {
                    unlock_thread = thread;
                }
                if (thread->threadId < unlock_thread->threadId) {
                    unlock_thread = thread;
                }
            }
        }
    }
    if (unlock_thread) {
        cs = env_cpu(unlock_thread);
        print_thread("\tWaiting thread found", cs);
        ATOMIC_STORE(unlock_thread->tlb_lock_state, HEX_LOCK_OWNER);
        SET_SYSCFG_FIELD(unlock_thread, SYSCFG_TLBLOCK, 1);
        cpu_resume(cs);
    }

    if (qemu_loglevel_mask(CPU_LOG_MMU)) {
        qemu_log_mask(CPU_LOG_MMU, "Threads after hex_tlb_unlock:\n");
        print_thread_states("\tThread");
    }
    UNLOCK_IOTHREAD(exception_context);
}

