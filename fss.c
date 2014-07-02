/*	$Id: fss.c,v 1.56 2010/11/05 02:53:23 nis Exp $	*/

/*-
 * Copyright (c) 2006 Shingo Nishioka.
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
static char rcsid[] = "$Id: fss.c,v 1.56 2010/11/05 02:53:23 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/time.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#if defined XFSS_NCPU
#include <pthread.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>

#if defined FLWD_UTF32
#include <unicode/utf.h>
#include <unicode/utf8.h>
#endif

#include "nwam.h"
#include "util.h"
#include "pi.h"

#include "xstrcmpP.h"
#include "fssP.h"
#include "nwamP.h"
#include "xgserv.h"

#include <gifnoc.h>

#define DBG	1

#if defined ENABLE_GETA
#define	DWAM	"/dwam"		/* XXX: a clone is in stp.c */
#endif

struct fss_frag {
	struct idx_hdr const *header;

	struct {
		struct mapfile_t *m;
		char const *ptr;
	} flwd;

	struct {
		struct mapfile_t *m;
		void const *ptr;
		struct ptr_u const *p;
	} idx;

	struct {
#if defined ENABLE_GETA
		char path[MAXPATHLEN];
#endif
		struct mapfile_t *m;
		char const *ptr;
	} bitmap;
};

struct _fss {
	int n;	/* # of fragments */
	struct fss_frag p[MAXNFSSFRG]; 	/* fragments */
#if ! defined ENABLE_GETA
	struct fss_paths *paths;
#endif
#if defined FLWD_UTF32
	struct {
		char *ptr;
		/*size_t size;*/
	} gets_buf;
#endif
};

struct xkey {
	char const *pattern;
	size_t len;
	char const *flwd_ptr;
	xsc_opt_t options;
};

struct acc {
#if defined XFSS_NCPU
	pthread_rwlock_t *lock;
#endif
	struct art *arts;
	size_t artssize;
	df_t narts;
};

struct worker_arg {
	struct fss_simple_query *query;
	struct fss_frag *fp;
	struct xkey *key;
	struct acc *acc;
	size_t left, right;
#if defined XFSS_NCPU
	size_t cachesize;
#endif
};

static int x_simple_query(FSS *, struct fss_simple_query *);
static int x_simple_query_fragment(void *);
#if defined XFSS_NCPU
static int x_simple_query_fragment_pt(void *);
static void *x_simple_query_fragment_wlock(void *);
static void *eject(struct art *, size_t, struct acc *);
#endif
static struct flwd_idx const *seek_idx(struct fss_frag *, size_t, size_t *);
static int xcompar(void const *, void const *);
static int acompar(void const *, void const *);
static int uniq_qq(struct fss_simple_query *);
static int join_qq(struct fss_simple_query *, struct fss_simple_query *, struct fss_simple_query *);
static int join_qqPN(struct fss_simple_query *, struct fss_simple_query *, struct fss_simple_query *);
static int union_qq(struct fss_simple_query *, struct fss_simple_query *, struct fss_simple_query *);
static int fill_dummy(struct fss_simple_query *);
static void printdat(char const *, idx_t, unsigned int, size_t, FILE *);
#if defined FLWD_UTF32
static void xputcu4(unsigned int, FILE *);
#else
static void xputc(int, FILE *);
#endif
#if defined FLWD_UTF32
char *utf8toutf32(char const *, size_t *);
char *utf32toutf8(char const *);
#endif

#if ! defined ENABLE_GETA
static int xfss(FSS *, struct fss_query *, struct fss_simple_query *, struct fss_simple_query *);
static int fss_dump(FSS *, FILE *);
static ssize_t fss_gets(FSS *, idx_t, unsigned int, int, uint32_t, char const **);
#endif

/* NOTE: 

when the result is empty
 x_simple_query: returns NULL, 0
 arts2idvec, uniq_qq, join_qq, join_qqPN, union_qq:  return dummy pointer, 0
 arts2syminfo: causes ERROR

 xfss: returns dummy pointer, 0 (if query is not empty)   NULL, 0 (otherwise)

*/

