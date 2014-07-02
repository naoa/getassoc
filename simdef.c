/*	$Id: simdef.c,v 1.39 2011/09/14 02:36:15 nis Exp $	*/

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
static char rcsid[] = "$Id: simdef.c,v 1.39 2011/09/14 02:36:15 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <assert.h>
#if defined ENABLE_DLSIM
#include <dlfcn.h>
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>

#include "nwam.h"
#include "util.h"

#include "print.h"

#include "assvP.h"

#include <gifnoc.h>

#if defined ENABLE_DLSIM
#if ! defined RTLD_LAZY
#define	RTLD_LAZY	DL_LAZY
#endif
static int record_dlsim(char const *, char const *);
#endif
#define CT_MKTBL
#if defined CT_MKTBL
#define	CT_TBLSZ	8192
#endif

static int smartwa_init(struct assv_g *);
static double smartwa_wq(struct syminfo *, struct assv_g *);
static double smartwa_norm(idx_t, struct ovec_t *, struct assv_g *);
static double smartaw_wq(struct syminfo *, struct assv_g *);
static double smartaw_wd(struct xr_elem const *, struct syminfo *, struct assv_g *);
static double smartaw_norm(idx_t, struct ovec_t *, struct assv_g *);
static double smart_wq(struct syminfo *, struct assv_g *);
static int smart_init(struct assv_g *);

#if ! defined ENABLE_GETA
static int cosine_init(struct assv_g *);
static double cosine_wq(struct syminfo *, struct assv_g *);
static double cosine_wd(struct xr_elem const *, struct syminfo *, struct assv_g *);
static double cosine_norm(idx_t, struct ovec_t *, struct assv_g *);

static int cosine_tfidf_init(struct assv_g *);
static double cosine_tfidf_wq(struct syminfo *, struct assv_g *);
static double cosine_tfidf_wd(struct xr_elem const *, struct syminfo *, struct assv_g *);
static double cosine_tfidf_norm(idx_t, struct ovec_t *, struct assv_g *);
static void cosine_tfidf_cleanup(struct assv_g *);

static int okapi_bm25_init(struct assv_g *);
static double okapi_bm25_wd(struct xr_elem const *, struct syminfo *, struct assv_g *);
#endif

struct simdefs {
	int type;
	struct simfuns f;
	int (*init)(struct assv_g *);
};

#define	smartwa_wd	smartaw_wd

#define	smart_wd	smartaw_wd
#define	smart_norm	smartwa_norm

struct simdefs simdefs[] = {
	{ WT_SMARTWA,
	  { smartwa_wq, smartwa_wd, smartwa_norm, NULL, NULL },
	  smartwa_init },
	{ WT_SMARTAW,
	  { smartaw_wq, smartaw_wd, smartaw_norm, NULL, NULL },
	  NULL },
	{ WT_SMART,
	  { smart_wq, smart_wd, smart_norm, NULL, NULL },
	  smart_init },
#if ! defined ENABLE_GETA
	{ WT_COSINE,
	  { cosine_wq, cosine_wd, cosine_norm, NULL, NULL },
	  cosine_init },
	{ WT_DOT_PRODUCT,
	  { cosine_wq, cosine_wd, NULL, NULL, NULL },
	  NULL },
	{ WT_COSINE_TFIDF,
	  { cosine_tfidf_wq, cosine_tfidf_wd, cosine_tfidf_norm, NULL, cosine_tfidf_cleanup  },
	  cosine_tfidf_init },
	{ WT_DOT_PRODUCT_TFIDF,
	  { cosine_tfidf_wq, cosine_tfidf_wd, NULL, NULL, NULL },
	  cosine_tfidf_init },
	{ WT_OKAPI_BM25,
	  { NULL, okapi_bm25_wd, NULL, NULL, NULL },
	  okapi_bm25_init },
#endif
};

static double ln[] = {
#include "lntbl.h"
};

int
simdef_set_type(g, type)
struct assv_g *g;
{
#if defined ENABLE_DLSIM
	char const *path, *name;
	void *dl;
#endif
	int i;
	for (i = 0; i < nelems(simdefs); i++) {
		struct simdefs *s = &simdefs[i];
		if (s->type == type) {
			g->f = s->f;
			if (s->init && (*s->init)(g) == -1) {
				return -1;
			}
			return 0;
		}
	}

#if defined ENABLE_DLSIM
	if (recall_dlsim(type, &path, &name) == -1) {
		return -1;
	}
syslog(LOG_DEBUG, "loading file: [%s]", path);
	if (!(dl = dlopen(path, RTLD_LAZY))) {
		perror(path);
		return -1;
	}

syslog(LOG_DEBUG, "loading func: [%s]", name);

	g->f.wq = NULL;
	g->f.wd = NULL;
	g->f.norm = NULL;
	g->f.cleanup = NULL;
	if (!(g->f.weight = dlsym(dl, name))) {
		perror(name);
		return -1;
	}

/*	dlclose(dl); */

