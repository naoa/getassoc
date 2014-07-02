/*	$Id: ncs.c,v 1.29 2009/08/27 09:22:54 nis Exp $	*/

/*-
 * Copyright (c) 2009 Shingo Nishioka.
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
static char rcsid[] = "$Id: ncs.c,v 1.29 2009/08/27 09:22:54 nis Exp $";
#endif

#include <config.h>

#include <sys/types.h>
#include <sys/time.h>

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>

#include "nwam.h"

#include "util.h"
#include "priq.h"

#include <gifnoc.h>

/*#define	DBG	1 /* */
/* #define DBGxx	1 /* */
#define	LOGSC	1 /* */
/*#define	ARGMAX_SCAN	1 /* */
/*#define	ARGMAX_PRIQ	1 /* */
#define	ARGMAX_CACHE	1 /* */
/*#define	static  /* */

/*
  
  D = \{ d_1, d_2, ... , d_N \}
  
  
  C_0 = \{ c_1, c_2, ..., c_N \}   where c_i = \{ d_i \}
  calculate SC(c_i)
  calculate SC(c_i \cup c_j) for 1 <= i < j <= N
  
  
  for k = 1 to N - 1 do
    (c_x, c_y) = argmax({SC(c_x \cup c_y)} \over {SC(c_x) SC(c_y)})
    c_z = c_x \cup c_y
    C_k = C_{k - 1} - \{ c_x, c_y \} + \{ c_z \}
    calculate SC(c_z \cup c_i) for all c_i \in C_k where i /= z
  
  
  SC(c) = \Pi_{d \in C} P(d|c)
  
  P(d|c) = P(d)\Sum_t{P(t|d)P(t|c)} \over {P(t)}
 
  P(t|d)  relative frequency of a term t in a document d
  P(t|c)  relative frequency of a term t in a cluster c
  P(t)    relative frequency of a term t in the entire data set
 
******* ******* ******* ******* ******* ******* ******* ******* *******

 double *SC1
 _SC1 => SC(c\cup c) = SC(c)
 SC(c)  = \prod_{d\in c} P(d|c)

 double **SC2
 _SC2 => SC(C_x\cup C_y) \over {SC(C_x) SC(C_y)}
 SC(C_x\cup C_y)
        = \prod_{d\in (C_x\cup C_y)} P(d|(C_x\cup C_y))
        = \prod_{d\in C_x} P(d|(C_x\cup C_y)) *
          \prod_{d\in C_y} P(d|(C_x\cup C_y))

        = (\prod_{d\in C_x} P(d|C_x) * TF(.|C_x) / (TF(.|C_x) + TF(.|C_y)) +
                            P(d|C_y) * TF(.|C_y) / (TF(.|C_x) + TF(.|C_y)) )
        * (\prod_{d\in C_y} P(d|C_x) * TF(.|C_x) / (TF(.|C_x) + TF(.|C_y)) +
                            P(d|C_y) * TF(.|C_y) / (TF(.|C_x) + TF(.|C_y)) )

 c->freq_sum
 TF(.|C)

 *double _P
 P(d|(C_x\cup C_y)) 
        = \sum_t {P(t|d) P(t|(C_x\cup C_y))} \over P(t)
        = \sum_t {P(t|d) TF(t|C_x\cup C_y) / TF(.|(C_x\cup C_y))} \over P(t)
        = \sum_t {P(t|d) (TF(t|C_x) + TF(t|C_y)) / (TF(.|C_x) + TF(.|C_y)) } \over P(t)
        = \sum_t {P(t|d) TF(t|C_x) / (TF(.|C_x) + TF(.|C_y)) } \over P(t)
        + \sum_t {P(t|d) TF(t|C_y) / (TF(.|C_x) + TF(.|C_y)) } \over P(t)

        = \sum_t {P(t|d) TF(t|C_x) / TF(.|C_x) } \over P(t) * TF(.|C_x) / (TF(.|C_x) + TF(.|C_y))
        + \sum_t {P(t|d) TF(t|C_y) / TF(.|C_y) } \over P(t) * TF(.|C_y) / (TF(.|C_x) + TF(.|C_y))

        = P(d|C_x) * TF(.|C_x) / (TF(.|C_x) + TF(.|C_y))
        + P(d|C_y) * TF(.|C_y) / (TF(.|C_x) + TF(.|C_y))

 P(d|c) = \sum_t {P(t|d) P(t|c)} \over P(t)
        = \sum_t {P(t|d) TF(t|c) / TF(.|c)} \over P(t)
        = {1 / TF(.|c)} * {\sum_t {P(t|d) TF(t|c) } \over P(t)}

 P(t|d) = TF(t|d) / TF(.|d)
 P(t|c) = TF(t|c) / TF(.|c)
 P(t) = TF(t|D) / TF(.|D)

 */

struct syminfo_c {
	df_t n;
	struct syminfo *s;
	double weight;
	int height;
};

struct D {
	df_t qidx;		/* if leaf, points a document.
				 * also used to index P. */
	struct syminfo_c *qp;
				/* XXX always(qp - q == qidx)?  if so, can we omit this member? */
	struct D *next;
		/* cluster's document list.
		 * this list is updated dynamically, and
		 * at the end of clustering, all leavs are weaved into a list.
		 */
	df_t elem_num;
	freq_t freq_sum;
};

struct C {
	struct C *left, *right;
	struct D *doclist;
	df_t ndocs; 		/* <= length(doclist)  */ /* XXX elem_num ni henkou suru ? */
	freq_t freq_sum;
	double weight;
	df_t midx;		/* index of a element of the matrix, or -1 */
	int k;			/* temporary use */
};

union sc2_t {
#if defined ARGMAX_PRIQ
	size_t ridx;		/* index of priq (internal value) */
#endif
	double v;
};

#if defined ARGMAX_CACHE
struct cache {
	double maxval;
	df_t index, elem_num;
};
#endif

struct si_wrapper_t {
	df_t qidx, elem_num;
	freq_t freq_sum;
	struct syminfo *sp, *ep;
	idx_t wid;
};

struct csg {
	df_t N;
	double D_total_freq_sum;
	struct D *docs;
	struct C *clusters;
	struct C **xdim;
	double *_P;
	double *_SC1;
	union sc2_t *_SC2;
#if defined ARGMAX_PRIQ
	struct priq *Q;
#endif
#if defined ARGMAX_CACHE
	struct cache *_C;
#endif
};

/* P(d|c) */
/*#define	PIRC(d, c)	(assert((d) < g->N), assert((c) < g->N)), /* */
#define	PIRC(d, c)	/* */
#define	PI(d, c)	(PIRC(d, c) g->_P + (size_t) (d) * g->N + (c))
#define	P(d, c)		(PI((d)->qidx, (c)->midx))

/* SC(c) */
/*#define	SC1IRC(i)	(assert((i) < g->N)), /* */
#define	SC1IRC(i)	/* */
#define	SC1I(i)		(SC1IRC(i) g->_SC1 + (i))
#define	SC1(c)		(SC1I((c)->midx))