FSS *
fss_open(
#if ! defined ENABLE_GETA
paths, fss_j
#else
getaroot, base
#endif
)
#if ! defined ENABLE_GETA
struct fss_paths *paths;
size_t fss_j;
#else
char const *getaroot, *base;
#endif
{
	int m;
#if ! defined ENABLE_GETA
	char *flwd, *idx, *bitmap;
#else
	char dataroot[MAXPATHLEN];
	char flwd[MAXPATHLEN];
	char idx[MAXPATHLEN];
	char *bitmap;
#endif
	FSS *f;
	struct fss_frag *fp;

	if (!(f = calloc(1, sizeof *f))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}

#if ! defined ENABLE_GETA
	f->paths = paths;
#else
	if (index(base, '/')) {
		fprintf(stderr, "invalid base\n");
		return NULL;
	}
#endif

#if ! defined ENABLE_GETA
	f->n = fss_j;
	for (m = 0; m < f->n; m++) {
		fp = &f->p[m];

		flwd = f->paths[m].flwd;
		idx = f->paths[m].idx;
		bitmap = f->paths[m].bitmap;

		if (!(fp->flwd.m = mapfile(flwd))) {
			return NULL;
		}
		fp->flwd.ptr = fp->flwd.m->ptr;

		if (!(fp->idx.m = mapfile(idx))) {
			return NULL;
		}
		fp->idx.ptr = fp->idx.m->ptr;

		if (!(fp->bitmap.m = mapfile(bitmap))) {
			return NULL;
		}
		fp->bitmap.ptr = fp->bitmap.m->ptr;

		fp->header = fp->idx.ptr;
		fp->idx.p = (struct ptr_u const *)((char *)fp->idx.ptr + sizeof *fp->header);
	}
#else
	f->n = 1;
	m = 0;
	fp = &f->p[m];
	snprintf(dataroot, sizeof dataroot, "%s" DWAM "/%s", getaroot, base);
	snprintf(flwd, sizeof flwd, "/%s/@.flwd", dataroot);
	snprintf(idx, sizeof idx, "/%s/@.idx", dataroot);
	snprintf(fp->bitmap.path, sizeof fp->bitmap.path, "/%s/@.map", dataroot);
	bitmap = fp->bitmap.path;

	if (!(fp->flwd.m = mapfile(flwd))) {
		return NULL;
	}
	fp->flwd.ptr = fp->flwd.m->ptr;

	if (!(fp->idx.m = mapfile(idx))) {
		return NULL;
	}
	fp->idx.ptr = fp->idx.m->ptr;

	if (!(fp->bitmap.m = mapfile(bitmap))) {
		return NULL;
	}
	fp->bitmap.ptr = fp->bitmap.m->ptr;

	fp->header = fp->idx.ptr;
	fp->idx.p = (struct ptr_u const *)((char *)fp->idx.ptr + sizeof *fp->header);
#endif

#if defined FLWD_UTF32
	f->gets_buf.ptr = NULL;
#endif

	return f;
}

int
fss_close(f)
FSS *f;
{
	int m;
	for (m = 0; m < f->n; m++) {
		struct fss_frag *fp = &f->p[m];
		mapfile_destroy(fp->flwd.m);
		mapfile_destroy(fp->idx.m);
		mapfile_destroy(fp->bitmap.m);
	}
#if defined FLWD_UTF32
	free(f->gets_buf.ptr);
#endif
	free(f);
	return 0;
}

static int
x_simple_query(f, query)
FSS *f;
struct fss_simple_query *query;
{
	xsc_opt_t disabled;
	struct xkey key;
	size_t left, right;
	int m;
	struct acc acc0;

#if defined XFSS_NCPU
	acc0.lock = NULL;
#endif
	acc0.arts = NULL;
	acc0.artssize = 0;
	acc0.narts = 0;

#if defined FLWD_UTF32
	if (!(key.pattern = utf8toutf32(query->pattern, &key.len))) {
		return -1;
	}
#else
	key.pattern = query->pattern;
	key.len = strlen(query->pattern);
#endif

	for (m = 0; m < f->n; m++) {
		struct fss_frag *fp = &f->p[m];
		if (disabled = query->options & ~fp->header->xsc_options) {
			fprintf(stderr, "warning: disabled options: %08x\n", disabled);
		}

		key.flwd_ptr = fp->flwd.ptr;
		key.options = fp->header->xsc_options & ~POPTIONS_MASK;

		if (brsearch(&key, fp->idx.p, fp->header->nentries, sizeof *fp->idx.p, xcompar, &left, &right) != -1) {
			struct worker_arg arg;
			arg.query = query;
			arg.fp = fp;
			arg.key = &key;
			arg.acc = &acc0;
			arg.left = left;
			arg.right = right;
#if defined XFSS_NCPU
			if (right - left > 512) {
				if (x_simple_query_fragment_pt(&arg) == -1) {
#if defined FLWD_UTF32
					free(key.pattern);
					return -1;
#endif
				}
			} else
#endif
			if (x_simple_query_fragment(&arg) == -1) {
#if defined FLWD_UTF32
				free(key.pattern);
#endif
				return -1;
			}
		}
	}

	query->narts = acc0.narts;
	query->arts = acc0.arts;

	qsort(query->arts, query->narts, sizeof *query->arts, acompar);
#if defined FLWD_UTF32
	free(key.pattern);
#endif

	return 0;
}

static int
x_simple_query_fragment(cookie)
void *cookie;
{
	struct worker_arg *ap = cookie;
	struct fss_simple_query *query;
	struct fss_frag *fp;
	struct xkey *key;
	struct acc *acc;
	size_t left, right, j;
/*fprintf(stderr, "@");*/
	query = ap->query;
	fp = ap->fp;
	key = ap->key;
	acc = ap->acc;
	left = ap->left;
	right = ap->right;
	for (j = left; j < right; j++) {
		size_t p;
		char const *q;
		size_t disp;
		struct flwd_idx const *i;
		p = fp->idx.p[j].p;
		q = &fp->flwd.ptr[p];
		i = seek_idx(fp, p, &disp);
		if (i->id && CHKSEG(i->segid, query->segments) && xstrncmp(key->pattern, q, key->len, query->options) == 0) {
			struct art *a;
			NREALLOC1(acc->arts, acc->artssize, acc->narts, 32, syslog(LOG_DEBUG, "realloc: %s", strerror(errno)); return -1;);
			a = &acc->arts[acc->narts++];
			a->id = i->id;
			a->segid = i->segid;
			a->disp = disp;
			a->q = &fp->flwd.ptr[p];	/* for debug */
		}
	}
/*fprintf(stderr, "#");*/
	return 0;
}

