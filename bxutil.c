/*	$Id: bxutil.c,v 1.18 2011/09/14 02:36:15 nis Exp $	*/

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
static char rcsid[] = "$Id: bxutil.c,v 1.18 2011/09/14 02:36:15 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>

#include "nwam.h"
#include "assvP.h"
#include "bx.h"

#include <gifnoc.h>

/* #define DBG	1 /* */

struct bxu_t {
	struct bx *expr;
	int *vars;
	df_t nvars;
	struct bxe *o;
};

/*-----------------------------------------------------------------*/

/* q, qlen: wsh heno tuuzyou no hikisuu
   e, elen: bool expression. id==0 => attr=='(' | ')' | '&' | '|' | '!'
			     id!=0 => attr==0
*/

#if defined DBG
static void print_bxu(struct bxue_t *, df_t);
#endif

struct bxu_t *
bxu_conv(q, qlen, e, elen, newq, newqlen)
struct syminfo const *q;
df_t qlen;
struct bxue_t const *e;
df_t elen;
struct syminfo **newq;
df_t *newqlen;
{
	struct bxu_t *u;
	struct bxe *op;
	struct bxue_t const *ep;
	df_t i, j, k;
	struct syminfo *qq;

	df_t qe;
#if defined DBG
	fprintf(stderr, "bxu_conv: %p %"PRIdDF_T" %p %"PRIdDF_T"\n", q, qlen, e, elen);
	print_bxu(e, elen);
#endif
	u = NULL;
	qq = NULL;

	if (!(u = calloc(1, sizeof *u))) {
		goto error;
	}
	u->vars = NULL;
	u->expr = NULL;
	u->o = NULL;

	if (!(u->o = calloc(elen + 2, sizeof *u->o))) {
		goto error;
	}

	j = 0;

	op = &u->o[j++];
	op->type = BX_START;
	op->var = NULL;

	qe = qlen + elen;

	if (!(u->vars = calloc(qe, sizeof *u->vars))) {
		goto error;
	}

	if (!(qq = calloc(qe, sizeof *qq))) {
		goto error;
	}
	assert(sizeof *qq == sizeof *q);
	memcpy(qq, q, qlen * sizeof *qq);
	u->nvars = qlen;

	for (i = 0; i < elen; ) {
		ep = &e[i++];
		op = &u->o[j++];
		op->var = NULL;
/*
fprintf(stderr, "%"PRIdDF_T": %c\n", i, ep->type);
*/
		switch (ep->type) {
		case '(':
			op->type = BX_LPAR;
			break;
		case ')':
			op->type = BX_RPAR;
			break;
		case '&':
			op->type = BX_AND;
			break;
		case '|':
			op->type = BX_OR;
			break;
		case '!':
			op->type = BX_NOT;
			break;
		case 'T':
			op->type = BX_T;
			break;
		case 'F':
			op->type = BX_F;
			break;
		case '"':
			op->type = BX_VAR;
/*
fprintf(stderr, "%d\n", ep->id);
*/
			for (k = 0; k < u->nvars; k++) {
				if (qq[k].v == NULL && ep->id == qq[k].id) {
/*fprintf(stderr, "found: %"PRIdDF_T"\n", k);*/
					goto found;
				}
			}
			assert(k == u->nvars);
			assert(u->nvars < qe);
			qq[k].id = ep->id;
			qq[k].v = NULL;
			u->nvars++;
			qlen++;
/*fprintf(stderr, "not found: new qlen = %"PRIdDF_T"\n", k);*/
		found:
			op->var = &u->vars[k];
			break;
		default:
			goto error;
		}
	}

	assert(u->nvars == qlen);
	assert(u->nvars <= qe);

	op = &u->o[j++];
	op->type = BX_END;
	op->var = NULL;
/*
 {int k;
for (k = 0; k < j; k++) fprintf(stderr, "%d: %05x\n", k, u->o[k].type);
}
*/

	assert(j == elen + 2);

	if (!(u->expr = bx_compile(u->o, j))) {
		goto error;
	}
	/*free(o);*/

/*syslog(LOG_DEBUG, "%s", bx_print(u->o, u->expr));*/
	*newq = qq;
	*newqlen = qlen;

	return u;

error:
	free(u->o);
	if (u) free(u->vars);
	free(u);
	free(qq);
	return NULL;
}

void
bxu_free(u)
struct bxu_t *u;
{
	free(u->expr);
	free(u->vars);
	free(u->o);
	free(u);
}

int
bxu_eval(u, h, nh)
struct bxu_t *u;
struct hvec_t *h;
df_t nh;
{
	df_t i;
	bzero(u->vars, u->nvars * sizeof *u->vars);
	for (i = 0; i < nh; i++) {
		struct pq_container *t = h[i].t;
if (u->nvars <= t->qidx) syslog(LOG_DEBUG, "ASSERTION FAILED");
		u->vars[t->qidx] = 1;
	}
	return bx_exec(u->expr);
}

#if defined DBG
static void
print_bxu(b, blen)
struct bxue_t *b;
df_t blen;
{
	static char s[8192];
	size_t i;
	*s = 0;
	for (i = 0; i < blen; i++) {
		struct bxue_t *e = &b[i];
		sprintf(s + strlen(s), "%c", e->type);
		if (e->type == '"') sprintf(s + strlen(s), "%"PRIuIDX_T" ", e->id);
		else sprintf(s + strlen(s), " ");
	}
	syslog(LOG_DEBUG, "%s", s);
}
#endif
