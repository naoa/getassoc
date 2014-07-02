/*	$Id: priq.h,v 1.6 2009/07/18 11:21:47 nis Exp $	*/

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

struct priq;

typedef void priq_prop_t(struct priq *, size_t, void *);
typedef int priq_compar_t(const void *, const void *);

struct priq {
	size_t esize, size, n;
	void *q, *tmp;
	priq_compar_t *compar;
	priq_prop_t *prop_f, *prop_b;
	void *cookie;
};

struct priq *priq_creat(size_t, priq_compar_t *, void *);
struct priq *priq_creat_p(size_t, priq_compar_t *, priq_prop_t *, priq_prop_t *, void *);
void priq_free(struct priq *);
int priq_emptyp(struct priq *);
ssize_t priq_nmemb(struct priq *);
ssize_t priq_enq(struct priq *, void *);
void *priq_top(struct priq *);
ssize_t priq_deq(struct priq *);
void *priq_rplatop(struct priq *, void *);

ssize_t priq_delete_index(struct priq *, size_t);
void priq_update_index(struct priq *, size_t);

priq_prop_t priq_dfl_prop_f;
priq_prop_t priq_dfl_prop_b;

#define	PRIQ_EMPTYP(priq)	((priq)->n == 0)
#define	PRIQ_NMEMB(priq)	((priq)->n)
#define	PRIQ_TOP(priq)		((priq)->q)

#define PROP_F(name, cmp) \
void \
name(q, start, cookie) \
struct priq *q; \
size_t start; \
void *cookie; \
{ \
	size_t i, j, n, esize; \
	n = q->n; \
	esize = q->esize; \
	for (i = start; (j = i * 2 + 1) < n; i = j) { \
		void *a, *b, *t; \
		int e; \
		a = (char *)q->q + esize * i; \
		b = (char *)q->q + esize * j; \
		if (j + 1 < n) { \
			void *c; \
			c = (char *)q->q + esize * (j + 1); \
			cmp(e, b, c, q); \
			if (e > 0) { \
				j++; \
				b = c; \
			} \
		} \
		cmp(e, a, b, q); \
		if (e <= 0) { \
			break; \
		} \
		t = q->tmp; \
		memmove(t, a, esize); \
		memmove(a, b, esize); \
		memmove(b, t, esize); \
	} \
}

#define PROP_B(name, cmp) \
void \
name(q, start, cookie) \
struct priq *q; \
size_t start; \
void *cookie; \
{ \
	size_t i, j, esize; \
	if (q->n == 0) { \
		return; \
	} \
	esize = q->esize; \
	for (j = start; j > 0; j = i) { \
		void *a, *b, *t; \
		int e; \
		i = (j - 1) / 2; \
		a = (char *)q->q + esize * i; \
		b = (char *)q->q + esize * j; \
		cmp(e, a, b, q); \
		if (e <= 0) { \
			break; \
		} \
		t = q->tmp; \
		memmove(t, a, esize); \
		memmove(a, b, esize); \
		memmove(b, t, esize); \
	} \
}

#define PROP_F_TYPED(name, cmp, type) \
void \
name(q, start, cookie) \
struct priq *q; \
size_t start; \
void *cookie; \
{ \
	size_t i, j, n; \
	type a; \
	type b; \
	type t; \
	int e; \
	n = q->n; \
	for (i = start; (j = i * 2 + 1) < n; i = j) { \
		a = (type)q->q + i; \
		b = (type)q->q + j; \
		if (j + 1 < n) { \
			type c; \
			c = (type)q->q + (j + 1); \
			cmp(e, b, c, q); \
			if (e > 0) { \
				j++; \
				b = c; \
			} \
		} \
		cmp(e, a, b, q); \
		if (e <= 0) { \
			break; \
		} \
		t = q->tmp; \
		*t = *a; \
		*a = *b; \
		*b = *t; \
	} \
}

#define PROP_B_TYPED(name, cmp, type) \
void \
name(q, start, cookie) \
struct priq *q; \
size_t start; \
void *cookie; \
{ \
	size_t i, j; \
	type a; \
	type b; \
	type t; \
	int e; \
	if (q->n == 0) { \
		return; \
	} \
	for (j = start; j > 0; j = i) { \
		i = (j - 1) / 2; \
		a = (type)q->q + i; \
		b = (type)q->q + j; \
		cmp(e, a, b, q); \
		if (e <= 0) { \
			break; \
		} \
		t = q->tmp; \
		*t = *a; \
		*a = *b; \
		*b = *t; \
	} \
}