#if defined XFSS_NCPU
static int
x_simple_query_fragment_pt(cookie)
void *cookie;
{
	pthread_t *threads = NULL;
	struct worker_arg *worker_args = NULL, *ap0;
	void *value;
	size_t total;
	pthread_rwlock_t lock;
	size_t nthreads = XFSS_NCPU;
	size_t n, i, j;
	int e = 0;

	ap0 = cookie;
#define ERR(v)	e = (v); goto end;
	if (!(threads = calloc(nthreads, sizeof *threads))) {
		fprintf(stderr, "error: %s\n", strerror(errno));
		ERR(-1);
	}
	if (!(worker_args = calloc(nthreads, sizeof *worker_args))) {
		fprintf(stderr, "error: %s\n", strerror(errno));
		ERR(-1);
	}

	total = ap0->right - ap0->left;
	n = (total + nthreads - 1) / nthreads;
	for (i = 0, j = ap0->left; i < nthreads; i++) {
		struct worker_arg *ap;
		ap = &worker_args[i];
		*ap = *ap0;
		ap->left = j;
		j += n;
		if (ap0->right < j) {
			j = ap0->right;
		}
		ap->right = j;
		ap->cachesize= MIN(8192, ap->right - ap->left);
	}
	if (pthread_rwlock_init(&lock, NULL)) {
		fprintf(stderr, "pthread_rwlock_init: %s", strerror(errno));
		ERR(-1);
	}
#undef ERR

	ap0->acc->lock = &lock;

	for (i = 0; i < nthreads; i++) {
		struct worker_arg *ap;
		ap = &worker_args[i];
		if (pthread_create(&threads[i], NULL, x_simple_query_fragment_wlock, ap)) {
			fprintf(stderr, "pthread_create: %s", strerror(errno));
			e = -1;
			nthreads = i;
			break;
		}
	}

	for (i = 0; i < nthreads; i++) {
		if (pthread_join(threads[i], &value)) {
			fprintf(stderr, "pthread_join: %s", strerror(errno));
			e = -1;
		}
		if (value) {
			fprintf(stderr, "error: %s", (char *)value);
			e = -1;
		}
	}

	if (pthread_rwlock_destroy(&lock)) {
		fprintf(stderr, "pthread_rwlock_destroy: %s", strerror(errno));
		e = -1;
	}
end:

	free(threads);
	free(worker_args);
	return e;
}

static void *
x_simple_query_fragment_wlock(cookie)
void *cookie;
{
	struct worker_arg *ap = cookie;
	struct fss_simple_query *query;
	struct fss_frag *fp;
	struct xkey *key;
	struct acc *acc;
	size_t left, right, j;
	struct art *cache;
	size_t cachesize, ncache;
	void *e = NULL;
	cachesize = ap->cachesize;
/*fprintf(stderr, "@");*/
	query = ap->query;
	fp = ap->fp;
	key = ap->key;
	acc = ap->acc;
	left = ap->left;
	right = ap->right;
	if (!(cache = calloc(cachesize, sizeof *cache))) {
		e = "calloc failed";
		goto end;
	}
	ncache = 0;
	for (j = left; j < right; j++) {
		size_t p;
		char const *q;
		size_t disp;
		struct flwd_idx const *i;
		p = fp->idx.p[j].p;
		q = &fp->flwd.ptr[p];
		i = seek_idx(fp, p, &disp);
		if (i->id && CHKSEG(i->segid, query->segments) && xstrncmp(key->pattern, q, key->len, query->options) == 0) {
			struct art *a;
			if (cachesize <= ncache) {
				if (e = eject(cache, ncache, acc)) {
					goto end;
				}
				ncache = 0;
			}
			a = &cache[ncache++];
			a->id = i->id;
			a->segid = i->segid;
			a->disp = disp;
			a->q = &fp->flwd.ptr[p];	/* for debug */
		}
	}
/*fprintf(stderr, "#");*/
	if (ncache) {
		if (e = eject(cache, ncache, acc)) {
			goto end;
		}
	}
end:
	free(cache);
	return e;
}

static void *
eject(a, na, acc)
struct art *a;
size_t na;
struct acc *acc;
{
	pthread_rwlock_wrlock(acc->lock);
	NREALLOC0(acc->arts, acc->artssize, acc->narts + na, 32, fprintf(stderr, "realloc: %s", strerror(errno)); return "realloc failed";);
	memcpy(&acc->arts[acc->narts], a, na * sizeof *a);
	acc->narts += na;
	pthread_rwlock_unlock(acc->lock);
	return NULL;
}
#endif

