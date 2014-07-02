/*	$Id: nwdb.c,v 1.24 2009/07/30 00:08:15 nis Exp $	*/

/*-
 * Copyright (c) 2008 Shingo Nishioka.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#if ! defined lint
static char rcsid[] = "$Id: nwdb.c,v 1.24 2009/07/30 00:08:15 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>

#include <assert.h>
#if ENABLE_DB
#include <db.h>
#endif
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nwam.h"
#include "nwdb.h"
#include "nht.h"

#include <gifnoc.h>

#if ! ENABLE_DB
typedef struct {
	void *data;
	size_t size;
} DBT;
#endif

struct _nwam_db {
	int type;
	union {
#if ENABLE_DB
		DB *db;
#endif
		struct nht *ht;
		struct nit *it;
	} u;
};

int
nwdb_drop(path, type)
char const *path;
{
	switch (type) {
#if ENABLE_DB
	case NWDB_CW:
	case NWDB_CR:
	case NWDB_XR:
	case NWDB_PR:
		return unlink(path);
#endif
	case NWDB_NCW:
		return nht_drop(path);
		break;
	case NWDB_NCR:
	case NWDB_NXR:
	case NWDB_NPR:
		return nit_drop(path);
		break;
	default:
		return -1;
	}
	/* NOTREACHED */
}

NWDB *
nwdb_open(path, type, flags)
char const *path;
mode_t flags;
{
	NWDB *db = NULL;
	if (!(db = calloc(1, sizeof *db))) {
		goto error;
	}
	switch (db->type = type) {
#if ENABLE_DB
	case NWDB_CW:
	case NWDB_CR:
	case NWDB_XR:
	case NWDB_PR:
		if (!(db->u.db = dbopen(path, O_CREAT | O_RDWR, 0644, DB_HASH, NULL))) {
			goto error;
		}
		break;
#endif
	case NWDB_NCW:
		if (!(db->u.ht = nht_open(path, sizeof (struct cw_entry), flags))) {
			goto error;
		}
		break;
	case NWDB_NCR:
	case NWDB_NXR:
	case NWDB_NPR:
		if (!(db->u.it = nit_open(path, flags))) {
			goto error;
		}
		break;
	default:
		return NULL;
	}
	return db;
error:
	free(db);
	return NULL;
}

int
nwdb_close(db)
NWDB *db;
{
	int e;
	switch (db->type) {
#if ENABLE_DB
	case NWDB_CW:
	case NWDB_CR:
	case NWDB_XR:
	case NWDB_PR:
		e = (*db->u.db->close)(db->u.db);
		break;
#endif
	case NWDB_NCW:
		e = nht_close(db->u.ht);
		break;
	case NWDB_NCR:
	case NWDB_NXR:
	case NWDB_NPR:
		e = nit_close(db->u.it);
		break;
	default:
		fprintf(stderr, "nwdb_close: internal error\n");
		return -1;
	}
	free(db);
	return e;
}

int
nwdb_put(db, key, data, size)
NWDB *db;
void *key, *data;
size_t size;
{
	DBT key0;
#if ENABLE_DB
	DBT data0;
#endif
	key0.data = key;
	switch (db->type) {
#if ENABLE_DB
	case NWDB_CW:
		key0.size = strlen(key) + 1;
		break;
	case NWDB_CR:
	case NWDB_XR:
	case NWDB_PR:
		key0.size = sizeof (idx_t);
		break;
#endif
	case NWDB_NCW:
		if (!nht_enter(db->u.ht, key, data)) {
			return -1;
		}
		return 0;
	case NWDB_NCR:
	case NWDB_NXR:
	case NWDB_NPR:
		if (!nit_enter(db->u.it, *(idx_t *)key0.data, data, size)) {
/*XXX*/
			return -1;
		}
		return 0;
	default:
		fprintf(stderr, "nwdb_put: internal error: unkn type: %d\n", db->type);
		return -1;
	}
#if ENABLE_DB
	data0.data = data;
	data0.size = size;
	return (*db->u.db->put)(db->u.db, &key0, &data0, 0);
#endif
}

