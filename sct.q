/*	$Id: sct.q,v 1.86 2010/12/14 23:47:44 nis Exp $	*/

/*-
 * Copyright (c) 2005 Shingo Nishioka.
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
static char rcsid[] = "$Id: sct.q,v 1.86 2010/12/14 23:47:44 nis Exp $";
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
#include <time.h>
#include <unistd.h>

#include <expat.h>

#include "nwam.h"
#include "util.h"
#include "nio.h"

#include "cxml.h"

#include "nstring.h"
#include "common.h"
#include "print.h"
#include "getassocP.h"

#include <gifnoc.h>

#define	strtotimet(s)	(strtol((s), NULL, 10))

struct sct_g {
	df_t niwords, cutoff_df;

	char *nhandle;
	char *ohandle;
	df_t dmax;
	df_t qmax;
	int ctype;
	char *text, *xref_val, *ncsb_val, *fss;

	int xref, ncsb;
};

struct hlst {
	char const *handle;
	char const *title;
	char const *properties;
	char const *created;
	char const *narticles;
	char const *stemmer;
	char const *description;
};

struct fss_gettoken_state {
	char *p;
	int token, negativep;
	char buf[1024];
};

static struct element *long_query(char const *, int, char *[], char *);
static struct element *long_query_v(char const *, int, char *[], char *);
static int init_g(struct sct_g *, int, char *[]);
static int getval(char *, char const *, void *, int, void *, size_t, size_t, char *(*)(void *));
static struct element *result_page(char const *, int, char *[], WAM *, struct syminfo *, df_t, df_t, df_t, df_t, char const *, char const *, int, int, int, char *);
static union content *comment(char const *);
static char const *make_script(double **, struct syminfo *, df_t, struct cs_elem *, df_t);
static void minmax(double **, df_t, df_t, df_t, df_t, double *, double *);
static void sprgb(char *, size_t, double, double, double, int);
static void getrgb(int *, int *, int *, int, double);
static union content **make_rbody(struct syminfo *, df_t, WAM *, char const *);

static union content *cell(struct syminfo *, int, WAM *, WAM *, WAM *, WAM *);
static union content **thumblnk(WAM *, idx_t);

static struct cs_elem *make_cwlist(struct syminfo *, df_t, WAM *, char const *, df_t, int, df_t *, int);
static union content **make_cwlist2(WAM *, struct cs_elem *, df_t);
static union content **cwlist(WAM *, struct syminfole *, df_t, df_t);
static double **make_aamatrix(struct syminfo *, df_t, struct cs_elem *, df_t, WAM *);
static struct element *empty_result_page(df_t, df_t, char const *, int, int, int, char *);
static struct element *index_page(char *);
static struct element *error_page(void);
static struct element *page(char const *, struct element *);
static union content *getalink(void);
static union content *textarea(void);
static union content *textarea_fss(void);
static union content *selectors(df_t, df_t, char const *, int, int, int);
static union content *imagesel(void);
static union content *corpus_sel(char const *);
static union content *selector(char const *, void *, size_t, size_t, char *(*)(void *), char *(*)(void *), char const *, char const *);
static struct attribute *selected(char const *, char const *);
static union content *ctype_chooser(int);
static struct attribute *checked(int, int);
#define S	string2chardata
#if 0
static union content **debug(char const *, int, char *[]);
#endif
static int set_handle_list(struct element *);
static union content **catalist(struct element *);

static void errhtml(char const *, int);
static char *hlst_handle(void *);
static char *hlst_title(void *);
static char *char_pp(void *);
static int ncompar(void const *, void const *);
static int tcompar(void const *, void const *);
static int ccompar(void const *, void const *);

static char *utime_r(const time_t *, char *buf);
static int qstr2fss(char *, struct fss_query *);
static char *qstr2xml(char *);
static int fss_gettoken(struct fss_gettoken_state *);

struct hlst *handle_list = NULL;
size_t n_handle_list = 0;
char const *dmax_list[] = { "1", "2", "3", "4", "5", "10", "20", "50", "100", };
#define	DMAX_DFL	20
char const *qmax_list[] = { "10", "15", "20", "30", "60", };
#define	QMAX_DFL	20
char const *ctype_list[] = { "0", "1", };
#define	CTYPE_DFL	1
#define	XREF_DFL	0
#define	NCSB_DFL	1

char *style =
"\n"
"body { background: white; color: black }\n"
"h1 { color: maroon }\n"
"h2 { color: green }\n"
"h1.myclass { text-align: center }\n"
"div.rword { background: #F8F5D6 }\n"
"div.rbody { background: #F8F5D6 }\n"
".chkbtn { background: #F8F5D6 }\n"
"div.cls1 { background: #FFF2C0 }\n"
"div.cls2 { background: #EFE8D0 }\n"
"div.cls3 { background: #E7E3E2 }\n"
"div.warning { color: red }\n"
"div.info { color: black; font-size: small }\n"
"a:link { color: brown }\n"
"a:visited { color: maroon }\n"
"a:active { color: fuchsia }\n"
"table { border-collapse: collapse; border: 0px; padding: 0px; }\n"
"tr { border-spacing: 0px 0px; border: 0px; padding: 0px; }\n"
"td { border-spacing: 0px 0px; border: 0px; padding: 0px; }\n"
;

char *style_cat =
"\n"
"table { border-collapse: collapse; border: 0px; padding: 0px; }\n"
"tr { border-spacing: 0px 0px; border: 0px; padding: 0px; }\n"
"td { border-spacing: 0px 0px; border: 1px solid; padding: 2px; }\n"
;

#define	INTWN	200	/* the # of pivot words */
#define CUTOFF_DF 100000	/* if DF of a term is greater than or equal to CUTOFF_DF, the term is not used as a pivot */

extern char *getaroot;

#if 0
int
sct_post(in, path_info, argc, argv, out)
char const *path_info;
char *argv[];
{
	return sct(path_info, argc, argv, out);
}
#endif

int
sct(path_info, argc, argv, out)
char const *path_info;
char *argv[];
{
	char *p;
	struct element *page;
	struct element *catalogue;
	char *action = NULL;

	srandom(time(NULL));
	nputs("Content-Type: text/html; charset=UTF-8\r\n"
		"Pragma: no-cache\r\n"
		, out);
	if (!(catalogue = read_catalogue())) {
		nputs("\r\n", out);
		errhtml("ERROR: cannot read catalogue", out);
		return 0;
	}
	if (set_handle_list(catalogue) == -1) {
		nputs("\r\n", out);
		errhtml("ERROR: set handle list failed.", out);
		return 0;
	}
	if ((p = rindex(path_info, '/')) && !strcmp(p, "/index.html")) {
		action = "lq";
		page = index_page(action);
	}
	else if ((p = rindex(path_info, '/')) && !strcmp(p, "/indexv.html")) {
		action = "lqv";
		page = index_page(action);
	}
	else if ((p = rindex(path_info, '/')) && !strcmp(p, "/lq")) {
		action = "lq";
		page = long_query(path_info, argc, argv, action);
	}
	else if ((p = rindex(path_info, '/')) && !strcmp(p, "/lqv")) {
		action = "lqv";
		page = long_query_v(path_info, argc, argv, action);
	}
	else {
		page = error_page();
	}
	if (page) {
		struct nstring ns;
		ssize_t l;
		NS_ZERO(&ns);
		if ((l = unparse_fn(page, &ns, writefn_ns_append)) == -1) {
			goto error;
		}
		(void)send_text_content(&out, NULL, ns.ptr, l, 1);
		NS_FREE(&ns);
	}
	else {
error:
		nputs("\r\n", out);
		errhtml("ERROR!!!", out);
	}
	return 0;
}