/* SC(x\cup y) \over SC(x)SC(y) */
/*#define	SC2IRC(j, i)	(assert((j) < g->N - 1), assert((i) < g->N)), /* */
#define	SC2IRC(j, i)	/* */
#define	SC2I(j, i)	(SC2IRC(j, i) g->_SC2 + (size_t) (j) * g->N + (i))
#define	SC2(y, x)	(SC2I((y)->midx, (x)->midx))
#if defined ARGMAX_PRIQ
#define	SC2Iridx(j, i)	(SC2I((i) - 1, (j)))
#define	SC2ridx(y, x)	(SC2Iridx((y)->midx, (x)->midx))
#define	SC2RI(e, y, x)	((y) = ((e) - g->_SC2) / g->N, (x) = ((e) - g->_SC2) % g->N)
#endif

static int ncs(struct csg *, struct syminfo_c *, df_t);
static void argmax(struct csg *, struct C **, struct C **);
static struct C *merge(struct csg *, df_t, struct C *, struct C *);
static double sc1(struct csg *, struct C *);
static double sc2(struct csg *, struct C *, struct C *);
static struct D *nconc(struct D *, struct D *);
static int init_p(struct csg *);
static void eject(struct csg *, struct si_wrapper_t *, size_t);
static int cont_compar(const void *, const void *);
static int init_sc1(struct csg *);
static int init_sc2(struct csg *);
static struct syminfo_c *mksl_wsh(struct syminfo *, df_t, WAM *, int, int, df_t);
static struct syminfo_c *mksl_TF(struct syminfo *, df_t, WAM *, int);
static int dsort(struct syminfo_c *, df_t, struct C *);
static struct cs_elem *mk_cslist(struct csg *, struct syminfo *, df_t, df_t *);
static int cccompar(const void *, const void *);
static struct syminfo *ejedoc(struct syminfo *, struct C *, df_t *);
static int fill_csw(struct cs_elem *, df_t, WAM *, int, int, df_t);

#if defined DBG
static void prnt(struct csg *, FILE *);
#if defined ARGMAX_PRIQ
static void dump_priq(struct csg *, FILE *, int);
#endif
#endif

#if defined ARGMAX_PRIQ
static priq_prop_t prop_f_ri_sc2compar;
static priq_prop_t prop_b_ri_sc2compar;
#endif

#if defined ARGMAX_CACHE
static void update_cache_line(struct csg *, df_t);
#endif

static struct cs_elem *ckm(struct csg *, struct syminfo *, df_t, struct syminfo_c *, df_t *);

#if 0
struct cs_elem *
csb(q, nq, bias, w, dir, c_type, wt_type, elemn, ncp, th, ckkn, rr)
struct syminfo *q, **rr;
df_t nq;
df_t elemn;
df_t *ncp;
df_t th, ckkn;
WAM *w;
{
	/* bias => 0 */
	/* th => 1 */
	return ncsb(q, nq, c_type, w, dir, wt_type, elemn, ncp, wt_type, ckkn);
}
#endif

struct cs_elem *
ncsb(q, qlen, c_type, w, dir, fv_type, elemn, cno, fw_type, cswmax)
struct syminfo *q;
df_t qlen;
WAM *w;
df_t elemn, *cno, cswmax;
{
	struct syminfo_c *sl;
	struct cs_elem *ce;
	df_t i, sllen;

	struct csg g0, *g = &g0;

	g->clusters = NULL;
	g->docs = NULL;

	switch (fv_type) {
	case WT_NONE:
		if (!(sl = mksl_TF(q, qlen, w, dir))) {
			return NULL;
		}
		break;
	default:
		if (!(sl = mksl_wsh(q, qlen, w, dir, fv_type, elemn))) {
			return NULL;
		}
	}
	sllen = qlen;

	switch (c_type) {
	case CS_HBC:
		if (ncs(g, sl, sllen) == -1) {
			return NULL;
		}
		if (!(ce = mk_cslist(g, q, qlen, cno))) {
			return NULL;
		}
		break;
	case CS_K_MEANS:
		if (!(ce = ckm(g, q, qlen, sl, cno))) {
			return NULL;
		}
		break;
	default:
		return NULL;
	}

	if (fw_type != WT_NONE && fill_csw(ce, *cno, w, dir, fw_type, cswmax) == -1) {
		return NULL;
	}

	for (i = 0; i < sllen; i++) {
		struct syminfo_c *sc = &sl[i];
		free(sc->s);
	}
/*
bzero(sl, qlen * sizeof *sl);
*/
	free(sl);

/*
bzero(g->clusters, (qlen * 2 - 1) * sizeof g->clusters);
bzero(g->docs, qlen * sizeof g->docs);
*/
	free(g->clusters);
	free(g->docs);

	return ce;
}

static int
ncs(g, sl, sllen)
struct csg *g;
struct syminfo_c *sl;
df_t sllen;
{
	struct C *z;
	df_t i, j, k;

	g->N = sllen;

#if defined ARGMAX_PRIQ
	if (!(g->Q = priq_creat_p(sizeof (union sc2_t *), NULL, prop_f_ri_sc2compar, prop_b_ri_sc2compar, g))) {
		return -1;
	}
#endif

	if (!(g->xdim = calloc(g->N, sizeof *g->xdim))) {
		return -1;
	}
	if (!(g->clusters = calloc(g->N * 2 - 1, sizeof *g->clusters))) {
		return -1;
	}
	if (!(g->docs = calloc(g->N, sizeof *g->docs))) {
		return -1;
	}

	g->D_total_freq_sum = 0;
	for (j = 0; j < g->N; j++) {
		struct C *c;
		struct D *d;
		freq_t freq_sum;
		struct syminfo_c *p = &sl[j];

		for (freq_sum = 0, i = 0; i < p->n; i++) {
			struct syminfo *e = &p->s[i];
			freq_sum += e->TF_d;
		}
		g->D_total_freq_sum += freq_sum;

		d = &g->docs[j];
		d->qidx = j;
		d->qp = p;
		d->next = NULL;
		d->elem_num = p->n;
		d->freq_sum = freq_sum;

		c = &g->clusters[j];
		c->left = c->right = NULL;
		c->k = 0;
		c->doclist = d;
		c->ndocs = 1;
		c->freq_sum = d->freq_sum;
		c->weight = sl[j].weight;
		c->midx = j;
		g->xdim[c->midx] = c;
#if defined DBG
fprintf(stderr, "xdim[%"PRIdSSIZE_T"] = %p\n", (pri_ssize_t)c->midx, c);
#endif
	}

	if (init_p(g) == -1) {
		return -1;
	}
	if (init_sc1(g) == -1) {
		return -1;
	}
	if (init_sc2(g) == -1) {
		return -1;
	}
#if defined ARGMAX_CACHE
	if (!(g->_C = malloc((g->N - 1) * sizeof *g->_C))) {
		return -1;
	}
	for (j = 0; j < g->N - 1; j++) {
		update_cache_line(g, j);
	}
#endif

	z = NULL;
	for (k = 0; k < g->N - 1; k++) {
		struct C *x, *y;
#if defined DBG
prnt(g, stderr);
#endif
		argmax(g, &x, &y);
#if defined DBG
fprintf(stderr, "merge\n");
#endif
		z = merge(g, k, x, y);
	}
#if defined DBG
prnt(g, stderr);
#endif

#if defined ARGMAX_PRIQ
	priq_free(g->Q);
#endif
#if defined ARGMAX_CACHE
/*
bzero(g->_C, (g->N - 1) * sizeof *g->_C);
*/
	free(g->_C);
#endif

	if (dsort(sl, sllen, z) == -1) {
		return -1;
	}

/*
bzero(g->xdim, g->N * sizeof *g->xdim);
bzero(g->_P, g->N * g->N * sizeof *g->_P);
bzero(g->_SC1, g->N * sizeof *g->_SC1);
bzero(g->_SC2, g->N * (g->N - 1) * sizeof *g->_SC2);
*/
	free(g->xdim);
	free(g->_P);
	free(g->_SC1);
	free(g->_SC2);
g->xdim = NULL;
g->_P = NULL;
g->_SC1 = NULL;
g->_SC2 = NULL;

	return 0;
}