#if defined FLWD_UTF32
static struct flwd_idx const *
seek_idx(f, p, dispp)
struct fss_frag *f;
size_t p, *dispp;
{
	size_t p1 = p;

	while (p > sizeof (struct flwd_idx) && *(UChar32 *)&f->flwd.ptr[p]) {
		unsigned int k;
		k = *(UChar32 *)&f->flwd.ptr[p] >> 24 & 0xff;
		p -= k * 4;
	}
	if (dispp) {
		*dispp = p1 - (p + 4);
	}
	return (struct flwd_idx const *)&f->flwd.ptr[p - sizeof (struct flwd_idx)];
}
#else
static struct flwd_idx const *
seek_idx(f, p, dispp)
struct fss_frag *f;
size_t p, *dispp;
{
	size_t frame, bitmap_idx, p0;
	int bitmap_sft;
	unsigned char mask;
	size_t p1 = p;
#define	HDRSZ	(sizeof (struct flwd_idx))
#if defined DBG
	size_t q = p, o;

	for (; q > HDRSZ && f->flwd.ptr[q]; q--) ;
	o = q - HDRSZ;
#endif

	frame = p / bytes_per_frame;
	bitmap_idx = frame / 8;
	bitmap_sft = frame % 8;
	mask = 1 << bitmap_sft;
	if (frame == 0 || (f->bitmap.ptr[bitmap_idx] & mask)) {
		p0 = frame == 0 ? HDRSZ - 1 : frame * bytes_per_frame - 1;
		for (; p > p0 && f->flwd.ptr[p]; p--) ;
		if (p > p0) {
#if defined DBG
			assert(p - HDRSZ == o);
#endif
			goto end;
		}
		if (frame == 0) {
			fprintf(stderr, "no section marker found (1)\n");
			abort();
		}
	}
	frame--;
	if ((mask >>= 1) == 0) bitmap_idx--, mask = 0x80;
	while (frame > 0 && !(f->bitmap.ptr[bitmap_idx] & mask)) {
		if (!f->bitmap.ptr[bitmap_idx]) {
			bitmap_idx--, mask = 0x80;
			frame = (bitmap_idx + 1) * 8 - 1;
		}
		else {
			if ((mask >>= 1) == 0) bitmap_idx--, mask = 0x80;
			frame--;
		}
		assert(!(bitmap_idx == 0 && mask == 1) || frame == 0);
	}
	assert(frame != 0 || bitmap_idx == 0 && mask == 1 && (f->bitmap.ptr[bitmap_idx] & mask));
	p = (frame + 1) * bytes_per_frame - 1;
	p0 = frame == 0 ? HDRSZ - 1 : frame * bytes_per_frame - 1;
	for (; p > p0 && f->flwd.ptr[p]; p--) ;
	if (p > p0) {
#if defined DBG
		assert(p - HDRSZ == o);
#endif
		goto end;
	}
	fprintf(stderr, "no section marker found (2)\n");
	abort();
	/* NOTREACHED */

end:
	if (dispp) {
		*dispp = p1 - (p + 1);
	}
	return (struct flwd_idx const *)&f->flwd.ptr[p - HDRSZ];
#undef HDRSZ
}
#endif

static int
xcompar(va, vb)
void const *va;
void const *vb;
{
	struct xkey *a;
	struct ptr_u *b;

	a = (struct xkey *)va;
	b = (struct ptr_u *)vb;
	return xstrncmp(a->pattern, a->flwd_ptr + b->p, a->len, a->options);
}

static int
acompar(va, vb)
void const *va;
void const *vb;
{
	struct art const *a;
	struct art const *b;

	a = (struct art const *)va;
	b = (struct art const *)vb;
	if (a->id < b->id) {
		return -1;
	}
	else if (a->id > b->id) {
		return 1;
	}
	else if (a->segid < b->segid) {
		return -1;
	}
	else if (a->segid > b->segid) {
		return 1;
	}
	else if (a->disp < b->disp) {
		return -1;
	}
	else if (a->disp > b->disp) {
		return 1;
	}
	return 0;
}

int
parse_segs(s0, seg)
char const *s0;
fss_seg_t *seg;
{
	char *v[MAXSEGID];
	size_t n, i;
	char *s;
	*seg = 0;
	if (!(s = strdup(s0))) {
		return -1;
	}
	n = splitv(s, ',', v, nelems(v), 0);
	for (i = 0; i < n; i++) {
		unsigned int f, t, j;
		char *p = v[i], *end;
		if (!isdigit(*p & 0xff)) {
			free(s);
			return -1;
		}
		f = strtoul(p, &end, 10);
		if (!*end) {
			t = f;
		}
		else if (*end == '-') {
			p = end + 1;
			t = strtoul(p, &end, 10);
			if (!(*p && !*end)) {
				free(s);
				return -1;
			}
		}
		else {
			free(s);
			return -1;
		}
		if (t < f || MAXSEGID < t) {
			free(s);
			return -1;
		}
		for (j = f; j <= t; j++) {
			SETSEG(*seg, j);
		}
	}
	free(s);
	return 0;
}

static char *xsc_opt_names[] = XSC_OPT_NAMES;

