/*	$Id: tpsort_pcompar.c,v 1.36 2011/09/14 03:06:09 nis Exp $	*/

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
static char rcsid[] = "$Id: tpsort_pcompar.c,v 1.36 2011/09/14 03:06:09 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/resource.h>
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

#define SAVE_C	1 /* */
#define USE_STACK	1

#if defined CACHE_PREFETCH
#define astream_init_r astream_init_r_fast
#define astream_getc astream_getc_fast
#endif

struct meta_i {
#if defined SAVE_C
	unsigned int c;
#endif
	uint32_t u1ptr;
};

#define	swap(a, b, type) { type t; t = (a); (a) = (b); (b) = t; }
#define SWAP(a, b) do { \
		swap(ptrs[(a)], ptrs[(b)], struct ptr_u); \
		swap(meta[(a)], meta[(b)], struct meta_i); \
	} while (0)

#if defined SAVE_C

#define GETC_INIT(i, s, u) do { \
		astream_init_r(ptrs[(i)].p + g_base, -1, xsc_options, &(s), -1); \
		meta[(i)].c = astream_getc(&(s), &(u)); \
		meta[(i)].u1ptr = (u); \
	} while (0)

#define	GETC(i, s, t)	(meta[(i)].c)

#define GETC_ADVANCE(i, s, u) do { \
	astream_init_r(ptrs[(i)].p + g_base, -1, xsc_options, &(s), meta[(i)].u1ptr); \
	meta[(i)].c = astream_getc(&(s), &(u)); \
	meta[(i)].u1ptr = (u); \
} while (0)

#else

#define	GETC_INIT(i, s, u) (meta[(i)].u1ptr = 0)

#define	GETC(i, s, t)	( \
	astream_init_r(ptrs[(i)].p + g_base, -1, xsc_options, &(s), meta[(i)].u1ptr) \
)

#define GETC_ADVANCE(i, s, u) do { \
	astream_init_r(ptrs[(i)].p + g_base, -1, xsc_options, &(s), meta[(i)].u1ptr); \
	astream_getc(&(s), &(u)); \
	meta[(i)].u1ptr = (u); \
} while (0)

#endif

static int srt(struct ptr_u *, struct meta_i *, ssize_t);
#if ! defined X_PTHREAD && ! defined X_FORK
static ssize_t srt0(struct ptr_u *, struct meta_i *, ssize_t, ssize_t);
#else
static int srt0(struct ptr_u *, struct meta_i *, ssize_t, size_t);
#endif

#if defined USE_STACK
struct srt_stack {
	size_t sp, size;
	struct {
		struct ptr_u *ptrs;
		struct meta_i *meta;
		ssize_t nmemb;
#if defined X_PTHREAD || defined X_FORK
		size_t frk;
#endif
	} *stack;
};

static struct srt_stack *new_stack(void);
static void destroy_stack(struct srt_stack *);
#if ! defined X_PTHREAD && ! defined X_FORK
static int push(struct srt_stack *, struct ptr_u *, struct meta_i *, ssize_t);
static int pop(struct srt_stack *, struct ptr_u **, struct meta_i **, ssize_t *);
#else
static int push(struct srt_stack *, struct ptr_u *, struct meta_i *, ssize_t, size_t);
static int pop(struct srt_stack *, struct ptr_u **, struct meta_i **, ssize_t *, size_t *);
#endif
static int extend(struct srt_stack *);
#endif

static struct pi pi;

static char const *g_base;
static xsc_opt_t xsc_options;

int
tpsort_pcompar(ptrs, nmemb, gbase, xscopt)
struct ptr_u *ptrs;
size_t nmemb;
char const *gbase;
xsc_opt_t xscopt;
{
	struct mapfile_t *meta_m;
	struct meta_i *meta;
	size_t i;
	int e;

	g_base = gbase;
	xsc_options = xscopt;

	if (nmemb <= 1) {
		return 0;
	}
#if defined DBG
	fprintf(stderr, "nmemb = %"PRIuSIZE_T"\n", (pri_size_t)nmemb);
	fprintf(stderr, "meta = alloc(%.3fM)\n", (nmemb * sizeof *meta)/ (1024 * 1024.0));
#endif
#if USE_MMAP
	if (!(meta_m = mmap_alloc(nmemb * sizeof *meta))) {
		perror("mmap_alloc meta");
		return -1;
	}
	meta = meta_m->ptr;
#else
	if (!(meta = calloc(nmemb, sizeof *meta))) {
		perror("calloc");
		return -1;
	}
#endif
pi_init(&pi, nmemb, 'M', 0, 60, stderr);
	for (i = 0; i < nmemb; i++) {
		struct astream bs;
		ssize_t u;
		GETC_INIT(i, bs, u);
PI_CHECK(&pi, i);
	}
fprintf(stderr, "\n");
	e = srt(ptrs, meta, nmemb);
#if USE_MMAP
	mapfile_destroy(meta_m);
#else
	free(meta);
#endif
	return e;
}

