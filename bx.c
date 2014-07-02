/*	$Id: bx.c,v 1.12 2009/08/27 23:36:59 nis Exp $	*/

/*-
 * Copyright (c) 2003 Shingo Nishioka.
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
static char rcsid[] = "$Id: bx.c,v 1.12 2009/08/27 23:36:59 nis Exp $";
#endif

#include <config.h>

#include <sys/types.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "util.h"
#include "bx.h"

#include <gifnoc.h>

/* #define DBG	1 /* */

/*

expr	: a_expr
	;

a_expr	: m_expr
	| m_expr '|' a_expr
	;

m_expr	: n_expr
	| n_expr '&' n_expr

n_expr	: f_expr
	| '!' f_expr
	;

f_expr	: elem
	| '(' expr ')'
	;

elem	: VAR
	| T
	| F
	;
*/

struct a_expr {
	struct m_expr *e;
	struct a_expr *next;
};

struct m_expr {
	struct n_expr *e;
	struct m_expr *next;
};

struct n_expr {
	struct f_expr *e;
	int not;
};

struct f_expr {
	struct a_expr *e;
	int *var;
};

static int bx_true = 1;
static int bx_false = 0;

static int eval_a(struct a_expr *, struct bx *);
#if defined DBG
static void print_a(struct a_expr *, struct bx *, char *, struct bxe *);
#endif

static struct a_expr *conv_a(struct bxe *, size_t, size_t *);
static struct m_expr *conv_m(struct bxe *, size_t, size_t *);
static struct n_expr *conv_n(struct bxe *, size_t, size_t *);
static struct f_expr *conv_f(struct bxe *, size_t, size_t *);

static void free_a_expr(struct a_expr *);
static void free_m_expr(struct m_expr *);
static void free_n_expr(struct n_expr *);
static void free_f_expr(struct f_expr *);

struct bx {
	struct a_expr *expr;
};

struct bx *
bx_compile(attrs, len)
struct bxe *attrs;
size_t len;
{
	struct bx *b;
	size_t j;

	if (!(b = calloc(1, sizeof *b))) {
		return NULL;
	}
	b->expr = NULL;

	if (attrs[0].type != BX_START || attrs[len - 1].type != BX_END) {
		goto error;
	}
	if (!(b->expr = conv_a(attrs, 1, &j))) {
		goto error;
	}
	if (j != len - 1) {
		goto error;
	}
	return b;

error:
	bx_free(b);
	return NULL;
}

void
bx_free(b)
struct bx *b;
{
	if (!b) {
		return;
	}
	free_a_expr(b->expr);
	free(b);
}

int
bx_exec(b)
struct bx *b;
{
	return eval_a(b->expr, b);
}

static int
eval_a(a, b)
struct a_expr *a;
struct bx *b;
{
	for (; a; a=a->next) {
		struct m_expr *m = a->e;
		for (; m; m=m->next) {
			int v;
			struct n_expr *n = m->e;
			struct f_expr *f = n->e;
			if (f->e) {
				v = eval_a(f->e, b);
			}
			else {
				v = *f->var;
			}
			if (v == n->not) {
				goto cont;
			}
		}
		return 1;
	cont: ;
	}
	return 0;
}

static struct a_expr *
conv_a(attr, i, j)
struct bxe *attr;
size_t i, *j;
{
	struct a_expr *a, *n;
	if (!(n = a = calloc(1, sizeof *a))) {
		return NULL;
	}
	n->next = NULL;
	if (!(n->e = conv_m(attr, i, &i))) {
		goto error;
	}
	while (attr[i].type == BX_OR) {
		if (!(n = n->next = calloc(1, sizeof *n->next))) {
			goto error;
		}
		n->next = NULL;
		if (!(n->e = conv_m(attr, i+1, &i))) {
			goto error;
		}
	}
	*j = i;
	return a;
error:
	for (; a; ) {
		n = a;
		a = a->next;
		free_m_expr(n->e);
		free(n);
	}
	return NULL;
}