static void
argmax(g, x, y)
struct csg *g;
struct C **x, **y;
{
#if defined ARGMAX_SCAN
	struct C *cj, *ci;
	double max;
	int first = 1;
	df_t j, i;

	*x = *y = NULL;
	for (j = 0; j < g->N - 1; j++) {
		if (!(cj = g->xdim[j])) {
			continue;
		}
/*assert(cj->midx == j);*/
		for (i = j + 1; i < g->N; i++) {
			double score;
			if (!(ci = g->xdim[i])) {
				continue;
			}
/*assert(ci->midx == i);*/
			score = SC2(cj, ci)->v;
			if (first || score > max) {
				*y = ci;
				*x = cj;
				max = score;
				first = 0;
			}
		}
	}
#endif
#if defined ARGMAX_PRIQ
	df_t j, i;
	union sc2_t *q = *(union sc2_t **)priq_top(g->Q);

	SC2RI(q, j, i);

#if defined DBG
fprintf(stderr, "ARGMAX: %"PRIdDF_T" %"PRIdDF_T"\n", j, i);
#endif
	*x = g->xdim[j];
	*y = g->xdim[i];
	/*assert(*x);
	assert(*y);*/
#endif
#if defined ARGMAX_CACHE
	int first = 1;
	struct cache *cp;
	df_t j, k = g->N;
	double maxval;
	for (j = 0; j < g->N - 1; j++) {
		if (!g->xdim[j]) continue;
		cp = &g->_C[j];
		if (cp->index == g->N) continue;
		if (first || maxval < cp->maxval) {
#if defined DBG
fprintf(stderr, "j = %"PRIdDF_T", cp->index = %"PRIdDF_T" g->xdim[j] = %p\n", j, cp->index, g->xdim[j]);
#endif
			first = 0;
			maxval = cp->maxval;
			k = j;
		}
	}
	assert(k != g->N);
	cp = &g->_C[k];
	*x = g->xdim[k];
	*y = g->xdim[cp->index];
#endif
}

/*
 * k  -- merged order  (0 .. N-1)
 * x, y --  cluster to be merged
 * z -- new cluster
*/
static struct C *
merge(g, k, x, y)
struct csg *g;
df_t k;
struct C *x, *y;
{
	df_t i, j;
	struct C *z;
	double w1, w2;
	struct C *l, *r;
#if defined ARGMAX_PRIQ || defined DBG
	struct C *u;
#endif
#if defined ARGMAX_CACHE
	struct cache *cp;
#endif
#if defined DBG
fprintf(stderr, "merge: %"PRIdDF_T" %p %p\n", k, x, y);
#endif

/*assert(0 <= k && g->N + k < g->N * 2 - 1);*/
	z = &g->clusters[g->N + k];
#if defined DBG
fprintf(stderr, "merge: %"PRIdDF_T" %"PRIdSSIZE_T" %"PRIdSSIZE_T"\n", k, (pri_size_t)x->midx, (pri_size_t)y->midx);
#endif

	z->k = 0;
	if (x->weight / x->ndocs < y->weight / y->ndocs) {
		l = y, r = x;
	}
	else {
		l = x, r = y;
	}
	z->doclist = nconc(l->doclist, r->doclist);
	z->left = l;
	z->right = r;
	z->ndocs = x->ndocs + y->ndocs;
	z->freq_sum = x->freq_sum + y->freq_sum;
	z->weight = x->weight + y->weight;

	z->midx = x->midx;		/* reuse x's area */

	w1 = (double)x->freq_sum / z->freq_sum;
	w2 = (double)y->freq_sum / z->freq_sum;

	for (j = 0; j < g->N; j++) {
		struct D *d = &g->docs[j];
		*P(d, z) = w1 * *P(d, x) + w2 * *P(d, y);
#if defined DBG
fprintf(stderr, "_P[%"PRIdSSIZE_T"][%"PRIdSSIZE_T"] = %12.6e\n", (pri_ssize_t)d->qidx, (pri_ssize_t)z->midx, *P(d, z));
#endif
		*P(d, y) = 0;
	}
	*SC1(y) = 0;

#if defined ARGMAX_PRIQ
#if defined DBG
dump_priq(g, stderr, 1);
#endif
#endif

	/* NOTE: hereafter, k is reused */
#if defined ARGMAX_PRIQ || defined DBG
	k = y->midx;
	for (j = 0; j < k; j++) {
		if (u = g->xdim[j]) {
			SC2(u, y)->v = 0;	/* for debug */
#if defined ARGMAX_PRIQ
#if defined DBG
fprintf(stderr, "PRIQ_DELETE_INDEX %p (%"PRIdDF_T",%"PRIdSSIZE_T") (%p %"PRIuSIZE_T")\n", SC2(u, y), j, (pri_ssize_t)y->midx, ((union sc2_t **)g->Q->q)[SC2ridx(u, y)->ridx], (pri_size_t)SC2ridx(u, y)->ridx);
#endif
			if (priq_delete_index(g->Q, SC2ridx(u, y)->ridx) == -1) {
				fprintf(stderr, "priq_delete_index failed\n");
				return NULL;
			}
#if defined DBG
dump_priq(g, stderr, 1);
#endif
#endif
		}
	}
	for (i = k + 1; i < g->N; i++) {
		if (u = g->xdim[i]) {
			SC2(y, u)->v = 0;	/* for debug */
#if defined ARGMAX_PRIQ
#if defined DBG
fprintf(stderr, "PRIQ_DELETE_INDEX %p (%"PRIdSSIZE_T",%"PRIdDF_T") (%p %"PRIuSIZE_T")\n", SC2(y, u), (pri_ssize_t)y->midx, i, ((union sc2_t **)g->Q->q)[SC2ridx(y, u)->ridx], (pri_size_t)SC2ridx(y, u)->ridx);
#endif
			if (priq_delete_index(g->Q, SC2ridx(y, u)->ridx) == -1) {
				fprintf(stderr, "priq_delete_index failed\n");
				return NULL;
			}
#if defined DBG
dump_priq(g, stderr, 1);
#endif
#endif
		}
	}
#endif

