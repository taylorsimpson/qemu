/*
 *  Copyright(c) 2019-2022 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#include <stdio.h>
#include <string.h>

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;


static inline void S4_storerhnew_rr(void *p, int index, uint16_t v)
{
  asm volatile("{\n\t"
               "    r0 = %0\n\n"
               "    memh(%1+%2<<#2) = r0.new\n\t"
               "}\n"
               :: "r"(v), "r"(p), "r"(index)
               : "r0", "memory");
}

static uint32_t data;
static inline void *S4_storerbnew_ap(uint8_t v)
{
  void *ret;
  asm volatile("{\n\t"
               "    r0 = %1\n\n"
               "    memb(%0 = ##data) = r0.new\n\t"
               "}\n"
               : "=r"(ret)
               : "r"(v)
               : "r0", "memory");
  return ret;
}

static inline void *S4_storerhnew_ap(uint16_t v)
{
  void *ret;
  asm volatile("{\n\t"
               "    r0 = %1\n\n"
               "    memh(%0 = ##data) = r0.new\n\t"
               "}\n"
               : "=r"(ret)
               : "r"(v)
               : "r0", "memory");
  return ret;
}

static inline void *S4_storerinew_ap(uint32_t v)
{
  void *ret;
  asm volatile("{\n\t"
               "    r0 = %1\n\n"
               "    memw(%0 = ##data) = r0.new\n\t"
               "}\n"
               : "=r"(ret)
               : "r"(v)
               : "r0", "memory");
  return ret;
}

static inline void S4_storeirbt_io(void *p, int pred)
{
  asm volatile("p0 = cmp.eq(%0, #1)\n\t"
               "if (p0) memb(%1+#4)=#27\n\t"
               :: "r"(pred), "r"(p)
               : "p0", "memory");
}

static inline void S4_storeirbf_io(void *p, int pred)
{
  asm volatile("p0 = cmp.eq(%0, #1)\n\t"
               "if (!p0) memb(%1+#4)=#27\n\t"
               :: "r"(pred), "r"(p)
               : "p0", "memory");
}

static inline void S4_storeirbtnew_io(void *p, int pred)
{
  asm volatile("{\n\t"
               "    p0 = cmp.eq(%0, #1)\n\t"
               "    if (p0.new) memb(%1+#4)=#27\n\t"
               "}\n\t"
               :: "r"(pred), "r"(p)
               : "p0", "memory");
}

static inline void S4_storeirbfnew_io(void *p, int pred)
{
  asm volatile("{\n\t"
               "    p0 = cmp.eq(%0, #1)\n\t"
               "    if (!p0.new) memb(%1+#4)=#27\n\t"
               "}\n\t"
               :: "r"(pred), "r"(p)
               : "p0", "memory");
}

static inline void S4_storeirht_io(void *p, int pred)
{
  asm volatile("p0 = cmp.eq(%0, #1)\n\t"
               "if (p0) memh(%1+#4)=#27\n\t"
               :: "r"(pred), "r"(p)
               : "p0", "memory");
}

static inline void S4_storeirhf_io(void *p, int pred)
{
  asm volatile("p0 = cmp.eq(%0, #1)\n\t"
               "if (!p0) memh(%1+#4)=#27\n\t"
               :: "r"(pred), "r"(p)
               : "p0", "memory");
}

static inline void S4_storeirhtnew_io(void *p, int pred)
{
  asm volatile("{\n\t"
               "    p0 = cmp.eq(%0, #1)\n\t"
               "    if (p0.new) memh(%1+#4)=#27\n\t"
               "}\n\t"
               :: "r"(pred), "r"(p)
               : "p0", "memory");
}

static inline void S4_storeirhfnew_io(void *p, int pred)
{
  asm volatile("{\n\t"
               "    p0 = cmp.eq(%0, #1)\n\t"
               "    if (!p0.new) memh(%1+#4)=#27\n\t"
               "}\n\t"
               :: "r"(pred), "r"(p)
               : "p0", "memory");
}

static inline void S4_storeirit_io(void *p, int pred)
{
  asm volatile("p0 = cmp.eq(%0, #1)\n\t"
               "if (p0) memw(%1+#4)=#27\n\t"
               :: "r"(pred), "r"(p)
               : "p0", "memory");
}

static inline void S4_storeirif_io(void *p, int pred)
{
  asm volatile("p0 = cmp.eq(%0, #1)\n\t"
               "if (!p0) memw(%1+#4)=#27\n\t"
               :: "r"(pred), "r"(p)
               : "p0", "memory");
}

static inline void S4_storeiritnew_io(void *p, int pred)
{
  asm volatile("{\n\t"
               "    p0 = cmp.eq(%0, #1)\n\t"
               "    if (p0.new) memw(%1+#4)=#27\n\t"
               "}\n\t"
               :: "r"(pred), "r"(p)
               : "p0", "memory");
}

static inline void S4_storeirifnew_io(void *p, int pred)
{
  asm volatile("{\n\t"
               "    p0 = cmp.eq(%0, #1)\n\t"
               "    if (!p0.new) memw(%1+#4)=#27\n\t"
               "}\n\t"
               :: "r"(pred), "r"(p)
               : "p0", "memory");
}

static int L2_ploadrifnew_pi(void *p, int pred)
{
  int result;
  asm volatile("%0 = #31\n\t"
               "{\n\t"
               "    p0 = cmp.eq(%2, #1)\n\t"
               "    if (!p0.new) %0 = memw(%1++#4)\n\t"
               "}\n\t"
               : "=&r"(result), "+r"(p) : "r"(pred)
               : "p0");
  return result;
}

/*
 * Test that compound-compare-jump is executed in 2 parts
 * First we have to do all the compares in the packet and
 * account for auto-anding.  Then, we can do the predicated
 * jump.
 */
