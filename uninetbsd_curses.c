/*

Copyright 2008, 2010, 2012, 2013, 2015 Lukas Mai.

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
#include "unibilium-internal.h"
#include "uninetbsd_curses.h"

#include <errno.h>
#include <string.h>
#include <assert.h>
#ifdef USE_NETBSD_CURSES
# include <cdbr.h>
#endif

unibi_term *unibi_from_nbc_mem(const char *p, size_t n) {
#define HANDLE_BYTE(p, v, r, t) \
    do { \
        DEL_FAIL_IF(r < sizeof(*p), EFAULT, t); \
        v = *p++; \
        r -= sizeof(*p); \
    } while (0)
#define HANDLE_USHOTR16(p, v, r, t) \
    do { \
        DEL_FAIL_IF(r < sizeof(unsigned short), EFAULT, t); \
        v = get_ushort16(p); \
        p += sizeof(unsigned short); \
        r -= sizeof(unsigned short); \
    } while (0)
#define HANDLE_SHOTR16(p, v, r, t) \
    do { \
        DEL_FAIL_IF(r < sizeof(short), EFAULT, t); \
        v = get_short16(p); \
        p += sizeof(short); \
        r -= sizeof(short); \
    } while (0)
#define HANDLE_STR1(p, v, r, t) \
    do { \
        unsigned short l; \
        HANDLE_USHOTR16(p, l, r, t); \
        v += l; \
        DEL_FAIL_IF(r < l, EFAULT, t); \
        p += l; \
        r -= l; \
    } while (0)
#define HANDLE_STR2(p, d, v, e, r, t) \
    do { \
        unsigned short l; \
        HANDLE_USHOTR16(p, l, r, t); \
        assert(e >= d + l); \
        memcpy(d, p, l); \
        DEL_FAIL_IF(r < l, EFAULT, t); \
        p += l; \
        r -= l; \
        v = l; \
    } while (0)
#define HANDLE_EXT_NAME(p, d, l, e, i, t) \
    do { \
        assert(e >= d + l); \
        memcpy(d, p, l); \
        t->ext_names.data[i++] = d; \
        d += l; \
    } while (0)
#define FOR_EACH(p, r, t, code1, code2) \
    do { \
        unsigned short v; \
        HANDLE_USHOTR16(p, v, r, t); \
        if (v != 0) { \
            code1 \
            HANDLE_USHOTR16(p, v, r, t); \
            for (; v != 0; v--) { \
                code2 \
            } \
        } \
    } while (0)
    unibi_term *t = NULL;
    const char *pos, *strs_pos, *ext_pos;
    unsigned short namlen, tablsz, round;
    unsigned short extboollen, extnumlen, extstrslen, ext_strssz, ext_namessz;
    unsigned short ext_bools_namessz, ext_nums_namessz, ext_strs_namessz;
    char *strp, *namp;
    size_t namco, remain, allocsz, exttablsz;

    strs_pos = ext_pos = NULL;
    strp = namp = NULL;
    namlen = 0;
    tablsz = 0;
    namco = 0;
    allocsz = 0;
    exttablsz = 0;
    extboollen = extnumlen = extstrslen = 0;
    ext_strssz = ext_namessz = 0;
    ext_bools_namessz = ext_nums_namessz = ext_strs_namessz = 0;

    FAIL_IF(*p++ != 1, EINVAL);
    n--;

    FAIL_IF(!(t = malloc(sizeof *t)), ENOMEM);
    t->alloc = NULL;
    memset(t->bools, '\0', sizeof t->bools);
    fill_1(t->nums, COUNTOF(t->nums));
    fill_null(t->strs, COUNTOF(t->strs));
    DYNARR(bool, init)(&t->ext_bools);
    DYNARR(num, init)(&t->ext_nums);
    DYNARR(str, init)(&t->ext_strs);
    DYNARR(str, init)(&t->ext_names);
    t->ext_alloc = NULL;

    for (round = 0; round < 2; round++){
        size_t len;
        size_t k = 0;

        pos = p;
        remain = n;

        if (!round) {
            HANDLE_STR1(pos, namlen, remain, t);
            namco++;
        } else {
            void *mem;
            allocsz = namco * sizeof *t->aliases + tablsz + namlen + 1;
            DEL_FAIL_IF(
                    !(mem = malloc(allocsz)),
                    ENOMEM, t);
            t->alloc = mem;
            t->aliases = mem;

            strp = t->alloc + namco * sizeof *t->aliases;
            namp = strp + tablsz;

            HANDLE_STR2(pos, namp, len, t->alloc + allocsz, remain, t);
            t->aliases[k++] = namp;
            namp += len;
        }

        // aliases
        if (!round) {
            len = 0;
            HANDLE_STR1(pos, len, remain, t);
            if (len != 0) {
                namco += mcount(pos - len, len, '|') + 1;
                namlen += len;
            }
        } else {
            assert(namp != NULL);
            HANDLE_STR2(pos, namp, len, t->alloc + allocsz, remain, t);
            if (len != 0) {
                char *start, *end;
                start = namp;
                while ((end = strchr(start, '|'))) {
                    *end = '\0';
                    t->aliases[k++] = start;
                    start = end + 1;
                }
                t->aliases[k++] = start;
                namp += len;
            }
        }

        // description
        if (!round) {
            len = 0;
            HANDLE_STR1(pos, len, remain, t);
            namlen += len;
        } else {
            assert(namp != NULL);
            HANDLE_STR2(pos, namp, len, t->alloc + allocsz, remain, t);
            if (len != 0) {
                t->name = namp;
            } else {
                t->name = t->aliases[0];
            }
        }

        if(!round) {
            namco++;
        } else {
            assert(k < namco);
            t->aliases[k] = NULL;
        }

        if (!round) {
            FOR_EACH(pos, remain, t, {}, {
                    unsigned short idx;
                    char flag __attribute__((unused));
                    HANDLE_USHOTR16(pos, idx, remain, t);
                    HANDLE_BYTE(pos, flag, remain, t);
                    DEL_FAIL_IF(idx >= COUNTOF(nc_bools2nbc), EINVAL, t);
                    t->bools[nc_bools2nbc[idx] / CHAR_BIT]
                    |= 1 << nc_bools2nbc[idx] % CHAR_BIT;
                    });

            FOR_EACH(pos, remain, t, {}, {
                    unsigned short idx;
                    HANDLE_USHOTR16(pos, idx, remain, t);
                    DEL_FAIL_IF(idx >= COUNTOF(nc_nums2nbc), EINVAL, t);
                    HANDLE_SHOTR16(pos, t->nums[nc_nums2nbc[idx]], remain, t);
                    });
        }

        if (!round) {
            unsigned short slen, num;

            strs_pos = pos;
            HANDLE_USHOTR16(pos, slen, remain, t);
            if (slen) {
                HANDLE_USHOTR16(pos, num, remain, t);
                DEL_FAIL_IF(remain < slen, EFAULT, t);
                tablsz += slen - sizeof(unsigned short)
                    - sizeof(unsigned short) * 2 * num;
                pos += slen - sizeof(unsigned short);
                remain -= slen - sizeof(unsigned short);
            }
        } else {
            assert(strs_pos != NULL);
            pos = strs_pos;
            assert(p < strs_pos);
            assert(n > (size_t)(strs_pos - p));
            remain = n - (size_t)(strs_pos - p);
            FOR_EACH(pos, remain, t, {}, {
                    unsigned idx;
                    HANDLE_USHOTR16(pos, idx, remain, t);
                    DEL_FAIL_IF(idx >= COUNTOF(nc_strs2nbc), EINVAL, t);
                    HANDLE_STR2(pos, strp, len, t->alloc + allocsz, remain, t);
                    assert(idx < COUNTOF(nc_strs2nbc));
                    if (len) {
                    t->strs[nc_strs2nbc[idx]] = strp;
                    } else {
                    t->strs[nc_strs2nbc[idx]] = NULL;
                    }
                    strp += len;
                    });
        }

        if (!round) {
            ext_pos = pos;
        } else {
            unsigned short extalllen = extboollen + extnumlen + extstrslen;
            exttablsz = ext_strssz + ext_namessz;

            assert(ext_pos != NULL);
            pos = ext_pos;
            assert(p < ext_pos);
            assert(n > (size_t)(ext_pos - p));
            remain = n - (size_t)(ext_pos - p);

            DEL_FAIL_IF(
                    !DYNARR(bool, ensure_slots)(&t->ext_bools, extboollen) ||
                    !DYNARR(num, ensure_slots)(&t->ext_nums, extnumlen) ||
                    !DYNARR(str, ensure_slots)(&t->ext_strs, extstrslen) ||
                    !DYNARR(str, ensure_slots)(&t->ext_names, extalllen) ||
                    (exttablsz && !(t->ext_alloc = malloc(exttablsz))),
                    ENOMEM,
                    t
                    );
        }
        FOR_EACH(pos, remain, t,
                size_t bool_idx;
                size_t num_idx;
                size_t strs_idx;
                size_t bool_names_idx;
                size_t num_names_idx;
                size_t strs_names_idx;
                char *ext_strs_pos;
                char *ext_bools_names_pos;
                char *ext_nums_names_pos;
                char *ext_strs_names_pos;

                bool_idx = num_idx = strs_idx = 0;
                bool_names_idx = 0;
                num_names_idx = extboollen;
                strs_names_idx = num_names_idx + extnumlen;

                t->ext_names.used = extboollen + extnumlen + extstrslen;
                t->ext_bools.used = extboollen;
                t->ext_nums.used = extnumlen;
                t->ext_strs.used = extstrslen;

                ext_strs_pos = t->ext_alloc;
                ext_bools_names_pos = ext_strs_pos + ext_strssz;
                ext_nums_names_pos = ext_bools_names_pos + ext_bools_namessz;
                ext_strs_names_pos = ext_nums_names_pos + ext_nums_namessz;
                ,
                    char type;
                size_t name_len;
                const char *name_pos;

                HANDLE_USHOTR16(pos, name_len, remain, t);
                ext_namessz += name_len;
                remain -= name_len;
                name_pos = pos;
                pos += name_len;
                HANDLE_BYTE(pos, type, remain, t);
                switch (type) {
                    case 'f':
                        {
                            char flag;
                            HANDLE_BYTE(pos, flag, remain, t);
                            if (!round) {
                                extboollen++;
                                ext_bools_namessz += name_len;
                            } else {
                                t->ext_bools.data[bool_idx++] = !!flag;
                                HANDLE_EXT_NAME(name_pos, ext_bools_names_pos,
                                        name_len,
                                        t->ext_alloc + exttablsz,
                                        bool_names_idx, t);
                            }
                            break;
                        }
                    case 'n':
                        {
                            unsigned short num;
                            HANDLE_USHOTR16(pos, num, remain, t);
                            if (!round) {
                                extnumlen++;
                                ext_nums_namessz += name_len;
                            } else {
                                t->ext_nums.data[num_idx++] = num;
                                HANDLE_EXT_NAME(name_pos, ext_nums_names_pos,
                                        name_len,
                                        t->ext_alloc + exttablsz,
                                        num_names_idx, t);
                            }
                            break;
                        }
                    case 's':
                        if (!round) {
                            extstrslen++;
                            HANDLE_STR1(pos, ext_strssz, remain, t);
                            ext_strs_namessz += name_len;
                        } else {
                            HANDLE_STR2(pos, ext_strs_pos, len,
                                    t->ext_alloc + ext_strssz + ext_namessz,
                                    remain, t);
                            if (len) {
                                t->ext_strs.data[strs_idx++] = ext_strs_pos;
                            } else {
                                t->ext_strs.data[strs_idx++] = NULL;
                            }
                            ext_strs_pos += len;
                            HANDLE_EXT_NAME(name_pos, ext_strs_names_pos, name_len,
                                    t->ext_alloc + exttablsz,
                                    strs_names_idx, t);
                        }
                        break;
                    default:
                        unibi_destroy(t);
                        errno = EINVAL;
                        return NULL;
                }
                );
                assert(remain == 0);
    }

    return t;
#undef HANDLE_BYTE
#undef HANDLE_USHOTR16
#undef HANDLE_SHOTR16
#undef HANDLE_STR1
#undef HANDLE_STR2
#undef FOR_EACH
}

size_t unibi_dump_nbc(const unibi_term *t, char *ptr, size_t n) {
#define PUT_BYTE(p, v, r, n) \
    do { \
        r += sizeof(char); \
        if (r <= n) { \
            *p++ = v; \
        } \
    } while (0)
#define PUT_USHORT16(p, v, r, n) \
    do { \
        r += sizeof(unsigned short); \
        if (r <= n) { \
            put_ushort16(p, (unsigned short)v); \
            p += sizeof(unsigned short); \
        } \
    } while (0)
#define PUT_SHORT16(p, v, r, n) \
    do { \
        r += sizeof(unsigned short); \
        if (r <= n) { \
            put_short16(p, (short)v); \
            p += sizeof(short); \
        } \
    } while (0)
#define PUT_STR(p, s, r, n) \
    do { \
        assert(s != NULL); \
        if (s != NULL) { \
            size_t l = strlen(s) + 1; \
            PUT_USHORT16(p, l, r, n); \
            r += l; \
            if (r <= n) { \
                memcpy(p, s, l); \
                p += l; \
            } \
        } \
    } while (0)
#define PUT_EXT_STR(p, s, r, n) \
    do { \
        if (s != NULL) { \
            PUT_STR(p, s, r, n); \
        } else { \
            PUT_USHORT16(p, 0, r, n); \
        } \
    } while (0)
#define PUT_CAP2EXT_NAME(p, s, r, n) \
    do { \
        if (s != NULL && s[0] != '\0' && s[1] != '\0' \
                && s[0] == 'O' && s[1] == 'T') { \
            PUT_STR(p, s + 2, r, n); \
        } else { \
            PUT_STR(p, s, r, n); \
        }\
    } while (0)
#define HANDLE_CAP(p, l, code, r, n) \
    do { \
        char *s; \
        s = p; \
        l = 0; \
        r += sizeof(unsigned short) * 2; \
        if (r < n) { \
            p += sizeof(unsigned short) * 2; \
        } \
        code; \
        r -= sizeof(unsigned short) * 2; \
        if (l) { \
            assert(p >= s); \
            PUT_USHORT16(s, \
                    (size_t)(p - s) - sizeof(unsigned short), r, n); \
            PUT_USHORT16(s, l, r, n); \
        } else { \
            PUT_USHORT16(s, 0, r, n); \
            p = s; \
        } \
    } while (0)
#define FOR_EACH_CAP(p, l, c, cond, tbl, code, r, n) \
    do { \
        for (i = 0; i < c; i++) { \
            if (cond) { \
                short idx; \
                idx = tbl[i]; \
                if (idx != -1) { \
                    l++; \
                    PUT_USHORT16(p, idx, r, n); \
                    code; \
                } \
            } \
        } \
    } while (0)
#define FOR_EACH_CAP2EXT(p, l, T, type, c, flag, code, r, n) \
    do { \
        for (i = 0; i < COUNTOF(nbc_##type##s2ncext); i++) { \
            T v; \
            v = unibi_get_##type(t, nbc_##type##s2ncext[i].index); \
            if (c) { \
                l++; \
                PUT_CAP2EXT_NAME(p, nbc_##type##s2ncext[i].name, r, n); \
                PUT_BYTE(p, flag, r, n); \
                code; \
            } \
        } \
    } while (0)
#define FOR_EACH_EXT(p, l, type, idx, flag, code, r, n) \
    do { \
        for (i = 0; i < t->ext_##type##s.used; i++) { \
            l++; \
            PUT_STR(p, t->ext_names.data[idx], r, n); \
            idx++; \
            PUT_BYTE(p, flag, r, n); \
            code; \
        } \
    } while (0)
    char *pos, *start;
    size_t req, i;
    size_t boollen, numlen, strslen, extlen;
    size_t ext_names_idx;

    pos = ptr;
    req = 0;
    PUT_BYTE(pos, 1, req, n);

    PUT_STR(pos, t->aliases[0], req, n);
    assert(req > n || req == (size_t)(pos - ptr));

    start = pos;
    i = 1;
    while (t->aliases[i]) {
        PUT_STR(pos, t->aliases[i], req, n);
        if (t->aliases[i + 1]) {
            *(pos - 1) = '|';
        }
    }
    if (start == pos) {
        PUT_USHORT16(pos, 0, req, n);
    }
    assert(req > n || req == (size_t)(pos - ptr));

    PUT_STR(pos, t->name, req, n);
    assert(req > n || req == (size_t)(pos - ptr));

    HANDLE_CAP(pos, boollen,
            FOR_EACH_CAP(pos, boollen, unibi_boolean_end_ - 1,
                t->bools[i / CHAR_BIT] & 1 << i % CHAR_BIT,
                nbc_bools2nc, PUT_BYTE(pos, 1, req, n), req, n),
            req, n);
    assert(req > n || req == (size_t)(pos - ptr));

    HANDLE_CAP(pos, numlen,
            FOR_EACH_CAP(pos, numlen, COUNTOF(t->nums), t->nums[i] >= 0,
                nbc_nums2nc, PUT_SHORT16(pos, t->nums[i], req, n),
                req, n), req, n);
    assert(req > n || req == (size_t)(pos - ptr));

    HANDLE_CAP(pos, strslen,
            FOR_EACH_CAP(pos, strslen, COUNTOF(t->strs), t->strs[i],
                nbc_strs2nc, PUT_STR(pos, t->strs[i], req, n),
                req, n), req, n);
    assert(req > n || req == (size_t)(pos - ptr));

    start = pos;
    ext_names_idx = 0;
    HANDLE_CAP(pos, extlen, {
            FOR_EACH_CAP2EXT(pos, extlen, int, bool, v, 'f',
                    PUT_BYTE(pos, 1, req, n), req, n);
            FOR_EACH_CAP2EXT(pos, extlen, int, num, v > 0, 'n',
                    PUT_SHORT16(pos, v, req, n), req, n);
            FOR_EACH_CAP2EXT(pos, extlen, const char *, str, v != NULL, 's',
                    PUT_STR(pos, v, req, n), req, n);
            FOR_EACH_EXT(pos, extlen, bool, ext_names_idx, 'f',
                    PUT_BYTE(pos, (char)t->ext_bools.data[i], req, n),
                    req, n);
            FOR_EACH_EXT(pos, extlen, num, ext_names_idx, 'n',
                    PUT_SHORT16(pos, t->ext_nums.data[i], req, n),
                    req, n);
            FOR_EACH_EXT(pos, extlen, str, ext_names_idx, 's',
                    PUT_EXT_STR(pos, t->ext_strs.data[i], req, n),
                    req, n);
            }, req, n);
    assert(req > n || req == (size_t)(pos - ptr));

    if (req > n) {
        errno = EFAULT;
    }

    return req;
#undef PUT_BYTE
#undef PUT_USHORT16
#undef PUT_SHORT16
#undef PUT_STR
#undef PUT_EXT_STR
#undef HANDLE_CAP
#undef FOR_EACH_CAP
#undef FOR_EACH_CAP2EXT
#undef FOR_EACH_CAP
}

#ifdef USE_NETBSD_CURSES
unibi_term *unibi_from_nbc_db(const char *file, const char *term) {
    struct cdbr *dbp;
    unibi_term *ut = NULL;

    if (file == NULL || file[0] == '\0'|| term == NULL || term[0] == '\0') {
        errno = EINVAL;
        return ut;
    }

    if ((dbp = cdbr_open(file, CDBR_DEFAULT)) != NULL) {
        char *data;
        size_t datalen, keylen;
        keylen = strlen(term) + 1;
        if (cdbr_find(dbp, (void *)term, keylen,
                    (void *)&data, &datalen) == 0) {
            if (datalen >= 8 && data[0] == 2
                    && ((get_ushort16(data + 5)) == keylen)
                    && (strncmp(term, (char *)(data + 7), keylen) == 0)) {
                uint32_t idx = get_uint32(data + 1);
                if (cdbr_get(dbp, idx, (void *)&data, &datalen) != 0) {
                    goto noent;
                }
            } else if (datalen < 3 + keylen || data[0] != 1
                    || (get_ushort16(data + 1) != keylen)
                    || (strncmp(term, (char *)(data + 3), keylen) != 0)) {

                goto noent;
            }
            ut = unibi_from_nbc_mem(data, datalen);
        } else {
            goto noent;
        }
        cdbr_close(dbp);
    }

    return ut;
noent:
    cdbr_close(dbp);
    errno = ENOENT;
    return NULL;
}
#else
unibi_term *unibi_from_nbc_db(const char *file, const char *term) {
    errno = ENOTSUP;
    return NULL;
}
#endif