int
parse_opts(s0, opt)
char const *s0;
xsc_opt_t *opt;
{
	char *v[nelems(xsc_opt_names)];
	char *s;
	size_t n, i;
	*opt = 0;
	if (!(s = strdup(s0))) {
		return -1;
	}
	n = splitv(s, ',', v, nelems(v), 0);
	for (i = 0; i < n; i++) {
		size_t j;
		char *p = v[i];
		for (j = 0; j < nelems(xsc_opt_names); j++) {
			if (!strcasecmp(p, xsc_opt_names[j])) {
				*opt |= 1 << j;
				break;
			}
		}
		if (j == nelems(xsc_opt_names)) {
			fprintf(stderr, "unknown option: %s\n", p);
			free(s);
			return -1;
		}
	}
	free(s);
	return 0;
}

#if ! defined ENABLE_GETA
int
wam_xfss(w, query, rp, rn)
WAM *w;
struct fss_query *query;
struct fss_simple_query *rp, *rn;
{
	WAM *master;

	query->pa = NULL;
	query->na = NULL;
	assert(w->type == NWAM_D_TYPE_FSS);
	master = w->u.fss.master;
	if (master && master->u.w.dist.role == NDWAM_ROLE_PARENT) {
		if (cb_xfss(master->u.w.xs, query, rp, rn, w->u.w.allowerror, &w->u.w.may_incomplete) == -1) {
			return -1;
		}
		qsort(rp->arts, rp->narts, sizeof *rp->arts, acompar);
		qsort(rn->arts, rn->narts, sizeof *rn->arts, acompar);
		return 0;
	}
	return xfss(w->u.fss.f, query, rp, rn);
}
#endif

#if ! defined ENABLE_GETA
static
#endif
int
xfss(f, query, rp, rn)
FSS *f;
struct fss_query *query;
struct fss_simple_query *rp, *rn;
{
	df_t i;
	struct fss_simple_query x, z;

	query->pa = NULL;
	query->na = NULL;

	bzero(&x, sizeof x);
	bzero(&z, sizeof z);
	x.arts = z.arts = NULL;

	for (i = 0; i < query->nq; i++) {
		struct fss_simple_query *u;
		struct fss_con_query *qi = &query->query[i];
		df_t j;
/* fprintf(stderr, "qi->n == %d\n", qi->n); */
		struct fss_simple_query y;
		y.arts = NULL;
		for (j = 0; j < qi->n; j++) {
			struct fss_simple_query *qij = &qi->q[j];
/* fprintf(stderr, "pattern: %s\n", qij->pattern); */
/* fprintf(stderr, "options: %08x\n", qij->options); */
/* fprintf(stderr, "segments: %08x\n", qij->segments); */
			if (x_simple_query(f, qij) == -1) {
				fprintf(stderr, "x_simple_query failed\n");
				return -1;
			}
			if (uniq_qq(qij) == -1) {
				return -1;
			}
			if (!y.arts) {
				y = *qij;
				qij->arts = NULL;
			}
			else {
				int e;
				struct fss_simple_query t;
				if (!y.negativep && !qij->negativep) {
					e = join_qq(&t, &y, qij);
				}
				else if (y.negativep && qij->negativep) {
					e = union_qq(&t, &y, qij);
				}
				else if (y.negativep) {
					assert(!qij->negativep);
					e = join_qqPN(&t, qij, &y);
				}
				else {
					assert(!y.negativep);
					assert(qij->negativep);
					e = join_qqPN(&t, &y, qij);
				}
				if (e == -1) {
					fprintf(stderr, "join failed\n");
					return -1;
				}
				free(y.arts), y.arts = NULL;
				free(qij->arts), qij->arts = NULL;
				y = t;
			}
/* fprintf(stderr, "narts = %d\n", qij->narts); */
		}
		u = y.negativep ? &z : &x;
		if (!u->arts) {
			*u = y;
			y.arts = NULL;
		}
		else {
			int e;
			struct fss_simple_query t;
			if (y.negativep) {
				e = join_qq(&t, u, &y);
			}
			else {
				e = union_qq(&t, u, &y);
			}
			if (e == -1) {
				fprintf(stderr, "union failed\n");
				return -1;
			}
			free(u->arts), u->arts = NULL;
			free(y.arts), y.arts = NULL;
			*u = t;
		}
	}

	x.total = x.narts;
	z.total = z.narts;
	if (rp->narts != -1) {
		x.narts = MIN(x.narts, rp->narts);
	}
	if (rn->narts != -1) {
		z.narts = MIN(z.narts, rn->narts);
	}

	query->pa = x.arts;
	query->na = z.arts;

	*rp = x;
	*rn = z;
	return 0;
}

struct syminfo *
arts2syminfo(arts, n)
struct art *arts;
df_t n;
{
	struct syminfo *r;
	df_t k;

	if (!(r = calloc(n, sizeof *r))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}
	for (k = 0; k < n; k++) {
		struct syminfo *a = &r[k];
		struct art *xa = &arts[k];
		a->id = xa->id;
		a->DF = a->DF_d = 0;
		a->TF = a->TF_d = 0;
		a->weight = 1;
		a->v = NULL;
#if defined ENABLE_GETA
		a->attr = a->sel_score = 0;
#endif
	}
	return r;
}