static struct element *
long_query(path_info, argc, argv, action)
char const *path_info;
char *argv[];
char *action;
{
	struct sct_g g0, *g = &g0;
	int i;
	df_t ndsels, nquery;
	df_t nd, query_size;
	int dir, type;
#if ! defined ENABLE_GETA
	df_t total;
#else
	ssize_t total;
#endif
	struct syminfo *query, *d = NULL, *dsels, *it;
	WAM *tw, *sw;
	idx_t id;
	int iscross;
	struct element *rp;
	char const *stemmer = NULL;

	if (init_g(g, argc, argv) == -1) {
		return NULL;
	}

	if (!g->nhandle) {
		return NULL;
	}

	wam_init(getaroot);
	if (!(tw = wam_open(g->nhandle, NULL))) {
		syslog(LOG_DEBUG, g->nhandle, strerror(errno));
		return NULL;
	}

	query = NULL;
	query_size = 0;
	nquery = 0;
#define	XQ() do { \
	if (query_size <= nquery) { \
		void *new; \
		size_t newsize = nquery; \
		newsize = NA(newsize + 1, 128); \
		if (!(new = realloc(query, newsize * sizeof *query))) { \
			syslog(LOG_DEBUG, "realloc: %s", strerror(errno)); \
			return NULL; \
		} \
		query_size = newsize; \
		query = new; \
	} \
} while (0)

#if ! defined ENABLE_GETA
#define	SETR(r, i, w) \
			((r)->id = (i), \
			 (r)->TF = (r)->TF_d = 1, \
			 (r)->DF = (r)->DF_d = 1, \
			 (r)->weight = (w), \
			 (r)->v = NULL)
#else
#define	SETR(r, i, w) \
			((r)->id = (i), \
			 (r)->TF = (r)->TF_d = 1, \
			 (r)->DF = (r)->DF_d = 1, \
			 (r)->weight = (w), \
			 (r)->v = NULL, \
			 (r)->attr = WSH_OR)
#endif

	if (g->text) {
		struct xr_vec *tv;
		if (!stemmer) {
			stemmer = STEMMER_AUTO;
		}
		if (!(tv = text2vec(g->text, tw, WAM_COL, stemmer))) {
			syslog(LOG_DEBUG, "text2vec failed");
			return NULL;
		}
		for (i = 0; i < tv->elem_num; i++) {
			struct xr_elem *e = &tv->elems[i];
			struct syminfo *r;
			XQ();
			r = &query[nquery++];
			SETR(r, e->id, 1.0);
		}
		free(tv);
	}

	for (i = 0; i < argc; i++) {
		dir = WAM_COL;
		if (!strncmp(argv[i], "qsel=", 5) && (id = wam_name2id(tw, dir, argv[i] + 5))) {
			struct syminfo *r;
			XQ();
			r = &query[nquery++];
			SETR(r, id, 1.0);
		}
	}

	iscross = !g->ohandle || strcmp(g->ohandle, g->nhandle);
	if (!iscross) {
		sw = tw;
	}
	else if (g->ohandle) {
		if (!(sw = wam_open(g->ohandle, NULL))) {
			syslog(LOG_DEBUG, g->ohandle, strerror(errno));
			return NULL;
		}
	}
	else {
		sw = NULL;
	}

	if (!(dsels = calloc(argc, sizeof *dsels))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}
	for (i = 0, ndsels = 0; i < argc; i++) {
		if (!strncmp(argv[i], "dsel=", 5)) {
			if (!sw) {
				syslog(LOG_DEBUG, "malformed cross corpus search");
				return NULL;
			}
			dir = WAM_ROW;
			if (id = wam_name2id(sw, dir, argv[i] + 5)) {
				struct syminfo *r = &dsels[ndsels++];
				SETR(r, id, 0.0);
			}
		}
	}
	if (ndsels) {
#if defined ENABLE_GETA
		ssize_t tmp;
#endif
		assert(sw);
		dir = WAM_ROW;
		type = WT_SMARTAW;
		if (!iscross) {
			nd = g->niwords;
		}
		else {
			nd = g->niwords * 2;
		}
#if ! defined ENABLE_GETA
		if (!(it = wsh(dsels, ndsels, sw, dir, type, &nd, &total, NULL)) || !nd) {
			goto empty;
		}
#else
		tmp = nd;
		if (!(it = wsh(dsels, ndsels, sw, dir, type, &tmp, &total)) || !nd) {
			goto empty;
		}
		nd = tmp;
#endif
		if (iscross) {
			struct syminfo *nit;
			if (!(nit = syminfo_dup2(sw, tw, WAM_COL, it, &nd))) {
				syslog(LOG_DEBUG, "syminfo_dup2 failed");
				return NULL;
			}
			free(it);
			it = nit;
			nd = MIN(nd, g->niwords);
		}
		for (i = 0; i < nd; i++) {
			struct syminfo *q = &it[i];
			struct syminfo *r;
			XQ();
			r = &query[nquery++];
			*r = *q;
		}
	}

	if (sw && sw != tw) {
		wam_close(sw);
	}
	sw = NULL;

	dir = WAM_COL;
	type = WT_SMARTWA;
	nd = g->dmax;

	if (*g->fss) {
#if ! defined ENABLE_GETA
		WAM *f;
		struct fss_query q;
		struct fss_simple_query p, n;
		if (qstr2fss(g->fss, &q) == -1) {
			syslog(LOG_DEBUG, "qstr2fss failed");
			goto empty;
		}
		if (!(f = wam_fss_open(tw))) {
			syslog(LOG_DEBUG, "fss_open failed");
			goto empty;
		}
		p.narts = nd;
		n.narts = 0;
		if (wam_xfss(f, &q, &p, &n) == -1) {
			syslog(LOG_DEBUG, "xfss failed");
			goto empty;
		}
		if (!p.narts) {
			syslog(LOG_DEBUG, "empty result (FSS)");
			goto empty;
		}
		/* XXX n wo tukatte, hosyuugou mo kensaku dekiru youni suru? */
		nd = p.narts;
		if (!(d = arts2syminfo(p.arts, p.narts))) {
			syslog(LOG_DEBUG, "arts2syminfo failed");
			goto empty;
		}
		total = p.total;
		/*free(p.arts);		XXX */
		/*free(n.arts);		XXX */
		/* XXX free_fss_query(g->fss.query); */
/*
		if (!nd) {
			syslog(LOG_DEBUG, "arts2syminfo failed (FSS)");
			goto empty;
		}
*/
		wam_close(f);
		/* XXX free((&q)->....) */
#else	/* !ENABLE_GETA */
		FSS *f;
		struct fss_query q;
		struct fss_simple_query p, n;
		if (qstr2fss(g->fss, &q) == -1) {
			syslog(LOG_DEBUG, "qstr2fss failed");
			goto empty;
		}
		if (!(f = fss_open(getaroot, g->nhandle))) {
			syslog(LOG_DEBUG, "fss_open failed");
			goto empty;
		}
		p.narts = nd;
		n.narts = 0;
		if (xfss(f, &q, &p, &n) == -1) {
			syslog(LOG_DEBUG, "xfss failed");
			goto empty;
		}
		if (!p.narts) {
			syslog(LOG_DEBUG, "empty result (FSS)");
			goto empty;
		}
		/* XXX n wo tukatte, hosyuugou mo kensaku dekiru youni suru? */
		nd = p.narts;
		if (!(d = arts2syminfo(p.arts, p.narts))) {
			syslog(LOG_DEBUG, "arts2syminfo failed");
			goto empty;
		}
		total = p.total;
		/*free(p.arts);		XXX */
		/*free(n.arts);		XXX */
		/* XXX free_fss_query(g->fss.query); */
/*
		if (!nd) {
			syslog(LOG_DEBUG, "arts2syminfo failed (FSS)");
			goto empty;
		}
*/
		fss_close(f);
		/* XXX free((&q)->....) */
#endif	/* !ENABLE_GETA */
	}
	else {
#if ! defined ENABLE_GETA
		if (!(d = wsh(query, nquery, tw, dir, type, &nd, &total, NULL)) || !nd) {
			goto empty;
		}
#else
		ssize_t tmp = nd;
		if (!(d = wsh(query, nquery, tw, dir, type, &tmp, &total)) || !nd) {
			goto empty;
		}
		nd = tmp;
#endif
	}

	rp = result_page(path_info, argc, argv, tw, d, nd, total, g->dmax, g->qmax, g->nhandle, g->ohandle, g->ctype, g->xref, g->ncsb, action);
	wam_close(tw);
	return rp;
empty:
	return empty_result_page(g->dmax, g->qmax, g->nhandle, g->ctype, g->xref, g->ncsb, action);
}