	return 0;
#else
	return -1;
#endif
}

static int
smartwa_init(g)
struct assv_g *g;
{
#if ! defined ENABLE_GETA
	g->u.smartwa.avelen = g->Nr == 0 ? 0 : wam_total_elem_num(g->w) / (double)g->Nr;
#else
	g->u.smartwa.avelen = g->Nr == 0 ? 0 : wam_total_elem_num(g->w, g->rdir) / (double)g->Nr;
#endif
	g->u.smartwa.Slope = 0.2;
	return 0;
}

static double
smartwa_wq(qp, g)
struct syminfo *qp;
struct assv_g *g;
{
	return qp->weight;
}

static double
smartwa_norm(d, dd, g)
idx_t d;
struct ovec_t *dd;
struct assv_g *g;
{
	df_t df = wam_elem_num(g->w, g->rdir, d);
	freq_t tf = wam_freq_sum(g->w, g->rdir, d);
	if (df == 0) return 1;
	return (g->u.smartwa.avelen + g->u.smartwa.Slope * (df - g->u.smartwa.avelen)) * (1.0 + log(tf / (double)df));
}

static double
smartaw_wq(qp, g)
struct syminfo *qp;
struct assv_g *g;
{
	df_t df = qp->DF;
	freq_t tf = qp->TF;

	if (df == 0) return 0;
	if (0 < tf && tf < nelems(ln) &&
	    0 < df && df < nelems(ln)) {
		return 1 / (1 + ln[tf] - ln[df]);
	}
	else {
		return 1 / (1 + log((double)tf / df));
	}
}

static double
smartaw_wd(dp, qp, g)
struct xr_elem const *dp;
struct syminfo *qp;
struct assv_g *g;
{
	freq_t f = dp->freq;
	if (f == 0) {
		syslog(LOG_DEBUG, "log(0)");
		return 0;
	}
	if (0 < f && f < nelems(ln)) {
		return 1 + ln[f];
	}
	return 1 + log(f);
}

static double
smartaw_norm(d, dd, g)
idx_t d;
struct ovec_t *dd;
struct assv_g *g;
{
	df_t df = wam_elem_num(g->w, g->rdir, d);
	if (df == 0) return 1;
	return g->nq / log1p((double)g->Nr / df);
}

static double
smart_wq(qp, g)
struct syminfo *qp;
struct assv_g *g;
{
	double idf;
	freq_t f;
	double w;
	df_t df = qp->DF;

	if (df == 0) return 0;
	f = qp->TF_d;
	if (f == 0) {
		return 0;
	}
	if (0 < f && f < nelems(ln)) {
		w = 1 + ln[f];
	}
	else w = 1 + log(f);

	idf = log1p((double)g->Nr / df);
	return (w / (1.0 + log(g->u.smartwa.aveTFq))) * idf;
}

static int
smart_init(g)
struct assv_g *g;
{
#if ! defined ENABLE_GETA
	g->u.smartwa.avelen = g->Nr == 0 ? 0 : wam_total_elem_num(g->w) / (double)g->Nr;
#else
	g->u.smartwa.avelen = g->Nr == 0 ? 0 : wam_total_elem_num(g->w, g->rdir) / (double)g->Nr;
#endif
	g->u.smartwa.Slope = 0.2;
	g->u.smartwa.aveTFq = g->nq == 0 ? 0 : (double)g->tfq / g->nq;
	return 0;
}

#if ! defined ENABLE_GETA
static int
cosine_init(g)
struct assv_g *g;
{
	df_t i;
	double l;

	for (l = 0, i = 0; i < g->nq; i++) {
		double a = g->q[i].TF_d;
		l += a * a;
	}
	g->u.cosine.ql = sqrt(l);
	return 0;
}

static double
cosine_wq(qp, g)
struct syminfo *qp;
struct assv_g *g;
{
	return qp->TF_d;
}

static double
cosine_wd(dp, qp, g)
struct xr_elem const *dp;
struct syminfo *qp;
struct assv_g *g;
{
	return dp->freq;
}

static double
cosine_norm(d, dd, g)
idx_t d;
struct ovec_t *dd;
struct assv_g *g;
{
	df_t i;
	double l;
	struct xr_vec const *v;

	if (wam_get_vec(g->w, g->rdir, d, &v) == -1) {
		return 1;
	}

	/* v->elem_num should be full length... */
	for (l = 0, i = 0; i < v->elem_num; i++) {
		double a = v->elems[i].freq;
		l += a * a;
	}
	return g->u.cosine.ql * sqrt(l);
}

static int
cosine_tfidf_init(g)
struct assv_g *g;
{
	df_t i;
	double l;

#if defined CT_MKTBL
	df_t k;
	if (g->u.cosine.idf = calloc(CT_TBLSZ, sizeof *g->u.cosine.idf)) {
		for (k = 1; k < CT_TBLSZ; k++) {
			g->u.cosine.idf[k] = log1p((double)g->Nr / k);
		}
	}
#endif

