/*	$Id: nht.c,v 1.48 2010/11/15 10:08:31 nis Exp $	*/

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
static char rcsid[] = "$Id: nht.c,v 1.48 2010/11/15 10:08:31 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/time.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>

#include "nwam.h"
#include "util.h"

#include "nvmt.h"
#include "nht.h"

#include <gifnoc.h>

/* #define DBG	1 /* */
/* #define DBGxx	1 /* */

typedef size_t fwptr_t;

struct nht {
	size_t dsize;	/* size of payload. it's fixed for each table. */
	hashval_t nentries, threshold;
	struct {
		size_t max, total;
		size_t dist[9];
	} stat;
	struct {
		struct nvmt *vt;
		hashval_t size;
	} ix;
	struct {
		struct nvmt *vt;
		size_t offs;
	} fw;
	hashval_t seq_ptr;
	int rdonly;
};

struct hte {
	char data[1];
};

struct nit {
	hashval_t nentries;
	struct {
		struct nvmt *vt;
		hashval_t size;
	} ix;
	struct {
		struct nvmt *vt;
		size_t offs;
	} fw;
	hashval_t seq_ptr;
	int rdonly;
};

struct ite {
	size_t size;
		/* unlike `nht', payload size may vary for each entry. */
	char data[1];
};

static int ht_extend(struct nht *);
static size_t *ht_cell0(struct nht *);
static void ht_stat(struct nht *, FILE *);
static hashval_t nsxhash(hashval_t, char const *, int);
static uint32_t cshash(unsigned char const *, size_t, int);
static int it_extend(struct nit *, hashval_t);
static size_t *it_cell0(struct nit *);

#if defined DBG
static char *dbg_r(void *, size_t, char *, size_t);
#endif

#define	IT_GROWSZ	1024
#define	HT_GROWSZ	(512 * 1024)		/* 512k entry / fragment */
/*#define	HT_GROWSZ	(6 * 1024)		/* 6k entry / fragment */

#define	THRESHOLD(t)	((t) / 7 * 6)

/*
 * hte:
 *  data[0..h->dsize)            -- payload
 *  data[h->dsize..strlen(name)] -- key + '\0'
 */
#define	HTE_NAME(h, v)	(&(v)->data[h->dsize])
#define	HTE_SIZE(h, l)	(offsetof(struct hte, data[(h)->dsize + (l)]))

#define DIRTY1	1

int
nht_drop(path)
char const *path;
{
	char ipath[MAXPATHLEN], dpath[MAXPATHLEN];
	int r = 0;

	snprintf(ipath, sizeof ipath, "%s.i", path);
	if (nvmt_drop(ipath) == -1) {
		syslog(LOG_DEBUG, "nvmt_drop failed: %s", ipath);
		r = -1;
	}

	snprintf(dpath, sizeof dpath, "%s.d", path);
	if (nvmt_drop(dpath) == -1) {
		syslog(LOG_DEBUG, "nvmt_drop failed: %s", dpath);
		r = -1;
	}

	return r;
}

struct nht *
nht_open(path, size, flags)
char const *path;
size_t size;
mode_t flags;
{
	size_t *e;
	char ipath[MAXPATHLEN], dpath[MAXPATHLEN];
	struct nht *h;

	if (!(h = calloc(1, sizeof *h))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}
	h->rdonly = (flags & O_ACCMODE) == O_RDONLY;
	h->dsize = size;
	snprintf(ipath, sizeof ipath, "%s.i", path);
	if (!(h->ix.vt = nvmt_open(ipath, sizeof (fwptr_t), 2, flags))) {
		syslog(LOG_DEBUG, "nvmt_open failed: %s", ipath);
		free(h);
		return NULL;
	}
	h->ix.size = nVMTxend(h->ix.vt);
	h->threshold = THRESHOLD(h->ix.size);
	snprintf(dpath, sizeof dpath, "%s.d", path);
	if (!(h->fw.vt = nvmt_open(dpath, 1, 2, flags))) {
		syslog(LOG_DEBUG, "nvmt_open failed: %s", dpath);
		free(h);
		return NULL;
	}
	h->fw.offs = nVMTxend(h->fw.vt);
	h->seq_ptr = 0;

	if (!(e = ht_cell0(h))) {
		syslog(LOG_DEBUG, "ht_cell0 failed");
		free(h);
		return NULL;
	}
	h->nentries = *e;

	return h;
}

