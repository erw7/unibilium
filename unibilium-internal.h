#ifndef GUARD_UNIBILIUM_INTERNAL_H_
#define GUARD_UNIBILIUM_INTERNAL_H_

/*

Copyright 2008, 2010-2013, 2015 Lukas Mai.

This file is part of unibilium.

Unibilium is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Unibilium is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with unibilium.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "unibilium.h"

#include <stdlib.h>
#include <limits.h>

#define ASSERT_RETURN(COND, VAL) do { \
    assert(COND); \
    if (!(COND)) return VAL; \
} while (0)

#define ASSERT_RETURN_(COND) ASSERT_RETURN(COND, )

#define COUNTOF(a) (sizeof (a) / sizeof *(a))

#define NCONTAINERS(n, csize) (((n) - 1) / (csize) + 1u)

#define SIZE_ERR ((size_t)-1)

#define MAX15BITS 0x7fff
#define MAX31BITS 0x7fffffff

#if INT_MAX < MAX31BITS
#error "int must be at least 32 bits wide"
#endif

#define DYNARR(W, X) DynArr_ ## W ## _ ## X
#define DYNARR_T(W) DYNARR(W, t)
#define DEFDYNARRY(T, W) \
    typedef struct { T (*data); size_t used, size; } DYNARR_T(W); \
    void DYNARR(W, init)(DYNARR_T(W) *const); \
    void DYNARR(W, free)(DYNARR_T(W) *const); \
    int DYNARR(W, ensure_slots)(DYNARR_T(W) *const, const size_t); \
    int DYNARR(W, ensure_slot)(DYNARR_T(W) *const);

DEFDYNARRY(unsigned char, bool)
DEFDYNARRY(int, num)
DEFDYNARRY(const char *, str)

#define FAIL_IF_(c, e, f) do { if (c) { f; errno = (e); return NULL; } } while (0)
#define FAIL_IF(c, e) FAIL_IF_(c, e, (void)0)
#define DEL_FAIL_IF(c, e, x) FAIL_IF_(c, e, unibi_destroy(x))

struct unibi_term {
    const char *name;
    const char **aliases;

    unsigned char bools[NCONTAINERS(unibi_boolean_end_ - unibi_boolean_begin_ - 1, CHAR_BIT)];
    int nums[unibi_numeric_end_ - unibi_numeric_begin_ - 1];
    const char *strs[unibi_string_end_ - unibi_string_begin_ - 1];
    char *alloc;

    DYNARR_T(bool) ext_bools;
    DYNARR_T(num) ext_nums;
    DYNARR_T(str) ext_strs;
    DYNARR_T(str) ext_names;
    char *ext_alloc;
};

#define ASSERT_EXT_NAMES(X) assert((X)->ext_names.used == (X)->ext_bools.used + (X)->ext_nums.used + (X)->ext_strs.used)


static unsigned short get_ushort16(const char *p) {
    const unsigned char *q = (const unsigned char *)p;
    return q[0] + q[1] * 256;
}

static inline short get_short16(const char *p) {
    unsigned short n = get_ushort16(p);
    return n <= MAX15BITS ? n : -1;
}

static inline unsigned int get_uint32(const char *p) {
    const unsigned char *q = (const unsigned char *)p;
    return q[0] + q[1] * 256u + q[2] * 256u * 256u + q[3] * 256u * 256u * 256u;
}

static inline int get_int32(const char *p) {
    unsigned int n = get_uint32(p);
    return n <= MAX31BITS ? (int)n : -1;
}

static inline void fill_1(int *p, size_t n) {
    while (n--) {
        *p++ = -1;
    }
}

static inline void fill_null(const char **p, size_t n) {
    while (n--) {
        *p++ = NULL;
    }
}

static inline size_t mcount(const char *p, size_t n, char c) {
    size_t r = 0;
    while (n--) {
        if (*p++ == c) {
            r++;
        }
    }
    return r;
}

static inline void put_ushort16(char *p, unsigned short n) {
    unsigned char *q = (unsigned char *)p;
    q[0] = n % 256;
    q[1] = n / 256 % 256;
}

static inline void put_short16(char *p, short n) {
#if MAX15BITS < SHRT_MAX
    assert(n <= MAX15BITS);
#endif
    put_ushort16(
        p,
        n < 0
#if MAX15BITS < SHRT_MAX
        || n > MAX15BITS
#endif
        ? 0xffffU
        : (unsigned short)n
    );
}

#endif /* GUARD_UNIBILIUM_INTERNAL_H_ */