idx_t *
arts2idvec(arts, n)
struct art *arts;
df_t n;
{
	idx_t *r;
	df_t k;

	if (!(r = calloc(n + 1, sizeof *r))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}
	for (k = 0; k < n; k++) {
		r[k] = arts[k].id;
	}
	return r;
}

static int
uniq_qq(q)
struct fss_simple_query *q;
{
	df_t n, i, j, k;
	struct art *a;
	a = q->arts;
	n = q->narts;
	for (i = k = 0; i < n; i = j) {
		a[k++] = a[i];
		for (j = i + 1; j < n && a[i].id == a[j].id; j++) ;
	}
	q->narts = k;
	return fill_dummy(q);
}

/* on success, arts will be non NULL, even though narts is 0.
   join_qqPN's behaviour is same. */
static int
join_qq(rp, q0, q1)
struct fss_simple_query *rp, *q0, *q1;
{
	df_t n0, n1, i, j;
	struct art *a0, *a1;
	struct art *r;
	df_t rsize, nr;
/*	fprintf(stderr, "join: %d %d\n", q0->narts, q1->narts); */
	assert(q0->negativep == q1->negativep);
	rsize = 8;
	if (!(r = malloc(rsize * sizeof *r))) {
		syslog(LOG_DEBUG, "malloc: %s", strerror(errno));
		return -1;
	}
	a0 = q0->arts;
	n0 = q0->narts;
	a1 = q1->arts;
	n1 = q1->narts;
	for (i = 0, j = 0, nr = 0; i < n0 && j < n1; ) {
		if (a0[i].id < a1[j].id) {
			i++;
		}
		else if (a0[i].id > a1[j].id) {
			j++;
		}
		else {
			if (rsize <= nr) {
				void *new;
				size_t newsize = (nr + 8) * 1.41;
				newsize = NA(newsize, 32);
				if (!(new = realloc(r, newsize * sizeof *r))) {
					syslog(LOG_DEBUG, "realloc: %s", strerror(errno));
					free(r);
					return -1;
				}
				rsize = newsize;
				r = new;
			}
			r[nr++] = a0[i];
			i++, j++;
		}
	}
	rp->negativep = q0->negativep;
	rp->arts = r;
	rp->narts = nr;
	return fill_dummy(rp);
}

static int
join_qqPN(rp, q0, q1)
struct fss_simple_query *rp, *q0, *q1;
{
	df_t n0, n1, i, j;
	struct art *a0, *a1;
	struct art *r;
	df_t rsize, nr, rest;
/*	fprintf(stderr, "join: %d %d\n", q0->narts, q1->narts); */
	assert(!q0->negativep);
	assert(q1->negativep);
	rsize = 8;
	if (!(r = malloc(rsize * sizeof *r))) {
		syslog(LOG_DEBUG, "malloc: %s", strerror(errno));
		return -1;
	}
	a0 = q0->arts;
	n0 = q0->narts;
	a1 = q1->arts;
	n1 = q1->narts;
	for (i = 0, j = 0, nr = 0; i < n0 && j < n1; ) {
		if (a0[i].id < a1[j].id) {
			if (rsize <= nr) {
				void *new;
				size_t newsize = (nr + 8) * 1.41;
				newsize = NA(newsize, 32);
				if (!(new = realloc(r, newsize * sizeof *r))) {
					syslog(LOG_DEBUG, "realloc: %s", strerror(errno));
					free(r);
					return -1;
				}
				rsize = newsize;
				r = new;
			}
			r[nr++] = a0[i];
			i++;
		}
		else if (a0[i].id > a1[j].id) {
			j++;
		}
		else {
			i++, j++;
		}
	}
	rest = n0 - i;
	if (rsize < nr + rest) {
		void *new;
		size_t newsize = nr + rest;
		if (!(new = realloc(r, newsize * sizeof *r))) {
			syslog(LOG_DEBUG, "realloc: %s", strerror(errno));
			return -1;
		}
		rsize = newsize;
		r = new;
	}
#if 1
	if (rest) {
		memcpy(&r[nr], &a0[i], rest * sizeof *r);
		nr += rest;
		/* i = n0; */
	}
#else
	for (; i < n0; ) {
		r[nr++] = a0[i++];
	}
#endif
	rp->negativep = 0;
	rp->arts = r;
	rp->narts = nr;
	return fill_dummy(rp);
}

