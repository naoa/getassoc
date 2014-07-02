/*	$Id: xsort_pcompar.c,v 1.25 2011/09/14 02:36:16 nis Exp $	*/

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
static char rcsid[] = "$Id: xsort_pcompar.c,v 1.25 2011/09/14 02:36:16 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#if defined X_PTHREAD
#include <pthread.h>
#include <semaphore.h>
#endif

#include <unicode/utf.h>
#include <unicode/utf8.h>

#include "nwam.h"
#include "util.h"
#include "pi.h"

#include "xstrcmpP.h"
#include "fssP.h"

#include "sort_pcompar.h"

#include <gifnoc.h>

#define DBG	1

#define	SHOW_PI	1

struct segm {
	int d;
	FILE *f;
	size_t nmemb, size;
	char path[MAXPATHLEN];
#if defined X_FORK
	pid_t pid;
#endif
#if defined X_PTHREAD
	int processed;
#endif
};

#if defined X_PTHREAD
static void *start_routine(void *);
struct ck {
	sem_t sem;
	struct segm *segm;
	size_t nsegm;
};
#endif

#if defined X_FORK
static int waitone(struct segm *, size_t);
#endif

#if defined X_PTHREAD
static void *start_routine(void *);
#endif

static size_t init_sel(void);
static size_t select_segm(unsigned int);
static char *seg_info_r(size_t, char *, size_t);

static char const *g_base;
static xsc_opt_t xsc_options;

#if defined ENABLE_GETA
#define	TMP	"/tmp"
#endif