static inline int cmpnd_cmp_jump(void)
{
    int retval;
    asm ("r5 = #7\n\t"
         "r6 = #9\n\t"
         "{\n\t"
         "    p0 = cmp.eq(r5, #7)\n\t"
         "    if (p0.new) jump:nt 1f\n\t"
         "    p0 = cmp.eq(r6, #7)\n\t"
         "}\n\t"
         "%0 = #12\n\t"
         "jump 2f\n\t"
         "1:\n\t"
         "%0 = #13\n\t"
         "2:\n\t"
         : "=r"(retval) :: "r5", "r6", "p0");
    return retval;
}

static inline int test_clrtnew(int arg1, int old_val)
{
  int ret;
  asm volatile("r5 = %2\n\t"
               "{\n\t"
                   "p0 = cmp.eq(%1, #1)\n\t"
                   "if (p0.new) r5=#0\n\t"
               "}\n\t"
               "%0 = r5\n\t"
               : "=r"(ret)
               : "r"(arg1), "r"(old_val)
               : "p0", "r5");
  return ret;
}

int err;

static void check(int val, int expect)
{
    if (val != expect) {
        printf("ERROR: 0x%04x != 0x%04x\n", val, expect);
        err++;
    }
}

static void check64(long long val, long long expect)
{
    if (val != expect) {
        printf("ERROR: 0x%016llx != 0x%016llx\n", val, expect);
        err++;
    }
}