static struct element *
long_query_v(path_info, argc, argv, action)
char const *path_info;
char *argv[];
char *action;
{
	struct assv_scookie sc;
	struct sct_g g0, *g = &g0;
	int i;
	df_t ndsels;
#if ! defined ENABLE_GETA
	df_t nd, total;
#else
	ssize_t nd, total;
#endif
	df_t nit;
	int dir, type;
	struct xr_vec *tsel = NULL;
	struct xr_vec *qsel;
	struct syminfo *d = NULL, *it, *dsels;
	WAM *tw, *sw;
	idx_t id;
	int iscross;
	struct element *rp;
	df_t *cutoff_df_vec;
	char const *stemmer = NULL;

	if (init_g(g, argc, argv) == -1) {
		return NULL;
	}

	if (!g->nhandle) {
		return NULL;
	}

	wam_init(getaroot);
	if (!(tw = wam_open(g->nhandle, NULL))) {
		syslog(LOG_DEBUG, g->nhandle, strerror(errno));
		return NULL;
	}

	if (g->text) {
		if (!stemmer) {
			stemmer = STEMMER_AUTO;
		}
		if (!(tsel = text2vec(g->text, tw, WAM_COL, stemmer))) {
			syslog(LOG_DEBUG, "text2vec failed");
			return NULL;
		}
	}

	if (!(qsel = malloc(offsetof(struct xr_vec, elems[argc])))) {
		syslog(LOG_DEBUG, "malloc qsel failed");
		return NULL;
	}

	qsel->elem_num = 0;
	qsel->freq_sum = 0;
	for (i = 0; i < argc; i++) {
		dir = WAM_COL;
		if (!strncmp(argv[i], "qsel=", 5) && (id = wam_name2id(tw, dir, argv[i] + 5))) {
			struct xr_elem *e = &qsel->elems[qsel->elem_num++];
			e->id = id;
			e->freq = 1;
			qsel->freq_sum += 1;
		}
	}
	if (qsel->elem_num == 0) {
		free(qsel);
		qsel = NULL;
	}

	if (!(dsels = calloc(argc + 1, sizeof *dsels))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}

	iscross = !g->ohandle || strcmp(g->ohandle, g->nhandle);
	if (!iscross) {
		sw = tw;
	}
	else if (g->ohandle) {
		if (!(sw = wam_open(g->ohandle, NULL))) {
			syslog(LOG_DEBUG, g->ohandle, strerror(errno));
			return NULL;
		}
	}
	else {
		sw = NULL;
	}

	for (i = 0, ndsels = 0; i < argc; i++) {
		if (!strncmp(argv[i], "dsel=", 5)) {
			struct xr_vec const *v;
			if (!sw) {
				syslog(LOG_DEBUG, "malformed cross corpus search");
				return NULL;
			}
			dir = WAM_ROW;
			if (wam_get_vec_byname(sw, dir, argv[i] + 5, &v) != -1) {
				if (iscross) {
					if (!(v = dxr_dup2(sw, tw, WAM_COL, v))) {
						syslog(LOG_DEBUG, "dxr_dup2 failed");
						return NULL;
					}
				}
/*XXX else v = dxr_dup(v);*/
				if (v->elem_num) {
					dsels[ndsels].id = 0;
					dsels[ndsels].v = v;
					ndsels++;
				}
			}
		}
	}
	if (sw && sw != tw) {
		wam_close(sw);
	}
	sw = NULL;

	if (qsel) {
		assert(ndsels < argc + 1);
		dsels[ndsels].id = 0;
		dsels[ndsels].v = qsel;
		ndsels++;
	}
	if (tsel) {
		assert(ndsels < argc + 1);
		dsels[ndsels].id = 0;
		dsels[ndsels].v = tsel;
		ndsels++;
	}

	if (!ndsels) {
		goto empty;
	}

	if (!(cutoff_df_vec = calloc(ndsels, sizeof *cutoff_df_vec))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}
	for (i = 0; i < ndsels; i++) {
		cutoff_df_vec[i] = g->cutoff_df;
	}

	dir = WAM_ROW;
	nit = INTWN;

	bzero(&sc, sizeof sc);
	sc.flag = ASSV_SC_CUTOFF_DF_LIST;
	sc.cutoff_df_list = cutoff_df_vec;
	if (!(it = assv(dsels, ndsels, tw, dir, WT_SMARTAW, &nit, NULL, &sc))) {
		goto empty;
	}

	dir = WAM_COL;
	type = WT_SMARTWA;
	nd = g->dmax;

	if (!(d = wsh(it, nit, tw, dir, type, &nd, &total
#if ! defined ENABLE_GETA
			, NULL
#endif
		)) || !nd) {
		goto empty;
	}

	rp = result_page(path_info, argc, argv, tw, d, nd, total, g->dmax, g->qmax, g->nhandle, g->ohandle, g->ctype, g->xref, g->ncsb, action);
	wam_close(tw);
	return rp;
empty:
	return empty_result_page(g->dmax, g->qmax, g->nhandle, g->ctype, g->xref, g->ncsb, action);

}

static int
init_g(g, argc, argv)
struct sct_g *g;
char *argv[];
{
	int i;

	g->niwords = INTWN;
	g->cutoff_df = CUTOFF_DF;

	g->nhandle = NULL;
	g->ohandle = NULL;
	g->dmax = DMAX_DFL;
	g->qmax = QMAX_DFL;
	g->ctype = CTYPE_DFL;
	g->xref = XREF_DFL;
	g->ncsb = NCSB_DFL;
	g->text = NULL;
	g->xref_val = NULL;
	g->ncsb_val = NULL;
	g->fss = NULL;
	for (i = 0; i < argc; i++) {
/* XXX: check return value? */
		getval(argv[i], "nhandle=", &g->nhandle, 's', handle_list, n_handle_list, sizeof *handle_list, hlst_handle);
		getval(argv[i], "ohandle=", &g->ohandle, 's', handle_list, n_handle_list, sizeof *handle_list, hlst_handle);
		getval(argv[i], "dmax=", &g->dmax, 'd', dmax_list, nelems(dmax_list), sizeof *dmax_list, char_pp);
		getval(argv[i], "qmax=", &g->qmax, 'd', qmax_list, nelems(qmax_list), sizeof *qmax_list, char_pp);
		getval(argv[i], "ctype=", &g->ctype, 'i', ctype_list, nelems(ctype_list), sizeof *ctype_list, char_pp);
		getval(argv[i], "text=", &g->text, 's', NULL, 0, 0, NULL);
		getval(argv[i], "xref=", &g->xref_val, 's', NULL, 0, 0, NULL);
		getval(argv[i], "ncsb=", &g->ncsb_val, 's', NULL, 0, 0, NULL);
		getval(argv[i], "fss=", &g->fss, 's', NULL, 0, 0, NULL);
	}
	g->xref = g->xref_val != NULL;
	g->ncsb = g->ncsb_val != NULL;
	return 0;
}

static int
getval(s, prefix, val, type, values, nmemb, size, access)
char *s;
char const *prefix;
void *val, *values;
size_t nmemb, size;
char *(*access)(void *);
{
	char *p;
	size_t l = strlen(prefix);
	if (!strncmp(s, prefix, l)) {
		int valid;
		p = s + l;
		valid = 0;
		if (values) {
			char *vv;
			int i;
			if (!access) {
				return -1;
			}
			for (i = 0; i < nmemb; i++) {
				vv = (char *)values + size * i;
				if (!strcmp((*access)(vv), p)) {
					goto ok;
				}
			}
			return -1;
		}
	ok:
		switch (type) {
			char *end;
		case 's':
			*(char **)val = p;
			break;
		case 'd':
			*(df_t *)val = strtol(p, &end, 10);
			if (!(*p && !*end)) return -1;
			break;
		case 'i':
			*(int *)val = strtol(p, &end, 10);
			if (!(*p && !*end)) return -1;
			break;
		default:
			break;
		}
	}
	return 0;
}

static union content *
comment(s)
char const *s;
{
	struct misc *m;
	if (!(m = alloc_comment())) {
		syslog(LOG_DEBUG, "alloc_comment: %s", strerror(errno));
		return NULL;
	}
	m->len = strlen(s);
	m->string = (char *)s;
	return (union content *)m;
}