int
nwdb_get(db, key, data, size)
NWDB *db;
void *key, *data;
size_t *size;
{
	DBT key0;
#if ENABLE_DB
	int e;
	DBT data0;
#endif
	key0.data = key;
	switch (db->type) {
		void *e;
#if ENABLE_DB
	case NWDB_CW:
		key0.size = strlen(key) + 1;
		break;
	case NWDB_CR:
	case NWDB_XR:
	case NWDB_PR:
		key0.size = sizeof (idx_t);
		break;
#endif
	case NWDB_NCW:
		if (!(e = nht_retrieve(db->u.ht, key))) {
			return 1;
		}
		*(struct cw_entry **)data = (struct cw_entry *)e;
		*size = sizeof (struct cr_entry);
		return 0;
	case NWDB_NCR:
	case NWDB_NXR:
	case NWDB_NPR:
		if (!(e = nit_retrieve(db->u.it, *(idx_t *)key0.data, size))) {
/*XXX*/
			return 1;
		}
		switch (db->type) {
		case NWDB_NCR:
			*(struct cr_entry **)data = e;
			break;
		case NWDB_NXR:
			*(struct xr_vec **)data = e;
			break;
		case NWDB_NPR:
			*(char **)data = e;
			break;
		}
		return 0;
	default:
		fprintf(stderr, "nwdb_get: internal error (1)\n");
		return -1;
	}
#if ENABLE_DB
	if ((e = (*db->u.db->get)(db->u.db, &key0, &data0, 0)) == -1) {
		return -1;
	}
	if (e == 1) {
		return 1;
	}
	assert(e == 0);
	switch (db->type) {
	case NWDB_CW:
		*(struct cw_entry **)data = data0.data;
		*size = data0.size;
assert(data0.size == sizeof (struct cw_entry));
		break;
	case NWDB_CR:
		*(struct cr_entry **)data = data0.data;
		*size = data0.size;
		break;
	case NWDB_XR:
		*(struct xr_vec **)data = data0.data;
		*size = data0.size;
		break;
	case NWDB_PR:
		*(char **)data = data0.data;
		*size = data0.size;
		break;
	default:
		fprintf(stderr, "nwdb_get: internal error (2)\n");
		return -1;
	}
	return e;
#endif
}

int
nwdb_seq(db, key, data, size)
NWDB *db;
void *key, *data;
size_t *size;
{
#if ENABLE_DB
	DBT key0, data0;
	int e;
#endif
	switch (db->type) {
		void *e;
	case NWDB_NCW:
		if (!(e = nht_seq(db->u.ht))) {
			return 1;
		}
		*(char **)key = &((char *)e)[sizeof (struct cw_entry)];
		*(struct cw_entry **)data = (struct cw_entry *)e;
		*size = sizeof (struct cr_entry);
		return 0;
	case NWDB_NCR:
	case NWDB_NXR:
	case NWDB_NPR:
		if (!(e = nit_seq(db->u.it, key, size))) {
			return 1;
		}
		switch (db->type) {
		case NWDB_NCR:
			*(struct cr_entry **)data = e;
			break;
		case NWDB_NXR:
			*(struct xr_vec **)data = e;
			break;
		case NWDB_NPR:
			*(char **)data = e;
			break;
		}
		return 0;
	default:
		break;
	}
#if ENABLE_DB
	if ((e = (*db->u.db->seq)(db->u.db, &key0, &data0, R_NEXT)) == -1) {
		return e;
	}
	if (e == 1) {
		return 1;
	}
	assert(e == 0);
	switch (db->type) {
	case NWDB_CW:
		*(char **)key = key0.data;
		*(struct cw_entry **)data = data0.data;
		*size = data0.size;
assert(data0.size == sizeof (struct cw_entry));
		break;
	case NWDB_CR:
		*(idx_t *)key = *(idx_t *)key0.data;
		*(struct cr_entry **)data = data0.data;
		*size = data0.size;
		break;
	case NWDB_XR:
		*(idx_t *)key = *(idx_t *)key0.data;
		*(struct xr_vec **)data = data0.data;
		*size = data0.size;
		break;
	case NWDB_PR:
		*(idx_t *)key = *(idx_t *)key0.data;
		*(char **)data = data0.data;
		*size = data0.size;
		break;
	case NWDB_NCW:
	case NWDB_NCR:
	case NWDB_NXR:
	case NWDB_NPR:
	default:
		fprintf(stderr, "nwdb_seq: internal error\n");
		return -1;
	}
	return e;
#else
	return -1;
#endif
}

int
nwdb_del(db, key)
NWDB *db;
void *key;
{
#if ENABLE_DB
	int e;
#endif
	DBT key0;
	key0.data = key;
	switch (db->type) {
#if ENABLE_DB
	case NWDB_CW:
		key0.size = strlen(key) + 1;
		break;
	case NWDB_CR:
	case NWDB_XR:
	case NWDB_PR:
		key0.size = sizeof (idx_t);
		break;
#endif
	case NWDB_NCW:
		if (nht_delete(db->u.ht, key) == -1) {
			return 1;
		}
		return 0;
	case NWDB_NCR:
	case NWDB_NXR:
	case NWDB_NPR:
		if (nit_delete(db->u.it, *(idx_t *)key0.data) == -1) {
			return 1;
		}
		return 0;
	default:
		fprintf(stderr, "nwdb_del: internal error (1)\n");
		return -1;
	}
#if ENABLE_DB
	if ((e = (*db->u.db->del)(db->u.db, &key0, 0)) == -1) {
		return -1;
	}
#endif
	return 0;
}

void
nwdb_rewind(db)
NWDB *db;
{
	switch (db->type) {
	case NWDB_NCW:
		nht_rewind(db->u.ht);
		break;
	case NWDB_NCR:
	case NWDB_NXR:
	case NWDB_NPR:
		nit_rewind(db->u.it);
		break;
	default:
		break;
	}
}
