/*	$Id: assv.c,v 1.67 2010/12/08 01:44:08 nis Exp $	*/

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
static char rcsid[] = "$Id: assv.c,v 1.67 2010/12/08 01:44:08 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
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
#include "pi.h"

#include "priq.h"

#include "print.h"

#include "assvP.h"
#include "fssP.h"
#include "nwamP.h"
#include "xgserv.h"

#include <gifnoc.h>

#if defined ENABLE_UNIQUIFY
struct uniquify_state_t {
	int hold, last, enabled;
	struct ovec_t dd;
};
#endif

static int eject(struct priq *, idx_t, struct hvec_t *, df_t, struct assv_g *
#if defined ENABLE_UNIQUIFY
	, struct uniquify_state_t *
#endif
	);
static int updateq(struct priq *, struct ovec_t *dd, struct assv_g *);
#if defined ENABLE_UNIQUIFY
static int uniquify_samep(WAM *, int, idx_t, idx_t);
#endif
static int assv_cutoff_test(idx_t, struct hvec_t *, df_t, struct assv_g *, void *);
#if ! defined ENABLE_GETA
static int assv_bx_test(idx_t, struct hvec_t *, df_t, struct assv_g *, void *);
static int assv_mask_test(idx_t, struct hvec_t *, df_t, struct assv_g *, void *);
#endif
static priq_prop_t prop_f_ccompar;
static priq_prop_t prop_b_ccompar;
static priq_prop_t prop_f_wcompar;
static priq_prop_t prop_b_wcompar;

static struct xr_vec zerovec = {0, 0, {{0, 0}, }};

struct syminfo *
assv(q, nq, w, qdir, type, nd, totalp, scookie)
struct syminfo *q;
WAM *w;
df_t nq, *nd;
df_t *totalp;
struct assv_scookie *scookie;
{
	df_t i;
	df_t nh;
	struct pq_container *c;
	struct hvec_t *h;
	struct priq *a, *b;
	df_t total;
	struct assv_g g0, *g;
	idx_t d;
	struct syminfo *result;
	freq_t freq_sum;
#if ! defined ENABLE_GETA
	struct bxu_t *expr = NULL;
	struct syminfo *newq = NULL;
	idx_t *pm = NULL, *nm = NULL;
#endif
#if defined DBG
char s[8192];
#endif
#if defined ENABLE_UNIQUIFY
	struct uniquify_state_t us;
#endif

#if defined DBG
snprint_syminfo(q, nq, 0, 12, NULL, qdir, s, sizeof s);
syslog(LOG_DEBUG, "assv: q: %s", s);
#endif

#if ! defined ENABLE_GETA
	assert(w->type == NWAM_D_TYPE_WAM);

	if (w->u.w.dist.role == NDWAM_ROLE_PARENT) {
		assert(w->u.w.xs);
		if (scookie && (scookie->flag & ASSV_SC_FSS)) {
			scookie->fssq->pa = NULL;
			scookie->fssq->na = NULL;
		}
		return cb_assv(w->u.w.xs, q, nq, qdir, type, nd, totalp, scookie, w->u.w.allowerror, &w->u.w.may_incomplete, w->u.w.need_diststat ? &w->u.w.diststat : NULL);
	}
#endif

	if (*nd == 0) {
		return NULL;
	}

	total = 0;
	bzero(&g0, sizeof g0);
	g = &g0;

	g->nd = *nd;

	g->w = w;
	g->qdir = qdir;
	g->rdir = WAM_REVERT_DIRECTION(qdir);
	g->q = q;
	g->nq = nq;
	g->Nq = wam_size(g->w, g->qdir);
	g->Nr = wam_size(g->w, g->rdir);
	g->scookie = scookie;

	g->ntests = 0;
	/* each tests[*].func will called from `eject'.
	   Unless all tests succeeds, the found object will be discarded.
	   (return value of tests: 0 -- fail  else -- success) */