	*SC1(z) = sc1(g, z);
#if defined DBG
fprintf(stderr, "_SC1[%"PRIdSSIZE_T"] = %12.6e\n", (pri_ssize_t)z->midx, *SC1(z));
fprintf(stderr, "_SC1[%"PRIdSSIZE_T"] = %12.6e\n", (pri_ssize_t)y->midx, 0.0);
#endif

/*assert(x->midx == z->midx);
 assert(y->midx != z->midx);*/
	g->xdim[y->midx] = NULL;
#if defined DBG
fprintf(stderr, "xdim[%"PRIdSSIZE_T"] = NULL\n", (pri_ssize_t)y->midx);
#endif
	g->xdim[z->midx] = z;	/* z takes place of x */
#if defined DBG
fprintf(stderr, "xdim[%"PRIdSSIZE_T"] = %p\n", (pri_ssize_t)z->midx, z);
#endif

#if defined ARGMAX_CACHE
	k = y->midx;
	for (j = 0; j < k; j++) {
		if (!g->xdim[j]) continue;
/*
assert(j < g->N - 1);
*/
		cp = &g->_C[j];
		if (cp->index == k) {
			update_cache_line(g, j);
		}
	}
	if (k < g->N - 1) {
		cp = &g->_C[k];
		cp->maxval = 0;
		cp->index = g->N;
		cp->elem_num = 0;
	}
#endif

	x->midx = y->midx = -1;

	/* NOTE: hereafter, x, and y are reused */

	k = z->midx;

	x = g->xdim[k];
	/*assert(x);
	assert(x->midx == k);*/
	for (j = 0; j < k; j++) {
#if defined DBG
fprintf(stderr, "k=%"PRIdDF_T", j=%"PRIdDF_T"\n", k, j);
#endif
		if (y = g->xdim[j]) {
			/*assert(y->midx == j);*/
			double v = sc2(g, y, x);
			SC2(y, x)->v = v;
#if defined DBG
fprintf(stderr, "_SC2[%"PRIdDF_T"][%"PRIdDF_T"] = %12.6e\n", y->midx, x->midx, v);
#endif
#if defined ARGMAX_PRIQ
			priq_update_index(g->Q, SC2ridx(y, x)->ridx);
#endif
#if defined ARGMAX_CACHE
/*
assert(j < g->N - 1);
*/
			cp = &g->_C[j];
			if (cp->elem_num == 1) {
				assert(cp->index == x->midx);
				if (cp->maxval != v) {
					cp->maxval = v;
				}
			}
			else if (cp->maxval < v) {
				cp->maxval = v;
				cp->index = x->midx;
			}
			else if (cp->index == x->midx && v < cp->maxval) {
				update_cache_line(g, j);
			}
#endif
		}
	}

	y = g->xdim[k];
	/*assert(y);
	assert(y->midx == k);*/
	for (i = k + 1; i < g->N; i++) {
		if (x = g->xdim[i]) {
			/*assert(x->midx == i);*/
			double v = sc2(g, y, x);
			SC2(y, x)->v = v;
#if defined DBG
fprintf(stderr, "_SC2[%"PRIdDF_T"][%"PRIdDF_T"] = %12.6e\n", y->midx, x->midx, v);
#endif
#if defined ARGMAX_PRIQ
			priq_update_index(g->Q, SC2ridx(y, x)->ridx);
#endif
		}
	}
#if defined ARGMAX_CACHE
	update_cache_line(g, k);
#endif

	return z;
}

static double
sc1(g, z)
struct csg *g;
struct C *z;
{
	struct D *d;
	double u;
#if defined LOGSC
	u = 0;
#else
	u = 1;
#endif
#if defined DBG
fprintf(stderr, "z->doclist = (");
#endif
	for (d = z->doclist; d; d = d->next) {
#if defined DBG
fprintf(stderr, "%"PRIdDF_T" ", d->qidx);
#endif
#if defined LOGSC
		u += log(*P(d, z));
#else
		u *= *P(d, z);
#endif
	}
#if defined DBG
fprintf(stderr, ")\n");
#endif
	return u;
}

static double
sc2(g, x, y)
struct csg *g;
struct C *x, *y;
{
	double w1, w2, sc, freq_sum;
	struct D *d;

	freq_sum = x->freq_sum + y->freq_sum;
	w1 = x->freq_sum / freq_sum;
	w2 = y->freq_sum / freq_sum;
#if defined LOGSC
	sc = 0;
#else
	sc = 1;
#endif
	for (d = x->doclist; d; d = d->next) {
#if defined LOGSC
		sc += log(w1 * *P(d, x) + w2 * *P(d, y));
#else
		sc *= w1 * *P(d, x) + w2 * *P(d, y);
#endif
	}
	for (d = y->doclist; d; d = d->next) {
#if defined LOGSC
		sc += log(w1 * *P(d, x) + w2 * *P(d, y));
#else
		sc *= w1 * *P(d, x) + w2 * *P(d, y);
#endif
	}
#if defined LOGSC
	return sc - (*SC1(x) + *SC1(y));
#else
	return sc / (*SC1(x) * *SC1(y));
#endif
}

static struct D *
nconc(a, b)
struct D *a, *b;
{
	struct D *c;
	/*assert(a);*/
	for (c = a; c->next; c = c->next) ;
	c->next = b;
	return a;
}

static int
init_p(g)
struct csg *g;
{
	df_t i, j;
	size_t nh;
	struct si_wrapper_t *c, *h;
	struct priq *q;

	if (!(g->_P = malloc(g->N * g->N * sizeof *g->_P))) {
		syslog(LOG_DEBUG, "malloc: %s", strerror(errno));
		return -1;
	}
	for (j = 0; j < g->N; j++) {
		for (i = 0; i < g->N; i++) {
			*PI(j, i) = 0;
		}
	}

	if (!(c = calloc(g->N, sizeof *c))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return -1;
	}

	if (!(h = calloc(g->N, sizeof *h))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return -1;
	}

	if (!(q = priq_creat(sizeof (struct si_wrapper_t *), cont_compar, NULL))) {
		syslog(LOG_DEBUG, "priq_creat: %s", strerror(errno));
		return -1;
	}

	for (i = 0; i < g->N; i++) {
		struct si_wrapper_t *t = &c[i];
		struct D *d;

		d = &g->docs[i];
		t->qidx = i;
#if defined DBG
fprintf(stderr, "i = %"PRIdDF_T"\n", i);
assert(i == d->qidx);
assert(d->elem_num == d->qp->elem_num);
assert(d->freq_sum == g->clusters[i].freq_sum);
#endif
		t->elem_num = d->elem_num;
		t->freq_sum = d->freq_sum;
		t->sp = d->qp->s;
		t->ep = t->sp + t->elem_num;
		t->wid = t->sp->id;
		if (priq_enq(q, &t) == -1) {
			syslog(LOG_DEBUG, "priq_enq: %s", strerror(errno));
			return -1;
		}
	}