static int
union_qq(rp, q0, q1)
struct fss_simple_query *rp, *q0, *q1;
{
	df_t n0, n1, i, j;
	struct art *a0, *a1;
	struct art *r = NULL;
	df_t rsize, nr, rest;
/*	fprintf(stderr, "union: %d %d\n", q0->narts, q1->narts); */
	assert(q0->negativep == q1->negativep);
	rsize = 8;
	if (!(r = malloc(rsize * sizeof *r))) {
		syslog(LOG_DEBUG, "malloc: %s", strerror(errno));
		return -1;
	}
	a0 = q0->arts;
	n0 = q0->narts;
	a1 = q1->arts;
	n1 = q1->narts;
	for (i = 0, j = 0, nr = 0; i < n0 && j < n1; ) {
		if (rsize <= nr) {
			void *new;
			size_t newsize = (nr + 8) * 1.41;
			newsize = NA(newsize, 32);
			if (!(new = realloc(r, newsize * sizeof *r))) {
				syslog(LOG_DEBUG, "realloc: %s", strerror(errno));
				free(r);
				return -1;
			}
			rsize = newsize;
			r = new;
		}
		if (a0[i].id < a1[j].id) {
			r[nr++] = a0[i++];
		}
		else if (a0[i].id > a1[j].id) {
			r[nr++] = a1[j++];
		}
		else {
			r[nr++] = a0[i];
			i++, j++;
		}
	}
	rest = (n0 - i) + (n1 - j);
	if (rsize < nr + rest) {
		void *new;
		size_t newsize = nr + rest;
		if (!(new = realloc(r, newsize * sizeof *r))) {
			syslog(LOG_DEBUG, "realloc: %s", strerror(errno));
			return -1;
		}
		rsize = newsize;
		r = new;
	}
#if 1
	if (rest = n0 - i) {
		memcpy(&r[nr], &a0[i], rest * sizeof *r);
		nr += rest;
		/* i = n0; */
	}
	if (rest = n1 - j) {
		memcpy(&r[nr], &a1[j], rest * sizeof *r);
		nr += rest;
		/* j = n1; */
	}
#else
	for (; i < n0; ) {
		r[nr++] = a0[i++];
	}
	for (; j < n1; ) {
		r[nr++] = a1[j++];
	}
#endif
	rp->negativep = q0->negativep;
	rp->arts = r;
	rp->narts = nr;
	return fill_dummy(rp);
}

static int
fill_dummy(q)
struct fss_simple_query *q;
{
	if (!q->arts && !q->narts) {
		if (!(q->arts = calloc(1, sizeof *q->arts))) {
			return -1;
		}
	}
	return 0;
}

#if ! defined ENABLE_GETA
int
wam_fss_dump(w, stream)
WAM *w;
FILE *stream;
{
	return fss_dump(w->u.fss.f, stream);
}
#endif

#if ! defined ENABLE_GETA
static
#endif
int
fss_dump(f, stream)
FSS *f;
FILE *stream;
{
	int m, j;
	for (m = 0; m < f->n; m++) {
		struct fss_frag *fp = &f->p[m];
		printf("fragment: %d\n", m);
		printf("# entries: %"PRIuSIZE_T"\n", (pri_size_t)fp->header->nentries);
		printf("options: %08x\n", fp->header->xsc_options);

		for (j = 0; j < fp->header->nentries; j++) {
			size_t p = fp->idx.p[j].p;
			char const *q = &fp->flwd.ptr[p];
			size_t disp;
			struct flwd_idx const *i = seek_idx(fp, p, &disp);
			printdat(q, i->id, i->segid, disp, stream);
		}
	}

	return 0;
}

static void
printdat(s, artid, segid, disp, stream)
char const *s;
idx_t artid;
unsigned int segid;
size_t disp;
FILE *stream;
{
	size_t i;

	fprintf(stream, "%"PRIuIDX_T"/%02u/%"PRIuSIZE_T": ", artid, segid, (pri_size_t)disp);
#if defined FLWD_UTF32
	if (s) {
		for (i = 0; i < 48 && ((unsigned int *)s)[i]; i++) {
			xputcu4(((unsigned int *)s)[i] & 0x1fffff, stream);
		}
	}
#else
	if (s) {
		for (i = 0; i < 48 && s[i]; i++) {
			xputc(s[i] & 0xff, stream);
		}
		for (; (s[i] & 0xc0) == 0x80; i++) {
			xputc(s[i] & 0xff, stream);
		}
	}
#endif
	fprintf(stream, "\n");
}

#if defined FLWD_UTF32
static void
xputcu4(c, stream)
unsigned int c;
FILE *stream;
{
	if (c == '\n') {
		fputs("\\n", stream);
	}
	else if (c == '\t') {
		fputs("\\t", stream);
	}
	else {
		UBool isError;
		int o;
		uint8_t u1[8];
		isError = 0, o = 0;
		U8_APPEND(u1, o, sizeof u1, c, isError);
		if (isError) memmove(u1, "?", 2); else u1[o] = '\0';
		fputs(u1, stream);
	}
}
#else

static void
xputc(c, stream)
FILE *stream;
{
	if (c == '\n') {
		fputs("\\n", stream);
	}
	else if (c == '\t') {
		fputs("\\t", stream);
	}
	else {
		putc(c, stream);
	}
}
#endif

void
free_fss_query(q)
struct fss_query *q;
{
	df_t i, j/*, k*/;
	for (i = 0; i < q->nq; i++) {
		struct fss_con_query *c;
		c = &q->query[i];
		for (j = 0; j < c->n; j++) {
			struct fss_simple_query *s;
			s = &c->q[j];
			free(s->pattern);
#if 0
			for (k = 0; k < s->narts; k++) {
				struct art *a;
				a = &s->arts[k];
				/* free(a->q); */
			}
#endif
			free(s->arts);
		}
		free(c->q);
	}
	free(q->pa);
	free(q->na);
	free(q->query);
}