#if ! defined X_PTHREAD && ! defined X_FORK

static int
srt(ptrs, meta, nmemb)
struct ptr_u *ptrs;
struct meta_i *meta;
ssize_t nmemb;
{
	ssize_t total;
pi_init(&pi, nmemb, 'S', 0, 60, stderr);
	if ((total = srt0(ptrs, meta, nmemb, 0)) < 0) {
		return -1;
	}
	fprintf(stderr, " %"PRIdSSIZE_T" %"PRIdSSIZE_T"\n", (pri_ssize_t)nmemb, (pri_ssize_t)total);
	return 0;
}

static ssize_t
srt0(ptrs, meta, nmemb, sofar)
struct ptr_u *ptrs;
struct meta_i *meta;
ssize_t nmemb, sofar;
{
	ssize_t l, r, i;
	unsigned int pivot;
	ssize_t a, b;
	unsigned int c, t;
	struct astream bs;

#if defined USE_STACK
	struct srt_stack *s;

	if (!(s = new_stack())) {
		return -1;
	}
	if (push(s, ptrs, meta, nmemb) == -1) {
		return -1;
	}

while (pop(s, &ptrs, &meta, &nmemb)) {
#endif

	i = random() % nmemb;

	pivot = GETC(i, bs, t);
	for (a = l = 0, b = r = nmemb - 1; l <= r; ) {
		for (; l <= r && (c = GETC(l, bs, t)) <= pivot; l++) {
			if (c == pivot) {
				SWAP(l, a);
				a++;
			}
		}
		for (; l <= r && (c = GETC(r, bs, t)) >= pivot; r--) {
			if (c == pivot) {
				SWAP(r, b);
				b--;
			}
		}

		if (l < r) {
			SWAP(l, r);
			l++, r--;
		}
	}
	for (i = 0; i < a; i++) {
		l--;
		SWAP(i, l);
	}
	for (b++, r++; b < nmemb; b++, r++) {
		SWAP(r, b);
	}

	if (l > 1) {
#if defined USE_STACK
		if (push(s, ptrs, meta, l) == -1) {
			return -1;
		}
#else
		sofar = srt0(ptrs, meta, l, sofar);
#endif
	}
	else {
		sofar += l;
PI_CHECK(&pi, sofar);
	}
	if (pivot != 0 && r - l > 1) {
		for (i = l; i < r; i++) {
			struct astream bs;
			ssize_t u;
			GETC_ADVANCE(i, bs, u);
		}
#if defined USE_STACK
		if (push(s, ptrs + l, meta + l, r - l) == -1) {
			return -1;
		}
#else
		sofar = srt0(ptrs + l, meta + l, r - l, sofar);
#endif
	}
	else {
		sofar += r - l;
PI_CHECK(&pi, sofar);
	}
	if (nmemb - r > 1) {
#if defined USE_STACK
		if (push(s, ptrs + r, meta + r, nmemb - r) == -1) {
			return -1;
		}
#else
		sofar = srt0(ptrs + r, meta + r, nmemb - r, sofar);
#endif
	}
	else {
		sofar += nmemb - r;
PI_CHECK(&pi, sofar);
	}
#if defined USE_STACK
}
	destroy_stack(s);
#endif
	return sofar;
}

#else	/* !X_PTHREAD && !X_FORK */

struct srt0_args {
	struct ptr_u *ptrs;
	struct meta_i *meta;
	ssize_t nmemb;
	size_t frk, t;
};

static void *start_srt0(void *);

#define	C	24

struct srt0_args srt0_args[C];