	nh = 0;
	while (!priq_emptyp(q)) {
		struct si_wrapper_t *t = *(struct si_wrapper_t **)priq_top(q);

		if (nh != 0 && t->wid != h[0].wid) {
			eject(g, h, nh);
			nh = 0;
		}

		h[nh++] = *t;

		if (++t->sp < t->ep) {
			t->wid = t->sp->id;
			priq_rplatop(q, &t);
		}
		else {
			priq_deq(q);
		}
	}

	if (nh != 0) {
		eject(g, h, nh);
	}

	priq_free(q);
	free(c);
	free(h);

	for (j = 0; j < g->N; j++) {
		for (i = j + 1; i < g->N; i++) {
			*PI(j, i) = *PI(i, j);
		}
	}
	return 0;
}

static void
eject(g, h, nh)
struct csg *g;
struct si_wrapper_t *h;
size_t nh;
{
	ssize_t i, j;
	freq_t d_freq_sum;
	d_freq_sum = 0;
	for (i = 0; i < nh; i++) {
		struct si_wrapper_t *hx = &h[i];
		d_freq_sum += hx->sp->TF_d;
	}
	for (i = 0; i < nh; i++) {
		for (j = 0; j <= i; j++) {
			struct si_wrapper_t *hx, *hy;
			double wx, wy;
			hx = &h[i];
			hy = &h[j];
			wx = (double)hx->sp->TF_d / hx->freq_sum;
			wy = (double)hy->sp->TF_d / hy->freq_sum;
			*PI(hx->qidx, hy->qidx) += wx * wy / ((double)d_freq_sum / g->D_total_freq_sum);
		}
	}
}

static int
cont_compar(va, vb)
const void *va;
const void *vb;
{
	const struct si_wrapper_t **a;
	const struct si_wrapper_t **b;

	a = (const struct si_wrapper_t **)va;
	b = (const struct si_wrapper_t **)vb;

	if ((*a)->wid == (*b)->wid) {
		if ((*a)->qidx == (*b)->qidx) {
			return 0;
		}
		else if ((*a)->qidx < (*b)->qidx) {
			return -1;
		}
		else {
			return 1;
		}
	}
	else if ((*a)->wid < (*b)->wid) {
		return -1;
	}
	else {
		return 1;
	}
}

static int
init_sc1(g)
struct csg *g;
{
	df_t i;

	if (!(g->_SC1 = malloc(g->N * sizeof *g->_SC1))) {
		syslog(LOG_DEBUG, "malloc: %s", strerror(errno));
		return -1;
	}

	for (i = 0; i < g->N; i++) {
#if defined LOGSC
		*SC1I(i) = log(*PI(i, i));
#else
		*SC1I(i) = *PI(i, i);
#endif
	}
	return 0;
}

static int
init_sc2(g)
struct csg *g;
{
	df_t j, i;
	if (!(g->_SC2 = malloc(g->N * (g->N - 1) * sizeof *g->_SC2))) {
		syslog(LOG_DEBUG, "malloc: %s", strerror(errno));
		return -1;
	}

	for (j = 0; j < g->N - 1; j++) {
		struct C *cj = g->xdim[j];
		for (i = j + 1; i < g->N; i++) {
			struct C *ci = g->xdim[i];
#if defined ARGMAX_PRIQ
			union sc2_t *v = SC2(cj, ci);
#endif
			SC2(cj, ci)->v = sc2(g, cj, ci);
#if defined ARGMAX_PRIQ
			SC2Iridx(j, i)->ridx = 0;
			if (priq_enq(g->Q, &v) == -1) {
				return -1;
			}
#endif
		}
	}
	return 0;
}

static struct syminfo_c *
mksl_wsh(q, qlen, w, dir, fv_type, elemn)
struct syminfo *q;
df_t qlen;
WAM *w;
df_t elemn;
{
	struct syminfo_c *sl;
	df_t i;
	df_t en;

	if (!(sl = calloc(qlen, sizeof *sl))) {
		return NULL;
	}
	for (i = 0; i < qlen; i++) {
		struct syminfo_c *sc = &sl[i];
		en = elemn;
		if (!(sc->s = wsh(&q[i], 1, w, dir, fv_type, &en, NULL, NULL))) {
			syslog(LOG_DEBUG, "wsh: %s", strerror(errno));
			return NULL;
		}
		qsort(sc->s, en, sizeof *sc->s, n_sym_icompar);

		sc->n = en;
		sc->weight = q[i].weight;
		sc->height = 0;
	}
	return sl;
}

static struct syminfo_c *
mksl_TF(q, qlen, w, dir)
struct syminfo *q;
df_t qlen;
WAM *w;
{
	struct syminfo_c *sl;
	df_t i, j;
	df_t en;

	if (!(sl = calloc(qlen, sizeof *sl))) {
		return NULL;
	}
	for (i = 0; i < qlen; i++) {
		struct syminfo_c *sc = &sl[i];
		struct xr_vec const *v;
		if ((en = wam_get_vec(w, dir, q[i].id, &v)) == 0) {
			syslog(LOG_DEBUG, "wam_get_vec: %s", strerror(errno));
			return NULL;
		}
		if (!(sc->s = calloc(v->elem_num, sizeof *sc->s))) {
			syslog(LOG_DEBUG, "wsh: %s", strerror(errno));
			return NULL;
		}
		for (j = 0; j < en; j++) {
			sc->s[j].id = v->elems[j].id;
			sc->s[j].TF = 0;
			sc->s[j].TF_d = 0;
			sc->s[j].DF = 0;
			sc->s[j].DF_d = 0;
			sc->s[j].weight = v->elems[j].freq;
			sc->s[j].v = NULL;
		}

		qsort(sc->s, en, sizeof *sc->s, n_sym_icompar);

		sc->n = en;
		sc->weight = q[i].weight;
		sc->height = 0;
	}
	return sl;
}

static int
dsort(sl, sllen, root)
struct syminfo_c *sl;
df_t sllen;
struct C *root;
{
	struct syminfo_c *tmp;
	df_t i;
	struct D *d;

#if defined DBG
fprintf(stderr, "dsort = (");
#endif
	if (!(tmp = calloc(sllen, sizeof *tmp))) {
		return -1;
	}
	for (i = 0, d = root->doclist; d; d = d->next, i++) {
		tmp[i] = sl[d->qidx];
#if defined DBG
fprintf(stderr, " %"PRIdDF_T, d->qidx);
#endif
		/*assert(i < sllen);*/
	}
#if defined DBG
fprintf(stderr, ")\n");
#endif
	/*assert(i == sllen);*/
	memcpy(sl, tmp, sllen * sizeof *sl);
	free(tmp);
	return 0;
}