int
#if ! defined ENABLE_GETA
xsort_pcompar(tmpdir, ptrs, n, ncpu, gbase, xscopt)
char const *tmpdir;
#else
xsort_pcompar(getaroot, ptrs, n, ncpu, gbase, xscopt)
char const *getaroot;
#endif
struct ptr_u *ptrs;
size_t n;
char const *gbase;
xsc_opt_t xscopt;
{
	int e = 0;
	size_t i, m, k;
	struct segm *segm;
	size_t nsegm;
#if defined X_FORK
	int active = 0;
	char seginfo[128];
#elif defined X_PTHREAD
	pthread_t *threads;
	struct ck ck0, *ck = &ck0;
#else
	char seginfo[128];
#endif
#if defined SHOW_PI
	size_t pi[80];
	int j;
#endif
fprintf(stderr, "xsort_pcompar\n");

	if (n <= 1) return 0;

	g_base = gbase;
	xsc_options = xscopt;

	nsegm = init_sel();
fprintf(stderr, "nsegm = %"PRIuSIZE_T"\n", (pri_size_t)nsegm);

#if defined DBG
fprintf(stderr, "calloc segm: %"PRIuSIZE_T"\n", (pri_size_t)(nsegm * sizeof *segm));
#endif
	if (!(segm = calloc(nsegm, sizeof *segm))) {
		perror("calloc");
		return -1;
	}
#define	ERROR()	do { e = -1; goto error; } while (0)

	for (k = 0; k < nsegm; k++) {
		struct segm *u = &segm[k];
#if ! defined ENABLE_GETA
		snprintf(u->path, sizeof u->path, "%s/nwamXXXXXX", tmpdir);
#else
		snprintf(u->path, sizeof u->path, "%s" TMP "/XXXXXX", getaroot);
#endif
		if ((u->d = mkstemp(u->path)) == -1) {
			perror("mkstemp");
			*u->path = '\0';
			ERROR();
		}
#if ! defined ENABLE_GETA
		nwam_add_tmp(u->path, NWAM_F);
#endif
		if (!(u->f = fdopen(u->d, "r+b"))) {
			perror("fdopen");
			ERROR();
		}
	}

#if defined SHOW_PI
	for (j = 0; j < nelems(pi); j++) {
		pi[j] = (double)(n - 1) * j / (nelems(pi) - 1);
	}
	j = 0;
#endif

	for (i = 0; i < n; i++) {
		struct ptr_u *a = &ptrs[i];
		struct segm *u;
		struct astream bs;
		k = select_segm(x1stchar_r(a->p + g_base, xsc_options, &bs));
		u = &segm[k];
		if (fwrite(a, sizeof *a, 1, u->f) != 1) {
			perror("fwrite");
			ERROR();
		}
#if defined SHOW_PI
		while (j < nelems(pi) && pi[j] <= i) {
			fprintf(stderr, ".");
			j++;
		}
#endif
	}
#if defined SHOW_PI
	fprintf(stderr, "\n");
#endif
	for (k = 0; k < nsegm; k++) {
		struct segm *u = &segm[k];
		u->nmemb = ftell(u->f) / sizeof *ptrs;
		u->size = sizeof *ptrs;
	}

#if defined X_FORK
	for (k = 0; k < nsegm; k++) {
		struct segm *u = &segm[k];
		fclose(u->f);
		u->f = NULL;
		u->pid = -1;
	}
	for (k = 0; k < nsegm; k++) {
		struct segm *u = &segm[k];
		switch (u->pid = fork()) {
		case -1:
			perror("fork");
			ERROR();
			break;
		case 0:
			if (!(u->f = fopen(u->path, "r+b"))) {
				perror(u->path);
				_exit(1);
			}
			if (fsort(u->f, u->nmemb, u->size, pcompar) == -1) {
				_exit(1);
			}
			fclose(u->f);
			_exit(0);
			/* NOTREACHED */
			break;
		default:
fprintf(stderr, "%d: %s %"PRIuSIZE_T"\n", u->pid, seg_info_r(k, seginfo, sizeof seginfo), (pri_size_t)u->nmemb);
			active++;
			break;
		}
		if (active >= ncpu) {
			if (waitone(segm, nsegm) == -1) {
				ERROR();
			}
			active--;
		}
	}
	for (; active > 0; ) {
		if (waitone(segm, nsegm) == -1) {
			ERROR();
		}
		active--;
	}

	for (k = 0; k < nsegm; k++) {
		struct segm *u = &segm[k];
		if (!(u->f = fopen(u->path, "r+b"))) {
			perror(u->path);
			ERROR();
		}
	}
#elif defined X_PTHREAD
	for (k = 0; k < nsegm; k++) {
		struct segm *u = &segm[k];
		u->processed = 0;
	}
	if (sem_init(&ck->sem, 0, 1)) {
		perror("sem_init");
		ERROR();
	}
	ck->segm = segm;
	ck->nsegm = nsegm;
	if (!(threads = calloc(ncpu, sizeof *threads))) {
		perror("calloc");
		return -1;
	}
	for (k = 0; k < ncpu; k++) {
		if (pthread_create(&threads[k], NULL, start_routine, ck)) {
			perror("pthread_create");
			ERROR();
		}
	}
	for (k = 0; k < ncpu; k++) {
		void *value;
		if (pthread_join(threads[k], &value)) {
			perror("pthread_join");
			ERROR();
		}
		if (value) {
			fprintf(stderr, "error: %s\n", (char *)value);
			ERROR();
		}
	}
	if (sem_destroy(&ck->sem)) {
		perror("sem_destroy");
		ERROR();
	}
	free(threads);
#else	/* !X_FORK !X_PTHREAD */
	for (k = 0; k < nsegm; k++) {
		struct segm *u = &segm[k];
fprintf(stderr, "%s %"PRIuSIZE_T"\n", seg_info_r(k, seginfo, sizeof seginfo), (pri_size_t)u->nmemb);
		if (fsort(u->f, u->nmemb, u->size, pcompar) == -1) {
			ERROR();
		}
	}
#endif
	m = 0;
	for (k = 0; k < nsegm; k++) {
		struct segm *u = &segm[k];
		rewind(u->f);
		if (fread(ptrs + m, sizeof *ptrs, u->nmemb, u->f) != u->nmemb) {
			perror("fread");
			ERROR();
		}
		fclose(u->f);
		m += u->nmemb;
	}

	assert(m == n);

#undef ERROR
error:

	for (k = 0; k < nsegm; k++) {
		struct segm *u = &segm[k];
		if (*u->path) {
			unlink(u->path);
		}
	}

	return e;
}

#if defined X_FORK
static int
waitone(segm, nsegm)
struct segm *segm;
size_t nsegm;
{
	pid_t pid;
	struct segm *u = NULL;
	int status;
	size_t k;
	char pids[32];
waitone:
	if ((pid = wait(&status)) == -1) {
		if (errno == EINTR) {
			perror("EINTR");
			goto waitone;
		}
		else {
			perror("wait");
			return -1;
		}
	}
	for (k = 0; k < nsegm; k++) {
		u = &segm[k];
		if (u->pid == pid) {
			break;
		}
	}
	if (k == nsegm) {
		fprintf(stderr, "unrelated pid: %d\n", pid);
		goto waitone;
	}
	snprintf(pids, sizeof pids, "%d", u->pid);
	if (diag_stat(pids, status) == -1) {
		return -1;
	}
/*
	if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
		fprintf(stderr, "%d aborted\n", u->pid);
		return -1;
	}
*/
	u->pid = -1;
	return 0;
}
#endif