static int
srt(ptrs, meta, nmemb)
struct ptr_u *ptrs;
struct meta_i *meta;
ssize_t nmemb;
{
	int e;
	if ((e = srt0(ptrs, meta, nmemb, 0)) < 0) {
		return -1;
	}
	fprintf(stderr, " %"PRIdSSIZE_T"\n", (pri_ssize_t)nmemb);
	return 0;
}

static int
srt0(ptrs, meta, nmemb, frk)
struct ptr_u *ptrs;
struct meta_i *meta;
ssize_t nmemb;
size_t frk;
{
	ssize_t l, r, i;
	unsigned int pivot;
	ssize_t a, b;
	unsigned int c;
#if ! defined SAVE_C
	unsigned int t;
	struct astream bs;
#endif

#if defined X_FORK
	pid_t th1[C];
#else	/* X_FORK: assert(defined X_PTHREAD) */
	int th1[C];
	pthread_t threads[C];
#endif
#if defined USE_STACK
	struct srt_stack *s;
#endif

	for (i = 0; i < nelems(th1); i++) {
#if defined X_FORK
		th1[i] = -1;
#else	/* X_FORK: assert(defined X_PTHREAD) */
		bzero(&threads[i], sizeof threads[i]);
		th1[i] = 0;
#endif	/* X_FORK */
	}

#if defined USE_STACK
	if (!(s = new_stack())) {
		return -1;
	}
	if (push(s, ptrs, meta, nmemb, frk) == -1) {
		return -1;
	}

while (pop(s, &ptrs, &meta, &nmemb, &frk)) {
#endif

	i = random() % nmemb;

	pivot = GETC(i, bs, t);
	for (a = l = 0, b = r = nmemb - 1; l <= r; ) {
		for (; l <= r && (c = GETC(l, bs, t)) <= pivot; l++) {
			if (c == pivot) {
				SWAP(l, a);
				a++;
			}
		}
		for (; l <= r && (c = GETC(r, bs, t)) >= pivot; r--) {
			if (c == pivot) {
				SWAP(r, b);
				b--;
			}
		}

		if (l < r) {
			SWAP(l, r);
			l++, r--;
		}
	}
	for (i = 0; i < a; i++) {
		l--;
		SWAP(i, l);
	}
	for (b++, r++; b < nmemb; b++, r++) {
		SWAP(r, b);
	}

/*fprintf(stderr, ">> %"PRIdSSIZE_T" %"PRIdSSIZE_T" %"PRIdSSIZE_T"\n", (pri_ssize_t)l, (pri_ssize_t)(r - l), (pri_ssize_t)(nmemb - r));*/
	if (l > 1) {
		if (frk < nelems(th1) && frk % 3 != 2) {
			srt0_args[frk].ptrs = ptrs;
			srt0_args[frk].meta = meta;
			srt0_args[frk].nmemb = l;
			srt0_args[frk].frk = 3 * frk + 1;
			srt0_args[frk].t = frk;
fprintf(stderr, "T%02d(%"PRIdSSIZE_T" - %"PRIdSSIZE_T")\n", (int)frk, (pri_ssize_t)l, (pri_ssize_t)(nmemb - l));
#if defined X_FORK
			if (th1[frk] != -1) {
				fprintf(stderr, "internal error\n");
				return -1;
			}
			switch (th1[frk] = fork()) {
			case -1:
				perror("fork");
				return -1;
			case 0:
				if (start_srt0(&srt0_args[frk])) {
					_exit(1);
				}
				_exit(0);
			default:
				break;
			}
#else	/* X_FORK: assert(defined X_PTHREAD) */
			if (th1[frk]) {
				fprintf(stderr, "internal error\n");
				return -1;
			}
			if (pthread_create(&threads[frk], NULL, start_srt0, &srt0_args[frk])) {
				perror("pthread_create");
				return -1;
			}
			th1[frk] = 1;
#endif	/* X_FORK */
		}
		else {
#if defined USE_STACK
			if (push(s, ptrs, meta, l, 3 * frk + 1) == -1) {
				return -1;
			}
#else
			if (srt0(ptrs, meta, l, 3 * frk + 1) == -1) {
				return -1;
			}
#endif
		}
	}
	if (pivot != 0 && r - l > 1) {
		for (i = l; i < r; i++) {
			struct astream bs;
			ssize_t u;
			GETC_ADVANCE(i, bs, u);
		}
#if defined USE_STACK
		if (push(s, ptrs + l, meta + l, r - l, 3 * frk + 2) == -1) {
			return -1;
		}
#else
		if (srt0(ptrs + l, meta + l, r - l, 3 * frk + 2) == -1) {
			return -1;
		}
#endif
PI_CHECK(&pi, l);
	}
	if (nmemb - r > 1) {
#if defined USE_STACK
		if (push(s, ptrs + r, meta + r, nmemb - r, 3 * frk + 3) == -1) {
			return -1;
		}
#else
		if (srt0(ptrs + r, meta + r, nmemb - r, 3 * frk + 3) == -1) {
			return -1;
		}
#endif
	}
#if defined USE_STACK
}
	destroy_stack(s);
#endif
	for (i = 0; i < nelems(th1); i++) {
#if defined X_FORK
		int status;
		if (th1[i] != -1) {
			fprintf(stderr, "W%02d ", (int)i);
			if (waitpid(th1[i], &status, 0) == -1) {
				perror("waitpid");
				return -1;
			}
		}
#else
		if (th1[i]) {
			void *value;
			fprintf(stderr, "J%02d ", (int)i);
			if (pthread_join(threads[i], &value)) {
				perror("pthread_join");
				return -1;
			}
			if (value) {
				fprintf(stderr, "error: %p\n", (char *)value);
				fprintf(stderr, "error: %s\n", (char *)value);
				return -1;
			}
		}
#endif	/* X_FORK || X_PTHREAD */
	}
	return 0;
}