static struct cs_elem *
mk_cslist(g, q, qlen, cno)
struct csg *g;
struct syminfo *q;
df_t qlen, *cno;
{
	df_t step, i;
	struct C **cc;
	size_t xcno;
	struct cs_elem *ce;

	if (!(cc = calloc(*cno, sizeof *cc))) {
		return NULL;
	}

/* XXX step ga minus ni natteha ikenai!!!! */
	for (i = 0, step = 2 * g->N - 2; i + 1 < *cno; i++, step--) {
		struct C *c = &g->clusters[step];
		c->k = -1;
	}
	xcno = 0;
/* XXX step ga minus ni natteha ikenai!!!! */
	for (i = 0, step = 2 * g->N - 2; i + 1 < *cno; i++, step--) {
		struct C *c = &g->clusters[step];
		if (c->left->k != -1) {
			cc[xcno++] = c->left;
		}
		if (c->right->k != -1) {
			cc[xcno++] = c->right;
		}
	}
	/*assert(xcno <= *cno);*/

	qsort(cc, xcno, sizeof *cc, cccompar);

	if (!(ce = calloc(*cno, sizeof *ce))) {
		return NULL;
	}

	for (i = 0; i < *cno; i++) {
		struct syminfo *a;
		df_t alen;
		if (!(a = ejedoc(q, cc[i], &alen))) {
			return NULL;
		}
#if defined DBG
 {df_t j;
fprintf(stderr, "alen = %"PRIdDF_T", docs =", alen);
for (j = 0; j < alen; j++) {
fprintf(stderr, " %"PRIuIDX_T, a[j].id);
}
fprintf(stderr, "\n");
}
#endif
		ce[i].csa.s = a;
		ce[i].csa.n = alen;
		ce[i].csw.s = NULL;
		ce[i].csw.n = 0;
	}

	free(cc);
	return ce;
}

static int
cccompar(va, vb)
const void *va;
const void *vb;
{
	struct C const **a = (struct C const **)va;
	struct C const **b = (struct C const **)vb;

	if ((*a)->doclist->qidx < (*b)->doclist->qidx) {
		return -1;
	}
	else if ((*a)->doclist->qidx > (*b)->doclist->qidx) {
		return 1;
	}
	return 0;
}

static struct syminfo *
ejedoc(q, c, alen)
struct syminfo *q;
struct C *c;
df_t *alen;
{
	df_t j;
	struct D *d;
	struct syminfo *r;
	if (!(r = calloc(c->ndocs, sizeof *r))) {
		return NULL;
	}
#if defined DBG
fprintf(stderr, "ndocs = %"PRIdDF_T", docs =", c->ndocs);
#endif
	for (j = 0, d = c->doclist; j < c->ndocs; j++) {
		/*assert(d);*/
		r[j] = q[d->qidx];
#if defined DBG
fprintf(stderr, " (%"PRIdDF_T")%"PRIuIDX_T, d->qidx, r[j].id);
#endif
		d = d->next;
	}
#if defined DBG
fprintf(stderr, "\n");
#endif
	*alen = c->ndocs;
	return r;
}

static int
fill_csw(ce, cno, w, dir, fw_type, cswmax)
struct cs_elem *ce;
WAM *w;
df_t cno, cswmax;
{
	df_t i;

	for (i = 0; i < cno; i++) {
		struct syminfo *a, *r;
		df_t alen, rlen;
		a = ce[i].csa.s;
		alen = ce[i].csa.n;
		rlen = cswmax;
		if (!(r = wsh(a, alen, w, dir, fw_type, &rlen, NULL, NULL))) {
			syslog(LOG_DEBUG, "wsh: %s", strerror(errno));
			return -1;
		}
#if defined DBG
 {int j;
fprintf(stderr, "rlen = %"PRIdDF_T", docs =", rlen);
for (j = 0; j < rlen; j++) {
fprintf(stderr, " %"PRIuIDX_T, r[j].id);
}
fprintf(stderr, "\n");
}
#endif
		ce[i].csw.s = r;
		ce[i].csw.n = rlen;
	}
	return 0;
}

#if defined DBG
static void
prnt(g, stream)
struct csg *g;
FILE *stream;
{
	df_t i, j;

	fprintf(stream, "c.fr: ");
	for (i = 0; i < g->N; i++) fprintf(stream, "%12"PRIdFREQ_T" ", g->xdim[i] ? g->xdim[i]->freq_sum : 0);
	fprintf(stream, "\n");
	fprintf(stream, "c.nd: ");
	for (i = 0; i < g->N; i++) fprintf(stream, "%12"PRIdDF_T" ", g->xdim[i] ? g->xdim[i]->ndocs : 0);
	fprintf(stream, "\n");
	fprintf(stream, "_SC1: ");
	for (i = 0; i < g->N; i++) fprintf(stream, "%12.6e ", *SC1I(i));
	fprintf(stream, "\n");

	fprintf(stream, "_P:   ");
	for (i = 0; i < g->N; i++) fprintf(stream, "         %c%2"PRIdDF_T" ", g->xdim[i] ? ' ' : 'x', i);
	fprintf(stream, "\n");
	for (i = 0; i < g->N; i++) {
		fprintf(stream, "%c %2"PRIdDF_T": ", g->xdim[i] ? ' ' : 'x', i);
		for (j = 0; j < g->N; j++) {
			fprintf(stream, "%12.6e ", *PI(i, j));
		}
		fprintf(stream, "\n");
	}

	fprintf(stream, "_SC2:\n");
	for (j = 0; j < g->N - 1; j++) {
		fprintf(stream, "%c %2"PRIdDF_T": ", g->xdim[j] ? ' ' : 'x', j);
		for (i = 0; i <= j; i++) {
#if defined ARGMAX_PRIQ
			fprintf(stream, "%12"PRIuSIZE_T" ", (pri_size_t)SC2I(j, i)->ridx);
#else
			fprintf(stream, "------------ ");
#endif
		}
		for (i = j + 1; i < g->N; i++) {
			fprintf(stream, "%12.6f ", SC2I(j, i)->v);
		}
		fprintf(stream, "\n");
	}
#if defined ARGMAX_PRIQ
	fprintf(stream, "PRIQ:\n");
	dump_priq(g, stream, 0);
#endif
#if defined ARGMAX_CACHE
	fprintf(stream, "CACHE:");
	for (i = 0; i < g->N - 1; i++) fprintf(stream, "%12.6e ", g->_C[i].maxval);
	fprintf(stream, "\n");
	fprintf(stream, "CACHE:");
	for (i = 0; i < g->N - 1; i++) fprintf(stream, "%12"PRIdDF_T" ", g->_C[i].index);
	fprintf(stream, "\n");
	fprintf(stream, "CACHE:");
	for (i = 0; i < g->N - 1; i++) fprintf(stream, "%12"PRIdDF_T" ", g->_C[i].elem_num);
	fprintf(stream, "\n");
#endif
	fprintf(stream, "--\n");
}