#if defined X_PTHREAD
static void *
start_routine(cookie)
void *cookie;
{
	struct ck *ck = cookie;
	char seginfo[128];
	for (;;) {
		struct segm *u = NULL;
		size_t k;
		if (sem_wait(&ck->sem)) {
			perror("sem_wait");
			return "sem_wait";
		}
		u = NULL;
		for (k = 0; k < ck->nsegm; k++) {
			if (ck->segm[k].processed == 0) {
				u = &ck->segm[k];
				u->processed = 1;
				break;
			}
		}
		if (sem_post(&ck->sem)) {
			perror("sem_post");
			return "sem_post";
		}
		if (!u) {
			fprintf(stderr, "pthread_exit(NULL)\n");
			return NULL;
		}
fprintf(stderr, "%s %"PRIuSIZE_T"\n", seg_info_r(k, seginfo, sizeof seginfo), (pri_size_t)u->nmemb);
		if (fsort(u->f, u->nmemb, u->size, pcompar) == -1) {
			return "fsort";
		}
	}
	/* NOTREACHED */
}
#endif

static struct {
	unsigned int from, to;
	size_t nbins, binsize, binbase;
} tbl[] = {
/*#define	X	0x110000*/
	{0, 0x0020, 1, 0, 0},
	{0, 0x0080, 24, 0, 0},
	{0, 0x3001, 1, 0, 0},
	{0, 0x3003, 2, 0, 0},
	{0, 0x3100, 24, 0, 0},
	{0, 0x4e00, 1, 0, 0},
	{0, 0xa000, 24, 0, 0},
	{0, 0xff00, 1, 0, 0},
	{0, 0xff20, 1, 0, 0},
	{0, 0x10000, 1, 0, 0},
	{0, 0x110000, 1, 0, 0},
};

static size_t
init_sel()
{
	size_t i, from = 0, binbase = 0;
	for (i = 0; i < nelems(tbl); i++) {
		unsigned int to;
		size_t binsize, nbins;
		tbl[i].from = from;
		tbl[i].binbase = binbase;
		to = tbl[i].to;
		nbins = tbl[i].nbins;
		binsize = (to - from + nbins - 1) / nbins;
		tbl[i].binsize = binsize;
		binbase += nbins;
fprintf(stderr, "%"PRIuSIZE_T": %lx - %lx, n=%"PRIuSIZE_T", size=%"PRIuSIZE_T", base=%"PRIuSIZE_T"\n", (pri_size_t)i, (pri_size_t)from, (pri_size_t)to, (pri_size_t)nbins, (pri_size_t)binsize, (pri_size_t)binbase);
		assert((to - from - 1) / binsize < nbins);
		from = to;
	}
	return binbase;
}

static size_t
select_segm(u)
unsigned int u;
{
	size_t i;
	for (i = 0; i < nelems(tbl); i++) {
		unsigned int to;
		to = tbl[i].to;
		if (u < to) {
			unsigned int from;
			size_t binbase, binsize;
			from = tbl[i].from;
			binbase = tbl[i].binbase;
			binsize = tbl[i].binsize;
			return binbase + (u - from) / binsize;
		}
	}
	return 0;
}

static char *
seg_info_r(k, msg, size)
size_t k;
char *msg;
size_t size;
{
	UChar32 from, to;
	UBool isError;
	int o;
	uint8_t u1[8], u2[8];
	size_t binsize, binoffs, i;

	for (i = 1; i < nelems(tbl) && tbl[i].binbase <= k; i++) ;
	i--;

	binsize = tbl[i].binsize;
	binoffs = k - tbl[i].binbase;

	from = tbl[i].from + binoffs * binsize;
	to = from + binsize;
	isError = 0, o = 0;
	U8_APPEND(u1, o, sizeof u1, from, isError);
	if (isError) memmove(u1, "?", 2); else u1[o] = '\0';
	isError = 0, o = 0;
	U8_APPEND(u2, o, sizeof u2, to, isError);
	if (isError) memmove(u2, "?", 2); else u2[o] = '\0';

	snprintf(msg, size, "*** %"PRIuSIZE_T"+%"PRIuSIZE_T": %x(%s) - %x(%s)", (pri_size_t)i, (pri_size_t)binoffs, from, u1, to, u2);
	return msg;
}

static int
pcompar(va, vb)
void const *va;
void const *vb;
{
	struct ptr_u *a, *b;
	int e;

	a = (struct ptr_u *)va;
	b = (struct ptr_u *)vb;
	if (e = xstrcmp(a->p + g_base, b->p + g_base, xsc_options)) {
		return e;
	}
	else if (a->p < b->p) {
		return -1;
	}
	else if (a->p > b->p) {
		return 1;
	}
	return 0;
}
