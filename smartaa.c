/*	$Id: smartaa.c,v 1.17 2011/09/14 02:36:16 nis Exp $	*/

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
static char rcsid[] = "$Id: smartaa.c,v 1.17 2011/09/14 02:36:16 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <assert.h>
#include <errno.h>
#if HAVE_IEEEFP_H
#include <ieeefp.h>
#endif
#include <libgen.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "nwam.h"

#include "priq.h"

#include <gifnoc.h>

#if ! defined ENABLE_GETA
#define	sym_icompar	n_sym_icompar
#endif

struct qvec {
	freq_t TF;
	struct syminfole *sl;
	double aveTF, logAveTF1p;
	struct syminfo *top, *tail, *prev;
	double idf;
	df_t idx;
};

static int ccompar(const void *, const void *);

double
smartaa(q, qlen, TF_q, r, rlen, TF_r, N, TF)
struct syminfo *q, *r;
df_t qlen, rlen, N;
freq_t TF_q, TF_r, TF;
{
	struct syminfo *qe, *re;
	double s, aveTFq, aveTFr, avelen;
	double Slope = 0.2;

	qe = q + qlen;
	re = r + rlen;

	for (s = 0; ; q++, r++) {
		double wq, wr, idf;
		while (q < qe && r < re && q->id != r->id) {
			while (q->id < r->id && q < qe) q++;
			while (r->id < q->id && r < re) r++;
		}
		if (q == qe || r == re) break;
		wq = q->weight;
		wr = r->weight;
		idf = log1p((double)N / q->DF);
		s += wq * wr * idf;
	}
	aveTFq = (double)TF_q / qlen;
	aveTFr = (double)TF_r / rlen;
	avelen = (double)TF / N;	/* average document length (TF) */
	s /= (avelen * (1.0 - Slope) + rlen * Slope) * (1.0 + log(aveTFq)) * (1.0 + log(aveTFr));
	return s;
}