int
nht_close(h)
struct nht *h;
{
	size_t *e;

	if (!h->rdonly) {
		if (!(e = ht_cell0(h))) {
			free(h);
			return -1;
		}
		*e = h->nentries;
	}
	nvmt_close(h->ix.vt);
	nvmt_close(h->fw.vt);
	free(h);
	return 0;
}

void *
nht_enter(h, p, ptr)
struct nht *h;
char const *p;
void *ptr;
{
	int c;

	if (h->rdonly) {
		return NULL;
	}

	if (h->threshold <= h->nentries) {
		if (ht_extend(h) == -1) {
			return NULL;
		}
	}

	c = 0;
	for (;;) {
		hashval_t s = nsxhash(h->ix.size, p, c++);
		fwptr_t *e = nVMTa(h->ix.vt, s, fwptr_t);
		struct hte *vv;
		if (!*e || *e == DIRTY1) {	/* new */
			size_t offs;
			size_t l = strlen(p) + 1, end;
			if (!(offs = nvmt_alloc(h->fw.vt, h->fw.offs, HTE_SIZE(h, l), &end, 0))) {
				syslog(LOG_DEBUG, "nvmt_alloc failed");
				return NULL;
			}
			h->fw.offs = end;
			h->nentries++;
			*e = offs;
assert((*e & DIRTY1) == 0 || *e == DIRTY1);
			vv = (struct hte *)nVMTa(h->fw.vt, *e, char);
			memmove(vv->data, ptr, h->dsize);
			strcpy(HTE_NAME(h, vv), p);	/* safe */
			return vv->data;
		}
		else if (vv = (struct hte *)nVMTa(h->fw.vt, *e, char), !strcmp(HTE_NAME(h, vv), p)) {	/* update */
			memmove(vv->data, ptr, h->dsize);
			return vv->data;
		}
		/* else conflict */
	}
	/* NOTREACHED */
}

void *
nht_retrieve(h, p)
struct nht *h;
char const *p;
{
	int c;

	if (h->ix.size == 0) {
		return NULL;	/* not found (table is empty) */
	}
	c = 0;
	for (;;) {
		hashval_t s = nsxhash(h->ix.size, p, c++);
		fwptr_t *e = nVMTa(h->ix.vt, s, fwptr_t);
		struct hte *vv;
		if (*e == DIRTY1) { /* void */
			;
		}
		else if (!*e) {	/* not found */
			return NULL;
		}
		else if (vv = (struct hte *)nVMTa(h->fw.vt, *e, char), !strcmp(HTE_NAME(h, vv), p)) {
			return vv->data;	/* found */
		}
		/* else conflict */
	}
	/* NOTREACHED */
}

void *
nht_seq(h)
struct nht *h;
{
	hashval_t s;

	for (s = h->seq_ptr; s < h->ix.size; s++) {
		fwptr_t *e = nVMTa(h->ix.vt, s, fwptr_t);
		if (*e && *e != DIRTY1) {
			struct hte *vv;
			h->seq_ptr = s + 1;
			vv = (struct hte *)nVMTa(h->fw.vt, *e, char);
			return vv->data;
		}
	}
	h->seq_ptr = s;
	return NULL;
}

int
nht_delete(h, p)
struct nht *h;
char const *p;
{
	int c;

	if (h->rdonly) {
		return -1;
	}

	if (h->ix.size == 0) {
		return -1;	/* not found (table is empty) */
	}
	c = 0;
	for (;;) {
		hashval_t s = nsxhash(h->ix.size, p, c++);
		fwptr_t *e = nVMTa(h->ix.vt, s, fwptr_t);
		struct hte *vv;
		if (*e == DIRTY1) { /* void */
			;
		}
		else if (!*e) {	/* not found */
			return -1;
		}
		else if (vv = (struct hte *)nVMTa(h->fw.vt, *e, char), !strcmp(HTE_NAME(h, vv), p)) {
			*e = DIRTY1;	/* found. erase it. */
			return 0;
		}
		/* else conflict */
	}
	/* NOTREACHED */
	return -1;
}