	if (scookie) {
		/* enable_cutoff test if cutoff_df_list is given*/
		if (scookie->flag & ASSV_SC_CUTOFF_DF_LIST) {
			struct test_c *tp = &g->tests[g->ntests++];
			assert(g->ntests <= nelems(g->tests));
			if (!scookie->cutoff_df_list) {
				syslog(LOG_DEBUG, "assv: cutoff_df_list == NULL");
				return NULL;
			}

			tp->killed = 0;
			tp->tested = 0;
			tp->func = assv_cutoff_test;
			tp->cookie = scookie->cutoff_df_list;
		}

#if ! defined ENABLE_GETA
		if (scookie->flag & ASSV_SC_BX) {
			df_t newnq;
			struct test_c *tp = &g->tests[g->ntests++];
			assert(g->ntests <= nelems(g->tests));

			if ((expr = bxu_conv(q, nq, scookie->bx.b, scookie->bx.blen, &newq, &newnq)) == NULL) {
				return NULL;
			}

			tp->killed = 0;
			tp->tested = 0;
			tp->func = assv_bx_test;
			tp->cookie = expr;
			nq = newnq;
			q = newq;
		}

		if (scookie->flag & ASSV_SC_FSS) {
			WAM *f = NULL;
			struct fss_simple_query p, n;
			if (scookie->flag & ASSV_SC_MASK) {
				syslog(LOG_DEBUG, "fss and mask are mutually exclusive");
				return NULL;
			}

			if (!(f = wam_fss_open(w))) {
				return NULL;
			}

			assert(w->u.w.dist.role != NDWAM_ROLE_PARENT);
			p.narts = n.narts = -1;
			if (wam_xfss(f, scookie->fssq, &p, &n) == -1) {
				return NULL;
			}
			if (p.arts && !(pm = arts2idvec(p.arts, p.narts))) {
				return NULL;
			}
			if (n.arts && !(nm = arts2idvec(n.arts, n.narts))) {
				return NULL;
			}
			wam_close(f);

			scookie->flag |= ASSV_SC_MASK;
			scookie->mask.p.len = p.narts;
			scookie->mask.p.m = pm;
			scookie->mask.n.len = n.narts;
			scookie->mask.n.m = nm;
			/* pm, nm is sorted */
		}

		if (scookie->flag & ASSV_SC_MASK) {
			struct test_c *tp = &g->tests[g->ntests++];
			assert(g->ntests <= nelems(g->tests));

			tp->killed = 0;
			tp->tested = 0;
			tp->func = assv_mask_test;
			tp->cookie = &scookie->mask;
			scookie->mask.p.idx = scookie->mask.n.idx = 0;
		}

		if (scookie->flag & ASSV_SC_UTEST) {
			struct test_c *tp = &g->tests[g->ntests++];
			assert(g->ntests <= nelems(g->tests));

			tp->killed = 0;
			tp->tested = 0;
			tp->func = scookie->utest.test;
			tp->cookie = scookie->utest.cookie;
		}
#endif
	}

	for (i = 0; i < nq; i++) {
		struct xr_vec const *v;
		if (q[i].v && q[i].id == 0) {
			q[i].DF = q[i].v->elem_num;
			q[i].TF = q[i].v->freq_sum;
#if ! defined ENABLE_GETA
			if (w->u.w.dist.role == NDWAM_ROLE_CHILD) {
#if defined DBG
syslog(LOG_DEBUG, "reduce_vec: before: %"PRIdDF_T, q[i].v->elem_num);
snprint_xr_vec(q[i].v, 12, NULL, g->rdir, s, sizeof s);
syslog(LOG_DEBUG, "reduce_vec: before: %s", s);
#endif
				reduce_vec(q[i].v, &w->u.w.dist);
#if defined DBG
snprint_xr_vec(q[i].v, 12, NULL, g->rdir, s, sizeof s);
syslog(LOG_DEBUG, "reduce_vec: after: %s", s);
#endif
			}
#endif
		}
		else if (!q[i].v && q[i].id != 0) {
			if (wam_get_vec(w, qdir, q[i].id, &v) != -1) {
				if (!(q[i].v = dxr_dup(v))) {
					return NULL;
				}
				q[i].DF = wam_elem_num(w, qdir, q[i].id);
				q[i].TF = wam_freq_sum(w, qdir, q[i].id);
			}
			else {
				q[i].v = &zerovec;
				q[i].DF = 0;
				q[i].TF = 0;
			}
		}
		else {
			syslog(LOG_DEBUG, "assv: assertion failed: v and id should be mutually exclusive");
			return NULL;
		}
	}

	if (!(c = calloc(nq, sizeof *c))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}

	if (!(h = calloc(nq, sizeof *h))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}
	if (!(a = priq_creat_p(sizeof (struct pq_container *), NULL, prop_f_ccompar, prop_b_ccompar, NULL))) {
		syslog(LOG_DEBUG, "priq_creat: %s", strerror(errno));
		return NULL;
	}

