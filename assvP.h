/*	$Id: assvP.h,v 1.29 2010/12/15 01:27:21 nis Exp $	*/

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

struct assv_g;

struct pq_container {
	df_t qidx;
	struct xr_elem const *ep;
	struct xr_elem const *ep_end;
	idx_t epid;	/* == ep->id */
	double wq;
};

struct ovec_t {
	idx_t id;
	double weight;
	freq_t TF_d;
	df_t DF_d;
#if defined ASSV_DBGINFO
	double *qw;
#endif
};

struct hvec_t {
	struct pq_container *t;
	struct xr_elem const *dp;
};

struct test_c {
	assv_test_t func;
	void *cookie;
	df_t killed;	/* # of killed objects by this test */
	df_t tested;	/* # of tested objects by this test */
};

struct simfuns {
	double (*wq)(struct syminfo *, struct assv_g *);
	double (*wd)(struct xr_elem const *, struct syminfo *, struct assv_g *);
	double (*norm)(idx_t, struct ovec_t *, struct assv_g *);
	double (*weight)(double, idx_t, struct ovec_t *, struct hvec_t *, df_t, struct assv_g *);
	void (*cleanup)(struct assv_g *);
};

struct assv_g {
	WAM *w;
	int qdir, rdir;
	struct syminfo *q;
	df_t Nq, Nr, nq, nd, oqlen;
	freq_t tfq;
	struct simfuns f;
	struct test_c tests[4];
	int ntests;
	struct assv_scookie *scookie;
	union {
		struct {
			double avelen;
			double Slope;
			double aveTFq;	/* smart */
		} smartwa;
		struct {
			double ql, *idf;
		} cosine;
		struct {
			double k1, b, avgdl;
		} okapi_bm25;
	} u;
};

int simdef_set_type(struct assv_g *, int);
int recall_dlsim(int, char const **, char const **);

#if ! defined ENABLE_GETA
int bxu_eval(struct bxu_t *, struct hvec_t *, df_t);
#endif

#define	epsilon 2.22e-16
/*#define	fequal(a, b)	(fabs((a)-(b)) <= fabs(a)*epsilon)*/
#define	fequal(a, b)	((a) == (b))
