/*

Copyright 2008, 2010, 2012 Lukas Mai.

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#ifdef _MSC_VER
# include <BaseTsd.h>
# define ssize_t SSIZE_T
#else
# include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef USE_HASHED_DB
# include <db.h>
# ifdef DB_VERSION_MAJOR
#  define HASHED_DB_API DB_VERSION_MAJOR
# else
#  define HASHED_DB_API 1
# endif
#endif
#ifdef USE_NETBSD_CURSES
# include <cdbr.h>
#endif

#ifndef TERMINFO_DIRS
#error "internal error: TERMINFO_DIRS is not defined"
#endif

enum {MAX_BUF = 4096};

const char *const unibi_terminfo_dirs = TERMINFO_DIRS;

unibi_term *unibi_from_fp(FILE *fp) {
    char buf[MAX_BUF];
    size_t n, r;

    for (n = 0; n < sizeof buf && (r = fread(buf + n, 1, sizeof buf - n, fp)) > 0; ) {
        n += r;

        if (feof(fp)) {
            break;
        }
    }

    if (ferror(fp)) {
        return NULL;
    }

    return unibi_from_mem(buf, n);
}

unibi_term *unibi_from_fd(int fd) {
    char buf[MAX_BUF];
    size_t n;
    ssize_t r;

    for (n = 0; n < sizeof buf && (r = read(fd, buf + n, sizeof buf - n)) > 0; ) {
        n += r;
    }

    if (r < 0) {
        return NULL;
    }

    return unibi_from_mem(buf, n);
}

#ifdef USE_HASHED_DB
static DB * unibi_db_open(const char *path) {
  DB *dbp = NULL;

#if HASHED_DB_API >= 4
  if (db_create(&dbp, NULL, 0) != 0
      || dbp->open(dbp, NULL, path, NULL, DB_HASH, DB_RDONLY, 0644) != 0)
    dbp = NULL;
#elif HASHED_DB_API >= 3
  if (db_create(&dbp, NULL, 0) != 0
      || dbp->open(dbp, path, NULL, DB_HASH, DB_RDONLY, 0644) != 0)
    dbp = NULL;
#elif HASHED_DB_API >= 2
  if (db_open(path, DB_HASH, DB_RDONLY, 0644,
              (DB_ENV *)NULL, (DB_INFO *)NULL, &dbp) != 0)
    dbp = NULL;
#else
  dbp = dbopen(path, O_RDONLY, 0644, DB_HASH, NULL);
#endif

  return dbp;
}

static void unibi_db_close(DB *dbp) {
#if HASHED_DB_API >= 2
  dbp->close(dbp, 0);
#else
  dbp->close(dbp);
#endif
}

static int unibi_db_get(DB *dbp, DBT *key, DBT *data) {
#if HASHED_DB_API >= 2
  return dbp->get(dbp, NULL, key, data, 0);
#else
  return dbp->get(dbp, key, data, 0);
#endif
}

unibi_term *unibi_from_db(const char *file, const char *term) {
  DB *dbp;
  unibi_term *ut = NULL;

  if (file == NULL || file[0] == '\0'|| term == NULL || term[0] == '\0') {
    return ut;
  }

  if ((dbp = unibi_db_open(file)) != NULL) {
    int reccnt = 0;
    char *save = strdup(term);
    DBT key = { 0 };
    DBT data = { 0 };

    key.data = save;
    key.size = strlen(term);

    while (unibi_db_get(dbp, &key, &data) == 0) {
      char *buf = (char *)data.data;
      int n = (int)data.size - 1;

      if (*buf++ == 0) {
        ut = unibi_from_mem(buf, n);
        break;
      }
      if (++reccnt >= 3) {
        break;
      }
      key.data = buf;
      key.size = n;
    }

    free(save);
    unibi_db_close(dbp);
  }

  return ut;
}
#elif defined(USE_NETBSD_CURSES)
static struct cdbr *unibi_db_open(const char *path) {
  return cdbr_open(path, CDBR_DEFAULT);
}

static void unibi_db_close(struct cdbr *dbp) {
  cdbr_close(dbp);
}

static int unibi_db_get(struct cdbr *dbp, uint32_t index,
                        const void **data, size_t *len) {
  return cdbr_get(dbp, index, data, len);
}

static int unibi_db_find(struct cdbr *dbp, const void *key, size_t keylen,
                         const void **data, size_t *datalen) {
  return cdbr_find(dbp, key, keylen, data, datalen);
}

unibi_term *unibi_from_db(const char *file, const char *term) {
  struct cdbr *dbp;
  unibi_term *ut = NULL;

  if (file == NULL || file[0] == '\0'|| term == NULL || term[0] == '\0') {
    return ut;
  }

  if ((dbp = unibi_db_open(file)) != NULL) {
    char *data;
    size_t datalen;
    if (unibi_db_find(dbp, (void *)term, strlen(term) + 1,
                  (void *)&data, &datalen) == 0) {
      if (datalen != 0 && data[0] == 2) {
        uint32_t idx = data[1] + (data[2] << 16);
        if (unibi_db_get(dbp, idx, (void *)&data, &datalen) != 0) {
          unibi_db_close(dbp);
          return NULL;
        }
      }
      ut = unibi_from_mem(data, datalen);
    }
    unibi_db_close(dbp);
  }

  return ut;
}
#else
unibi_term *unibi_from_file(const char *file) {
    int fd;
    unibi_term *ut;

    if ((fd = open(file, O_RDONLY)) < 0) {
        return NULL;
    }

    ut = unibi_from_fd(fd);
    close(fd);
    return ut;
}
#endif

static int add_overflowed(size_t *dst, size_t src) {
    *dst += src;
    return *dst < src;
}

static unibi_term *from_dir(const char *dir_begin, const char *dir_end, const char *mid, const char *term) {
    char *path;
    unibi_term *ut;
    size_t dir_len, mid_len, term_len, path_size;

    dir_len = dir_end ? (size_t)(dir_end - dir_begin) : strlen(dir_begin);
    mid_len = mid ? strlen(mid) + 1 : 0;
    term_len = strlen(term);

    path_size = 0;
    if (
        add_overflowed(&path_size, dir_len) ||
        add_overflowed(&path_size, mid_len) ||
#if defined(USE_HASHED_DB)
        add_overflowed(&path_size, 1 + 3 + 1)
                                /* /   .db \0 */
