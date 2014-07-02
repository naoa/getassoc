/*	$Id: gorjparse.y,v 1.20 2011/09/14 03:06:08 nis Exp $	*/

%{
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

/* GORJ (GETA Object Representaion in JSON) Parser */

#if ! defined lint
static char rcsid[] = "$Id: gorjparse.y,v 1.20 2011/09/14 03:06:08 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/time.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>

#include "nwam.h"

#include "gorj.h"

#include <gifnoc.h>

struct source {
	char const *s;
	char const *p;
	char const *e;
	char *sval;
	size_t sval_size;
	long ival;
	struct xr_vec *result;
	WAM *w;
	int dir;
};

#define NR

#if defined NR
static int yyparse(void);
static int yylex(void);
static struct source *prm = NULL;
#else
static int yyparse(void *);
static int yylex(void *);
#define YYPARSE_PARAM prm
#define YYLEX_PARAM prm
#endif

static struct xr_vec *nullvec(df_t *);
static void add_elem(struct xr_vec **, df_t *, idx_t, freq_t);
static int yyerror(char *);
static int ecompar(void const *, void const *);
static struct source *jssetstr(char const *, WAM *, int);
static int jsgettoken(struct source *);

%}

%union {
	struct { struct xr_vec *vec; df_t size; } svec;
	struct { idx_t id; freq_t freq; } elem;
	idx_t id;
	freq_t freq;
}

%start gorj

%type <svec> vec_items vec
%token <id> STRING
%token <freq> NUMBER
%type <elem> vec_item

%%

gorj		:	vec {
				((struct source *)prm)->result = $1.vec;
			}
		;

vec		:	'[' vec_items ']' {
				$$ = $2;
			}
		;

vec_items	:	/* empty */ {
				$$.vec = nullvec(&$$.size);
		}
		|	vec_item {
				$$.vec = nullvec(&$$.size);
				add_elem(&$$.vec, &$$.size, $1.id, $1.freq);
			}
		|	vec_items ',' vec_item {
				$$.vec = $1.vec;
				$$.size = $1.size;
				add_elem(&$$.vec, &$$.size, $3.id, $3.freq);
			}
		;

vec_item	:	'[' STRING ',' NUMBER ']' {
				$$.id = wam_name2id(((struct source *)prm)->w, ((struct source *)prm)->dir, ((struct source *)prm)->sval);
				$$.freq = ((struct source *)prm)->ival;
			}
		;

%%

static struct xr_vec *
nullvec(sp)
df_t *sp;
{
	df_t s;
	struct xr_vec *v;
	s = 24;
	if (!(v = calloc(1, offsetof(struct xr_vec, elems[s])))) {
		return NULL;
	}
	v->elem_num = 0;
	v->freq_sum = 0;
	*sp = s;
	return v;
}

static void
add_elem(vp, sp, id, freq)
struct xr_vec **vp;
df_t *sp;
idx_t id;
freq_t freq;
{
	struct xr_vec *v = *vp;
	df_t s = *sp;
	if (!v || id == 0 || freq == 0) {
		return;
	}
	else if (s <= v->elem_num) {
		assert(s != 0);
		s *= 2;
		assert(s > v->elem_num);
		if (!(v = realloc(v, offsetof(struct xr_vec, elems[s])))) {
			*vp = NULL;
			return;
		}
	}
	v->elems[v->elem_num].id = id;
	v->elems[v->elem_num].freq = freq;
	v->elem_num++;
	v->freq_sum += freq;
	*vp = v;
	*sp = s;
}

static int
yyerror(s)
char *s;
{
	syslog(LOG_DEBUG, "%s", s);
	return 0;
}

#if defined NR
static int
yylex()
{
	return jsgettoken(prm);
}
#else
static int
yylex(prm)
void *prm;
{
	return jsgettoken((struct source *)prm);
}
#endif

/* XXX NOTE: not reentrant */
struct xr_vec *
jsvec2vec(s, w, dir)
char const *s;
WAM *w;
{
	struct xr_vec *v;
#if defined NR
	if (prm) {
		/* entered jsvec2vec twice */
		return NULL;
	}
	if (!(prm = jssetstr(s, w, dir))) {
		return NULL;
	}
	yyparse();
	v = prm->result;
	free(prm->sval);
	free(prm);
	prm = NULL;
#else
	struct source *src;
	if (!(src = jssetstr(s, w, dir))) {
		return NULL;
	}
	yyparse(src);
	v = src->result;
	free(src->sval);
	free(src);
#endif
	if (v) {
		df_t i, j, k;
		qsort(v->elems, v->elem_num, sizeof *v->elems, ecompar);
		for (i = k = 0; i < v->elem_num; i = j) {
			for (j = i + 1; j < v->elem_num && v->elems[i].id == v->elems[j].id; j++) {
				v->elems[i].freq += v->elems[j].freq;
			}
			v->elems[k++] = v->elems[i];
		}
		v->elem_num = k;
	}
	return v;
}

/* XXX: a close is in dwsys.c */
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

static struct source *
jssetstr(s, w, dir)
char const *s;
WAM *w;
{
	struct source *t;
	if (!(t = calloc(1, sizeof *t))) {
		return NULL;
	}
	t->s = t->p = s;
	t->e = t->p + strlen(t->p);
	t->sval = NULL;
	t->sval_size = 0;
	t->ival = 0;
	t->result = NULL;
	t->w = w;
	t->dir = dir;
	return t;
}

static int
jsgettoken(s)
struct source *s;
{
	int c;

	if (s->p >= s->e) {
		return 0;
	}

	for (c = *s->p; s->p < s->e && isspace(c & 0xff); c = *++s->p) ;
	if (isdigit(c & 0xff)) {
		for (s->ival = 0; s->p < s->e && isdigit(*s->p & 0xff); s->p++) {
			s->ival = s->ival * 10 + (*s->p & 0xff) - '0';
		}
		return NUMBER;
	}
	else if (c == '"') {
		size_t i;
		for (s->p++, i = 0; s->p < s->e && *s->p != '"'; ) {
			if (*s->p == '\\' && ++s->p >= s->e) {
				break;
			}
#define	ALLC_TO_I() do { \
			if (s->sval_size <= i) { \
				void *new; \
				size_t newsize; \
				if (i == 0) { \
					newsize = 24; \
				} \
				else { \
					newsize = i * 2; \
				} \
				if (!(new = realloc(s->sval, newsize))) { \
					syslog(LOG_DEBUG, "realloc: %s", strerror(errno)); \
					return 0; \
				} \
				s->sval_size = newsize; \
				s->sval = new; \
			} \
		} while (0)
			ALLC_TO_I();
			s->sval[i++] = *s->p++;
		}
		if (s->p < s->e) {
			assert(*s->p == '"');
			s->p++;
		}
		ALLC_TO_I();
		s->sval[i] = '\0';
		return STRING;
	}
	else if (c == ',' || c == '[' || c == ']') {
		s->p++;
		return c;
	}
	return 0;
}