	if (!(b = priq_creat_p(sizeof (struct ovec_t), NULL, prop_f_wcompar, prop_b_wcompar, NULL))) {
		syslog(LOG_DEBUG, "priq_creat: %s", strerror(errno));
		return NULL;
	}

	freq_sum = 0;
	for (i = 0; i < g->nq; i++) {
		freq_sum += q[i].TF_d;
	}
	g->tfq = freq_sum;

	if (simdef_set_type(g, type) == -1) {
		syslog(LOG_DEBUG, "unsupported type: %d", type);
		return NULL;
	}

	for (i = 0; i < nq; i++) {
		struct pq_container *t = &c[i];
		t->qidx = i;
		if (g->f.wq) {
			t->wq = (*g->f.wq)(&q[i], g);
		}
		else {
			t->wq = 1;
		}
		t->ep = &q[i].v->elems[0];
		t->ep_end = t->ep + q[i].v->elem_num;
		if (q[i].v->elem_num) {
			t->epid = t->ep->id;
			if (priq_enq(a, &t) == -1) {
				syslog(LOG_DEBUG, "priq_enq: %s", strerror(errno));
				return NULL;
			}
		}
	}

#if defined ENABLE_UNIQUIFY
	us.hold = 0;
	us.last = 0;
	us.enabled = scookie && (scookie->flag & ASSV_SC_UNIQUIFY);
#endif

	nh = 0;
	d = 0;
	while (!PRIQ_EMPTYP(a)) {
		struct pq_container *t = *(struct pq_container **)PRIQ_TOP(a);

		if (nh == 0) {
			d = t->epid;
		}
		else if (t->epid != d) {
			if (d) {
				switch (eject(b, d, h, nh, g
#if defined ENABLE_UNIQUIFY
					, &us
#endif
					)) {
				case -1:
					syslog(LOG_DEBUG, "assv: eject failed");
					return NULL;
				case 0:
					total++;
					break;
				default:
					/* killed */
					break;
				}
			}
			nh = 0;
			d = t->epid;
		}

		if(nh >= nq) {
			syslog(LOG_DEBUG, "assv: nh >= nq");
			return NULL;
		}
		h[nh].t = t;
		h[nh].dp = t->ep;
		nh++;

		if (++t->ep < t->ep_end) {
			t->epid = t->ep->id;
			priq_rplatop(a, &t);
		}
		else {
			priq_deq(a);
		}
	}

	if (nh != 0) {
#if defined ENABLE_UNIQUIFY
		us.last = 1;
#endif
		switch (eject(b, d, h, nh, g
#if defined ENABLE_UNIQUIFY
			, &us
#endif
			)) {
		case -1:
			syslog(LOG_DEBUG, "assv: eject failed");
			return NULL;
		case 0:
			total++;
			break;
		default:
			/* killed */
			break;
		}
	}

	priq_free(a);
	free(c);
	free(h);

	if (totalp) {
		*totalp = total;
	}

	*nd = PRIQ_NMEMB(b);

	if (!(result = calloc(*nd, sizeof *result))) {
		priq_free(b);
		syslog(LOG_DEBUG, "assv: preq_free(b) failed");
		return NULL;
	}
	for (i = 0; i < *nd; i++) {
		struct syminfo *rp = &result[*nd - i - 1];
		struct ovec_t *top = PRIQ_TOP(b);
		rp->id = top->id;
		rp->weight = top->weight;
		rp->TF = wam_freq_sum(g->w, g->rdir, rp->id);
		rp->DF = wam_elem_num(g->w, g->rdir, rp->id);
		rp->TF_d = top->TF_d;
		rp->DF_d = top->DF_d;
		rp->v = NULL;
#if defined ASSV_DBGINFO
		rp->u.d.qw.n = g->nq;
		rp->u.d.qw.w = top->qw;
#endif
		priq_deq(b);
	}

	priq_free(b);

#if defined DBG
if (g->ntests) {
	int j;
	for (j = 0; j < g->ntests; j++) {
		struct test_c *tp = &g->tests[j];
		syslog(LOG_DEBUG, "(%"PRIdDF_T" %"PRIdDF_T")", *nd, tp->killed);
	}
}
#endif