#elif defined(USE_NEBBSD_CURSES)
        add_overflowed(&path_size, 1 + 4 + 1)
                                /* /   .cdb \0 */
#else
        add_overflowed(&path_size, term_len) ||
        add_overflowed(&path_size, 1 + 2           + 1 + 1)
                                /* /   (%c | %02x)   /   \0 */
#endif
    ) {
        /* overflow */
        errno = ENOMEM;
        return NULL;
    }
    if (!(path = malloc(path_size))) {
        return NULL;
    }

    memcpy(path, dir_begin, dir_len);
#if defined(USE_HASHED_DB) || defined(USE_NETBSD_CURSES)
# ifdef USE_HASHED_DB
    sprintf(path + dir_len, "%s"             "%s.db",
#else
    sprintf(path + dir_len, "%s"             "%s.cdb",
#endif
                             mid ? "/" : "", mid ? mid : "");
    ut = unibi_from_db(path, term);
#else
    sprintf(path + dir_len, "/" "%s"            "%s"             "%c" "/" "%s",
                                 mid ? mid : "", mid ? "/" : "",  term[0], term);

    errno = 0;
    ut = unibi_from_file(path);
    if (!ut && errno == ENOENT) {
        /* OS X likes to use /usr/share/terminfo/<hexcode>/name instead of the first letter */
        sprintf(path + dir_len + 1 + mid_len, "%02x/%s",
                                               (unsigned int)((unsigned char)term[0] & 0xff),
                                               term);
        ut = unibi_from_file(path);
    }
#endif
    free(path);
    return ut;
}

static unibi_term *from_dirs(const char *list, const char *term) {
    const char *a, *z;

    if (list[0] == '\0') {
        errno = ENOENT;
        return NULL;
    }

    a = list;

    for (;;) {
        unibi_term *ut;

        while (*a == ':') {
            ++a;
        }
        if (*a == '\0') {
            break;
        }

        z = strchr(a, ':');

        ut = from_dir(a, z, NULL, term);
        if (ut || errno != ENOENT) {
            return ut;
        }

        if (!z) {
            break;
        }
        a = z + 1;
    }

    errno = ENOENT;
    return NULL;
}

unibi_term *unibi_from_term(const char *term) {
    unibi_term *ut;
    const char *env;

    assert(term != NULL);

    if (term[0] == '\0' || term[0] == '.' || strchr(term, '/')) {
        errno = EINVAL;
        return NULL;
    }

    if ((env = getenv("TERMINFO"))) {
        ut = from_dir(env, NULL, NULL, term);
        if (ut) {
            return ut;
        }
    }

    if ((env = getenv("HOME"))) {
        ut = from_dir(env, NULL, ".terminfo", term);
        if (ut || errno != ENOENT) {
            return ut;
        }
    }

    if ((env = getenv("TERMINFO_DIRS"))) {
        return from_dirs(env, term);
    }

    return from_dirs(unibi_terminfo_dirs, term);
}

unibi_term *unibi_from_env(void) {
    const char *term = getenv("TERM");
    if (!term) {
        errno = ENOENT;
        return NULL;
    }

    return unibi_from_term(term);
}