static struct m_expr *
conv_m(attr, i, j)
struct bxe *attr;
size_t i, *j;
{
	struct m_expr *m, *n;
	if (!(n = m = calloc(1, sizeof *m))) {
		return NULL;
	}
	n->next = NULL;
	if (!(n->e = conv_n(attr, i, &i))) {
		goto error;
	}
	while (attr[i].type == BX_AND) {
		if (!(n = n->next = calloc(1, sizeof *n->next))) {
			goto error;
		}
		n->next = NULL;
		if (!(n->e = conv_n(attr, i+1, &i))) {
			goto error;
		}
	}
	*j = i;
	return m;
error:
	for (; m; ) {
		n = m;
		m = m->next;
		free_n_expr(n->e);
		free(n);
	}
	return NULL;
}

struct n_expr *
conv_n(attr, i, j)
struct bxe *attr;
size_t i, *j;
{
	struct n_expr *n;
	if (!(n = calloc(1, sizeof *n))) {
		return NULL;
	}
	if (attr[i].type == BX_NOT) {
		i++;
		n->not = 1;
	}
	else {
		n->not = 0;
	}
	if (!(n->e = conv_f(attr, i, j))) {
		free(n);
		return NULL;
	}
	return n;
}

static struct f_expr *
conv_f(attr, i, j)
struct bxe *attr;
size_t i, *j;
{
	struct f_expr *f;
	if (!(f = calloc(1, sizeof *f))) {
		return NULL;
	}
	f->var = NULL;
	f->e = NULL;
	switch (attr[i].type) {
	case BX_VAR:
		f->var = attr[i].var;
		*j = i + 1;
		return f;
	case BX_T:
		f->var = &bx_true;
		*j = i + 1;
		return f;
	case BX_F:
		f->var = &bx_false;
		*j = i + 1;
		return f;
	case BX_LPAR:
		if (!(f->e = conv_a(attr, i + 1, &i))) {
			goto error;
		}
		if (attr[i].type != BX_RPAR) {
			goto error;
		}
		*j = i + 1;
		return f;
	}
error:
	free_a_expr(f->e);
	free(f);
	return NULL;
}

static void
free_a_expr(e)
struct a_expr *e;
{
	struct a_expr *n;
	for (; e; ) {
		n = e;
		e = e->next;
		free_m_expr(n->e);
		free(n);
	}
}

static void
free_m_expr(e)
struct m_expr *e;
{
	struct m_expr *n;
	for (; e; ) {
		n = e;
		e = e->next;
		free_n_expr(n->e);
		free(n);
	}
}

static void
free_n_expr(e)
struct n_expr *e;
{
	if (e) {
		free_f_expr(e->e);
	}
	free(e);
}

static void
free_f_expr(e)
struct f_expr *e;
{
	if (e) {
		free_a_expr(e->e);
	}
	free(e);
}

#if defined DBG

char *
bx_print(attrs, b)
struct bxe *attrs;
struct bx *b;
{
	static char s[2048];
	*s = 0;
	print_a(b->expr, b, s, attrs);
	return s;
}

static void
print_a(a, b, s, attrs)
struct a_expr *a;
struct bx *b;
char *s;
struct bxe *attrs;
{
	int e = a && a->next;
	if (e) sprintf(s + strlen(s), "(or ");
	for (; a; a=a->next) {
		struct m_expr *m = a->e;
		int ee = m && m->next;
		if (ee) sprintf(s + strlen(s), "(and ");
		for (; m; m=m->next) {
			struct n_expr *n = m->e;
			struct f_expr *f = n->e;
			if (f->e) {
				print_a(f->e, b, s, attrs);
			}
			else if (f->var == &bx_true) {
				sprintf(s + strlen(s), "#t");
			}
			else if (f->var == &bx_false) {
				sprintf(s + strlen(s), "#f");
			}
			else {
				sprintf(s + strlen(s), "%"PRIuSIZE_T, (pri_size_t)(((char *)f->var - (char *)&attrs[0].var) / sizeof attrs[0]));
			}
			if (m->next) sprintf(s + strlen(s), " ");
		}
		if (ee) sprintf(s + strlen(s), ")");
		if (a->next) sprintf(s + strlen(s), " ");
	}
	if (e) sprintf(s + strlen(s), ")");
}

#endif
