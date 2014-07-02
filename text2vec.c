/*	$Id: text2vec.c,v 1.39 2011/09/14 02:36:16 nis Exp $	*/

/*-
 * Copyright (c) 2005 Shingo Nishioka.
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
static char rcsid[] = "$Id: text2vec.c,v 1.39 2011/09/14 02:36:16 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/time.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <unistd.h>

#include <expat.h>

#include "nwam.h"
#include "util.h"

#include "xstem.h"

#include <gifnoc.h>

/* #define  ENABLE_CSTEM	1 /* */

static struct xr_vec zerovec = {0, 0, {{0, 0}, }};
static int ecompar(const void *, const void *);

struct xr_vec *
text2vec(text, w, dir, stemmer)
char const *text, *stemmer;
WAM *w;
{
	struct xr_vec *v = NULL;
	size_t v_size = 0;
	df_t elem_num;
	freq_t freq_sum;
	char const *p;
	char *sp = NULL;
	df_t i, j, k;
	struct xr_elem *es;
	idx_t id;
#if ! defined ENABLE_CSTEM
	char const *host = STMDHOST;
	char const *serv = STMDSERV;
#if defined NO_LOCALSOCKET
	char const *localsocket = NULL;
#else
	char const *localsocket = STMDLOCALSOCKET;
#endif
#endif
#if ! defined ENABLE_CSTEM
	struct xstems *xp = NULL;
#endif

	elem_num = 0;
	freq_sum = 0;

/* fprintf(stderr, "text->vec: enter\n"); */

#define	XV() do { \
	if (v_size <= elem_num) { \
		void *new; \
		size_t newsize = elem_num + 1; \
		newsize = NA(newsize, 128); \
		if (!(new = realloc(v, offsetof(struct xr_vec, elems[newsize])))) { \
			syslog(LOG_DEBUG, "realloc: %s", strerror(errno)); \
			free(v); \
			v = NULL; \
			goto end; \
		} \
		v_size = newsize; \
		v = new; \
	} \
} while (0)

#define	SETE(e, i, f)  ((e)->id = (i), (e)->freq = (f))

#if ! defined ENABLE_CSTEM
	if (!(xp = xstem_create(host, serv, localsocket, stemmer))) {
/*fprintf(stderr, "xstem_create: failed\n");*/
		return NULL;
	}
/* fprintf(stderr, "xstem_create: xp = %p\n", xp); */

/* fprintf(stderr, "xstem: text = [%s]\n", text); */
	if (!(sp = xstem(xp, text))) {
/*
		fprintf(stderr, "xstem failed\n");
*/
		free(v);
		v = NULL;
		goto end;
	}
#else /* ! ENABLE_CSTEM */
	if (!(sp = cstem_to_string(text, stemmer))) {
		return NULL;
	}
#endif /* ! ENABLE_CSTEM */

	dir = WAM_COL;

	for (p = sp; *p; ) {
		char wrd[1024];
		int i;
		for (; *p && isspace(*p & 0xff); p++) ;
		if (!*p) {
			break;
		}
		for (i = 0; *p && !isspace(*p & 0xff); p++) {
			if (i < sizeof wrd - 1) {
				wrd[i++] = *p;
			}
		}
		wrd[i] = '\0';
/* fprintf(stderr, "xstem: [%s]\n", wrd); */
		if (id = wam_name2id(w, dir, wrd)) {
			struct xr_elem *e;
			XV();
			e = &v->elems[elem_num++];
			SETE(e, id, 1);
			freq_sum++;
		}
	}

	if (!v) {
/* fprintf(stderr, "zerovec\n"); */
#if ! defined ENABLE_CSTEM
		if (xstem_destroy(xp) == -1) {
/*fprintf(stderr, "xstem_destroy: failed\n");*/
			/* return NULL; */
		}
#else /* ! ENABLE_CSTEM */
		free(sp);
#endif /* ! ENABLE_CSTEM */
		return bdup(&zerovec, sizeof(zerovec));
	}
	qsort(v->elems, elem_num, sizeof *v->elems, ecompar);

	es = v->elems;
	for (i = k = 0; i < elem_num; i = j) {
		freq_t frq;
		frq = 0;
		for (j = i; j < elem_num && es[i].id == es[j].id; j++) {
			frq += es[j].freq;
		}
		es[k].id = es[i].id;
		es[k].freq = frq;
		k++;
	}

	v->elem_num = k;
	v->freq_sum = freq_sum;
end:
#if ! defined ENABLE_CSTEM
	if (xp && xstem_destroy(xp) == -1) {
/*fprintf(stderr, "xstem_destroy: failed\n");*/
		/* return NULL; */
	}
#else
	free(sp);
#endif
/* fprintf(stderr, "text->vec: %p\n", v); */
	return v;
#undef XV
}

/* XXX: a clone is in dwsys.c */
static int
ecompar(va, vb)
void const *va, *vb;
{
	struct xr_elem const *a, *b;
	a = va;
	b = vb;
	if (a->id < b->id) {
		return -1;
	}
	else if (a->id > b->id) {
		return 1;
	}
	return 0;
}

char *
normalize_text(text)
char const *text;
{
#if ! defined ENABLE_CSTEM
	struct xstems *xp = NULL;
	char const *host = STMDHOST;
	char const *serv = STMDSERV;
#if defined NO_LOCALSOCKET
	char const *localsocket = NULL;
#else
	char const *localsocket = STMDLOCALSOCKET;
#endif
	char *pattern, *pat0;

	if (!(xp = xstem_create(host, serv, localsocket, NORMALIZER))) {
		goto err;
	}
	if (!(pat0 = xstem(xp, text))) {
		goto err;
	}
	if (!(pattern = strdup(pat0))) {
		goto err;
	}
	if (xstem_destroy(xp) == -1) {
		/* free(pattern);
		return -1; */
	}
	xp = NULL;
	return pattern;
err:
	if (xp) xstem_destroy(xp);
	return NULL;
#else /* ! ENABLE_CSTEM */
	char *p;
	size_t l;
	if (!(p = cstem_to_string(text, NORMALIZER))) {
		return NULL;
	}
	l = strlen(p);
	if (l > 0 && p[l - 1] == '\n') {
		p[l - 1] = '\0';
	}
	return p;
#endif /* ! ENABLE_CSTEM */
}