#if defined FLWD_UTF32
char *
utf8toutf32(s, size)
char const *s;
size_t *size;
{
	size_t length = strlen(s);
	int32_t i, j;
	char *r;
	for (i = j = 0; i < length; j++) {
		UChar32 u;
		U8_NEXT(s, i, length, u);
		if (u < 0) {
			return NULL;
		}
	}
	if (!(r = malloc((j + 1) * 4))) {
		return NULL;
	}
	for (i = j = 0; i < length; j++) {
		UChar32 u;
		U8_NEXT(s, i, length, u);
		if (u < 0) {
			free(r);
			return NULL;
		}
		*(UChar32 *)&r[j * 4] = u;
	}
	*(UChar32 *)&r[j * 4] = 0;
	*size = j * 4;
	return r;
}

char *
utf32toutf8(s)
char const *s;
{
	char *q, *r;
	int o;
	size_t size;
	for (q = s; *(unsigned int *)q; q += 4) ;
	size = q - s + 1;	/* because utf32(1char) => utf8(1..4byte), `q - s + 1' is sufficient. */
	if (!(r = malloc(size))) {
		return NULL;
	}
	o = 0;
	for (q = s; *(unsigned int *)q; q += 4) {
		UChar32 c;
		UBool isError = 0;
		c = *(unsigned int *)q & 0x1fffff;
		U8_APPEND(r, o, size, c, isError);
		if (isError) {
			free(r);
			return NULL;
		}
	}
	if (o >= size) {
		free(r);
		return NULL;
	}
	r[o] = '\0';
	return r;
}
#endif

#if ! defined ENABLE_GETA
ssize_t
wam_fss_gets(w, id, segid, s)
WAM *w;
idx_t id;
unsigned int segid;
char const **s;
{
	WAM *master, *rmap;
	size_t size;
	struct fss_rmap_ent_t *rmap_e;

	assert(w->type == NWAM_D_TYPE_FSS);
	master = w->u.fss.master;
	if (master && master->u.w.dist.role == NDWAM_ROLE_PARENT) {
		if (!(*s = cb_wam_fss_gets(master->u.w.xs, id, segid))) {
			return -1;
		}
		return strlen(*s);
	}

	if (!(rmap = wam_prop_open(master, WAM_ROW, RMAPKEY))) {
		syslog(LOG_DEBUG, "%s: %s", RMAPKEY, strerror(errno));
		return 0;
	}

	switch (nwdb_get(rmap->u.p.pr, &id, &rmap_e, &size)) {
	case -1:
	case 1:
		syslog(LOG_DEBUG, "nwdb_get " RMAPKEY " failed");
		return -1;
	case 0:
		break;
	}
	return fss_gets(w->u.fss.f, id, segid, rmap_e->j, rmap_e->offset, s);
}
#endif

#if ! defined ENABLE_GETA
static ssize_t
fss_gets(f, id, segid, m, offset, s)
FSS *f;
idx_t id;
unsigned int segid;
uint32_t offset;
char const **s;
{
	struct fss_frag *fp = &f->p[m];
	char const *q;

/* fprintf(stderr, "%d %d\n", m, offset); */
/* fprintf(stderr, "fragment: %d\n", m); */
/* fprintf(stderr, "# entries: %"PRIuSIZE_T"\n", (pri_size_t)fp->header->nentries); */
/* fprintf(stderr, "options: %08x\n", fp->header->xsc_options); */

	for (q = &fp->flwd.ptr[offset]; q < fp->flwd.ptr + fp->flwd.m->size; ) {
		ssize_t l;
		struct flwd_idx const *i;
#if ! defined FLWD_UTF32
		size_t pad;
#endif
		i = (struct flwd_idx const *)q;

/* fprintf(stderr, "%p: %d.%d %d.%d\n", i, i->id, i->segid, id, segid); */
		if (i->id != id) {
/* fprintf(stderr, "mismatch!\n"); */
			return -1;
		}

		q += sizeof *i;
#if defined FLWD_UTF32
		q += 4;
#else
		q++;
#endif
		if (i->segid == segid) {
#if defined FLWD_UTF32
			free(f->gets_buf.ptr);
			if (!(f->gets_buf.ptr = utf32toutf8(q))) {
				return -1;
			}
			*s = f->gets_buf.ptr;
#else
			*s = q;
#endif
			return strlen(*s);
		}
#if defined FLWD_UTF32
		for (; *(unsigned int *)q; q += 4) ;
		q += 4;
#else
		for (; *q; q++) ;
		q++;
		l = q - (char *)i;
		pad = FWIDXALIGN - l % FWIDXALIGN;
		if (pad != FWIDXALIGN) {
			q += pad;
		}
#endif
	}

	return -1;
}
#else
ssize_t
fss_gets(f, id, segid, s)
FSS *f;
idx_t id;
unsigned int segid;
char const **s;
{
	return -1;	/* not available */
}
#endif