double **
smartmtrxQ(sl1, nsl1, sl2, nsl2, N, TF)
struct syminfole *sl1, *sl2;
df_t nsl1, nsl2, N;
freq_t TF;
{
	idx_t id0;
	struct priq *a1, *a2;
	double **mtrx;
	df_t i, j;
	struct qvec *qv1, *qv2;
	double norm, avelen;
	double Slope = 0.2;
	struct qvec **h1, **h2;
	df_t nh1, nh2;

	if (!(a1 = priq_creat(sizeof *&qv1, ccompar, NULL))) {
		return NULL;
	}

	if (!(a2 = priq_creat(sizeof *&qv2, ccompar, NULL))) {
		return NULL;
	}

	if (!(qv1 = calloc(nsl1, sizeof *qv1))) {
		return NULL;
	}

	if (!(qv2 = calloc(nsl2, sizeof *qv2))) {
		return NULL;
	}

	for (i = 0; i < nsl1; i++) {
		struct syminfo *q;
		df_t qlen;
		freq_t TFq;
		df_t k;
		struct syminfole *s = &sl1[i];
		struct qvec *qv = &qv1[i];
		qv->idx = i;
		qsort(s->s, s->n, sizeof *s->s, sym_icompar);
		qv->sl = s;
		q = s->s;
		qlen = s->n;
		for (TFq = 0, k = 0; k < qlen; k++) {
			struct syminfo *p = &q[k];
			TFq += p->TF_d;
			p->weight = 1.0 + log(p->TF_d);
			p->u.idf = log1p((double)N / p->DF);
		}
		qv->TF = TFq;
		qv->aveTF = (double)qv->TF / s->n;
		qv->logAveTF1p = 1.0 + log(qv->aveTF);
		qv->top = s->s;
		qv->tail = qv->top + s->n;
		if (qv->top < qv->tail && priq_enq(a1, &qv) == -1) {
			return NULL;
		}
	}

	for (i = 0; i < nsl2; i++) {
		struct syminfo *q;
		df_t qlen;
		freq_t TFq;
		df_t k;
		struct syminfole *s = &sl2[i];
		struct qvec *qv = &qv2[i];
		qv->idx = i;
		qsort(s->s, s->n, sizeof *s->s, sym_icompar);
		qv->sl = s;
		q = s->s;
		qlen = s->n;
		for (TFq = 0, k = 0; k < qlen; k++) {
			struct syminfo *p = &q[k];
			TFq += p->TF_d;
			p->weight = 1.0 + log(p->TF_d);
			p->u.idf = log1p((double)N / p->DF);
		}
		qv->TF = TFq;
		qv->aveTF = (double)qv->TF / s->n;
		qv->logAveTF1p = 1.0 + log(qv->aveTF);
		qv->top = s->s;
		qv->tail = qv->top + s->n;
		if (qv->top < qv->tail && priq_enq(a2, &qv) == -1) {
			return NULL;
		}
	}

	if (!(mtrx = calloc(nsl1, sizeof *mtrx))) {
		return NULL;
	}
	for (i = 0; i < nsl1; i++) {
		if (!(mtrx[i] = calloc(nsl2, sizeof *mtrx[i]))) {
			return NULL;
		}
		for (j = 0; j < nsl2; j++) {
			mtrx[i][j] = 0.0;
		}
	}

	if (!(h1 = calloc(nsl1 + 1, sizeof *h1))) {
		return NULL;
	}
	if (!(h2 = calloc(nsl1 + 1, sizeof *h2))) {
		return NULL;
	}

	while (!priq_emptyp(a1) && !priq_emptyp(a2)) {
		struct qvec *t1, *t2;
		t1 = *(struct qvec **)priq_top(a1);
		t2 = *(struct qvec **)priq_top(a2);

		for (; t1->top->id != t2->top->id; ) {
			for (id0 = t2->top->id; t1->top->id < id0; ) {
				if (++t1->top < t1->tail) {
					priq_rplatop(a1, &t1);
				}
				else if (priq_deq(a1) == 0) {
					goto end;
				}
				t1 = *(struct qvec **)priq_top(a1);
			}
			for (id0 = t1->top->id; t2->top->id < id0; ) {
				if (++t2->top < t2->tail) {
					priq_rplatop(a2, &t2);
				}
				else if (priq_deq(a2) == 0) {
					goto end;
				}
				t2 = *(struct qvec **)priq_top(a2);
			}
		}

		assert(t1->top->id == t2->top->id);
		id0 = t1->top->id;

		for (nh1 = 0; t1->top->id == id0; ) {
			h1[nh1++] = t1;
			t1->prev = t1->top;
			if (++t1->top < t1->tail) {
				priq_rplatop(a1, &t1);
			}
			else if (priq_deq(a1) == 0) {
				break;
			}
			t1 = *(struct qvec **)priq_top(a1);
		}

		for (nh2 = 0; t2->top->id == id0; ) {
			h2[nh2++] = t2;
			t2->prev = t2->top;
			if (++t2->top < t2->tail) {
				priq_rplatop(a2, &t2);
			}
			else if (priq_deq(a2) == 0) {
				break;
			}
			t2 = *(struct qvec **)priq_top(a2);
		}

		for (i = 0; i < nh1; i++) {
			double s, wq, wr, idf, w;
			struct qvec *q1 = h1[i];
			wq = q1->prev->weight;
			idf = q1->prev->u.idf;
			w = wq * idf;
			for (j = 0; j < nh2; j++) {
				struct qvec *q2 = h2[j];
				wr = q2->prev->weight;
				s = w * wr;
				mtrx[q1->idx][q2->idx] += s;
			}
		}
	}

end:

	avelen = (double)TF / N;	/* average document length (TF) */
	for (i = 0; i < nsl1; i++) {
		struct qvec *v1 = &qv1[i];
		for (j = 0; j < nsl2; j++) {
			struct qvec *v2 = &qv2[j];
			norm = (avelen * (1.0 - Slope) + v2->sl->n * Slope) * v1->logAveTF1p * v2->logAveTF1p;
			mtrx[i][j] /= norm;
		}
	}

	free(qv1);
	free(qv2);
	priq_free(a1);
	priq_free(a2);
	free(h1);
	free(h2);
	return mtrx;
}

static int
ccompar(va, vb)
const void *va;
const void *vb;
{
	struct qvec const * const *a;
	struct qvec const * const *b;
	a = va;
	b = vb;
	if ((*a)->top->id < (*b)->top->id) {
		return -1;
	}
	if ((*a)->top->id > (*b)->top->id) {
		return 1;
	}
	return 0;
}