void
nht_rewind(h)
struct nht *h;
{
	h->seq_ptr = 0;
}

static int
ht_extend(h)
struct nht *h;
{
	fwptr_t *p;
	hashval_t j;
	int c;
	hashval_t osize, newsize;
	hashval_t i, m;
	size_t cnf;

	osize = h->ix.size;

#if defined DBGxx
syslog(LOG_DEBUG, "NHT_EXTEND(%p): osize=%"PRIuHASHVAL_T, h, osize);
#endif
	assert(nVMTxend(h->ix.vt) == osize);
	newsize = h->ix.size + HT_GROWSZ;
	if (nvmt_extend(h->ix.vt, newsize, 0) == -1) {
		return -1;
	}
	assert(nVMTxend(h->ix.vt) == newsize);
#if defined DBGxx
syslog(LOG_DEBUG, "NHT_EXTEND(%p): FILL_ZERO: %"PRIuHASHVAL_T" -- %"PRIuHASHVAL_T, h, osize, newsize);
#endif
	for (j = osize; j < newsize; j++) {
		p = nVMTa(h->ix.vt, j, fwptr_t);
		*p = 0;		/* fw.vt[0] is occupied by a nentries cell, so we can use 0 as an empty marker. see also it_cell0 */
	}

	h->ix.size = newsize;
	h->threshold = THRESHOLD(h->ix.size);

	for (i = 0, m = 0; i < osize; i++) {
		fwptr_t *o = nVMTa(h->ix.vt, i, fwptr_t);
		if (*o == DIRTY1) {
			*o = 0;
		}
assert((*o & DIRTY1) == 0);
		if (*o) {
			*o |= DIRTY1;
			assert(*o != DIRTY1);
			m++;
		}
	}
	assert(m == h->nentries);
	h->stat.max = h->stat.total = 0;
#if 1
	bzero(h->stat.dist, sizeof h->stat.dist);
#else
	for (j = 0; j < nelems(h->stat.dist); j++) {
		h->stat.dist[j] = 0;
	}
#endif
	for (i = 0, m = 0; i < osize; ) {
		struct hte *vv;
		fwptr_t *o = nVMTa(h->ix.vt, i, fwptr_t);
		if (*o & DIRTY1) {
			char *opname;
			*o &= ~DIRTY1;
			vv = (struct hte *)nVMTa(h->fw.vt, *o, char);
			opname = HTE_NAME(h, vv);
			m++;
			cnf = 0;
			c = 0;
			for (;;) {
				hashval_t s = nsxhash(h->ix.size, opname, c++);
				fwptr_t *e = nVMTa(h->ix.vt, s, fwptr_t);
				if (*e & DIRTY1) {	/* dirty */
					fwptr_t t;
					assert(i < s);
					t = *e;
					*e = *o;
					*o = t;
					break;
				}
				else if (!*e) {	/* empty */
					*e = *o;
					*o = 0;
					i++;
					break;
				}
				else if (vv = (struct hte *)nVMTa(h->fw.vt, *e, char), !strcmp(HTE_NAME(h, vv), opname)) { /* self */
					assert(i == s);
					assert((*e & DIRTY1) == 0);
					i++;
					break;
				}
				/* else conflict */
				cnf++;
			}
			if (h->stat.max < cnf) {
				h->stat.max = cnf;
			}
			h->stat.total += cnf;
			if (cnf < nelems(h->stat.dist) - 1) {
				h->stat.dist[cnf]++;
			}
			else {
				h->stat.dist[nelems(h->stat.dist) - 1]++;
			}
		}
		else {
			i++;
		}
	}
	assert(m == h->nentries);
	ht_stat(h, stderr);
	return 0;
}

