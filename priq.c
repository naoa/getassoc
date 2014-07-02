/*	$Id: priq.c,v 1.10 2009/07/30 00:08:15 nis Exp $	*/

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
static char rcsid[] = "$Id: priq.c,v 1.10 2009/07/30 00:08:15 nis Exp $";
#endif

#include <config.h>

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

#include "priq.h"

#include <gifnoc.h>

struct priq *
priq_creat(size, compar, cookie)
size_t size;
priq_compar_t *compar;
void *cookie;
{
	return priq_creat_p(size, compar, priq_dfl_prop_f, priq_dfl_prop_b, cookie);
}

struct priq *
priq_creat_p(size, compar, prop_f, prop_b, cookie)
size_t size;
priq_compar_t *compar;
priq_prop_t *prop_f, *prop_b;
void *cookie;
{
	struct priq *q;
	if (!(q = calloc(1, sizeof *q))) {
		return NULL;
	}
	if (!(q->tmp = calloc(1, size))) {
		free(q);
		return NULL;
	}
	q->esize = size;
	q->size = 0;
	q->n = 0;
	q->q = NULL;
	q->compar = compar;
	q->prop_f = prop_f;
	q->prop_b = prop_b;
	q->cookie = cookie;
	return q;
}

void
priq_free(q)
struct priq *q;
{
	free(q->tmp);
	free(q->q);
	free(q);
}

int
priq_emptyp(q)
struct priq *q;
{
	return q->n == 0;
}

ssize_t
priq_nmemb(q)
struct priq *q;
{
	return q->n;
}

#define	A(prq, idx)	((char *)(prq)->q + (prq)->esize * (idx))
#define	C1(prq, dst, src)	(memmove((dst), (src), (prq)->esize))

ssize_t
priq_enq(q, d)
struct priq *q;
void *d;
{
	if (q->size <= q->n) {
		void *new;
		size_t newsize = q->n;
		newsize = NA(newsize + 1, 128);
		if (!(new = realloc(q->q, newsize * q->esize))) {
			return -1;
		}
		q->size = newsize;
		q->q = new;
	}
	q->n++;
	C1(q, A(q, q->n - 1), d);
	(*q->prop_b)(q, q->n - 1, q->cookie);
	return q->n;
}

void *
priq_top(q)
struct priq *q;
{
	return q->q;
}

ssize_t
priq_deq(q)
struct priq *q;
{
	return priq_delete_index(q, 0);
#if 0
	if (q->n == 0) {
		return -1;
	}
	--q->n;
	if (q->n == 0) {
		free(q->q);
		q->size = 0;
		q->q = NULL;
		return 0;
	}
	C1(q, A(q, 0), A(q, q->n));
	(*q->prop_f)(q, 0, q->cookie);
	return q->n;
#endif
}

void *
priq_rplatop(q, d)
struct priq *q;
void *d;
{
	if (q->n == 0) {
		return NULL;
	}
	C1(q, A(q, 0), d);
	(*q->prop_f)(q, 0, q->cookie);
	return q->q;
}

ssize_t
priq_delete_index(q, i)
struct priq *q;
size_t i;
{
	if (q->n == 0 || q->n <= i) {
		return -1;
	}
	--q->n;
	if (q->n == i) {
		return q->n;
	}
	if (q->n == 0) {
		free(q->q);
		q->size = 0;
		q->q = NULL;
		return 0;
	}
	C1(q, A(q, i), A(q, q->n));
	priq_update_index(q, i);
	return q->n;
}

void
priq_update_index(q, i)
struct priq *q;
size_t i;
{
	(*q->prop_f)(q, i, q->cookie);
	if (i != 0) (*q->prop_b)(q, i, q->cookie);
}

#define compar_dfl(e, a, b, q)	e = ((*q->compar)(a, b))

PROP_F(priq_dfl_prop_f, compar_dfl)
PROP_B(priq_dfl_prop_b, compar_dfl)