static char const *
make_script(m_aw, d_ignored, nd, cslst, nc)
double **m_aw;
struct syminfo *d_ignored;
df_t nd, nc;
struct cs_elem *cslst;
{
	char buf[64];
	char *ret = NULL;
	size_t ret_tail = 0, ret_size = 0;
	df_t i, k, c;
	df_t nt;
	double min, max;
	df_t d;

#define	A(s)	do { \
		size_t l = strlen(s); \
		if (!ret || ret_size <= ret_tail + l) { \
			void *new; \
			size_t newsize = ret_tail + l; \
			newsize = NA(newsize + 1, 1024); \
			if (!(new = realloc(ret, newsize))) { \
				return NULL; \
			} \
			ret_size = newsize; \
			ret = new; \
		} \
		memmove(ret + ret_tail, (s), l + 1); \
		ret_tail += l; \
	} while (0)

	for (c = 0, nt = 0; c < nc; c++) {
		struct cs_elem *e = &cslst[c];
		nt += e->csa.n;
	}

	A("\n");
	snprintf(buf, sizeof buf, "na = %"PRIdDF_T";\n", nd);
	A(buf);
	snprintf(buf, sizeof buf, "nw = %"PRIdDF_T" + %"PRIdDF_T";\n", nt, nc);
	A(buf);
	A("wa = [");
	A("[");
	for (d = 0; d < nd; d++) {
		snprintf(buf, sizeof buf, "\"c%da%"PRIdDF_T"\",", 1, d);
		A(buf);
	}
	A("],\n");
	for (c = k = 0; c < nc; c++) {
		struct cs_elem *e = &cslst[c];
		for (i = 0; i < e->csa.n; i++, k++) {
			A("[");
			minmax(m_aw, 0, nd, k, k, &min, &max);
			for (d = 0; d < nd; d++) {
				sprgb(buf, sizeof buf, m_aw[d][k], min, max, 0);
				A(buf);
			}
			A("],\n");
		}
	}
	for (c = 0; c < nc; c++, k++) {
		A("[");
		minmax(m_aw, 0, nd, k, k, &min, &max);
		for (d = 0; d < nd; d++) {
			sprgb(buf, sizeof buf, m_aw[d][k], min, max, 0);
			A(buf);
		}
		A("],\n");
	}
	A("];\n");
	A("aw = [");
	A("[");
	for (c = 0; c < nc; c++) {
		struct cs_elem *e = &cslst[c];
		for (i = 0; i < e->csa.n; i++) {
			snprintf(buf, sizeof buf, "\"c%"PRIdDF_T"w%"PRIdDF_T"\",", c + 1, i);
			A(buf);
		}
	}
	for (c = 0; c < nc; c++) {
		snprintf(buf, sizeof buf, "\"c%"PRIdDF_T"wg\",", c + 1);
		A(buf);
	}
	A("],\n");
	for (d = 0; d < nd; d++) {
		A("[");
		minmax(m_aw, d, d, 0, nt, &min, &max);
		for (c = k = 0; c < nc; c++) {
			struct cs_elem *e = &cslst[c];
			for (i = 0; i < e->csa.n; i++, k++) {
				sprgb(buf, sizeof buf, m_aw[d][k], min, max, c + 1);
				A(buf);
			}
		}
		minmax(m_aw, d, d, nt, nt + nc, &min, &max);
		for (c = 0; c < nc; c++, k++) {
			sprgb(buf, sizeof buf, m_aw[d][k], min, max, c + 1);
			A(buf);
		}
		A("],\n");
	}
	A("];\n");
	A("// ");

assert(k == nt + nc);

	return ret;
}

static void
minmax(m, x0, x1, y0, y1, minp, maxp)
double **m, *minp, *maxp;
df_t x0, y0, x1, y1;
{
	double min, max;
	df_t i;
	if (y0 == y1) {
		for (min = max = m[x0][y0], i = x0; i < x1; i++) {
			double v = m[i][y0];
			if (v > max) { max = v; }
			if (v < min) { min = v; }
		}
	}
	else if (x0 == x1) {
		for (min = max = m[x0][y0], i = y0; i < y1; i++) {
			double v = m[x0][i];
			if (v > max) { max = v; }
			if (v < min) { min = v; }
		}
	}
	else {
		min = max = 0.0;
	}
	*minp = min;
	*maxp = max;
}

static void
sprgb(buf, size, v, min, max, cls)
char *buf;
size_t size;
double v, min, max;
{
	int r, g, b;
	if (max != min) v = (v - min) / (max - min);
	else if (max != 0) v = v / max;
	else v = 1;
	getrgb(&r, &g, &b, cls, v);
	snprintf(buf, size, "\"#%02x%02x%02x\",", r, g, b);
}

static void
getrgb(r, g, b, c, s)
int *r, *g, *b;
double s;
{
/*
"div.rword { background: #F8F5D6 }\n"
"div.rbody { background: #F8F5D6 }\n"
"div.cls1 { background: #FFF2C0 }\n"
"div.cls2 { background: #EFE8D0 }\n"
"div.cls3 { background: #E7E3E2 }\n"
"a:link { color: brown }\n"
"a:visited { color: maroon }\n"
b0  30  60             maroon
a5  2a  2a             brown
*/
	int r0, g0, b0, r1, g1, b1;

	switch (c) {
	case 0:
		r0 = 0xf8, g0 = 0xf5, b0 = 0xd6;
		r1 = 0xa5, g1 = 0x2a, b1 = 0x2a;
		break;
	case 1:
		r0 = 0xff, g0 = 0xf2, b0 = 0xc0;
		r1 = 0x00, g1 = 0x00, b1 = 0x00;
		break;
	case 2:
		r0 = 0xef, g0 = 0xe8, b0 = 0xd0;
		r1 = 0x00, g1 = 0x00, b1 = 0x00;
		break;
	case 3:
		r0 = 0xe7, g0 = 0xe3, b0 = 0xe2;
		r1 = 0x00, g1 = 0x00, b1 = 0x00;
		break;
	default:
		r0 = 0x00, g0 = 0x00, b0 = 0x00;
		r1 = 0x00, g1 = 0x00, b1 = 0x00;
		break;
	}
	*r = r0 * (1 - s) + r1 * s;
	*g = g0 * (1 - s) + g1 * s;
	*b = b0 * (1 - s) + b1 * s;
}