	for (l = 0, i = 0; i < g->nq; i++) {
		struct xr_vec const *t;
		double idf, a;
		df_t df = g->q[i].DF;
		t = g->q[i].v;
#if defined CT_MKTBL
		if (0 < df && df < CT_TBLSZ) {
			idf = g->u.cosine.idf[df];
		}
		else
#endif
		idf = log1p((double)g->Nr / df);
		a = g->q[i].TF_d * idf;
		l += a * a;
	}
	g->u.cosine.ql = sqrt(l);
	return 0;
}

static double
cosine_tfidf_wq(qp, g)
struct syminfo *qp;
struct assv_g *g;
{
	df_t df = qp->DF;
	double idf;
#if defined CT_MKTBL
	if (0 < df && df < CT_TBLSZ) {
		idf = g->u.cosine.idf[df];
	}
	else
#endif
	idf = log1p((double)g->Nr / df);
	return qp->TF_d * idf;
}

static double
cosine_tfidf_wd(dp, qp, g)
struct xr_elem const *dp;
struct syminfo *qp;
struct assv_g *g;
{
	df_t df = qp->DF;
	double idf;
#if defined CT_MKTBL
	if (0 < df && df < CT_TBLSZ) {
		idf = g->u.cosine.idf[df];
	}
	else
#endif
	idf = log1p((double)g->Nr / df);
	return dp->freq * idf;
}

static double
cosine_tfidf_norm(d, dd, g)
idx_t d;
struct ovec_t *dd;
struct assv_g *g;
{
	df_t i;
	double l;
	struct xr_vec const *v;

	if (wam_get_vec(g->w, g->rdir, d, &v) == -1) {
		return 1;
	}

	/* v->elem_num should be full length... */
	for (l = 0, i = 0; i < v->elem_num; i++) {
		df_t df;
		double a, idf;
		df = wam_elem_num(g->w, g->qdir, v->elems[i].id);	/* XXX SLOW! */
#if defined CT_MKTBL
		if (0 < df && df < CT_TBLSZ) {
			idf = g->u.cosine.idf[df];
		}
		else
#endif
		idf = log1p((double)g->Nr / df);
		a = v->elems[i].freq * idf;
		l += a * a;
	}
	return g->u.cosine.ql * sqrt(l);
}

static void
cosine_tfidf_cleanup(g)
struct assv_g *g;
{
#if defined CT_MKTBL
	free(g->u.cosine.idf);
#endif
}

static int
okapi_bm25_init(g)
struct assv_g *g;
{
	g->u.okapi_bm25.k1 = 2.0;
	g->u.okapi_bm25.b = 0.75;
	g->u.okapi_bm25.avgdl = (double)wam_total_freq_sum(g->w) / g->Nr;
	return 0;
}

static double
okapi_bm25_wd(dp, qp, g)
struct xr_elem const *dp;
struct syminfo *qp;
struct assv_g *g;
{
	df_t df;
	freq_t tf, tfd;
	double idf;

	df = qp->DF;
	tf = wam_freq_sum(g->w, g->rdir, dp->id);
	tfd = dp->freq;
	idf = log((g->Nr - df + 0.5) / (df + 0.5));
	return idf * (tfd * (g->u.okapi_bm25.k1 + 1)) / (tfd + g->u.okapi_bm25.k1 * (1 - g->u.okapi_bm25.b + g->u.okapi_bm25.b * tf / g->u.okapi_bm25.avgdl));
}
#endif

#if defined ENABLE_DLSIM
#define	DLS_KEYBASE	0x10000000

static struct {
	int flag;
	char const *dls_path, *dls_name;
} dls_rec[1] = {{0, NULL, NULL}};

int
find_dlsim(name)
char const *name;
{
	char path[MAXPATHLEN];
	char dname[1024];
	struct stat sb;

	if (name[0] != '.') {
		return -1;
	}
	snprintf(path, sizeof path, "%s/%s" DLS_EXT, DLS_DIRNAME, name + 1);
	if (stat(path, &sb) == -1) {
		perror(path);
		return -1;
	}

	snprintf(dname, sizeof dname, DLSFN_PREFIX "%s", name + 1);

	return record_dlsim(path, dname);
}

static int
record_dlsim(path, name)
char const *path, *name;
{
	int key = 0;

	dls_rec[key].flag = 1;
	dls_rec[key].dls_name = strdup(name);
	dls_rec[key].dls_path = strdup(path);

	return key + DLS_KEYBASE;
}

int
recall_dlsim(key, path, name)
char const **path, **name;
{
	key -= DLS_KEYBASE;
	if (dls_rec[key].flag == 0) {
		return -1;
	}
	*path = dls_rec[key].dls_path;
	*name = dls_rec[key].dls_name;
	return 0;
}
#endif