#if defined ARGMAX_PRIQ
static void
dump_priq(g, stream, nl)
struct csg *g;
FILE *stream;
{
	size_t i;
	int level = 1;
	int rest = level;
	fprintf(stream, "priq: n = %"PRIuSIZE_T"\n", (pri_size_t)priq_nmemb(g->Q));
	for (i = 0; i < priq_nmemb(g->Q); i++) {
		union sc2_t *p = ((union sc2_t **)g->Q->q)[i];
		size_t x, y;
		SC2RI(p, y, x);
		fprintf(stream, "{%"PRIuSIZE_T" %f %p, (%"PRIuSIZE_T",%"PRIuSIZE_T")} ", (pri_size_t)i, p->v, p, (pri_size_t)y, (pri_size_t)x);
if (nl) {
	rest--;
	if (rest == 0) {
		fprintf(stream, "\n");
		level += level;
		rest = level;
	}
}
	}
	fprintf(stream, "\n");
}
#endif
#endif

#if defined ARGMAX_PRIQ

#define	RIDX(p)		(SC2RI(*p, y, x), &SC2Iridx(y, x)->ridx)

#define	compar_sc2compar(e, a, b, q) \
{ \
	union sc2_t **va; \
	union sc2_t **vb; \
\
	va = a; \
	vb = b; \
\
	if ((*va)->v < (*vb)->v) { \
		e = 1; /* inverse order */ \
	} \
	else if ((*va)->v > (*vb)->v)  { \
		e = -1; \
	} \
	else { \
		e = 0; \
	} \
}

#define	PROP_F_TYPED_RI(name, cmp, type, fnrx) \
void \
name(q, start, cookie) \
struct priq *q; \
size_t start; \
void *cookie; \
{ \
	struct csg *g = cookie; \
	size_t y, x; \
	size_t i, j, n; \
	type a; \
	type b; \
	type t; \
	int e; \
	n = q->n; \
	a = (type)q->q + start; \
	*fnrx(a) = a - (type)q->q; \
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
		*fnrx(a) = a - (type)q->q; \
		*fnrx(b) = b - (type)q->q; \
	} \
}

#define	PROP_B_TYPED_RI(name, cmp, type, fnrx) \
void \
name(q, start, cookie) \
struct priq *q; \
size_t start; \
void *cookie; \
{ \
	struct csg *g = cookie; \
	size_t y, x; \
	size_t i, j; \
	type a; \
	type b; \
	type t; \
	int e; \
	if (q->n == 0) { \
		return; \
	} \
	a = (type)q->q + start; \
	*fnrx(a) = a - (type)q->q; \
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
		*fnrx(a) = a - (type)q->q; \
		*fnrx(b) = b - (type)q->q; \
	} \
}

static PROP_F_TYPED_RI(prop_f_ri_sc2compar, compar_sc2compar, union sc2_t **, RIDX)
static PROP_B_TYPED_RI(prop_b_ri_sc2compar, compar_sc2compar, union sc2_t **, RIDX)

#endif

#if defined ARGMAX_CACHE
static void
update_cache_line(g, j)
struct csg *g;
df_t j;
{
	struct cache *cp;
	df_t i, idx = g->N, elem_num;
	double v, maxval = 0;
	int first = 1;

#if defined DBG
	fprintf(stderr, "update_cache_line: %"PRIdDF_T"\n", j);
#endif

/*
assert(g->xdim[j]);
assert(j < g->N - 1);
*/
	cp = &g->_C[j];

	elem_num = 0;
	for (i = j + 1; i < g->N; i++) {
		if (!g->xdim[i]) continue;
		v = SC2I(j, i)->v;
		if (first || maxval < v) {
			maxval = v;
			idx = i;
			first = 0;
		}
		elem_num++;
	}
	cp->maxval = maxval;
	cp->index = idx;
	cp->elem_num = elem_num;
#if defined DBG
	fprintf(stderr, "update_cache_line: %"PRIdDF_T": %"PRIdDF_T" %f\n", j, idx, maxval);
#endif
}
#endif

/*
 * K-means method
 */

struct scluster {
	df_t n;			/* # of articles of this cluster */
	struct syminfo_c **a;	/* articles of this cluster * is a pointer to `sl' */
};

struct cluster {
	struct syminfole centre;
	struct scluster c, n;
};

struct clusters {
	df_t cno;
	struct cluster *c;
};

struct qcont_t {
	struct syminfo *s;
	df_t n, i;
};

static df_t find_nearest(struct syminfo_c *, struct clusters *);
static double distance(struct syminfo *, df_t, struct syminfo *, df_t);
struct syminfo *calc_centre(struct syminfo_c **, df_t, df_t *);
static priq_prop_t prop_f_qccompar;
static priq_prop_t prop_b_qccompar;

static struct cs_elem *
ckm(g, q, qlen, sl, cno)
struct csg *g;
struct syminfo *q;
df_t qlen, *cno;
struct syminfo_c *sl;
{
#if defined DBGxx
#endif
#define	MAXLOOP	128
	df_t x, i;
	int differ, count = 0;
	struct clusters cs0, *cs = &cs0;
	struct cs_elem *ce;

	cs->cno = *cno;
	if (!(cs->c = calloc(cs->cno, sizeof *cs->c))) {
		perror("calloc");
		return NULL;
	}

	for (x = 0; x < cs->cno; x++) {
		struct cluster *c;
		c = &cs->c[x];
		c->centre.s = NULL;
		c->centre.n = 0;
		c->c.n = 0;
		if (!(c->c.a = calloc(qlen, sizeof *c->c.a))) {
			perror("calloc");
			return NULL;
		}
		c->n.n = 0;
		if (!(c->n.a = calloc(qlen, sizeof *c->n.a))) {
			perror("calloc");
			return NULL;
		}
	}
#if defined DBGxx
fprintf(stderr, "input vectors:\n");
#endif

	/* Initial grouping : c->c */
	for (i = 0, x = 0; i < qlen; i++) {
		struct cluster *c;
		struct syminfo_c *sp;
		c = &cs->c[x];
		sp = &sl[i];
		qsort(sp->s, sp->n, sizeof *sp->s, n_sym_icompar);

		if (sp->n > 0 && sp->s[0].id == 0) {
			fprintf(stderr, "assertion failed\n");
			return NULL;
		}
#if defined DBGxx
print_syminfo(sp->s, sp->n, 0, 8, NULL, 0, stderr);
#endif
		c->c.a[c->c.n++] = sp;
		if (++x >= cs->cno) {
			x = 0;
		}
	}