/*
 * NOTE: offset 0 points nentries cell.
 * It is also used as an empty marker.
 */
static size_t *
ht_cell0(h)
struct nht *h;
{
	size_t *e;

	if (nVMTMMAPEDP(h->fw.vt, 0)) {
		assert(h->fw.vt->map.bases[0]);
		e = (size_t *)nVMTa(h->fw.vt, 0, char);
	}
	else {
		size_t end = 0;
		if (h->rdonly) {
			syslog(LOG_DEBUG, "nvmt_alloc failed: readonly");
			return NULL;
		}
		if (nvmt_alloc(h->fw.vt, 0, sizeof *e, &end, 0) != 0 || end == 0) {
			syslog(LOG_DEBUG, "nvmt_alloc failed");
			return NULL;
		}
		h->fw.offs = end;
		e = (size_t *)nVMTa(h->fw.vt, 0, char);
		*e = 0;
	}
	return e;
}

static void
ht_stat(h, stream)
struct nht *h;
FILE *stream;
{
	size_t i;
	fprintf(stream, "#(");
	fprintf(stream, "size: %"PRIuHASHVAL_T, h->ix.size);
	fprintf(stream, ", nent: %"PRIuHASHVAL_T, h->nentries);
	fprintf(stream, ", dens: %.4f%%", (double)h->nentries / h->ix.size * 100);
	if (h->nentries) {
		fprintf(stream, ", maxc: %"PRIuSIZE_T" %.4f\n", (pri_size_t)(h->stat.max), (double)h->stat.total / h->nentries);
		for (i = 0; i < nelems(h->stat.dist); i++) {
			fprintf(stream, ", %"PRIuSIZE_T": %"PRIuSIZE_T" (%.4f%%)", (pri_size_t)(i), (pri_size_t)(h->stat.dist[i]), (double)h->stat.dist[i] / h->nentries * 100);
		}
	}
	fprintf(stream, "#)");
}

static hashval_t
nsxhash(size, p, k)
hashval_t size;
char const *p;
int k;
{
	size_t l = strlen(p);

	return (cshash((unsigned char const *)p, l, k) + k) % size;
}

static uint32_t
cshash(p, l, k)
unsigned char const *p;
size_t l;
{
#include "pptbls.h"
#define	MASK	0xffffffff
        uint32_t r;
	size_t i;
	uint32_t *table;

	if (0 <= k && k < nelems(tbls)) {
		table = tbls[k];
	}
	else {
		table = tbls[0];
	}
	r = MASK;
	for (i = 0; i < l; i++) {
		uint32_t d = p[i] & 0xff;
		uint32_t v;
		r ^= d;
		v = table[r & 0xff];
		r >>= 8;
		r ^= v;
	}
	return ~r & MASK;
}

int
nit_drop(path)
char const *path;
{
	return nht_drop(path);
}

struct nit *
nit_open(path, flags)
char const *path;
mode_t flags;
{
	size_t *e;
	char ipath[MAXPATHLEN], dpath[MAXPATHLEN];
	struct nit *h;

	if (!(h = calloc(1, sizeof *h))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}
	h->rdonly = (flags & O_ACCMODE) == O_RDONLY;

	snprintf(ipath, sizeof ipath, "%s.i", path);
	if (!(h->ix.vt = nvmt_open(ipath, sizeof (fwptr_t), 2, flags))) {
		syslog(LOG_DEBUG, "nvmt_open failed: %s", ipath);
		free(h);
		return NULL;
	}
	h->ix.size = nVMTxend(h->ix.vt);

	snprintf(dpath, sizeof dpath, "%s.d", path);
	if (!(h->fw.vt = nvmt_open(dpath, 1, 2, flags))) {
		syslog(LOG_DEBUG, "nvmt_open failed: %s", dpath);
		free(h);
		return NULL;
	}
	h->fw.offs = nVMTxend(h->fw.vt);
	h->seq_ptr = 0;

	if (!(e = it_cell0(h))) {
		syslog(LOG_DEBUG, "it_cell0 failed");
		free(h);
		return NULL;
	}
	h->nentries = *e;

	return h;
}