	for (i = 0; i < nq; i++) {
		if (q[i].id != 0 && q[i].v) {
			free(q[i].v);
			q[i].v = NULL;
		}
	}

#if ! defined ENABLE_GETA
	if (scookie) {
		if (scookie->flag & ASSV_SC_BX) {
			free(newq);
			bxu_free(expr);
		}
		if (scookie->flag & ASSV_SC_FSS) {
			free(pm);
			free(nm);
		}
	}
#endif
	if (g->f.cleanup) {
		(*g->f.cleanup)(g);
	}

#if defined DBG
snprint_syminfo(result, *nd, 0, 128, NULL, g->rdir, s, sizeof s);
syslog(LOG_DEBUG, "assv: result: %s", s);
#endif

	return result;
}

/* RETURN VALUE:
  -1: error
   0: success. count up `total'.	(or hold)
   1: killed. do not count up `total'.
 */
static int
eject(b, d, h, nh, g
#if defined ENABLE_UNIQUIFY
	, us
#endif
	)
struct priq *b;
idx_t d;
struct hvec_t *h;
df_t nh;
struct assv_g *g;
#if defined ENABLE_UNIQUIFY
struct uniquify_state_t *us;
#endif
{
	df_t i;
	double wsum;
	struct ovec_t dd;
	df_t DF_d;
	freq_t TF_d;
	int j;
#if defined ASSV_DBGINFO
	double *qw = NULL;
#endif

	for (i = 0; i < nh; i++) {
		struct pq_container *t = h[i].t;
		if (t->qidx < g->nq) {
			break;
		}
	}
	if (i == nh) {	/* all items in h are from bool-expression! */
		return 1;
	}

	for (j = 0; j < g->ntests; j++) {
		struct test_c *tp = &g->tests[j];
		tp->tested++;
		if (!(*tp->func)(d, h, nh, g, tp->cookie)) {
			tp->killed++;
			return 1;
		}
	}

	wsum = 0;
	DF_d = 0;
	TF_d = 0;
#if defined ASSV_DBGINFO
	if (g->scookie && (g->scookie->flag & ASSV_SC_DBGINFO) && (qw = calloc(g->nq, sizeof *qw))) {
		df_t i;
		for (i = 0; i < g->nq; i++) {
			qw[i] = 0;
		}
	}
#endif
	for (i = 0; i < nh; i++) {
		struct pq_container *t = h[i].t;
		if (t->qidx < g->nq) {
			if (g->f.wd) {
				double w;
				w = t->wq * (*g->f.wd)(h[i].dp, &g->q[t->qidx], g);
#if defined ASSV_DBGINFO
				if (qw) {
					qw[t->qidx] += w;
				}
#endif
				wsum += w;
			}
			TF_d += h[i].dp->freq;
			DF_d++;
		}
		assert(h[i].dp->id == d);
	}
	dd.id = d;
	dd.weight = 0;
	dd.TF_d = TF_d;
	dd.DF_d = DF_d;	/* != nh */
#if defined ASSV_DBGINFO
	dd.qw = NULL;
#endif
	if (wsum) {
		double norm;
		if (g->f.norm && (norm = (*g->f.norm)(d, &dd, g)) != 0) {
			wsum /= norm;
#if defined ASSV_DBGINFO
			if (qw) {
				df_t i;
				for (i = 0; i < g->nq; i++) {
					qw[i] /= norm;
				}
			}
#endif
		}
	}
	if (g->f.weight) {
		wsum = (*g->f.weight)(wsum, d, &dd, h, nh, g);
	}
	dd.weight = wsum;
#if defined ASSV_DBGINFO
	dd.qw = qw;
#endif
#if defined ENABLE_UNIQUIFY
	if (us->enabled) {
		if (!us->hold) {
			us->dd = dd;
			us->hold = 1;
		}
		else {
			if (uniquify_samep(g->w, g->rdir, us->dd.id, dd.id)) {
				if (us->dd.weight < dd.weight) {
#if defined ASSV_DBGINFO
					free(us->dd.qw);
#endif
					us->dd = dd;
				}
				else {
#if defined ASSV_DBGINFO
					free(dd.qw);
#endif
				}
			}
			else {
				if (updateq(b, &us->dd, g) == -1) {
					return -1;
				}
				us->dd = dd;
			}
		}

		if (us->last && us->hold) {
			if (updateq(b, &us->dd, g) == -1) {
				return -1;
			}
		}
		return 0;
	}
#endif

	return updateq(b, &dd, g);
}