	do {
		/* calculate centre : c->c */
#if defined DBGxx
fprintf(stderr, "centre:\n");
#endif
		for (x = 0; x < cs->cno; x++) {
			struct cluster *c;
			c = &cs->c[x];
			free(c->centre.s);
			if (!(c->centre.s = calc_centre(c->c.a, c->c.n, &c->centre.n))) {
				return NULL;
			}
#if defined DBGxx
print_syminfo(c->centre.s, c->centre.n, 0, 8, NULL, 0, stderr);
#endif
		}

		/* clear next stage : c->n */
		for (x = 0; x < cs->cno; x++) {
			struct cluster *c;
			c = &cs->c[x];
			c->n.n = 0;
		}

		/* new grouping : c->n */
		for (i = 0; i < qlen; i++) {
			struct cluster *c;
			struct syminfo_c *sp;
			sp = &sl[i];
			x = find_nearest(sp, cs);
			c = &cs->c[x];
			c->n.a[c->n.n++] = sp;
		}

#if defined DBGxx

fprintf(stderr, "before:\n");
	for (x = 0; x < cs->cno; x++) {
		struct cluster *c;
		c = &cs->c[x];
		fprintf(stderr, "C%ld: ", (long)x);
		for (i = 0; i < c->c.n; i++) {
			struct syminfo_c *sp;
			sp = c->c.a[i];
			fprintf(stderr, "%ld ", (long)q[sp - sl].id);
		}
		fprintf(stderr, "\n");
	}

fprintf(stderr, "after:\n");
	for (x = 0; x < cs->cno; x++) {
		struct cluster *c;
		c = &cs->c[x];
		fprintf(stderr, "C%ld: ", (long)x);
		for (i = 0; i < c->n.n; i++) {
			struct syminfo_c *sp;
			sp = c->n.a[i];
			fprintf(stderr, "%ld ", (long)q[sp - sl].id);
		}
		fprintf(stderr, "\n");
	}

#endif

		/* compare */
		differ = 0;
		for (x = 0; x < cs->cno; x++) {
			struct cluster *c;
			c = &cs->c[x];
			if (c->c.n != c->n.n) {
				differ = 1;
				goto brk;
			}
			for (i = 0; i < c->c.n; i++) {
				struct syminfo_c *sp, *tp;
				sp = c->c.a[i];
				tp = c->n.a[i];
				if (sp != tp) {
					differ = 1;
					goto brk;
				}
			}
		}
	brk:
#ifdef DBGxx
		fprintf(stderr, "DIFFER: %d\n", differ);
#endif

		/* swap : c->n :: c->c */
		for (x = 0; x < cs->cno; x++) {
			struct cluster *c;
			struct scluster t;
			c = &cs->c[x];
			t = c->n;
			c->n = c->c;
			c->c = t;
		}

	} while (differ && ++count < MAXLOOP);

	/* pullup the result : c->c */
	if (!(ce = calloc(cs->cno, sizeof *ce))) {
		return NULL;
	}
	for (x = 0; x < cs->cno; x++) {
		struct syminfo *a;
		struct cluster *c;
		struct cs_elem *e;

		c = &cs->c[x];
		e = &ce[x];

		if (!(a = calloc(c->c.n, sizeof *a))) {
			return NULL;
		}
		for (i = 0; i < c->c.n; i++) {
			struct syminfo *se = &a[i];
			struct syminfo_c *sp;
			sp = c->c.a[i];
			se->id = q[sp - sl].id;
			se->v = NULL;
			se->TF_d = 0;
			se->weight = 0;
		}

		e->csa.s = a;
		e->csa.n = c->c.n;
		e->csw.s = NULL;
		e->csw.n = 0;
	}

	/* cleanup */
	for (x = 0; x < cs->cno; x++) {
		struct cluster *c;
		c = &cs->c[x];
		free(c->centre.s);
		free(c->c.a);
		free(c->n.a);
	}
	free(cs->c);

	return ce;
}

static df_t
find_nearest(a, cs)
struct syminfo_c *a;
struct clusters *cs;
{
	df_t x, nearest;
	double min;
	nearest = 0;
	for (x = 0; x < cs->cno; x++) {
		struct cluster *c;
		double d;
		c = &cs->c[x];
		d = distance(a->s, a->n, c->centre.s, c->centre.n);
		if (x == 0 || min > d) {
			min = d;
			nearest = x;
		}
	}

	return nearest;
}

/* euclid distance ? */
static double
distance(a, na, b, nb)
struct syminfo *a, *b;
df_t na, nb;
{
	double l, d;
	df_t i, j;

	l = 0;
	for (i = j = 0; i < na && j < nb; ) {
		if (a[i].id == b[j].id) {
			d = a[i].weight - b[j].weight; i++, j++;
		}
		else if (a[i].id < b[j].id) {
			d = a[i].weight; i++;
		}
		else {
			d = b[j].weight; j++;
		}
		l += d * d;

	}
	while (i < na) {
		d = a[i].weight; i++;
		l += d * d;
	}
	while (j < nb) {
		d = b[j].weight; j++;
		l += d * d;
	}
	return sqrt(l);
}

/* arithmetic mean */
struct syminfo *
calc_centre(a, na, clen)
struct syminfo_c **a;
df_t na, *clen;
{
	df_t i;
	df_t n, j;
	struct syminfo *o = NULL;
	size_t size = 0;
	struct priq *q;
	struct qcont_t *cont;
	idx_t lastid;

	cont = calloc(na, sizeof *cont);
	if (!(q = priq_creat_p(sizeof (struct qcont_t *), NULL, prop_f_qccompar, prop_b_qccompar, NULL))) {
		return NULL;
	}
	for (i = 0; i < na; i++) {
		struct qcont_t *t = &cont[i];
		struct syminfo_c *ai;
		ai = a[i];
		t->s = ai->s;
		t->n = ai->n;
		t->i = 0;
		if (priq_enq(q, &t) == -1) {
			return NULL;
		}
	}
	lastid = 0;
	n = -1;
	while (!PRIQ_EMPTYP(q)) {
		struct qcont_t *t = *(struct qcont_t **)PRIQ_TOP(q);
		struct syminfo *e;

		e = &t->s[t->i];
		if (lastid != e->id) {
			n++;
			lastid = e->id;
			if (size <= n) {
				void *new;
				size_t newsize = n;
				newsize = NA(newsize + 1, 64);
				if (!(new = realloc(o, newsize * sizeof *o))) {
					free(o);
					syslog(LOG_DEBUG, "realloc: %s", strerror(errno));
					return NULL;
				}
				size = newsize;
				o = new;
			}
			o[n].id = e->id;
			o[n].TF = 0;
			o[n].TF_d = 0;
			o[n].DF = 0;
			o[n].DF_d = 0;
			o[n].weight = 0;
			o[n].v = NULL;
		}
		assert(n >= 0);
		o[n].weight += e->weight;

		if (++t->i < t->n) {
			priq_rplatop(q, &t);
		}
		else {
			priq_deq(q);
		}
	}

	for (j = 0; j < n; j++) {
		o[j].weight /= na;
	}

	priq_free(q);
	free(cont);

	*clen = n;
	return o;
}

#define compar_qccompar(e, a, b, q) \
{ \
	struct qcont_t * const *va; \
	struct qcont_t * const *vb; \
	struct qcont_t *wa; \
	struct qcont_t *wb; \
 \
	va = a; \
	vb = b; \
	wa = *a; \
	wb = *b; \
 \
	if (wa->s[wa->i].id == wb->s[wb->i].id) { \
		e = 0; \
	} \
	else if (wa->s[wa->i].id < wb->s[wb->i].id) { \
		e = -1; \
	} \
	else { \
		e = 1; \
	} \
}

static PROP_F_TYPED(prop_f_qccompar, compar_qccompar, struct qcont_t **)
static PROP_B_TYPED(prop_b_qccompar, compar_qccompar, struct qcont_t **)