static struct element *
result_page(path_info, argc, argv, w, d, nd, total, dmax, qmax, nhandle, ohandle, ctype, xref, ncsb, action)
char const *path_info;
char *argv[];
char const *nhandle;
char const *ohandle;
WAM *w;
struct syminfo *d;
df_t nd, total, dmax, qmax;
char *action;
{
	int dir;
	union content *rbody, *cwbody;
	char const *scr;
	char totalinfo[64];
	double **m_aw;
	df_t nc;
	struct cs_elem *cslst;
	dir = WAM_ROW;
	snprintf(totalinfo, sizeof totalinfo, "%"PRIdDF_T" of %"PRIdDF_T" documents", nd, total);
	rbody = (union content *)`table() {
		,@make_rbody(d, nd, w, nhandle);
	};
	if (!rbody) {
		return NULL;
	}
	if (!(cslst = make_cwlist(d, nd, w, nhandle, qmax, ctype, &nc, ncsb))) {
		return NULL;
	}
	cwbody = (union content *)`table() {
		,@make_cwlist2(w, cslst, nc);
	};
	if (!cwbody) {
		return NULL;
	}
/* XXX: make_aamatrix is NOT common in sct.q and gss3.q.
   to make them be so, alter d's type to be cluster 
   (currently it is syminfo_t *) */
	if (!(m_aw = make_aamatrix(d, nd, cslst, nc, w))) {
		return NULL;
	}
	if (!(scr = make_script(m_aw, d, nd, cslst, nc))) {
		return NULL;
	}
	return page("result",
		`body() {
			script(type = "text/javascript" language = "JavaScript") {
				,comment(scr);
			}
			script(type = "text/javascript" language = "JavaScript" src = "../sct.js") { "" }
			p() {
				,S(totalinfo);
			}
			form(method = "POST" action = ,action; name = "longquery") {
				input (type = "hidden" name = "push" value = "post") { }
				,textarea();
				br() { }
				,textarea_fss();
				a(href = "../sct_help.html") { "syntax..." }
				br() { }
				,imagesel();
				br() { }
				table() {
					tr() {
						td() {
							div(class = "rbody") { "result" }
						}
						td() {
							div(class = "rword") { "topic-words" }
						}
					}
					tr() {
						td(valign = "top") { ,rbody; }
						td(valign = "top") { ,cwbody; }
					}
				}
				,selectors(dmax, qmax, nhandle, ctype, xref, ncsb);
			}
		}
	);
}

static union content **
make_rbody(d, nd, w, handle)
struct syminfo *d;
df_t nd;
WAM *w;
char const *handle;
{
	WAM *a, *l, *img;
	union content **r;
	int i;
#if ! defined ENABLE_GETA
	char const *titlei = "title";
	char const *linki = "link";
	char const *imgi = "img";
	if (!(a = wam_prop_open(w, WAM_ROW, titlei))) {
		syslog(LOG_DEBUG, "wam_prop_open title: %s", strerror(errno));
		return NULL;
	}
	l = wam_prop_open(w, WAM_ROW, linki);
/*
	if (!(l = wam_prop_open(w, WAM_ROW, linki))) {
		syslog(LOG_DEBUG, "wam_prop_open link: %s", strerror(errno));
		return NULL;
	}
*/
	img = wam_prop_open(w, WAM_ROW, imgi);
#else
	char const *titlei = "titlei";
	char const *linki = "linki";
	char const *imgi = "imgi";
	if (!(a = wam_prop_open(handle, WAM_ROW, titlei))) {
		syslog(LOG_DEBUG, "wam_prop_open title: %s", strerror(errno));
		return NULL;
	}
	if (!(l = wam_prop_open(handle, WAM_ROW, linki))) {
		syslog(LOG_DEBUG, "wam_prop_open link: %s", strerror(errno));
		return NULL;
	}
	img = wam_prop_open(handle, WAM_ROW, imgi);
#endif
	if (img) {
		int j;
#define	NR	7
		if (!(r = calloc((nd + NR - 1)/ NR + 1, sizeof *r))) {
			syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
			return NULL;
		}
		for (i = j = 0; i < nd; j++) {
			int k;
			union content **rr;
			if (!(rr = calloc(NR + 1, sizeof *rr))) {
				return NULL;
			}
			for (k = 0; k < NR && i < nd; k++, i++) {
				union content *c;
				if (!(c = cell(&d[i], i, w, a, l, img))) {
					return NULL;
				}
				rr[k] = (union content *)`td() { ,c; };
			}
			rr[k] = NULL;
			r[j] = (union content *)`tr(valign = "top" ) { ,@rr; };
		}
		r[j] = NULL;
	}
	else {
		if (!(r = calloc(nd + 1, sizeof *r))) {
			syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
			return NULL;
		}
		for (i = 0; i < nd; i++) {
			if (!(r[i] = cell(&d[i], i, w, a, l, img))) {
				return NULL;
			}
		}
		r[i] = NULL;
	}
	return r;
}

static union content *
cell(d, i, w, a, l, img)
struct syminfo *d;
WAM *w, *a, *l, *img;
{
	char score[32];
	char const *p;
	char const *q;
	char *ids, *link;
	union content *title;
	char scaw[32], aid[32];
	idx_t id = d->id;
	snprintf(score, sizeof score, "%5.3f", d->weight);
	snprintf(scaw, sizeof scaw, "scaw(%d)", i + 1);
	snprintf(aid, sizeof aid, "c%da%d", 1, i);
	if (l) link = wam_prop_gets(l, id, &p) != -1 ? strdup(p) : NULL;
	title = wam_prop_gets(a, id, &q) != -1 ? S(q) : NULL;
	ids = strdup(wam_id2name(w, WAM_ROW, id));
	if (!link) link = strdup(ids);
	if (!title) title = strdup("(no title)");
	if (img) {
		return (union content *)`table(valign = "top" align = "center") {
			tr () {
				td(class = "chkbtn") {
					input(type = "checkbox" name = "dsel" value = ,ids;) { }
					,S(score);
				}
			}
			tr() {
				td(onmouseover = ,strdup(scaw); onmouseout = "scaw(0);") {
					a(href = ,link; id = ,strdup(aid); target = "article") {
						,@thumblnk(img, id);
						,title;
					}
				}
			}
		};
	}
	else {
		return (union content *)`tr(valign = "top") {
			td(class = "chkbtn") {
				input(type = "checkbox" name = "dsel" value = ,ids;) { }
			}
			td() {
				,S(score);
			}
			td(onmouseover = ,strdup(scaw); onmouseout = "scaw(0);") {
				a(href = ,link; id = ,strdup(aid); target = "article") {
					,@thumblnk(img, id);
					,title;
				}
			}
		};
	}
}

static union content **
thumblnk(img, id)
WAM *img;
idx_t id;
{
	union content **cs;
	if (img) {
		char const *p;
		char *imgl;
		if (!(cs = calloc(3, sizeof *cs))) {
			return NULL;
		}
		imgl = wam_prop_gets(img, id, &p) != -1 ? strdup(p) : NULL;
		cs[0] = (union content *)`img(src = ,imgl;) { };
		cs[1] = (union content *)`br() { };
		cs[2] = NULL;
	}
	else {
		if (!(cs = calloc(1, sizeof *cs))) {
			return NULL;
		}
		cs[0] = NULL;
	}
	return cs;
}

#if defined ENABLE_GETA
extern int cs_use_new_ism;
#endif

struct cs_elem *
make_cwlist(d, nd, w, handle, qmax, ctype, ncp, ncsb)
struct syminfo *d;
df_t nd, *ncp, qmax;
WAM *w;
char const *handle;
{
	int i, dir, type;
	struct syminfo *rw;
#if ! defined ENABLE_GETA
	df_t nr;
#else
	ssize_t nr;
#endif
	struct cs_elem *cslst;

	dir = WAM_ROW;
	type = WT_SMARTAW;

	nr = qmax;
	if (!(rw = wsh(d, nd, w, dir, type, &nr, NULL
#if ! defined ENABLE_GETA
			, NULL
#endif
		)) || !nd) {
		syslog(LOG_DEBUG, "wsh: %s", strerror(errno));
		return NULL;
	}
	if (ctype == 1) {
		idx_t *m;
		if (!(m = calloc(nd, sizeof *m))) {
			syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
			return NULL;
		}
		for (i = 0; i < nd; i++) {
			m[i] = d[i].id;
		}
		wam_setmask(w, WAM_COL, m, nd, NULL, 0);
		free(m);
	}

	dir = WAM_COL;
	*ncp = 3;
#if defined ENABLE_GETA
	cs_use_new_ism = ncsb;
#endif

	cslst = csb1(rw, nr, w, dir, ncp, CS_HBC, WT_SMART, WT_SMART);

	if (ctype == 1) {
		wam_setmask(w, WAM_COL, NULL, 0, NULL, 0);
	}

	return cslst;
}

static union content **
make_cwlist2(w, cslst, nc)
WAM *w;
struct cs_elem *cslst;
df_t nc;
{
	union content **c;
	df_t i, offset, nt;

	for (i = nt = 0; i < nc; i++) {
		struct cs_elem *e = &cslst[i];
		nt += e->csa.n;
	}

	if (!(c = calloc(nc + 1, sizeof *c))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}
	for (i = offset = 0; i < nc; i++) {
		struct cs_elem *e = &cslst[i];
		char clsname[32];
		char grpname[32];
		char scwga[32], wgid[32];
		snprintf(clsname, sizeof clsname, "cls%"PRIdDF_T, i + 1);
		snprintf(grpname, sizeof grpname, "[%"PRIdDF_T"]", i + 1);
		snprintf(scwga, sizeof scwga, "scwa(%"PRIdDF_T")", nt + i + 1);
		snprintf(wgid, sizeof wgid, "c%"PRIdDF_T"wg", i + 1);
		c[i] = (union content *) `tr() {
			td() {
				div(class = ,strdup(clsname);) {
					table(width = "100%") {
						tr() {
							td(id = ,strdup(wgid);
							   onmouseover = ,strdup(scwga);
							   onmouseout = "scwa(0);"
							   colspan = "2" align = "left"
							) {
								,S(grpname);
							}
						}
						,@cwlist(w, &e->csa, i + 1, offset);
					}
				}
			}
		};
		if (!c[i]) {
			return NULL;
		}
		offset += e->csa.n;
	}
	c[i] = NULL;

	return c;
}