int
nit_close(h)
struct nit *h;
{
	size_t *e;

	if (!h->rdonly) {
		if (!(e = it_cell0(h))) {
			free(h);
			return -1;
		}
		*e = h->nentries;
	}
	nvmt_close(h->ix.vt);
	nvmt_close(h->fw.vt);
	free(h);
	return 0;
}

void *
nit_enter(h, i, ptr, size)
struct nit *h;
hashval_t i;
void *ptr;
size_t size;
{
	fwptr_t *e;
	struct ite *vv;

	if (h->rdonly) {
		return NULL;
	}

	if (h->ix.size <= i) {
		if (it_extend(h, i) == -1) {
			return NULL;
		}
	}

	e = nVMTa(h->ix.vt, i, fwptr_t);

	if (*e) {
		/* update */
		vv = (struct ite *)nVMTa(h->fw.vt, *e, char);
		if (vv->size == size) {
			memmove(vv->data, ptr, size);
			return vv->data;
		}
		else {
/*XXX			nvmt_dealloc(h->fw.vt, *e, vv->size);*/
			h->nentries--;
			goto new;
		}
	}
	else {
		/* new entry */
		size_t offs, end;
	new:
		if (!(offs = nvmt_alloc(h->fw.vt, h->fw.offs, offsetof(struct ite, data[size]), &end, 0))) {
			syslog(LOG_DEBUG, "nvmt_alloc failed");
			return NULL;
		}
		h->fw.offs = end;
		h->nentries++;
		*e = offs;
		vv = (struct ite *)nVMTa(h->fw.vt, *e, char);
		vv->size = size;
		memmove(vv->data, ptr, size);
#if defined DBGxx
 {char buf1[1024];
syslog(LOG_DEBUG, "NIT_ENTER(%p): i=%"PRIuHASHVAL_T" e=%p e->u=%"PRIuSIZE_T" nentries=%"PRIuHASHVAL_T" size=%"PRIuHASHVAL_T" ptr=[%s].%"PRIuSIZE_T, h, i, e, (pri_size_t)*e, h->nentries, h->ix.size, dbg_r(ptr, size, buf1, sizeof buf1), (pri_size_t)size); }
#endif
		return vv->data;
	}
}

void *
nit_retrieve(h, i, sizep)
struct nit *h;
hashval_t i;
size_t *sizep;
{
	fwptr_t *e;
	struct ite *vv;

	if (h->ix.size <= i) {
#if defined DBGxx
syslog(LOG_DEBUG, "NIT_RETRIEVE(%p): i=%"PRIuHASHVAL_T" NULL : OUT OF RANGE (h->ix.size = %"PRIuHASHVAL_T")", h, i, h->ix.size);
#endif
		return NULL;
	}

	e = nVMTa(h->ix.vt, i, fwptr_t);

#if defined DBGxx
syslog(LOG_DEBUG, "NIT_RETRIEVE(%p): i=%"PRIuHASHVAL_T" e=%p U=%"PRIuSIZE_T, h, i, e, (pri_size_t)*e);
#endif
	if (!*e) {	/* empty */
		return NULL;
	}

	vv = (struct ite *)nVMTa(h->fw.vt, *e, char);
	*sizep = vv->size;
	return vv->data;
}

void *
nit_seq(h, key, sizep)
struct nit *h;
hashval_t *key;
size_t *sizep;
{
	hashval_t i;

	for (i = h->seq_ptr; i < h->ix.size; i++) {
		fwptr_t *e;
		assert(nVMTxend(h->ix.vt) > i);
		e = nVMTa(h->ix.vt, i, fwptr_t);
		if (*e) {
			struct ite *vv;
			h->seq_ptr = i + 1;
#if defined DBGxx
syslog(LOG_DEBUG, "NIT_SEQ(%p): i=%"PRIuHASHVAL_T" e=%p U=%"PRIuSIZE_T, h, i, e, (pri_size_t)*e);
#endif
			*key = i;
			vv = (struct ite *)nVMTa(h->fw.vt, *e, char);
			*sizep = vv->size;
			return vv->data;
		}
#if defined DBGxx
syslog(LOG_DEBUG, "NIT_SEQ(%p): i=%"PRIuHASHVAL_T" NULL", h, i);
#endif
	}
	h->seq_ptr = i;
	return NULL;
}