static void *
start_srt0(c)
void *c;
{
	struct srt0_args *srt0_args = c;
	if (srt0(srt0_args->ptrs, srt0_args->meta, srt0_args->nmemb, srt0_args->frk) == -1) {
		return "srt0 failed";
	}
	fprintf(stderr, "E%02d ", (int)srt0_args->t);
	return NULL;
}
#endif	/* !X_PTHREAD && !X_FORK */

#if defined USE_STACK
static struct srt_stack *
new_stack()
{
	struct srt_stack *s;

	if (!(s = calloc(1, sizeof *s))) {
		return NULL;
	}
	s->sp = s->size = 0;
	s->stack = NULL;
	return s;
}

static void
destroy_stack(s)
struct srt_stack *s;
{
	fprintf(stderr, "SS:%"PRIuSIZE_T" ", (pri_size_t)s->size);
	free(s->stack);
	free(s);
}

static int
#if ! defined X_PTHREAD && ! defined X_FORK
push(s, ptrs, meta, nmemb)
#else
push(s, ptrs, meta, nmemb, frk)
size_t frk;
#endif
struct srt_stack *s;
struct ptr_u *ptrs;
struct meta_i *meta;
ssize_t nmemb;
{
	if (s->sp == s->size && extend(s) == -1) {
		return -1;
	}
	s->stack[s->sp].ptrs = ptrs;
	s->stack[s->sp].meta = meta;
	s->stack[s->sp].nmemb = nmemb;
#if defined X_PTHREAD || defined X_FORK
	s->stack[s->sp].frk = frk;
#endif
	s->sp++;
	return 0;
}

static int
#if ! defined X_PTHREAD && ! defined X_FORK
pop(s, ptrs, meta, nmemb)
#else
pop(s, ptrs, meta, nmemb, frk)
size_t *frk;
#endif
struct srt_stack *s;
struct ptr_u **ptrs;
struct meta_i **meta;
ssize_t *nmemb;
{
	if (s->sp == 0) {
		return 0;
	}
	s->sp--;
	*ptrs = s->stack[s->sp].ptrs;
	*meta = s->stack[s->sp].meta;
	*nmemb = s->stack[s->sp].nmemb;
#if defined X_PTHREAD || defined X_FORK
	*frk = s->stack[s->sp].frk;
#endif
	return 1;
}

static int
extend(s)
struct srt_stack *s;
{
	void *new;
	size_t newsize = s->size + 2048;
	if (!(new = realloc(s->stack, newsize * sizeof *s->stack))) {
		return -1;
	}
	s->size = newsize;
	s->stack = new;
	return 0;
}
#endif