uint32_t init[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
uint32_t array[10];

uint32_t early_exit;

/*
 * Write these as functions because we can't guarantee the compiler will
 * allocate a frame with just the SL2_return_?new packet.
 */
static void SL2_return_tnew(int x);
asm ("SL2_return_tnew:\n\t"
     "   allocframe(#0)\n\t"
     "   r1 = #1\n\t"
     "   memw(##early_exit) = r1\n\t"
     "   {\n\t"
     "       p0 = cmp.eq(r0, #1)\n\t"
     "       if (p0.new) dealloc_return:nt\n\t"    /* SL2_return_tnew */
     "   }\n\t"
     "   r1 = #0\n\t"
     "   memw(##early_exit) = r1\n\t"
     "   dealloc_return\n\t"
    );

static void SL2_return_fnew(int x);
asm ("SL2_return_fnew:\n\t"
     "   allocframe(#0)\n\t"
     "   r1 = #1\n\t"
     "   memw(##early_exit) = r1\n\t"
     "   {\n\t"
     "       p0 = cmp.eq(r0, #1)\n\t"
     "       if (!p0.new) dealloc_return:nt\n\t"    /* SL2_return_fnew */
     "   }\n\t"
     "   r1 = #0\n\t"
     "   memw(##early_exit) = r1\n\t"
     "   dealloc_return\n\t"
    );

static long long creg_pair(int x, int y)
{
    long long retval;
    asm ("m0 = %1\n\t"
         "m1 = %2\n\t"
         "%0 = c7:6\n\t"
         : "=r"(retval) : "r"(x), "r"(y) : "m0", "m1");
    return retval;
}

static long long decbin(long long x, long long y, int *pred)
{
    long long retval;
    asm ("%0 = decbin(%2, %3)\n\t"
         "%1 = p0\n\t"
         : "=r"(retval), "=r"(*pred)
         : "r"(x), "r"(y));
    return retval;
}

/* Check that predicates are auto-and'ed in a packet */
static void auto_and(void)
{
    int res;
    asm ("r5 = #1\n\t"
         "{\n\t"
         "    p0 = cmp.eq(r1, #1)\n\t"
         "    p0 = cmp.eq(r1, #2)\n\t"
         "}\n\t"
         "%0 = p0\n\t"
         : "=r"(res)
         :
         : "r5", "p0");
    check(res, 0);

    asm ("r4 = #7\n\t"
         "r5 = #5\n\t"
         "r6 = #3\n\t"
         "{\n\t"
         "    p0 = !cmp.gt(r6, r5)\n\t"
         "    p0 = cmp.gt(r6, r4); if (!p0.new) jump:nt 1f\n\t"
         "}\n\t"
         "1:\n\t"
         "%0 = p0\n\t"
         : "=r"(res) : : "p0", "r4", "r5", "r5");
    check(res, 0);
}

void test_lsbnew(void)
{
    int result;

    asm("r0 = #2\n\t"
        "r1 = #5\n\t"
        "{\n\t"
        "    p0 = r0\n\t"
        "    if (p0.new) r1 = #3\n\t"
        "}\n\t"
        "%0 = r1\n\t"
        : "=r"(result) :: "r0", "r1", "p0");
    check(result, 5);
}

void test_l2fetch(void)
{
    /* These don't do anything in qemu, just make sure they don't assert */
    asm volatile ("l2fetch(r0, r1)\n\t"
                  "l2fetch(r0, r3:2)\n\t");
}

static int cl0(int x)
{
    int retval;
    asm("%0 = cl0(%1)\n\t" : "=r"(retval) : "r"(x));
    return retval;
}

#define extractu(RET, SRC, WIDTH, OFF) \
    asm("%0 = extractu(%1, #%2, #%3)\n\t" \
        : "=r"(RET) : "r"(SRC), "i"(WIDTH), "i"(OFF))

static void test_extractu(void)
{
    uint32_t res32;
    uint64_t res64;

    extractu(res32, -1, 0, 10);
    check(res32, 0);

    extractu(res32, -1, 31, 31);
    check(res32, 1);

    extractu(res64, -1LL, 0, 10);
    check64(res64, 0);

    extractu(res64, -1LL, 63, 63);
    check64(res64, 1);
}

static uint32_t satub(uint32_t src, int *ovf_result)
{
    uint32_t result;
    uint32_t usr;

    /*
     * This instruction can set bit 0 (OVF/overflow) in usr
     * Clear the bit first, then return that bit to the caller
     */
    asm volatile("r2 = usr\n\t"
                 "r2 = clrbit(r2, #0)\n\t"        /* clear overflow bit */
                 "usr = r2\n\t"
                 "%0 = satub(%2)\n\t"
                 "%1 = usr\n\t"
                 : "=r"(result), "=r"(usr)
                 : "r"(src)
                 : "r2", "usr");
  *ovf_result = (usr & 1);
  return result;
}

static void test_satub(void)
{
    uint32_t result;
    int ovf_result;

    result = satub(0, &ovf_result);
    check(result, 0);
    check(ovf_result, 0);

    result = satub(0xff, &ovf_result);
    check(result, 0xff);
    check(ovf_result, 0);

    result = satub(0xfff, &ovf_result);
    check(result, 0xff);
    check(ovf_result, 1);

    result = satub(-1, &ovf_result);
    check(result, 0);
    check(ovf_result, 1);
}

#define insert(RET, SRC, WIDTH, OFFSET) \
    asm("%0 = insert(%1, #%2, #%3)\n\t" \
        : "+r"(RET) : "r"(SRC), "i"(WIDTH), "i"(OFFSET))

static void test_insert(void)
{
    int res;

    res = 0x12345678;
    insert(res, 0xff, 8, 0);
    check(res, 0x123456ff);

    res = 0x12345678;
    insert(res, 0xff, 0, 0);
    check(res, 0x12345678);

    res = 0x12345678;
    insert(res, 0xff, 1, 31);
    check(res, 0x92345678);

    res = 0x12345678;
    insert(res, 0xff, 2, 31);
    check(res, 0x92345678);

    res = 0x12345678;
    insert(res, 0xff, 31, 31);
    check(res, 0x92345678);
}

#define bitspliti(RET, SRC, BITS) \
    asm("%0 = bitsplit(%1, #%2)\n\t" \
        : "=r"(RET) : "r"(SRC), "i"(BITS))

static void test_bitspliti(void)
{
    long long res64;

    bitspliti(res64, 0x12345678, 0);
    check64(res64, 0x1234567800000000ULL);

    bitspliti(res64, 0x12345678, 8);
    check64(res64, 0x0012345600000078ULL);

    bitspliti(res64, 0x12345678, 31);
    check64(res64, 0x0000000012345678ULL);
}

static void test_addipc(void)
{
    uint32_t pc, addipc_res;

    /* Normal version */
    asm("%0 = pc\n\t"
        "%1 = add(pc, #24)\n\t"
        : "=r"(pc), "=r"(addipc_res));
    check(addipc_res - pc, 28);

    /* Use constant extender */
    asm("%0 = pc\n\t"
        "%1 = add(pc, #100)\n\t"
        : "=r"(pc), "=r"(addipc_res));
    check(addipc_res - pc, 104);
}
int main()
{
    int res;
    long long res64;
    int pred;

    memcpy(array, init, sizeof(array));
    S4_storerhnew_rr(array, 4, 0xffff);
    check(array[4], 0xffff);

    data = ~0;
    check((uint32_t)S4_storerbnew_ap(0x12), (uint32_t)&data);
    check(data, 0xffffff12);

    data = ~0;
    check((uint32_t)S4_storerhnew_ap(0x1234), (uint32_t)&data);
    check(data, 0xffff1234);

    data = ~0;
    check((uint32_t)S4_storerinew_ap(0x12345678), (uint32_t)&data);
    check(data, 0x12345678);

    /* Byte */
    memcpy(array, init, sizeof(array));
    S4_storeirbt_io(&array[1], 1);
    check(array[2], 27);
    S4_storeirbt_io(&array[2], 0);
    check(array[3], 3);

    memcpy(array, init, sizeof(array));
    S4_storeirbf_io(&array[3], 0);
    check(array[4], 27);
    S4_storeirbf_io(&array[4], 1);
    check(array[5], 5);

    memcpy(array, init, sizeof(array));
    S4_storeirbtnew_io(&array[5], 1);
    check(array[6], 27);
    S4_storeirbtnew_io(&array[6], 0);
    check(array[7], 7);

    memcpy(array, init, sizeof(array));
    S4_storeirbfnew_io(&array[7], 0);
    check(array[8], 27);
    S4_storeirbfnew_io(&array[8], 1);
    check(array[9], 9);

    /* Half word */
    memcpy(array, init, sizeof(array));
    S4_storeirht_io(&array[1], 1);
    check(array[2], 27);
    S4_storeirht_io(&array[2], 0);
    check(array[3], 3);

    memcpy(array, init, sizeof(array));
    S4_storeirhf_io(&array[3], 0);
    check(array[4], 27);
    S4_storeirhf_io(&array[4], 1);
    check(array[5], 5);

    memcpy(array, init, sizeof(array));
    S4_storeirhtnew_io(&array[5], 1);
    check(array[6], 27);
    S4_storeirhtnew_io(&array[6], 0);
    check(array[7], 7);

    memcpy(array, init, sizeof(array));
    S4_storeirhfnew_io(&array[7], 0);
    check(array[8], 27);
    S4_storeirhfnew_io(&array[8], 1);
    check(array[9], 9);

    /* Word */
    memcpy(array, init, sizeof(array));
    S4_storeirit_io(&array[1], 1);
    check(array[2], 27);
    S4_storeirit_io(&array[2], 0);
    check(array[3], 3);

    memcpy(array, init, sizeof(array));
    S4_storeirif_io(&array[3], 0);
    check(array[4], 27);
    S4_storeirif_io(&array[4], 1);
    check(array[5], 5);

    memcpy(array, init, sizeof(array));
    S4_storeiritnew_io(&array[5], 1);
    check(array[6], 27);
    S4_storeiritnew_io(&array[6], 0);
    check(array[7], 7);

    memcpy(array, init, sizeof(array));
    S4_storeirifnew_io(&array[7], 0);
    check(array[8], 27);
    S4_storeirifnew_io(&array[8], 1);
    check(array[9], 9);

    memcpy(array, init, sizeof(array));
    res = L2_ploadrifnew_pi(&array[6], 0);
    check(res, 6);
    res = L2_ploadrifnew_pi(&array[7], 1);
    check(res, 31);

    int x = cmpnd_cmp_jump();
    check(x, 12);

    SL2_return_tnew(0);
    check(early_exit, 0);
    SL2_return_tnew(1);
    check(early_exit, 1);

    SL2_return_fnew(0);
    check(early_exit, 1);
    SL2_return_fnew(1);
    check(early_exit, 0);

    long long pair = creg_pair(5, 7);
    check((int)pair, 5);
    check((int)(pair >> 32), 7);

    res = test_clrtnew(1, 7);
    check(res, 0);
    res = test_clrtnew(2, 7);
    check(res, 7);

    res64 = decbin(0xf0f1f2f3f4f5f6f7LL, 0x7f6f5f4f3f2f1f0fLL, &pred);
    check64(res64, 0x357980003700010cLL);
    check(pred, 0);

    res64 = decbin(0xfLL, 0x1bLL, &pred);
    check64(res64, 0x78000100LL);
    check(pred, 1);

    auto_and();

    test_lsbnew();

    test_l2fetch();

    res = cl0(0x7fff);
    check(res, 17);
    res = cl0(0);
    check(res, 32);

    test_extractu();

    test_satub();

    test_insert();

    test_bitspliti();

    test_addipc();

    puts(err ? "FAIL" : "PASS");
    return err;
}