int
nit_delete(h, i)
struct nit *h;
hashval_t i;
{
	fwptr_t *e;

	if (h->rdonly) {
		return -1;
	}

	if (h->ix.size <= i) {
#if defined DBG
syslog(LOG_DEBUG, "NIT_DELETE(%p): i=%"PRIuHASHVAL_T" NULL : OUT OF RANGE", h, i);
#endif
		return -1;
	}

	e = nVMTa(h->ix.vt, i, fwptr_t);

#if defined DBGxx
syslog(LOG_DEBUG, "NIT_DELETE(%p): i=%"PRIuHASHVAL_T" e=%p U=%"PRIuSIZE_T, h, i, e, (pri_size_t)*e);
#endif
	if (!*e) {	/* empty */
		return -1;
	}

	assert(h->nentries > 0);
	h->nentries--;

	*e = 0;
	return 0;
}

void
nit_rewind(h)
struct nit *h;
{
	h->seq_ptr = 0;
}

static int
it_extend(h, i)
struct nit *h;
hashval_t i;
{
	fwptr_t *p;
	hashval_t j;
	hashval_t osize, newsize;

	osize = h->ix.size;

#if defined DBGxx
syslog(LOG_DEBUG, "NIT_EXTEND(%p): i=%"PRIuHASHVAL_T, h, i);
#endif
	assert(nVMTxend(h->ix.vt) == osize);
	newsize = i + IT_GROWSZ;
	assert(i < newsize);
	if (nvmt_extend(h->ix.vt, newsize, 0) == -1) {
		return -1;
	}
	assert(nVMTxend(h->ix.vt) == newsize);
#if defined DBGxx
syslog(LOG_DEBUG, "NIT_EXTEND(%p): FILL_ZERO: %"PRIuHASHVAL_T" -- %"PRIuHASHVAL_T, h, osize, newsize);
#endif
	for (j = osize; j < newsize; j++) {
		p = nVMTa(h->ix.vt, j, fwptr_t);
		*p = 0;		/* fw.vt[0] is occupied by a nentries cell, so we can use 0 as an empty marker. see also it_cell0 */
	}
	h->ix.size = newsize;
	return 0;
}

/*
 * NOTE: offset 0 points nentries cell.
 * It is also used as an empty marker.
 */
static size_t *
it_cell0(h)
struct nit *h;
{
	size_t *e;

	if (nVMTMMAPEDP(h->fw.vt, 0)) {
		assert(h->fw.vt->map.bases[0]);
		e = (size_t *)nVMTa(h->fw.vt, 0, char);
	}
	else {
		size_t end = 0;
		if (h->rdonly) {
			syslog(LOG_DEBUG, "nvmt_alloc failed: readonly");
			return NULL;
		}
		if (nvmt_alloc(h->fw.vt, 0, sizeof *e, &end, 0) != 0 || end == 0) {
			syslog(LOG_DEBUG, "nvmt_alloc failed");
			return NULL;
		}
		h->fw.offs = end;
		e = (size_t *)nVMTa(h->fw.vt, 0, char);
		*e = 0;
	}
	return e;
}

#if defined DBG
static char *
dbg_r(ptr, size, buf, bufsize)
void *ptr;
size_t size, bufsize;
char *buf;
{
	size_t i, j;
	if (bufsize == 0) return "";
	for (i = j = 0; i < size; i++) {
		char s[4];
		size_t l;
		snprintf(s, sizeof s, "%02x ", ((unsigned char *)ptr)[i] & 0xff);
		l = strlen(s);
		if (j + l + 1 < bufsize) {
			memmove(&buf[j], s, l);
			j += l;
		}
		else {
			break;
		}
	}
	buf[j] = '\0';
	return buf;
}
#endif