static int
updateq(b, dd, g)
struct priq *b;
struct ovec_t *dd;
struct assv_g *g;
{
	if (PRIQ_NMEMB(b) >= g->nd) {
		struct ovec_t *top = PRIQ_TOP(b);
		if (top->weight > dd->weight) {
#if defined ASSV_DBGINFO
			free(dd->qw);
#endif
			return 0;
		}
#if defined ASSV_DBGINFO
		else {
			free(top->qw);
		}
#endif
		priq_rplatop(b, dd);
	}
	else {
		if (priq_enq(b, dd) == -1) {
			return -1;
		}
	}
	return 0;
}

#if defined ENABLE_UNIQUIFY
static int
uniquify_samep(w, dir, i1, i2)
WAM *w;
idx_t i1, i2;
{
	char const *n1, *n2;

	n1 = wam_id2name(w, dir, i1);
	n2 = wam_id2name(w, dir, i2);

	for (; *n1 && *n1 != '\001' && *n1 == *n2; n1++, n2++) ;

	return *n1 == *n2;
}
#endif

static int
assv_cutoff_test(d, h, nh, g, cookie)
idx_t d;
struct hvec_t *h;
df_t nh;
struct assv_g *g;
void *cookie;
{
	df_t *cutoff_df_list = cookie;
	df_t i, limit;
	limit = 0;
	for (i = 0; i < nh; i++) {
		struct pq_container *t = h[i].t;
		df_t lim = cutoff_df_list[t->qidx];
		if (lim == 0) {
			return 1;
		}
		if (limit < lim) {
			limit = lim;
		}
	}
	if (wam_elem_num(g->w, g->rdir, d) >= limit) {
		return 0;
	}
	return 1;
}

#if ! defined ENABLE_GETA
static int
assv_bx_test(d, h, nh, g, cookie)
idx_t d;
struct hvec_t *h;
df_t nh;
struct assv_g *g;
void *cookie;
{
	struct bxu_t *bx = cookie;

	return bxu_eval(bx, h, nh);
}

static int
assv_mask_test(d, h, nh, g, cookie)
idx_t d;
struct hvec_t *h;
df_t nh;
struct assv_g *g;
void *cookie;
{
	struct assv_cookie_masks_t *mask;
	struct assv_cookie_mask_t *s;

	mask = cookie;

	s = &mask->p;
	if (s->m) {
		while (s->idx < s->len && s->m[s->idx] < d) {
			s->idx++;
		}
		if ((s->idx < s->len && s->m[s->idx] == d)) {
			return 1;
		}
	}
	s = &mask->n;
	if (s->m) {
		while (s->idx < s->len && s->m[s->idx] < d) {
			s->idx++;
		}
		if (!(s->idx < s->len && s->m[s->idx] == d)) {
			return 1;
		}
	}

	return 0;
}
#endif

#define compar_ccompar(e, a, b, q) \
{ \
	struct pq_container * const *va; \
	struct pq_container * const *vb; \
 \
	va = a; \
	vb = b; \
 \
	if ((*va)->epid == (*vb)->epid) { \
		if ((*va)->qidx == (*vb)->qidx) { \
			e = 0; \
		} \
		else if ((*va)->qidx < (*vb)->qidx) { \
			e = -1; \
		} \
		else { \
			e = 1; \
		} \
	} \
	else if ((*va)->epid < (*vb)->epid) { \
		e = -1; \
	} \
	else { \
		e = 1; \
	} \
}

static PROP_F_TYPED(prop_f_ccompar, compar_ccompar, struct pq_container **)
static PROP_B_TYPED(prop_b_ccompar, compar_ccompar, struct pq_container **)

#define compar_wcompar(e, a, b, q) \
{ \
	struct ovec_t const *va; \
	struct ovec_t const *vb; \
 \
	va = a; \
	vb = b; \
 \
	if (fequal(va->weight, vb->weight)) { \
		if (va->id == vb->id) { \
			e = 0; \
		} \
		else if (va->id < vb->id) { \
			e = 1; \
		} \
		else { \
			e = -1; \
		} \
	} \
	else if (va->weight < vb->weight) { \
		e = -1; \
	} \
	else { \
		e = 1; \
	} \
}

static PROP_F_TYPED(prop_f_wcompar, compar_wcompar, struct ovec_t *)
static PROP_B_TYPED(prop_b_wcompar, compar_wcompar, struct ovec_t *)