static union content **
cwlist(w, s, cid, offset)
WAM *w;
struct syminfole *s;
df_t cid, offset;
{
	union content **c;
	df_t i;

	if (!(c = calloc(s->n + 1, sizeof *c))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}
	for (i = 0; i < s->n; i++) {
		struct syminfo *e = &s->s[i];
		char scwa[32], wid[32];
		char const *name;
		int dir = WAM_COL;
		if (!(name = wam_id2name(w, dir, e->id))) {
			return NULL;
		}
		snprintf(scwa, sizeof scwa, "scwa(%"PRIdDF_T")", offset + i + 1);
		snprintf(wid, sizeof wid, "c%"PRIdDF_T"w%"PRIdDF_T, cid, i);
		c[i] = (union content *)`tr() {
			td(width = "10") {
				input(type = "checkbox" name = "qsel" value = ,strdup(name);) { }
			}
			td(id = ,strdup(wid);
			   onmouseover = ,strdup(scwa);
			   onmouseout = "scwa(0);") {
				,S(name);
			}
		};
		if (!c[i]) {
			return NULL;
		}
	}
	c[s->n] = NULL;
	return c;
}

static double **
make_aamatrix(d, nd, cslst, nc, w)
struct syminfo *d;
struct cs_elem *cslst;
df_t nd, nc;
WAM *w;
{
	df_t i, j, c, k;
	struct syminfole *sl1, *sl2;
	df_t nsl1, nsl2;
	df_t N;
	freq_t TF;
	struct xr_vec const *v;

	nsl1 = nd;
	for (c = nsl2 = 0; c < nc; c++) {
		struct cs_elem *e = &cslst[c];
		nsl2 += e->csa.n;
	}
	nsl2 += c;

	if (!(sl1 = calloc(nsl1, sizeof *sl1))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}
	if (!(sl2 = calloc(nsl2, sizeof *sl2))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}
	N = wam_size(w, WAM_ROW);
	TF = wam_total_freq_sum(w, WAM_ROW);
	for (i = 0; i < nsl1; i++) {
		struct syminfole *s = &sl1[i];
		df_t n;
		if ((n = wam_get_vec(w, WAM_ROW, d[i].id, &v)) == -1) {
			return NULL;
		}
		s->n = n;
		if (!(s->s = calloc(n, sizeof *s->s))) {
			syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
			return NULL;
		}
		for (j = 0; j < n; j++) {
			struct syminfo *ssj = &s->s[j];
			struct xr_elem const *vej = &v->elems[j];
			ssj->id = vej->id;
			ssj->TF_d = vej->freq;
			ssj->DF = wam_elem_num(w, WAM_COL, vej->id);
		}
	}
	for (c = k = 0; c < nc; c++) {
		struct cs_elem *e = &cslst[c];
		for (i = 0; i < e->csa.n; i++) {
			struct syminfole *s = &sl2[k++];
			df_t n;
			n = 1;
			s->n = n;
			if (!(s->s = calloc(n, sizeof *s->s))) {
				syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
				return NULL;
			}
			j = 0; {
				struct syminfo *ssj = &s->s[j];
				*ssj = e->csa.s[i];
			}
		}
	}
	for (c = 0; c < nc; c++) {
		struct cs_elem *e = &cslst[c];
		df_t n;
		struct syminfole *s = &sl2[k++];
		n = e->csa.n;
		s->n = n;
		if (!(s->s = calloc(n, sizeof *s->s))) {
			syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
			return NULL;
		}
		for (j = 0; j < e->csa.n; j++) {
			struct syminfo *ssj = &s->s[j];
			*ssj = e->csa.s[j];
		}
	}
	assert(k == nsl2);

	return smartmtrxQ(sl1, nsl1, sl2, nsl2, N, TF);
}

static struct element *
empty_result_page(dmax, qmax, handle, ctype, xref, ncsb, action)
df_t dmax, qmax;
char const *handle;
char *action;
{
	return page("empty result",
		`body() {
			p() {
				"no documents found"
			}
			form(method = "POST" action = ,action; name = "longquery") {
				input(type = "hidden" name = "push" value = "post") { }
				,textarea();
				br() { }
				,textarea_fss();
				a(href = "../sct_help.html") { "syntax..." }
				br() { }
				,selectors(dmax, qmax, handle, ctype, xref, ncsb);
			}
		});
}

static struct element *
index_page(action)
char *action;
{
	df_t dmax, qmax;
	dmax = DMAX_DFL;
	qmax = QMAX_DFL;
	return page("index", 
		`body() {
			h1(class = "myclass") { "A Simple Association Service by GETA" }
			form(method = "POST" action = ,action; name = "longquery") {
				input (type = "hidden" name = "push" value = "post") { }
				div(align = "center") {
					table(width = "720") {
						tr(valign = "bottom") {
							td() {
								,textarea();
								br() { }
								,textarea_fss();
								a(href = "../sct_help.html") { "syntax..." }
							}
						}
						tr(align = "left") {
							td() {
								,selectors(dmax, qmax, "", CTYPE_DFL, XREF_DFL, NCSB_DFL);
							}
						}
					}
				}
				,getalink();
			}
		}
	);
}

static struct element *
error_page()
{
	return page("error", `body() { "Not found" });
}

static struct element *
page(title, body)
char const *title;
struct element *body;
{
	return `html(xmlns = "http://www.w3.org/1999/xhtml" lang = "ja") {
		head() {
			meta(http-equiv = "Content-Type"
			     content = "text/html; charset=UTF-8") { }
			title() { ,S(title); }
			style(type = "text/css") { ,S(style); }
		}
		,(union content *)body;
	};
}

static union content *
getalink()
{
	return (union content *)`div() {
		a(href = "http://geta.ex.nii.ac.jp") {
			img(src = "../geta.gif"
			    border = "0"
			    width = "114"
			    height = "28"
			    alt = "Powered by GETA.") { }
		}
	};
}

static union content *
textarea()
{
	return (union content *)`textarea(name = "text" cols = "45" rows = "3") { "" };
}

static union content *
textarea_fss()
{
	return (union content *)`textarea(name = "fss" cols = "45" rows = "3") { "" };
}

static union content *
selectors(dmax, qmax, handle, ctype, xref, ncsb)
df_t dmax, qmax;
char const *handle;
{
	char dmaxs[32], qmaxs[32];
	snprintf(dmaxs, sizeof dmaxs, "%"PRIdDF_T, dmax);
	snprintf(qmaxs, sizeof qmaxs, "%"PRIdDF_T, qmax);
	return (union content *)`table() {
		tr() {
			td() {
				,imagesel();
			}
			td() {
				input(type = "hidden" name = "ohandle" value = ,handle;) { }
			}
			td() {
				,corpus_sel(handle);
			}
			td() {
				,selector("dmax", dmax_list, nelems(dmax_list), sizeof *dmax_list, char_pp, char_pp, dmaxs, "documents");
			}
			td() {
				,selector("qmax", qmax_list, nelems(qmax_list), sizeof *qmax_list, char_pp, char_pp, qmaxs, "topic-words");
			}
			td() {
				"classify topic-words according to ..."
				,ctype_chooser(ctype);
			}
			td() {
				input(type = "checkbox" name = "xref" value = "xref"
				,@checked(xref, 1);) { }
				"feedback selection"
			}
			td() {
				input(type = "checkbox" name = "ncsb" value = "ncsb"
				,@checked(ncsb, 1);) { }
				"ncsb"
			}
		}
	};
}

static union content *
imagesel()
{
	return (union content *)`input(type = "image"
		      src = "../search.gif"
		      name = "i1" border = "0" width = "66" height = "29"
		      alt = "Search") { };
}

static union content *
corpus_sel(handle)
char const *handle;
{
	return selector("nhandle", handle_list, n_handle_list, sizeof *handle_list, hlst_handle, hlst_title, handle, "");
}

static union content *
selector(name, list, nmemb, size, value, print, val, suffix)
char const *name;
void *list;
size_t nmemb, size;
char *(*value)(void *);
char *(*print)(void *);
char const *val;
char const *suffix;
{
	union content **opts;
	int i;
	if (!(opts = calloc(nmemb + 1, sizeof *opts))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}
	for (i = 0; i < nmemb; i++) {
		char const *e = (*value)((char *)list + i * size);
		char const *p = (*print)((char *)list + i * size);
		opts[i] = (union content *)`option(value = ,e; ,selected(e, val);) {
			,S(p); ,S(suffix);
		};
		if (!opts[i]) {
			return NULL;
		}
	}
	opts[i] = NULL;
	return (union content *)`select(name = ,name;) { ,@opts; };
}

static struct attribute *
selected(val, ref)
char const *val;
char const *ref;
{
	if (!strcmp(val, ref)) {
/* XXX */
		return `dummy(selected = "selected") { }->attributes;
	}
	else {
/* XXX */
		return `dummy() { }->attributes;
	}
}

static union content *
ctype_chooser(val)
{
	return (union content *)`table() {
		tr() {
			td() {
				input(type = "radio" name = "ctype" value = "0"
				      ,@checked(val, 0);) { }
			}
			td() {
				"all documents in the corpus"
			}
		}
		tr() {
			td() {
				input(type = "radio" name = "ctype" value = "1"
				      ,@checked(val, 1);) { }
			}
			td() {
				"the retrieved documents"
			}
		}
	};
}

static struct attribute *
checked(val, ref)
{
	if (val == ref) {
/* XXX */
		return `dummy(checked = "checked") { }->attributes;
	}
	else {
/* XXX */
		return `dummy() { }->attributes;
	}
}

#if 0
static union content **
debug(path_info, argc, argv)
char const *path_info;
char *argv[];
{
	union content **r;
	int i, j;
	struct chardata *c;
	char s[8192];

	if (!(r = calloc(argc + 2, sizeof *r))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}

	j = 0;
	if (!(c = alloc_chardata())) {
		syslog(LOG_DEBUG, "alloc_chardata: %s", strerror(errno));
		return NULL;
	}
	r[j] = (union content *)c;
	snprintf(s, sizeof s, "path_info = %s\n", path_info);
	if (!(c->value = strdup(s))) {
		syslog(LOG_DEBUG, "strdup: %s", strerror(errno));
		return NULL;
	}
	c->len = strlen(c->value);

	for (i = 0, j++; i < argc; i++, j++) {
		if (!(c = alloc_chardata())) {
			syslog(LOG_DEBUG, "alloc_chardata: %s", strerror(errno));
			return NULL;
		}
		r[j] = (union content *)c;
		snprintf(s, sizeof s, "argv[%d] = %s\n", i, argv[i]);
		if (!(c->value = strdup(s))) {
			syslog(LOG_DEBUG, "strdup: %s", strerror(errno));
			return NULL;
		}
		c->len = strlen(c->value);
	}
	r[j] = NULL;

	return r;
}
#endif

static int
set_handle_list(c)
struct element *c;
{
	int i;

	if (!c->contents) {
		return -1;
	}
	for (n_handle_list = 0; c->contents[n_handle_list]; n_handle_list++) ;
	if (!(handle_list = calloc(n_handle_list, sizeof *handle_list))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return -1;
	}
	for (i = 0; c->contents[i]; i++) {
		struct attribute *as;
		struct hlst *hp = &handle_list[i];
		struct element *e = &c->contents[i]->element;
		if (e->type != CElem) {
			return -1;
		}
		hp->handle = hp->title = hp->properties = hp->created = hp->narticles = hp->stemmer = hp->description = NULL;
		for (as = e->attributes; as->name; as++) {
			if (!strcmp(as->name, "name")) {
				hp->handle = as->value;
			}
			else if (!strcmp(as->name, "title")) {
				hp->title = as->value;
			}
			else if (!strcmp(as->name, "properties")) {
				hp->properties = as->value;
			}
			else if (!strcmp(as->name, "created")) {
				hp->created = as->value;
			}
			else if (!strcmp(as->name, "number-of-articles")) {
				hp->narticles = as->value;
			}
			else if (!strcmp(as->name, "stemmer")) {
				hp->stemmer = as->value;
			}
			else if (!strcmp(as->name, "description")) {
				hp->description = as->value;
			}
		}
		if (!hp->handle) {
			return -1;
		}
		if (!hp->title) {
			hp->title = hp->handle;
		}
		if (!hp->properties) {
			hp->properties = "";
		}
		if (!hp->created) {
			hp->created = "";
		}
		if (!hp->narticles) {
			hp->narticles = "-";
		}
		if (!hp->stemmer) {
			hp->stemmer = "-";
		}
		if (!hp->description) {
			hp->description = "";
		}
	}
	return 0;
}

int
catalogue_html(path_info, argc, argv, out)
char const *path_info;
char *argv[];
{
	struct element *page;
	struct element *catalogue;

	nputs("Content-Type: text/html; charset=UTF-8\r\n"
		"Pragma: no-cache\r\n"
		, out);
	if (!(catalogue = read_catalogue())) {
		nputs("\r\n", out);
		errhtml("ERROR: cannot read catalogue", out);
		return 0;
	}
	if (set_handle_list(catalogue) == -1) {
		nputs("\r\n", out);
		errhtml("ERROR: set handle list failed.", out);
		return 0;
	}

	if (argc > 0 && !strcmp(argv[0], "sort_key=created")) {
		qsort(handle_list, n_handle_list, sizeof *handle_list, ccompar);
	}
	else if (argc > 0 && !strcmp(argv[0], "sort_key=title")) {
		qsort(handle_list, n_handle_list, sizeof *handle_list, tcompar);
	}
	else {
		qsort(handle_list, n_handle_list, sizeof *handle_list, ncompar);
	}

	page = `html() {
		head() {
			meta(http-equiv = "Content-Type"
			     content = "text/html; charset=UTF-8") { }
			title() { "GETAssoc Index Catalogue" }
			style(type = "text/css") { ,S(style_cat); }
		}
		body() {
			table() { 
				tr() {
					td() {
						a(href = "catalogue.html") {
							"NAME"
						}
					}
					td() {
						a(href = "catalogue.html?sort_key=title") {
							"TITLE"
						}
					}
					td() {
						"PROPERTIES"
					}
					td() {
						a(href = "catalogue.html?sort_key=created") {
							"CREATED"
						}
					}
					td() {
						"#ARTICLES"
					}
					td() {
						"#STEMMER"
					}
					td() {
						"DESCRIPTION"
					}
				}
				,@catalist(catalogue); }
		}
	};
	if (page) {
		struct nstring ns;
		ssize_t l;
		NS_ZERO(&ns);
		if ((l = unparse_fn(page, &ns, writefn_ns_append)) == -1) {
			goto error;
		}
		(void)send_text_content(&out, NULL, ns.ptr, l, 1);
		NS_FREE(&ns);
	}
	else {
error:
		nputs("\r\n", out);
		errhtml("ERROR!!", out);
	}
	return 0;
}

static union content **
catalist(c)
struct element *c;
{
	union content **cc;
	int i;

	if (!(cc = calloc(n_handle_list + 1, sizeof *cc))) {
		return NULL;
	}

	for (i = 0; i < n_handle_list; i++) {
		struct hlst *hp = &handle_list[i];
		char buf[32];
		time_t clock;
		clock = strtotimet(hp->created);
		cc[i] = (union content *)`tr() {
			td() {
				,S(hp->handle);
			}
			td() {
				,S(hp->title);
			}
			td() {
				,S(hp->properties);
			}
			td() {
				,S(utime_r(&clock, buf));
			}
			td() {
				,S(hp->narticles);
			}
			td() {
				,S(hp->stemmer);
			}
			td() {
				,S(hp->description);
			}
		};
		if (!cc[i]) {
			return NULL;
		}
	}
	cc[i] = NULL;
	return cc;
}

int
index_html(path_info, argc, argv, out)
char const *path_info;
char *argv[];
{
	struct element *page;
#if defined ENABLE_REN
	struct element *b, *ul, *li;
#endif

	nputs("Content-Type: text/html; charset=UTF-8\r\n"
		"Pragma: no-cache\r\n"
		, out);
	page = `html() {
		head() {
			meta(http-equiv = "Content-Type"
			     content = "text/html; charset=UTF-8") { }
			title() { "GETAssoc" }
		}
		body() {
			ul() {
				li() {
					a(href = "catalogue.html") {
						"catalogue"
					}
				}
				li() {
					a(href = "sct/index.html") {
						"getassoc search"
					}
				}
			}
		}
	};
#if defined ENABLE_REN
	b = &page->contents[1]->element;
	assert(!strcmp(b->name, "body"));
	ul = &b->contents[0]->element;
	assert(!strcmp(ul->name, "ul"));
	li = `li() {
		a(href = "rn/") {
			"REN!"
		}
	};
	add_content(ul, (union content *)li);
#endif
	if (page) {
		struct nstring ns;
		ssize_t l;
		NS_ZERO(&ns);
		if ((l = unparse_fn(page, &ns, writefn_ns_append)) == -1) {
			goto error;
		}
		(void)send_text_content(&out, NULL, ns.ptr, l, 1);
		NS_FREE(&ns);
	}
	else {
error:
		nputs("\r\n", out);
		errhtml("ERROR!", out);
	}
	return 0;
}

static void
errhtml(msg, out)
char const *msg;
{
	nputs("<html><head></head><body>", out);
	nputs(msg, out);
	nputs("</body>\n", out);
}

static char *
hlst_handle(v)
void *v;
{
	return ((struct hlst *)v)->handle;
}

static char *
hlst_title(v)
void *v;
{
	return ((struct hlst *)v)->title;
}

static char *
char_pp(v)
void *v;
{
	return *(char **)v;
}

static int
ncompar(va, vb)
void const *va;
void const *vb;
{
	struct hlst const *a;
	struct hlst const *b;
	a = va;
	b = vb;
	return strcmp(a->handle, b->handle);
}

static int
tcompar(va, vb)
void const *va;
void const *vb;
{
	struct hlst const *a;
	struct hlst const *b;
	a = va;
	b = vb;
	return strcmp(a->title, b->title);
}

static int
ccompar(va, vb)
void const *va;
void const *vb;
{
	struct hlst const *a;
	struct hlst const *b;
	time_t ca, cb;
	a = va;
	b = vb;
	ca = strtotimet(a->created);
	cb = strtotimet(b->created);
	if (ca < cb) {
		return -1;
	}
	else if (ca > cb) {
		return 1;
	}
	return 0;
}

static char *
utime_r(clock, buf)
const time_t *clock;
char *buf;
{
	static char const *wday[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	static char const *mon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	struct tm *tm;
	tm = gmtime(clock);
	sprintf(buf, "%s %s %2d %02d:%02d:%02d %04d", wday[tm->tm_wday], mon[tm->tm_mon], tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_year + 1900);
	return buf;
}





/*
 <query> ::= <cquery> [ | <cquery> ]...
 <cquery> ::= <squery> [ & <squery> ]...
 <squery> ::= [ ~ ] <text> [ @ <segment-spec> ] [ # <options> ]
 <segment-spec> ::= <empty> | <segment-spec-1> [ , <segment-spec-1> ]...
 <segment-spec-1> ::= <number> | <number>-<number>
 <options> ::= <option> [ , <option> ]...
 <option> ::= ignore-case | ...
 <text> ::= any string, escape character is a '\'
 */

#define	isdelim(c)	((c) == '#' || (c) == '@' || (c) == '&' || (c) == '|')

static int readfn(void *, char *, int);

struct cookie {
	char *p, *e;
};

static int
readfn(d, buf, nbytes)
void *d;
char *buf;
{
	struct cookie *c;
	int len, rest;

	c = d;

	rest = c->e - c->p;
	len = MIN(nbytes, rest);

	memmove(buf, c->p, len);
	c->p += len;

	return len;
}

static int
qstr2fss(src, fss)
char *src;
struct fss_query *fss;
{
	char *xml;
	struct document *d;
	struct cookie c;
	struct gss3_parser_t *psr;

	if (!(xml = qstr2xml(src))) {
		syslog(LOG_DEBUG, "qstr2xml failed");
		return -1;
	}
	c.p = xml;
	c.e = xml + strlen(xml);
	if (!(d = xml2cxmlNSfn(&c, readfn, "", strlen(xml), 0))) {
		syslog(LOG_DEBUG, "xml2cxmlNSfn failed");
		free(xml);
		return -1;
	}
	free(xml);
	if (!(psr = gss3_create_parser(GSS3_PARSER_XNS))) {
		return -1;
	}
	if (parse_search(d->element, fss, psr) == -1) {
		syslog(LOG_DEBUG, "parse_search failed");
		return -1;
	}
	gss3_destroy_parser(psr);
	/* XXX: cxml_free(d); siteha ikenai */

	return 0;
}

static char *
qstr2xml(src)
char *src;
{
	struct fss_gettoken_state s0, *s = &s0;
	int t;
	char *options = NULL, *segms, *text = NULL;
	struct nstring ns0, *ns = &ns0;
	char str[128];

	s->p = src;
	NS_ZERO(ns);

	segms = strdup("0-31");

	ns_append(ns, "<?xml version=\"1.0\"?><search\n><join\n>", -1);
	for (t = fss_gettoken(s); t; ) {
		if (t == 'E') {
			syslog(LOG_DEBUG, "tokenizer error");
			return NULL;
		}
		switch (t) {
			char c;
		case 'T':
			if (s->negativep) {
				c = 'n';
			}
			else {
				c = 'p';
			}
			free(text);
			text = strdup(s->buf);
			for (t = fss_gettoken(s); t == '@' || t == '#'; t = fss_gettoken(s)) {
				if (t == '#') {
					free(options);
					options = strdup(s->buf);
				}
				else {
					free(segms);
					segms = strdup(s->buf);
				}
			}
			snprintf(str, sizeof str, "<%c segments=\"", c);
			ns_append(ns, str, -1);

			if (segms) ns_append(ns, segms, -1);

			ns_append(ns, "\" options=\"", -1);

			if (options) ns_append(ns, options, -1);
			ns_append(ns, "\"\n><![CDATA[", -1);

			ns_append(ns, text, -1);

			snprintf(str, sizeof str, "]]></%c\n>", c);
			ns_append(ns, str, -1);
			break;
		case '@':
		case '#':
			syslog(LOG_DEBUG, "unexpected token: %c", t);
			return NULL;
		case '&':
			/* just ignore */
			t = fss_gettoken(s);
			break;
		case '|':
			ns_append(ns, "</join\n><join\n>", -1);
			t = fss_gettoken(s);
			break;
		default:
			syslog(LOG_DEBUG, "internal error");
			return NULL;
		}
	}
	ns_append(ns, "</join\n></search\n>", -1);

	free(options);
	free(segms);
	free(text);

	return ns->ptr;
}

static int
fss_gettoken(s)
struct fss_gettoken_state *s;
{
	int escaped = 0;
	char *q, *r;
	size_t l;

	s->token = '\0';
	s->negativep = 0;

	for (; isspace(*s->p & 0xff); s->p++) ;

	q = s->p;
	switch (*q) {
	case '\0':
		return 0;
	case '@':
	case '#':
		s->token = *q;
		s->p = ++q;
		for (; *q && !isdelim(*q); q++) ;
		break;

	case '&':
		s->token = *q;
		s->p = ++q;
		break;

	case '|':
		s->token = *q;
		s->p = ++q;
		break;

	case '~':
		s->negativep = 1;
		q++;
		for (; isspace(*q & 0xff); q++) ;
		s->p = q;
		/* FALLTHURU */
	default:
		s->token = 'T';
		for (; *q && !isdelim(*q); q++) {
			if (*q == '\\') {
				escaped++;
				q++;
			}
		}
		break;
	}

	for (r = q; s->p < r && isspace(*(r - 1) & 0xff); r--) ;

	l = r - s->p;
	if (l - escaped >= sizeof s->buf) {
		return 'E';
	}
	if (escaped) {
		size_t j, k;
		for (j = k = 0; j < l; j++) {
			if (s->p[j] == '\\') {
				j++;
			}
			s->buf[k++] = s->p[j];
		}
		s->buf[k] = '\0';
	}
	else {
		memmove(s->buf, s->p, l);
		s->buf[l] = '\0';
	}

	s->p = q;
	return s->token;
}

int
search_gif(path_info, argc, argv, out)
char const *path_info;
char *argv[];
{
	return send_file_datadir_relative("search.gif", out, "image/gif");
}

int
geta_gif(path_info, argc, argv, out)
char const *path_info;
char *argv[];
{
	char path[MAXPATHLEN];
	int n;
	n = random() % 5 + 1;
	snprintf(path, sizeof path, "geta_%d.gif", n);
	return send_file_datadir_relative(path, out, "image/gif");
}

int
sct_js(path_info, argc, argv, out)
char const *path_info;
char *argv[];
{
	return send_file_datadir_relative("sct.js", out, "text/javascript");
}

int
sct_help(path_info, argc, argv, out)
char const *path_info;
char *argv[];
{
	return send_file_datadir_relative("sct_help.html", out, "text/html");
}
