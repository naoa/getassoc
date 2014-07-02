/*	$Id: gss3.q,v 1.154 2011/10/25 08:41:33 nis Exp $	*/

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
static char rcsid[] = "$Id: gss3.q,v 1.154 2011/10/25 08:41:33 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <unistd.h>

#include <expat.h>

#include "nwam.h"
#include "util.h"
#include "nio.h"

#include "cxml.h"
#include "xstem.h"

#include "nstring.h"
#include "common.h"
#include "gorj.h"
#include "print.h"
#include "getassocP.h"

#include <gifnoc.h>

#define	NOPROPERROR	1

#if defined ENABLE_GETA
#undef ASSV_DBGINFO
#endif

struct props {
	int n;
	char **names;
	WAM **ws;
	WAM *fss;
};

struct gss3_g {
	int dir, rdir;
	char *target;
	df_t *cutoff_df_vec;
	struct syminfo *query;
	df_t nquery;
	df_t narticles, nkeywords;
	struct syminfo *it;
	struct {
		df_t nacls, nkcls, niwords, yykn;
	} ns_dflt, ns;
#if ! defined ENABLE_GETA
	df_t atotal, ktotal;
#else
	ssize_t atotal, ktotal;
#endif
	struct cs_elem *ac, *kc;
	df_t a_offset;
	char *a_props, *req_matrix;
	int xref_types, xstage;
	struct {
		char *stage1, *stage2, *stage3, *cs_type, *cs_v, *cs;
	} sim_nam;
	struct {
		int stage1, stage2, stage3, cs_type, cs_v, cs;
	} sim_dflt, sim;
	struct {
		char *expression;
		struct fss_query fss;
	} filter;
	struct fss_query fss;
	df_t nsl0m, nsl0aam, nsl0kkm;
	double **m, **aam, **kkm, **wm;
#if defined ASSV_DBGINFO
	int gen_assv_dbginfo;
#endif
	df_t cutoff_df_global;
	int have_cutoff_df_global;
	int xfss_downsample;
	struct {
		char *stemmer;
	} st_dflt, st;
};

#define      NS_GSS3      "http://gss3.cs.nii.ac.jp/"

#define	NSX	"\001"

struct elname {
	char const *name;
	unsigned int id;
};

#define M_ASSOC		1
#define M_GETVEC	2
#define M_GETPROP	3
#define M_INQUIRE	4
#define M_SEARCH	5
#define M_FILTER	6
#define M_FREETEXT	7
#define M_ARTICLE	8

#define M_JOIN	9
#define M_P	10
#define M_N	11
#define M_KEYWORD	12

#define M_GSS		13
#if defined ELEM_VEC
#define M_VECTOR	14
#endif
#define M_SIMV	15

/* XXX: this list must be sorted by ASCII order */
static struct elname elnames_ns[] = {
	{NS_GSS3 NSX "article", M_ARTICLE},
	{NS_GSS3 NSX "assoc", M_ASSOC},
	{NS_GSS3 NSX "filter", M_FILTER},
	{NS_GSS3 NSX "freetext", M_FREETEXT},
	{NS_GSS3 NSX "getprop", M_GETPROP},
	{NS_GSS3 NSX "getvec", M_GETVEC},
	{NS_GSS3 NSX "gss", M_GSS},
	{NS_GSS3 NSX "inquire", M_INQUIRE},
	{NS_GSS3 NSX "join", M_JOIN},
	{NS_GSS3 NSX "keyword", M_KEYWORD},
	{NS_GSS3 NSX "n", M_N},
	{NS_GSS3 NSX "p", M_P},
	{NS_GSS3 NSX "search", M_SEARCH},
	{NS_GSS3 NSX "simv", M_SIMV},
#if defined ELEM_VEC
	{NS_GSS3 NSX "vec", M_VECTOR},
#endif
};

static struct elname elnames_xns[] = {
	{"article", M_ARTICLE},
	{"assoc", M_ASSOC},
	{"filter", M_FILTER},
	{"freetext", M_FREETEXT},
	{"getprop", M_GETPROP},
	{"getvec", M_GETVEC},
	{"gss", M_GSS},
	{"inquire", M_INQUIRE},
	{"join", M_JOIN},
	{"keyword", M_KEYWORD},
	{"n", M_N},
	{"p", M_P},
	{"search", M_SEARCH},
	{"simv", M_SIMV},
#if defined ELEM_VEC
	{"vec", M_VECTOR},
#endif
};

#define M_TARGET	1
#define M_NARTICLES	2
#define M_NKEYWORDS	3
#define M_NACLS		4
#define M_NKCLS		5
#define M_AOFFSET	6
#define M_APROPS	7
#define M_CROSSREF	8
#define M_NIWORDS	9
#define M_YYKN		10
#define M_XSTAGE	11
#define M_TYPE 		12

#define	M_SEGMENTS	14
#define	M_OPTIONS	15
#define	M_EXPRESSION	16
#define	M_CUTOFF_DF	17
#define	M_NAME		18
#define	M_VEC		19
#define	M_SOURCE	20
#define	M_SCORE		21

#define	M_STAGE1_SIM	22
#define	M_STAGE2_SIM	23
#define	M_STAGE3_SIM	24
#define	M_CS_TYPE	25
#define	M_CS_VSIM	26
#define	M_CS_SIM	27

#define	M_LANG		28

#define	M_VERSION	29
#if defined ASSV_DBGINFO
#define	M_DBGINFO	30
#endif
#define M_DIRECTION	31
#define	M_TFd		32
#define	M_XFSS_DOWNSAMPLE	33

#define	M_STEMMER		34

/* XXX: this list must be sorted by ASCII order */
static struct elname atnames_xns[] = {
	{"TFd", M_TFd},
	{"a-offset", M_AOFFSET},
	{"a-props", M_APROPS},
	{"cross-ref", M_CROSSREF},
	{"cs-sim", M_CS_SIM},
	{"cs-type", M_CS_TYPE},
	{"cs-vsim", M_CS_VSIM},
	{"cutoff-df", M_CUTOFF_DF},
#if defined ASSV_DBGINFO
	{"dbginfo", M_DBGINFO},
#endif
	{"direction", M_DIRECTION},
	{"expression", M_EXPRESSION},
	{"lang", M_LANG},
	{"nacls", M_NACLS},
	{"name", M_NAME},
	{"narticles", M_NARTICLES},
	{"niwords", M_NIWORDS},
	{"nkcls", M_NKCLS},
	{"nkeywords", M_NKEYWORDS},
	{"options", M_OPTIONS},
	{"score", M_SCORE},
	{"segments", M_SEGMENTS},
	{"source", M_SOURCE},
	{"stage1-sim", M_STAGE1_SIM},
	{"stage2-sim", M_STAGE2_SIM},
	{"stage3-sim", M_STAGE3_SIM},
	{"stemmer", M_STEMMER},
	{"target", M_TARGET},
	{"type", M_TYPE},
	{"vec", M_VEC},
	{"version", M_VERSION},
	{"xfss-downsample", M_XFSS_DOWNSAMPLE},
	{"xstage", M_XSTAGE},
	{"yykn", M_YYKN},
};

struct gss3_parser_t {
	struct {
		struct elname *names;
		size_t size;
	} el, at;
};
#define	ELTYPE(name, psr) (eltype((name), (psr)->el.names, (psr)->el.size))
#define	ATTYPE(name, psr) (eltype((name), (psr)->at.names, (psr)->at.size))

struct xtt {
	char **targets;
	size_t size, ntargets;
};

struct bex_gettoken_t {
	char d[2];
	char *t;
	size_t tlen;
};

#if defined ENABLE_PROXY
static int extract_targets(struct element *, struct xtt *, struct gss3_parser_t *);
#endif
static struct element *process_gss(struct element *, struct gss3_parser_t *);
static struct element *process_assoc(struct element *, struct gss3_parser_t *);
static int init_g(struct gss3_g *);
#if 0
static struct element *process_simv(struct element *, struct gss3_parser_t *);
#endif
static struct element *read_stage1_data(WAM *, struct gss3_g *, struct element *, struct gss3_parser_t *, int);
static int parse_join(struct element *, fss_seg_t, xsc_opt_t, struct fss_con_query *, struct gss3_parser_t *);
static int parse_p(struct element *, int, fss_seg_t, xsc_opt_t, struct fss_simple_query *, struct gss3_parser_t *);
static int get_segm_opt(struct element *, fss_seg_t *, xsc_opt_t *, struct gss3_parser_t *);
static struct element *get_filter(struct gss3_g *, struct element *, struct gss3_parser_t *);
static struct element *parse_freetext(WAM *, struct gss3_g *, struct element *, struct xr_vec const **, df_t *, struct gss3_parser_t *);
static struct element *parse_article_keyword(WAM *, struct gss3_g *, struct element *, struct xr_vec const **, df_t *, struct gss3_parser_t *);
static struct element *read_stage2_data(WAM *, struct gss3_g *, struct element *, struct gss3_parser_t *);
static struct element *process_getvec(struct element *, struct gss3_parser_t *);
static struct element *process_getprop(struct element *, struct gss3_parser_t *);
static struct element *process_inquire(struct element *, struct gss3_parser_t *);
static int getval(struct attribute *, void *, int);
static int assoc_stage_1(WAM *, struct gss3_g *);
#if 0
static int simv_stage_1(WAM *, struct gss3_g *);
#endif
static int assoc_stage_2(WAM *, struct gss3_g *, char **);
static int stage2_p1(WAM *, int, struct gss3_g *, struct syminfo **, df_t *, char **);
static struct bxue_t *expr2bex(WAM *, int, char *, char **, df_t *);
static char *bex_gettoken_r(char *, char **, struct bex_gettoken_t *);
static struct element *assoc1_result(WAM *, struct gss3_g *);
static struct element *simv_result(WAM *, struct gss3_g *);
static struct element *result_page(WAM *, struct gss3_g *, struct cs_elem *, df_t, df_t, struct cs_elem *, df_t, df_t);
#if defined ASSV_DBGINFO
static union content **dbginfo_iw(WAM *, struct gss3_g *);
static char *dbginfo_iw_str(WAM *, struct gss3_g *);
#endif
static char **splitprops(char *);
static struct props *openprops(WAM *, char *, int, char **);
static void freeprops(struct props *);
static int pull_nam(WAM *, int, struct cs_elem *, df_t);
static union content **clslst(WAM *, int, struct cs_elem *, df_t, struct cs_elem *, df_t, struct props *, double **, double **);
static struct element *clsx(WAM *, int, struct cs_elem *, struct props *, struct cs_elem *, df_t, struct cs_elem *, df_t, double **, double **, int);
static int fn_clse_m(struct cs_elem *, df_t, struct syminfo *, df_t, void *, df_t);
#if defined ASSV_DBGINFO
static char *qw2str(double *, df_t);
#endif
static struct element *clse(WAM *, int, struct syminfo *, struct props *, struct cs_elem *, df_t, struct cs_elem *, df_t, double **, double **, int);
static int fill_as(WAM *, int, idx_t, struct props *, struct attribute *, size_t);
static struct element *xlist(char const *, char const *, double);
static struct element *x_error(char *, ...);
static int fn_ak_sl0(struct cs_elem *, df_t, struct syminfo *, df_t, void *, df_t);
static int fn_ak_sl1(struct cs_elem *, df_t, struct syminfo *, df_t, void *, df_t);
static double **make_akmatrix(struct cs_elem *, df_t, struct cs_elem *, df_t, WAM *, int, df_t *);
static double **make_aakkmatrix(struct cs_elem *, df_t, WAM *, int, df_t *);
static struct element *empty_result(char const *);
static df_t cutoff(df_t);
static char *cont2str(union content **);
static size_t contlen(union content **);
static unsigned int eltype(const XML_Char *, struct elname *, size_t);
static int elcompar(const void *, const void *);
static int decode_weight(char const *, int);
static int decode_cstype(char const *, int);
static int decode_xref_types(char const *);

static int readfn_fread(void *, char *, int);
static int scompar(const void *, const void *);

#if defined ENABLE_NEWLAYOUT
#define	E_RCFILE	(-2)
#define	E_RCMALF	(-3)
#define	E_RCMVAL	(-4)
static int read_rcfile(struct gss3_g *, char const *);
#endif

#define	S string2chardata
#define	XMLDECL	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"

static struct {
	char const *type;
	char const *target;
} cmd;

extern char *getaroot;

#if defined ENABLE_NEWLAYOUT
static char *gss3_rc = NULL;
static char gss3_rc0[MAXPATHLEN];
#endif

#if defined ENABLE_NEWLAYOUT
int
gss3_init(getaroot, rcfile)
char const *getaroot, *rcfile;
{

	if (*rcfile == '/' || !getaroot) {
		gss3_rc = rcfile;
	}
	else {
		snprintf(gss3_rc0, sizeof gss3_rc0, "%s/%s", getaroot, rcfile);
		gss3_rc = gss3_rc0;
	}
	return 0;
}
#endif

#if defined REC_REQ_RAW
FILE *rec_req_raw_f = NULL;
#endif

struct gss3_parser_t *
gss3_create_parser(type)
{
	struct gss3_parser_t *p;
/* XXX: qsort(atnames, nelems(atnames), sizeof *atnames, elcompar); */
/* XXX: qsort(elnames, nelems_elnames, sizeof *elnames, elcompar); */
	if (!(p = calloc(1, sizeof *p))) {
		return NULL;
	}
	assert(nelems(elnames_ns) == nelems(elnames_xns));
	p->el.size = nelems(elnames_xns);
	switch (type) {
	case GSS3_PARSER_XNS:
		p->el.names = elnames_xns;
		break;
	case GSS3_PARSER_NS:
		p->el.names = elnames_ns;
		break;
	default:
		free(p);
		return NULL;
	}
	p->at.names = atnames_xns;
	p->at.size = nelems(atnames_xns);
	return p;
}

void
gss3_parser_setxns(p)
struct gss3_parser_t *p;
{
	p->el.names = elnames_xns;
}

void
gss3_destroy_parser(p)
struct gss3_parser_t *p;
{
	free(p);
}

int
gss3(in, path_info, clen, out)
char const *path_info;
size_t clen;
{
	FILE *fin;

	if (!(fin = fdopen(in, "r"))) {
		syslog(LOG_DEBUG, "fdopen: %s", strerror(errno));
		return -1;
	}
	return gss3fn(fin, readfn_fread, path_info, clen, out, GSS3FN_HDR);
}

int
gss3fl(stream, path_info, clen, out, flags)
FILE *stream;
char const *path_info;
size_t clen;
{
	return gss3fn(stream, readfn_fread, path_info, clen, out, flags);
}

#if defined ENABLE_PROXY
struct document *
gss3xtargets(cookie, readfn, path_info, clen, targets, ntargets)
void *cookie;
int (*readfn)(void *, char *, int);
char const *path_info;
size_t clen, *ntargets;
char ***targets;
{
	struct document *d;
	struct element *e;
	size_t i, j, k;
	struct xtt xtt0, *xtt = &xtt0;
	struct gss3_parser_t *psr;

	xtt->targets = NULL;
	xtt->size = xtt->ntargets = 0;

	if (!(psr = gss3_create_parser(GSS3_PARSER_NS))) {
		return NULL;
	}
	if (!(d = xml2cxmlNSfn(cookie, readfn, "", clen, NSX[0]))) {
		syslog(LOG_DEBUG, "xml2cxmlNSfn failed");
		return NULL;
	}
	if (d->element->type != CElem) {
		e = x_error("malformed request (!ELEM)");
		syslog(LOG_DEBUG, "!CElem");
		return NULL;
	}
/* actually, extract_targets does not care about NS. we may omit this initialization. */
	if (!index(d->element->name, NSX[0])) {
		gss3_parser_setxns(psr);
	}
	if (extract_targets(d->element, xtt, psr) == -1) {
		free(xtt->targets);
		syslog(LOG_DEBUG, "!xtract_targets");
		return NULL;
	}
	if (xtt->ntargets) {
		qsort(xtt->targets, xtt->ntargets, sizeof *xtt->targets, scompar);
		for (i = k = 0; i < xtt->ntargets; i = j) {
			for (j = i + 1; j < xtt->ntargets && !strcmp(xtt->targets[i], xtt->targets[j]); j++) ;
			xtt->targets[k++] = xtt->targets[i];
		}
	}

	*targets = xtt->targets;
	*ntargets = xtt->ntargets;

	gss3_destroy_parser(psr);
	return d;
}

static int
extract_targets(e, xtt, psr)
struct element *e;
struct xtt *xtt;
struct gss3_parser_t *psr;
{
	char *target;
	struct attribute *as;
	union content **cs;

	for (as = e->attributes; as && as->name; as++) {
		unsigned int type;
		type = ATTYPE(as->name, psr);
		switch (type) {
		case M_TARGET:
		case M_SOURCE:
			getval(as, &target, 's');	/* never fails */
			if (xtt->size <= xtt->ntargets) {
				void *new;
				size_t newsize = xtt->ntargets;
				newsize = NA(newsize + 1, 32);
				if (!(new = realloc(xtt->targets, newsize * sizeof *xtt->targets))) {
					return -1;
				}
				xtt->size = newsize;
				xtt->targets = new;
			}
			xtt->targets[xtt->ntargets++] = target;
			break;
		default: break;
		}
	}
	for (cs = e->contents; cs && *cs; cs++) {
		struct element *ee = &(*cs)->element;
		if (ee->type != CElem) {
			continue;
		}
		if (extract_targets(ee, xtt, psr) == -1) {
			return -1;
		}
	}
	return 0;
}
#endif

int
gss3fn(cookie, readfn, path_info, clen, out, flags)
void *cookie;
int (*readfn)(void *, char *, int);
char const *path_info;
size_t clen;
{
	int ns_support;
	struct document *d;
	struct element *e;
	struct gss3_parser_t *psr;

	ns_support = 1;
	if (!(psr = gss3_create_parser(GSS3_PARSER_NS))) {
		return -1;
	}
	if (flags & GSS3FN_HDR) {
		nputs("Content-Type: text/xml; charset=UTF-8\r\n"
			"Pragma: no-cache\r\n", out);
	}
cmd.type = "";
cmd.target = "";
#if defined REC_REQ_RAW
 {
	struct timeval tm;
	char path[MAXPATHLEN];
	int fd;
	gettimeofday(&tm, NULL);
	tm.tv_sec -= 1205992080;
	snprintf(path, sizeof path, REC_DIR "/%08x%03d.XXXXXX", (int)tm.tv_sec, (int)(tm.tv_usec / 1000));
	if ((fd = mkstemp(path)) != -1) {
		if (rec_req_raw_f = fdopen(fd, "w")) {
			chmod(path, 0666);
		}
		else {
			close(fd);
			unlink(path);
		}
	}
	else {
		rec_req_raw_f = NULL;
	}
}
#endif
	if (d = xml2cxmlNSfn(cookie, readfn, "", clen, NSX[0])) {
#if defined REC_REQ
 {
	struct timeval tm;
	char path[MAXPATHLEN];
	int fd;
	gettimeofday(&tm, NULL);
	tm.tv_sec -= 1205992080;
	snprintf(path, sizeof path, REC_DIR "/%08x%03d.XXXXXX", (int)tm.tv_sec, (int)(tm.tv_usec / 1000));
	if ((fd = mkstemp(path)) != -1) {
		FILE *f;
		if (f = fdopen(fd, "w")) {
			unparse(d->element, f);
			fclose(f);
			chmod(path, 0666);
		}
		else {
			close(fd);
			unlink(path);
		}
	}
}
#endif

		if (d->element->type != CElem) {
			e = x_error("malformed request (!ELEM)");
			ns_support = 0;
			gss3_parser_setxns(psr);
		}
		else {
			unsigned int type;
			if (!index(d->element->name, NSX[0])) {
				ns_support = 0;
				gss3_parser_setxns(psr);
			}
			type = ELTYPE(d->element->name, psr);

			switch (type) {
			case M_GSS:
				e = process_gss(d->element, psr); break;
			case M_ASSOC:
				e = process_assoc(d->element, psr); break;
#if 0
			case M_SIMV:
				e = process_simv(d->element, psr); break;
#endif
			case M_GETVEC:
				e = process_getvec(d->element, psr); break;
			case M_GETPROP:
				e = process_getprop(d->element, psr); break;
			case M_INQUIRE:
				e = process_inquire(d->element, psr); break;
			default:
				e = x_error("unknown request"); break;
			}
		}
	}
	else {
		e = x_error("malformed request: parse error");
		ns_support = 0;
		gss3_parser_setxns(psr);
	}
#if defined REC_REQ_RAW
	if (rec_req_raw_f) {
		fclose(rec_req_raw_f);
		rec_req_raw_f = NULL;
	}
#endif
	if (!e) {
		e = x_error("internal error ~e");
	}
	if (e) {
		char elap[32];
		struct nstring ns;
		ssize_t l, ll;
		if (ns_support) {
			add_attribute(e, strdup("xmlns"), strdup(NS_GSS3));
		}
		timer_sprint_elapsed(NULL, NULL, elap, sizeof elap);
		ll = strlen(XMLDECL "\n");
		add_attribute(e, strdup("user-time"), strdup(elap));
		NS_ZERO(&ns);
		ns_append(&ns, XMLDECL "\n", ll);
		if ((l = unparse_fn(e, &ns, writefn_ns_append)) == -1) {
			goto error;
		}
		(void)send_text_content(&out, NULL, ns.ptr, l + ll, flags & GSS3FN_HDR);
		NS_FREE(&ns);
	}
	else {
error:
		if (flags & GSS3FN_HDR) {
			nputs("\r\n", out);
		}
		nputs(XMLDECL "\n", out);
		nputs("<result ", out);
		if (ns_support) {
			nputs("xmlns=\"" NS_GSS3 "\" ", out);
		}
		nputs("status=\"ERROR\" reason=\"UNKNOWN\"\n/>", out);
	}
	if (d) free_document(d);
	if (e) free_element(e);
	gss3_destroy_parser(psr);
	return 0;
}

static struct element *
process_gss(q, psr)
struct element *q;
struct gss3_parser_t *psr;
{
	struct attribute *as;
	char *version = NULL;
	union content **c;
	struct element *el, *e;
	unsigned int type;

	for (as = q->attributes; as->name; as++) {
		type = ATTYPE(as->name, psr);
		switch (type) {
		case M_VERSION: getval(as, &version, 's'); break;
		default: break;
		}
	}
	if (!version || strcmp(version, "3.0")) {
		e = x_error("malformed request (version is missing or mismatch)");
		goto end;
	}
	for (c = q->contents; c && c[0] && c[0]->element.type == CString; c++) ;
	if (!c || !c[0] || c[0]->element.type != CElem) {
		e = x_error("malformed request (no content)");
		goto end;
	}
	el = &c[0]->element;
	for (c++; c[0] && c[0]->element.type == CString; c++) ;
	if (c[0]) {
		e = x_error("malformed request (extra elements)");
		goto end;
	}
	type = ELTYPE(el->name, psr);
syslog(LOG_DEBUG, "request = %s", el->name);
	switch (type) {
	case M_ASSOC:
		e = process_assoc(el, psr); break;
#if 0
	case M_SIMV:
		e = process_simv(el, psr); break;
#endif
	case M_GETPROP:
		e = process_getprop(el, psr); break;
	case M_INQUIRE:
		e = process_inquire(el, psr); break;
	default:
		e = x_error("unknown request (subtype)"); break;
	}
end:
	return `gss(version = "3.0") { ,e; };
}

/*#define	INTWN	200	/* default # of intermediate words */
#define	INTWN	70	/* default # of intermediate words */
#define	YYKN	10	/* default # of articles which used for extracting the topic words */

static struct element *
process_assoc(q, psr)
struct element *q;
struct gss3_parser_t *psr;
{
	size_t i;
	struct gss3_g g0, *g = &g0;
	WAM *tw = NULL;	/* XXX kore mo `g' ni utusubeki? */
/* XXX atikoti ni tirabatte iru `dir' wo `g' ni irete okuto, soutui ban mo zitugen kanou ka? */
	struct attribute *as;
	struct element *r;
	struct element *e;
	char *msg;
#define	ERR(e)	do { r = (e); goto end; } while (0)
#define	DONE(e)	do { r = (e); goto end; } while (0)

	switch (init_g(g)) {
		static char msg[MAXPATHLEN + 64];
#if defined ENABLE_NEWLAYOUT
	case E_RCFILE:
		snprintf(msg, sizeof msg, "cannot read %s\n", gss3_rc);
		ERR(x_error(msg));
		break;
	case E_RCMALF:
		snprintf(msg, sizeof msg, "malformed rcfile: %s\n", gss3_rc);
		ERR(x_error(msg));
		break;
	case E_RCMVAL:
		snprintf(msg, sizeof msg, "malformed rcfile (value): %s\n", gss3_rc);
		ERR(x_error(msg));
		break;
#endif
	case -1:
		ERR(x_error("init_g failed"));
	default:
		break;
	}

	for (as = q->attributes; as->name; as++) {
/* XXX: check return value? */
		unsigned int type;
		type = ATTYPE(as->name, psr);
		switch (type) {
char *xdir;
		case M_TARGET:    getval(as, &g->target, 's'); break;
		case M_NARTICLES: getval(as, &g->narticles, 'd'); break;
		case M_NKEYWORDS: getval(as, &g->nkeywords, 'd'); break;
		case M_NACLS:     getval(as, &g->ns.nacls, 'd'); break;
		case M_NKCLS:     getval(as, &g->ns.nkcls, 'd'); break;
		case M_AOFFSET:   getval(as, &g->a_offset, 'd'); break;
		case M_APROPS:    getval(as, &g->a_props, 's'); break;
		case M_CROSSREF:  getval(as, &g->req_matrix, 's'); break;
		case M_DIRECTION: getval(as, &xdir, 's'); 
			if (!strcmp(xdir, "row")) {
				g->dir = WAM_ROW, g->rdir = WAM_COL;
			}
			else if (!strcmp(xdir, "col")) {
				g->dir = WAM_COL, g->rdir = WAM_ROW;
			}
			break;
		case M_NIWORDS:   getval(as, &g->ns.niwords, 'd'); break;
		case M_YYKN:      getval(as, &g->ns.yykn, 'd'); break;
		case M_XSTAGE:    getval(as, &g->xstage, 'i'); break;
		case M_STAGE1_SIM: getval(as, &g->sim_nam.stage1, 's'); break;
		case M_STAGE2_SIM: getval(as, &g->sim_nam.stage2, 's'); break;
		case M_STAGE3_SIM: getval(as, &g->sim_nam.stage3, 's'); break;
		case M_CS_TYPE:   getval(as, &g->sim_nam.cs_type, 's'); break;
		case M_CS_VSIM:   getval(as, &g->sim_nam.cs_v, 's'); break;
		case M_CS_SIM:    getval(as, &g->sim_nam.cs, 's'); break;
#if defined ASSV_DBGINFO
		case M_DBGINFO:   getval(as, &g->gen_assv_dbginfo, 'b'); break;
#endif
		case M_CUTOFF_DF: getval(as, &g->cutoff_df_global, 'd'); g->have_cutoff_df_global = 1; break;
		case M_XFSS_DOWNSAMPLE: getval(as, &g->xfss_downsample, 'b'); break;
		default: ERR(x_error("unknown attribute"));
		}
	}

	if ((g->sim.stage1 = decode_weight(g->sim_nam.stage1, g->sim_dflt.stage1)) == -1) {
		ERR(x_error("unknown weight type"));
	}
	if ((g->sim.stage2 = decode_weight(g->sim_nam.stage2, g->sim_dflt.stage2)) == -1) {
		ERR(x_error("unknown weight type"));
	}
	if ((g->sim.stage3 = decode_weight(g->sim_nam.stage3, g->sim_dflt.stage3)) == -1) {
		ERR(x_error("unknown weight type"));
	}
	if ((g->sim.cs_type = decode_cstype(g->sim_nam.cs_type, g->sim_dflt.cs_type)) == -1) {
		ERR(x_error("unknown weight type"));
	}
	if ((g->sim.cs_v = decode_weight(g->sim_nam.cs_v, g->sim.cs_type == CS_K_MEANS ? WT_NONE : g->sim_dflt.cs_v)) == -1) {
		ERR(x_error("unknown weight type"));
	}
	if ((g->sim.cs = decode_weight(g->sim_nam.cs, g->sim_dflt.cs)) == -1) {
		ERR(x_error("unknown weight type"));
	}

cmd.type = "assoc";
cmd.target = g->target;

/* XXX: check given values? */

	if ((g->xref_types = decode_xref_types(g->req_matrix)) == -1) {
		ERR(x_error("invalid cross-ref specification."));
	}

	if (!g->target) {
		ERR(x_error("target not supplied"));
	}

	wam_init(getaroot);
	if (!(tw = wam_open(g->target, NULL))) {
		syslog(LOG_DEBUG, "%s: %s", g->target, strerror(errno));
		ERR(x_error("cannot open target"));
	}

	if (g->xstage == 2) {
		if (e = read_stage2_data(tw, g, q, psr)) {
			ERR(e);
		}
		goto stage2;
	}

	if (e = read_stage1_data(tw, g, q, psr, 0)) {
		ERR(e);
	}

	if (g->fss.query) {
		goto stage2;
	}

	if (assoc_stage_1(tw, g) == -1) {
		ERR(empty_result("empty result (IW)"));
	}

	if (g->xstage == 1) {
		DONE(assoc1_result(tw, g));
	}

stage2:

	switch (assoc_stage_2(tw, g, &msg)) {
	case -2:
		r = empty_result(msg);
		break;
	case -1:
		ERR(x_error(msg));
	case 0:
		if (!(r = result_page(tw, g, g->ac, g->ns.nacls, g->atotal, g->kc, g->ns.nkcls, g->ktotal))) {
			ERR(x_error("unknown error"));
		}
		break;
	default:
		ERR(x_error("unkown error"));
	}
end:
#undef ERR
#undef DONE
	if (tw) {
		wam_close(tw);
	}
	for (i = 0; i < g->nquery; i++) {
		free(g->query[i].v);
	}
	free(g->query);
	free(g->cutoff_df_vec);
	for (i = 0; g->ac && i < g->ns.nacls; i++) {
		free(g->ac[i].csw.s);
		free(g->ac[i].csa.s);
	}
	free(g->ac);
	for (i = 0; g->kc && i < g->ns.nkcls; i++) {
		free(g->kc[i].csw.s);
		free(g->kc[i].csa.s);
	}
	free(g->kc);
	free(g->it);
	free(g->filter.expression);
	if (g->filter.fss.query) free_fss_query(&g->filter.fss);
	if (g->fss.query) free_fss_query(&g->fss);
	if (g->m) {
		for (i = 0; i < g->nsl0m; i++) {
			free(g->m[i]);
		}
		free(g->m);
	}
	if (g->aam) {
		for (i = 0; i < g->nsl0aam; i++) {
			free(g->aam[i]);
		}
		free(g->aam);
	}
	if (g->kkm) {
		for (i = 0; i < g->nsl0kkm; i++) {
			free(g->kkm[i]);
		}
		free(g->kkm);
	}
	assert(g->wm == NULL);
	return r;
}

static int
init_g(g)
struct gss3_g *g;
{
	int e = 0;

	g->ns_dflt.nacls = 1;
	g->ns_dflt.nkcls = 1;
	g->ns_dflt.niwords = INTWN;
	g->ns_dflt.yykn = YYKN;
	g->sim_dflt.stage1 = WT_SMARTAW;
	g->sim_dflt.stage2 = WT_SMARTWA;
	g->sim_dflt.stage3 = WT_SMARTAW;
	g->sim_dflt.cs_type = CS_HBC;
	g->sim_dflt.cs_v = WT_SMART;
	g->sim_dflt.cs = WT_SMART;
	g->st_dflt.stemmer = STEMMER_AUTO;

#if defined ENABLE_NEWLAYOUT
	if (gss3_rc) {
		e = read_rcfile(g, gss3_rc);
	}
#endif

	g->dir = WAM_ROW, g->rdir = WAM_COL;
	g->target = NULL;
	g->query = NULL;
	g->nquery = 0;
	g->cutoff_df_vec = NULL;
	g->narticles = 0;
	g->nkeywords = 0;

	g->ns.nacls = g->ns_dflt.nacls;
	g->ns.nkcls = g->ns_dflt.nkcls;

	g->atotal = 0;
	g->ktotal = 0;
	g->ac = NULL;
	g->kc = NULL;
	g->a_offset = 0;
	g->a_props = NULL;
	g->req_matrix = NULL;
	g->xref_types = 0;
	g->it = NULL;

	g->ns.niwords = g->ns_dflt.niwords;
	g->ns.yykn = g->ns_dflt.yykn;

	g->xstage = 0;
	g->filter.expression = NULL;
	g->filter.fss.query = NULL;
	g->fss.query = NULL;

	g->sim_nam.stage1 = NULL;
	g->sim_nam.stage2 = NULL;
	g->sim_nam.stage3 = NULL;
	g->sim_nam.cs_type = NULL;
	g->sim_nam.cs_v = NULL;
	g->sim_nam.cs = NULL;

	g->nsl0m = g->nsl0aam = g->nsl0kkm = 0;
	g->m = g->aam = g->kkm = g->wm = NULL;
#if defined ASSV_DBGINFO
	g->gen_assv_dbginfo = 0;
#endif
	g->cutoff_df_global = 0;
	g->have_cutoff_df_global = 0;
	g->xfss_downsample = 0;

	g->st.stemmer = NULL;
	return e;
}

#if 0
static struct element *
process_simv(q, psr)
struct element *q;
struct gss3_parser_t *psr;
{
	size_t i;
	struct gss3_g g0, *g = &g0;
	WAM *tw = NULL;	/* XXX kore mo `g' ni utusubeki? */
/* XXX atikoti ni tirabatte iru `dir' wo `g' ni irete okuto, soutui ban no zissou ga raku ka? */
	struct attribute *as;
	struct element *r;
	struct element *e;
	char *msg;
#define	ERR(e)	do { r = (e); goto end; } while (0)
#define	DONE(e)	do { r = (e); goto end; } while (0)

	switch (init_g(g)) {
		static char msg[MAXPATHLEN + 64];
#if defined ENABLE_NEWLAYOUT
	case E_RCFILE:
		snprintf(msg, sizeof msg, "cannot read %s\n", gss3_rc);
		ERR(x_error(msg));
		break;
	case E_RCMALF:
		snprintf(msg, sizeof msg, "malformed rcfile: %s\n", gss3_rc);
		ERR(x_error(msg));
		break;
	case E_RCMVAL:
		snprintf(msg, sizeof msg, "malformed rcfile (value): %s\n", gss3_rc);
		ERR(x_error(msg));
		break;
#endif
	case -1:
		ERR(x_error("init_g failed"));
	default:
		break;

	for (as = q->attributes; as->name; as++) {
/* XXX: check return value? */
		unsigned int type;
		type = ATTYPE(as->name, psr);
		switch (type) {
char *xdir;
		case M_TARGET:    getval(as, &g->target, 's'); break;
		case M_DIRECTION: getval(as, &xdir, 's'); 
			if (!strcmp(xdir, "row")) {
				g->dir = WAM_ROW, g->rdir = WAM_COL;
			}
			else if (!strcmp(xdir, "col")) {
				g->dir = WAM_COL, g->rdir = WAM_ROW;
			}
			break;
		case M_STAGE1_SIM: getval(as, &g->sim_nam.stage1, 's'); break;
		default: ERR(x_error("unknown attribute"));
		}
	}

	if ((g->sim.stage1 = decode_weight(g->sim_nam.stage1, g->sim_dflt.stage1)) == -1) {
		ERR(x_error("unknown weight type"));
	}

cmd.type = "simv";
cmd.target = g->target;

/* XXX: check given values? */

	if (!g->target) {
		ERR(x_error("target not supplied"));
	}

	wam_init(getaroot);
	if (!(tw = wam_open(g->target, NULL))) {
		syslog(LOG_DEBUG, "%s: %s", g->target, strerror(errno));
		ERR(x_error("cannot open target"));
	}

	if (e = read_stage1_data(tw, g, q, psr, 1)) {
		ERR(e);
	}

	if (simv_stage_1(tw, g) == -1) {
		ERR(empty_result("simv failed (unkn reason)"));
	}

	DONE(simv_result(tw, g));

end:
#undef ERR
#undef DONE
	if (tw) {
		wam_close(tw);
	}
	for (i = 0; i < g->nquery; i++) {
		free(g->query[i].v);
	}
	free(g->query);
	if (g->wm) {
		for (i = 0; i + 1 < g->nquery; i++) {
			free(g->wm[i]);
		}
		free(g->wm);
	}
	return r;
}
#endif

/*
 * no_filter: disables attribute filter and search.
 *		aslo disables accumulating cutoff_df.
 * 		(if supplied by a user, attribute cutoff_df will be ignored (not causing any error or warning))
 */
static struct element *
read_stage1_data(tw, g, q, psr, no_filter)
WAM *tw;
struct gss3_g *g;
struct element *q;
struct gss3_parser_t *psr;
{
	df_t N, cutoff_df_global;
	struct syminfo *query;
	df_t *cutoff_df_vec;
	df_t nquery, query_size;
	union content **cs;
	struct element *r = NULL;
#define	ERR(e)	do { r = (e); goto err; } while (0)

	query = NULL;
	cutoff_df_vec = NULL;
	query_size = 0;
	nquery = 0;

	N = wam_size(tw, g->dir);

	if (!no_filter) {
		if (g->have_cutoff_df_global) {
			cutoff_df_global = g->cutoff_df_global;
		}
		else {
			cutoff_df_global = cutoff(N);
		}
/*fprintf(stderr, "N = %d, cutoff = %d\n", N, cutoff_df_global);*/
	}
	else {
		cutoff_df_global = 0;
	}

	for (cs = q->contents; *cs; cs++) {
		struct element *e = &(*cs)->element;
		struct xr_vec const *v = NULL;
		df_t cutoff_df = cutoff_df_global;
		unsigned int type;
		if (e->type != CElem) {
			continue;
		}
		type = ELTYPE(e->name, psr);
		switch (type) {
			struct element *ee;
		case M_SEARCH:
			if (no_filter) {
				ERR(x_error("invalid attribute"));
			}
			if (g->dir != WAM_ROW) {
				ERR(x_error("constraint violation"));
			}
			if (parse_search(e, &g->fss, psr) == -1) {
				ERR(x_error("malformed search request"));
			}
			break;
		case M_FILTER:
			if (no_filter) {
				ERR(x_error("invalid attribute"));
			}
			if (ee = get_filter(g, e, psr)) {
				ERR(ee);
			}
			break;
		case M_FREETEXT:
			if (g->dir != WAM_ROW) {
				ERR(x_error("constraint violation"));
			}
			if (ee = parse_freetext(tw, g, e, &v, &cutoff_df, psr)) {
				ERR(ee);
			}
			break;
		case M_ARTICLE:
			if (g->dir != WAM_ROW) {
				ERR(x_error("constraint violation"));
			}
			if (ee = parse_article_keyword(tw, g, e, &v, &cutoff_df, psr)) {
				ERR(ee);
			}
			break;
		case M_KEYWORD:
			if (g->dir != WAM_COL) {
				ERR(x_error("constraint violation"));
			}
			if (ee = parse_article_keyword(tw, g, e, &v, &cutoff_df, psr)) {
				ERR(ee);
			}
			break;
		default:
			ERR(x_error("malformed search request"));
			break;
		}
		if (v && v->elem_num) {
			if (query_size <= nquery) {
				void *new;
				size_t newsize = nquery;
				newsize = NA(newsize + 1, 32);
				if (!(new = realloc(query, newsize * sizeof *query))) {
					syslog(LOG_DEBUG, "realloc: %s", strerror(errno));
					ERR(x_error("realloc"));
				}
				query_size = newsize;
				query = new;

				if (!no_filter) {
					if (!(new = realloc(cutoff_df_vec, newsize * sizeof *cutoff_df_vec))) {
						syslog(LOG_DEBUG, "realloc: %s", strerror(errno));
						ERR(x_error("calloc"));
					}
					cutoff_df_vec = new;
				}
			}
			if (!no_filter) {
				cutoff_df_vec[nquery] = cutoff_df;
			}
			query[nquery].id = 0;
			query[nquery].TF_d = 1;
			query[nquery].v = v;
			nquery++;
		}
	}

	if (!no_filter) {
		if (query && g->fss.query) {
			ERR(x_error("association and full-text-search is mutually exclusive"));
		}
	}

	g->query = query;
	g->nquery = nquery;
	if (!no_filter) {
		g->cutoff_df_vec = cutoff_df_vec;
	}
	else {
		g->cutoff_df_vec = NULL;
	}
	return NULL;
err:
#undef ERR
	free(query);
	if (!no_filter) {
		free(cutoff_df_vec);
	}
	return r;
}

int
parse_search(e, f, psr)
struct element *e;
struct fss_query *f;
struct gss3_parser_t *psr;
{
	union content **x;
	fss_seg_t segments = ANYSEG();
	xsc_opt_t options = 0;
	struct fss_con_query *qq = NULL;
	df_t j;
	size_t n = contlen(e->contents);
#define	ERR()	do { goto err; } while (0)
	if (!(qq = calloc(n, sizeof *qq))) {
		ERR();
	}
	if (get_segm_opt(e, &segments, &options, psr) == -1) {
		ERR();
	}
	if (!e->contents) {
		ERR();
	}
	for (x = e->contents, j = 0; *x; x++) {
		struct element *ee;
		unsigned int type;
		if ((*x)->element.type != CElem) {
			continue;
		}
		ee = &(*x)->element;
		type = ELTYPE(ee->name, psr);
		if (type == M_JOIN) {
			if (parse_join(ee, segments, options, &qq[j], psr) == -1) {
				int k;
				for (k = 0; k < j; k++) {
					int l;
					for (l = 0; l < qq[j].n; l++) {
						free(qq[j].q[l].pattern);
					}
					free(qq[j].q);
				}
				ERR();
			}
			j++;
		}
	}
	f->query = qq;
	f->nq = j;
	f->pa = f->na = NULL;
	return 0;
err:
#undef ERR
	free(qq);
	return -1;
}

static int
parse_join(e, segments, options, qq, psr)
struct element *e;
fss_seg_t segments;
xsc_opt_t options;
struct fss_con_query *qq;
struct gss3_parser_t *psr;
{
	union content **x;
	struct fss_simple_query *q = NULL;
	df_t j;
	size_t n = contlen(e->contents);
#define	ERR()	do { goto err; } while (0)
	if (n == 0) {
		qq->q = NULL;
		qq->n = 0;
		return 0;
	}
	if (!(q = calloc(n, sizeof *q))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		ERR();
	}
	if (get_segm_opt(e, &segments, &options, psr) == -1) {
		ERR();
	}
	if (!e->contents) {
		ERR();
	}
	for (x = e->contents, j = 0; *x; x++) {
		struct element *ee;
		unsigned int type;
		if ((*x)->element.type != CElem) {
			continue;
		}
		ee = &(*x)->element;
		type = ELTYPE(ee->name, psr);
		if (type == M_P || type == M_N) {
			assert(j < n);
			if (parse_p(ee, type == M_N, segments, options, &q[j], psr) == -1) {
				int k;
				for (k = 0; k < j; k++) {
					free(q[k].pattern);
				}
				ERR();
			}
			j++;
		}
	}
	qq->q = q;
	qq->n = j;
	return 0;
err:
#undef ERR
	free(q);
	return -1;
}

static int
parse_p(e, negativep, segments, options, q, psr)
struct element *e;
fss_seg_t segments;
xsc_opt_t options;
struct fss_simple_query *q;
struct gss3_parser_t *psr;
{
	char *pattern = NULL, *text = NULL;
	if (get_segm_opt(e, &segments, &options, psr) == -1) {
		goto err;
	}
	if (!(text = cont2str(e->contents))) {
		goto err;
	}
	if (!(pattern = normalize_text(text))) {
		goto err;
	}
	free(text), text = NULL;
	q->negativep = negativep;
	q->pattern = pattern;
	q->segments = segments;
	q->options = options;
	q->arts = NULL;
	q->narts = 0;
	return 0;
err:
	free(text);
	free(pattern);
	return -1;
}

static int
get_segm_opt(e, segmentsp, optionsp, psr)
struct element *e;
fss_seg_t *segmentsp;
xsc_opt_t *optionsp;
struct gss3_parser_t *psr;
{
	struct attribute *as;
	for (as = e->attributes; as->name; as++) {
		unsigned int type;
		type = ATTYPE(as->name, psr);
		if (type == M_SEGMENTS) {
			if (parse_segs(as->value, segmentsp) == -1) {
				return -1;
			}
		}
		else if (type == M_OPTIONS) {
			if (parse_opts(as->value, optionsp) == -1) {
				return -1;
			}
		}
	}
	return 0;
}

static struct element *
get_filter(g, e, psr)
struct gss3_g *g;
struct element *e;
struct gss3_parser_t *psr;
{
	struct attribute *as;
	union content **cs;
	for (as = e->attributes; as->name; as++) {
		unsigned int type;
		type = ATTYPE(as->name, psr);
		if (type == M_EXPRESSION) {
			if (g->filter.expression) {
				return x_error("filter/expression cannot appeared more than once");
			}
			if (!(g->filter.expression = strdup(as->value))) {
				return x_error("filter: strdup");
			}
		}
	}
	if (!e->contents) {
		return NULL;
	}
	for (cs = e->contents; *cs; cs++) {
		struct element *ee = &(*cs)->element;
		unsigned int type;
		if (ee->type != CElem) {
			continue;
		}
		type = ELTYPE(ee->name, psr);
		if (type == M_SEARCH) {
			if (g->dir != WAM_ROW) {
				return x_error("constraint violation");
			}
			if (g->filter.fss.query) {
				return x_error("filter/search cannot appeared more than once");
			}
			if (parse_search(ee, &g->filter.fss, psr) == -1) {
				return x_error("filter: malformed search request");
			}
		}
	}
/*
	if (g->filter.expression && g->filter.fss.query) {
		return x_error("filter: expression and search is mutually exclusive");
	}
*/
	return NULL;
}

static struct element *
parse_freetext(tw, g, e, v, cutoff_df, psr)
WAM *tw;
struct gss3_g *g;
struct element *e;
struct xr_vec const **v;
df_t *cutoff_df;
struct gss3_parser_t *psr;
{
	char *text;
	struct attribute *as;
	if (!e->contents) {
		return NULL;	/* v remains unset */
	}
	for (as = e->attributes; as->name; as++) {
		unsigned int type;
		type = ATTYPE(as->name, psr);
		if (type == M_CUTOFF_DF) {
			*cutoff_df = strtoul(as->value, NULL, 10);
		}
		else if (type == M_STEMMER) {
			g->st.stemmer = as->value;
		}
		else if (type == M_LANG && !g->st.stemmer) {	/* for backward compatibility */
			g->st.stemmer = as->value;
		}
	}
	if (!g->st.stemmer) {
		g->st.stemmer = g->st_dflt.stemmer;
	}
	if (!(text = cont2str(e->contents))) {
		return x_error("cont2str failed");
	}
	if (!(*v = text2vec(text, tw, WAM_COL, g->st.stemmer))) {
		free(text);
		return x_error("text2vec failed");
	}
	free(text);
	return NULL;
}

static struct element *
parse_article_keyword(tw, g, e, v, cutoff_df, psr)
WAM *tw;
struct gss3_g *g;
struct element *e;
struct xr_vec const **v;
df_t *cutoff_df;
struct gss3_parser_t *psr;
{
	struct attribute *as;
	char *name = NULL, *vec = NULL, *source = NULL;
#if defined ELEM_VEC
	char *evec = NULL;
#endif

	for (as = e->attributes; as->name; as++) {
		unsigned int type;
		type = ATTYPE(as->name, psr);
		if (type == M_NAME) {
			name = as->value;
		}
		else if (type == M_VEC) {
			vec = as->value;
		}
		else if (type == M_SOURCE) {
			source = as->value;
		}
		else if (type == M_CUTOFF_DF) {
			*cutoff_df = strtoul(as->value, NULL, 10);
		}
	}

#if defined ELEM_VEC
	if (!vec && e->contents) {
		union content **cs;
		size_t i;
		for (i = 0, cs = e->contents; *cs; cs++) {
			struct element *ee = &(*cs)->element;
			unsigned int type;
			if (ee->type != CElem) {
				continue;
			}
			type = ELTYPE(ee->name, psr);
			if (type == M_VECTOR) {
				vec = evec = cont2str(ee->contents);
				break;
			}
		}
	}
#endif

	if (name) {
		if (source && strcmp(g->target, source)) {
/* XXX: todo -- make more efficent code... */
			WAM *sw;
			if (!(sw = wam_open(source, NULL))) {
				return x_error(source);
			}
			if (wam_get_vec_byname(sw, g->dir, name, v) == -1) {
				wam_close(sw);
				return x_error("wam_get_vec_byname");
			}
			if (!(*v = dxr_dup2(sw, tw, g->rdir, *v))) {
				wam_close(sw);
				return x_error("dxr_dup2");
			}
			wam_close(sw);
		}
		else {
			if (wam_get_vec_byname(tw, g->dir, name, v) == -1) {
				return x_error("wam_get_vec_byname");
			}
			if (!(*v = dxr_dup(*v))) {
				return x_error("dxr_dup");
			}
		}
	}
	else if (vec) {
		if (!(*v = jsvec2vec(vec, tw, g->rdir))) {
			return x_error("jsvec2vec failed");
		}
	}
#if defined ELEM_VEC
	free(evec);
#endif
	return NULL;
}

static struct element *
read_stage2_data(tw, g, q, psr)
WAM *tw;
struct gss3_g *g;
struct element *q;
struct gss3_parser_t *psr;
{
	union content **cs;
	df_t i;
	for (cs = q->contents; *cs; cs++) ;
	if (!(g->it = calloc(cs - q->contents, sizeof *g->it))) {
		return x_error("calloc");
	}
	for (i = 0, cs = q->contents; *cs; cs++) {
		struct element *e = &(*cs)->element;
		struct syminfo *iti;
		struct attribute *as;
		char *name = NULL, *score = NULL, *tf_d = NULL, *end;
		idx_t id;
		unsigned int type;
		if (e->type != CElem) {
			continue;
		}
		type = ELTYPE(e->name, psr);
		if (type == M_FILTER) {
			struct element *ee = get_filter(g, e, psr);
			if (ee) {
				return ee;
			}
			continue;
		}
		if (type != M_KEYWORD && type != M_ARTICLE) {
			continue;
		}

		if (type == M_KEYWORD && g->dir != WAM_ROW) {
			return x_error("constraint violation");
		}
		if (type == M_ARTICLE && g->dir != WAM_COL) {
			return x_error("constraint violation");
		}

		for (as = e->attributes; as->name; as++) {
			type = ATTYPE(as->name, psr);
			if (type == M_NAME) {
				name = as->value;
			}
			else if (type == M_SCORE) {
				score = as->value;
			}
			else if (type == M_TFd) {
				tf_d = as->value;
			}
		}

		if (!name) {
			continue;
		}

		if (!(id = wam_name2id(tw, g->rdir, name))) {
			continue;
		}

		iti = &g->it[i++];
		iti->id = id;
		if (score) {
			iti->weight = strtod(score, &end);
			if (!(*score && !*end)) {
				return x_error("malformed score");
			}
		}
		else {
			iti->weight = 0;
		}
		iti->TF = 0;
		if (tf_d) {
			iti->TF_d = strtol(tf_d, &end, 10);
			if (!(*tf_d && !*end)) {
				return x_error("malformed TFd");
			}
		}
		else {
			iti->TF_d = 1;
		}
		iti->DF = iti->DF_d = 0;
		iti->v = NULL;
#if defined ENABLE_GETA
		iti->attr = 0;
#endif
	}
	g->ns.niwords = i;
	return NULL;
}

static struct element *
process_getvec(p, psr)
struct element *p;
struct gss3_parser_t *psr;
{
	int i, j, k;
	char const *target = NULL;
	union content **cs;
	struct attribute *as;
	WAM *w = NULL;
	union content **cc = NULL;
	size_t cc_size;
	struct element *r;
#define	ERR(e)	do { r = (e); goto err; } while (0)

	for (as = p->attributes; as->name; as++) {
		unsigned int type;
		type = ATTYPE(as->name, psr);
		switch (type) {
		case M_TARGET: getval(as, &target, 's'); break;
		default:
			ERR(x_error("unknown attribute"));
		}
	}

cmd.type = "getvec";
cmd.target = target;

	cc_size = 8;
	if (!(cc = calloc(cc_size, sizeof *cc))) {
		ERR(x_error("cannot alloc memory"));
	}
	if (!target) {
		syslog(LOG_DEBUG, "no target");
		ERR(x_error("no target"));
	}
	wam_init(getaroot);
	if (!(w = wam_open(target, NULL))) {
		syslog(LOG_DEBUG, "%s: %s", target, strerror(errno));
		ERR(x_error("cannot open target"));
	}
	cs = p->contents;
	for (i = j = 0; cs[i]; i++) {
		struct element *ee;
int dir, rdir;
		ee = &cs[i]->element;
		switch (ee->type) {
			char const *name;
			struct xr_vec const *v;
			unsigned int type;
		case CElem:
			type = ELTYPE(ee->name, psr);
			if (type != M_ARTICLE && type != M_KEYWORD) {
				continue;
			}
			switch (type) {
			case M_ARTICLE: dir = WAM_ROW, rdir = WAM_COL; break;
			case M_KEYWORD: dir = WAM_COL, rdir = WAM_ROW; break;
			default: ERR(x_error("internal error ~type"));
			}
			as = ee->attributes;
			if (!as->name) {
				continue;
			}
			type = ATTYPE(as->name, psr);
			if (type != M_NAME) {
				continue;
			}
			name = as->value;
			if (wam_get_vec_byname(w, dir, name, &v) >= 0) {
				char *jsvec = vec2jsvec(w, rdir, v);
				if (!jsvec) {
					for (k = 0; k < j; k++) {
						free_element(&cc[k]->element);
					}
					ERR(x_error("cannot alloc memory"));
				}
				if (cc_size <= j + 2) {
					void *new;
					size_t newsize = j + 2;
					newsize = NA(newsize + 1, 128);
					if (!(new = realloc(cc, newsize * sizeof *cc))) {
						for (k = 0; k < j; k++) {
							free_element(&cc[k]->element);
						}
						ERR(x_error("cannot alloc memory"));
					}
					cc_size = newsize;
					cc = new;
				}
				switch (dir) {
				case WAM_ROW:
					cc[j] = (union content *)`article(name = ,strdup(name); vec = ,jsvec;) { };
					break;
				case WAM_COL:
				cc[j] = (union content *)`keyword(name = ,strdup(name); vec = ,jsvec;) { };
					break;
				}
				if (!cc[j]) {
					for (k = 0; k < j; k++) {
						free(jsvec);
						free_element(&cc[k]->element);
					}
					ERR(x_error("cannot alloc memory"));
				}
				j++;
			}
			break;
		default:
			break;
		}
	}
	cc[j] = NULL;

	wam_close(w);

	r = `result(status = "OK") {
		,@cc;
	};
/* XXX */
	free(cc);
	return r;
err:
#undef ERR
	if (w) wam_close(w);
	free(cc);
	return r;
}

static struct element *
process_getprop(p, psr)
struct element *p;
struct gss3_parser_t *psr;
{
	int i, j, k;
	char *target = NULL;
	char *a_props = NULL;
	union content **cs;
	struct attribute *as;
	WAM *w = NULL;
	union content **cc = NULL;
	size_t cc_size;
	struct props *ap = NULL;
	char **ps = NULL, *p0 = NULL;
	struct element *r = NULL;
#define	ERR(e)	do { r = (e); goto err; } while (0)

	for (as = p->attributes; as->name; as++) {
		unsigned int type;
		type = ATTYPE(as->name, psr);
		switch (type) {
		case M_TARGET: getval(as, &target, 's'); break;
		case M_APROPS: getval(as, &a_props, 's'); break;
		default:
			ERR(x_error("unknown attribute"));
		}
	}

cmd.type = "getprop";
cmd.target = target;

	if (!(p0 = strdup(a_props))) {
		ERR(x_error("strdup failed"));
	}
	if (!(ps = splitprops(p0))) {
		ERR(x_error("invalid props specification"));
	}

	cc_size = 8;
	if (!(cc = calloc(cc_size, sizeof *cc))) {
		ERR(x_error("cannot alloc memory"));
	}
	if (!target) {
		syslog(LOG_DEBUG, "no target");
		ERR(x_error("no target"));
	}
	wam_init(getaroot);
	if (!(w = wam_open(target, NULL))) {
		syslog(LOG_DEBUG, "%s: %s", target, strerror(errno));
		ERR(x_error("cannot open target"));
	}

	if (!(ap = openprops(w, target, WAM_ROW, ps))) {
		ERR(x_error("open props failed"));
	}
	free(ps); ps = NULL;
	free(p0); p0 = NULL;

	cs = p->contents;
	for (i = j = 0; cs[i]; i++) {
		struct element *ee;
		ee = &cs[i]->element;
		switch (ee->type) {
			struct attribute *attrs;
			char const *name;
			struct element *a;
			idx_t id;
			size_t n;
			int dir;
			unsigned int type;
		case CElem:
			type = ELTYPE(ee->name, psr);
			if (type != M_ARTICLE) {
				continue;
			}
			as = ee->attributes;
			if (!as->name) {
				continue;
			}
			type = ATTYPE(as->name, psr);
			if (type != M_NAME) {
				continue;
			}
			name = as->value;

			dir = WAM_ROW;	/* XXX currently fixed */

			n = 2 + ap->n;
			if (!(attrs = calloc(n, sizeof *attrs))) {
for (k = 0; k < j; k++) free_element(&cc[k]->element);
				ERR(x_error("!calloc"));
			}

			if (!(attrs[0].name = strdup("name"))) {
for (k = 0; k < j; k++) free_element(&cc[k]->element);
free(attrs);
			}
			if (!(attrs[0].value = strdup(name))) {
for (k = 0; k < j; k++) free_element(&cc[k]->element);
free(attrs[0].name);
free(attrs);
				ERR(x_error("!strdup"));
			}
			if (!(id = wam_name2id(w, dir, name))) {
for (k = 0; k < j; k++) free_element(&cc[k]->element);
free(attrs[0].value);
free(attrs[0].name);
free(attrs);
				ERR(x_error("!name2id"));
			}

			if (fill_as(w, dir, id, ap, attrs + 1, n - 1) == -1) {
for (k = 0; k < j; k++) free_element(&cc[k]->element);
free(attrs[0].value);
free(attrs[0].name);
free(attrs);
				ERR(x_error("!fill_as"));
			}

			a = `article(,@attrs;) { };
			if (!a) {
for (k = 0; k < j; k++) free_element(&cc[k]->element);
for (k = 0; k < n; k++) {
	free(attrs[k].value);
	free(attrs[k].name);
}
free(attrs);
				ERR(x_error("cannot alloc memory"));
			}
			free(attrs);
			if (cc_size <= j + 2) {
				void *new;
				size_t newsize = j + 2;
				newsize = NA(newsize + 1, 128);
				if (!(new = realloc(cc, newsize * sizeof *cc))) {
for (k = 0; k < j; k++) free_element(&cc[k]->element);
					ERR(x_error("cannot alloc memory"));
				}
				cc_size = newsize;
				cc = new;
			}
			cc[j++] = (union content *)a;
			break;
		default:
			break;
		}
	}
	cc[j] = NULL;

	r = `result(status = "OK") {
		,@cc;
	};
/* XXX */

err:
#undef ERR
	free(cc);
	freeprops(ap);
	if (w) wam_close(w);
	free(ps);
	free(p0);
	return r;
}

static struct element *
process_inquire(p, psr)
struct element *p;
struct gss3_parser_t *psr;
{
	char const *type = NULL;
	struct attribute *as;

	for (as = p->attributes; as->name; as++) {
		unsigned int atype;
		atype = ATTYPE(as->name, psr);
		switch (atype) {
		case M_TYPE:  getval(as, &type, 's'); break;
		default:
			return x_error("unknown attribute");
		}
	}

	if (!type) {
		syslog(LOG_DEBUG, "no type");
		return x_error("no type");
	}

	if (!strcmp(type, "catalogue")) {
		struct element *c;
		struct element *r;
		if (!(c = read_catalogue())) {
			return x_error("cannot read a catalogue");
		}
		r = `result(status = "OK") {
			,@c->contents;
		};
/* XXX */
		free(c->contents);
		c->contents = NULL;
		free_element(c);
		return r;
	}
	else {
		syslog(LOG_DEBUG, "unknown inquire type: %s", type);
		return x_error("unknown inquire type");
	}
}

static int
getval(a, val, type)
struct attribute *a;
void *val;
{
	switch (type) {
		char *end;
	case 's':
		*(char **)val = a->value;
		break;
	case 'd':
		*(df_t *)val = strtol(a->value, &end, 10);
		if (!(*a->value && !*end)) return -1;
		break;
	case 'i':
		*(int *)val = strtol(a->value, &end, 10);
		if (!(*a->value && !*end)) return -1;
		break;
	case 'b':
		*(int *)val = !strcmp(a->value, "yes");
		if (strcmp(a->value, "yes") && strcmp(a->value, "no")) return -1;
		break;
	default:
		break;
	}
	return 0;
}

static int
assoc_stage_1(w, g)
WAM *w;
struct gss3_g *g;
{
	struct assv_scookie sc;

	bzero(&sc, sizeof sc);
	sc.flag = ASSV_SC_CUTOFF_DF_LIST;
	sc.cutoff_df_list = g->cutoff_df_vec;
/* fprintf(stderr, "g->query = %p\n", g->query); */
/*fprintf(stderr, "niwords = %"PRIdDF_T"\n", g->ns.niwords);*/
	if (!(g->it = assv(g->query, g->nquery, w, g->dir, g->sim.stage1, &g->ns.niwords, NULL, &sc))) {
		return -1;
	}
	return 0;
}

#if 0
static int
simv_stage_1(w, g)
WAM *w;
struct gss3_g *g;
{
	df_t i, j;
	double **wm;

	if (g->nquery <= 1) {
		return -1;
	}

	if (!(wm = calloc(g->nquery - 1, sizeof *wm))) {
		return -1;
	}
	for (i = 0; i + 1 < g->nquery; i++) {
		if (!(wm[i] = calloc(g->nquery - (i + 1), sizeof **wm))) {
			return -1;
		}
		for (j = 0; j + i + 1 < g->nquery; j++) {
			struct xr_vec const *qv, *dv;
			double wt;
			qv = g->query[i].v;
			dv = g->query[j + i + 1].v;
			assert(qv);
			assert(dv);
/* XXX argument mismatch!!!! -- will cause a crash. */
			wt = simv(qv, qv->elem_num, w, g->dir, g->sim.stage1, dv, dv->elem_num);
			wm[i][j] = wt;
		}
	}
	g->wm = wm;
	return 0;
}
#endif

static int
assoc_stage_2(w, g, msg)
WAM *w;
struct gss3_g *g;
char **msg;
{
	struct syminfo *a, *k;
#if ! defined ENABLE_GETA
	df_t nk;
#else
	ssize_t nk;
#endif
	df_t na;

	na = MAX(g->a_offset + g->narticles, g->ns.yykn);

	switch (stage2_p1(w, g->sim.stage2, g, &a, &na, msg)) {
	case -2:
		return -2;
	case -1:
		return -1;
	case 0:
		break;
	default:
		return -1;
	}

	if (g->nkeywords) {
		nk = g->nkeywords;
/*
print_syminfo(a, na, na, 12, w, WAM_ROW, stderr);
fprintf(stderr, "wsh: %p %"PRIdSSIZE_T" %p %d %d %"PRIdSSIZE_T" %p\n", a, (pri_ssize_t)MIN(na, g->ns.yykn), w, WAM_ROW, g->sim.stage3, (pri_ssize_t)nk, &g->ktotal);
*/
		if (!(k = 
#if ! defined ENABLE_GETA
			assv
#else
			wsh
#endif
				(a, MIN(na, g->ns.yykn), w, WAM_ROW, g->sim.stage3, &nk, &g->ktotal
#if ! defined ENABLE_GETA
			, NULL
#endif
			))) {
			*msg = "wsh failed (KS)";
			return -1;
		}
/*print_syminfo(k, nk, g->ktotal, 12, w, WAM_COL, stderr);*/
		if (!nk) {
			free(k);
			k = NULL;
			g->ktotal = 0;
		}
	}
	else {
		k = NULL;
		nk = 0;
		g->ktotal = 0;
	}

	if (a) {
		g->ac = csb1(a + g->a_offset, MIN(na - g->a_offset, g->narticles), w, g->dir, &g->ns.nacls, g->sim.cs_type, g->sim.cs_v, g->sim.cs);
	}
	else {
		g->ac = NULL;
		g->ns.nacls = 0;
	}
	if (k) {
		g->kc = csb1(k, nk, w, g->rdir, &g->ns.nkcls, g->sim.cs_type, g->sim.cs_v, g->sim.cs);
	}
	else {
		g->kc = NULL;
		g->ns.nkcls = 0;
	}

	if ((g->xref_types&(1|2)) && !(g->m = make_akmatrix(g->ac, g->ns.nacls, g->kc, g->ns.nkcls, w, g->dir, &g->nsl0m))) {
		*msg = "make_akmatrix failed";
		return -1;
	}

	if ((g->xref_types&4) && !(g->aam = make_aakkmatrix(g->ac, g->ns.nacls, w, g->dir, &g->nsl0aam))) {
		*msg = "make_aa(kk)matrix failed";
		return -1;
	}

	if ((g->xref_types&8) && !(g->kkm = make_aakkmatrix(g->kc, g->ns.nkcls, w, g->rdir, &g->nsl0kkm))) {
		*msg = "make_(aa)kkmatrix failed";
		return -1;
	}

/*XXX*/

free(a);
free(k);

	return 0;
}

static int
stage2_p1(w, type, g, a, na, msg)
WAM *w;
struct gss3_g *g;
struct syminfo **a;
df_t *na;
char **msg;
{
	if (*na == 0) {
		*msg = "empty request (AS)";
		return -2;
	}

	if (g->fss.query && (g->filter.expression || g->filter.fss.query)) {
		*msg = "full text search and filter is exclusive";
		return -1;
	}

#if ! defined ENABLE_GETA
	if (g->fss.query) {
		WAM *f;
		struct fss_simple_query p, n;
		if (!(f = wam_fss_open(w))) {
			syslog(LOG_DEBUG, "fss_open failed 1");
			*msg = "fss_open";
			return -1;
		}
		if (g->xfss_downsample) {
			if (*na <= 0) {
				*msg = "parameter out of range (FSS)";
				return -1;
			}
			p.narts = -1;
		}
		else {
			p.narts = *na;
		}
		n.narts = 0;
		if (wam_xfss(f, &g->fss, &p, &n) == -1) {
			*msg = "xfss failed";
			return -1;
		}
/* p.arts and n.arts are alias. do not free */
		if (!p.narts) {
			*msg = "empty result (FSS)";
			return -2;
		}
		if (g->xfss_downsample && p.narts > *na) {
			downsample(p.arts, p.narts, *na, sizeof *p.arts);
			p.narts = *na;
		}
		/* XXX n wo tukatte, hosyuugou mo kensaku dekiru youni suru? */
		*na = p.narts;
		if (!(*a = arts2syminfo(p.arts, p.narts))) {
			*msg = "arts2syminfo failed";
			return -1;
		}
		g->atotal = p.total;
		wam_close(f);
	}
	else {
		struct bxue_t *bex = NULL;	/* XXX */
		struct assv_scookie sc;

		if (!g->ns.niwords) {
			*msg = "empty IW (AS)";
			return -2;
		}

		bzero(&sc, sizeof sc);

		if (g->filter.expression) {
			df_t i;
			if (!(bex = expr2bex(w, g->rdir, g->filter.expression, msg, &i))) {
				return -1;
			}
			sc.flag |= ASSV_SC_BX;
			sc.bx.b = bex;
			sc.bx.blen = i;
		}

		if (g->filter.fss.query) {
			sc.flag |= ASSV_SC_FSS;
			sc.fssq = &g->filter.fss;
		}

#ifdef ASSV_DBGINFO
		if (g->gen_assv_dbginfo) {
			sc.flag |= ASSV_SC_DBGINFO;
		}
#endif
		if (!(*a = 
#if ! defined ENABLE_GETA
			assv
#else
			wsh
#endif
				(g->it, g->ns.niwords, w, g->rdir, type, na, &g->atotal, &sc))) {
			*msg = "wsh failed (AS)";
			if (bex) {
				free(bex);
			}
			return -1;
		}

		if (bex) {
			free(bex);
		}

		if (!*na) {
			*msg = "empty result (AS)";
			return -2;
		}
	}
#if 0
++	if (g->fss.query) {
++		WAM *f;
++		struct fss_simple_query p, n;
++		if (!(f = 
++			wam_fss_open(w)
++)) {
++			syslog(LOG_DEBUG, "fss_open failed 1");
++			*msg = "fss_open";
++			return -1;
++		}
++		if (g->xfss_downsample) {
++			if (*na <= 0) {
++				*msg = "parameter out of range (FSS)";
++				return -1;
++			}
++			p.narts = -1;
++		}
++		else {
++			p.narts = *na;
++		}
++		n.narts = 0;
++		if (
++			wam_xfss
++			(f, &g->fss, &p, &n) == -1) {
++			*msg = "xfss failed";
++			return -1;
++		}
++/* p.arts and n.arts are alias. do not free */
++		if (!p.narts) {
++			*msg = "empty result (FSS)";
++			return -2;
++		}
++		if (g->xfss_downsample && p.narts > *na) {
++			downsample(p.arts, p.narts, *na, sizeof *p.arts);
++			p.narts = *na;
++		}
++		/* XXX n wo tukatte, hosyuugou mo kensaku dekiru youni suru? */
++		*na = p.narts;
++		if (!(*a = arts2syminfo(p.arts, p.narts))) {
++			*msg = "arts2syminfo failed";
++			return -1;
++		}
++		g->atotal = p.total;
++		wam_close(f);
++	}
++	else if (g->filter.expression) {
++		char *p, *t;
++		struct bex_gettoken_t r;
++		struct bxue_t *bex;	/* XXX */
++		df_t i;
++		size_t n;
++		idx_t id;
++		if (!g->ns.niwords) {
++			*msg = "empty IW (BAS)";
++			return -2;
++		}
++
++		r.t = NULL;
++		r.tlen = 0;
++		for (p = g->filter.expression, n = 0; t = bex_gettoken_r(p, &p, &r); n++) ;
++		free(r.t);
++		if (!p || *p != '\0') {
++			*msg = "bool-expression: parse error";
++			return -1;
++		}
++		if (!(bex = calloc(n, sizeof *bex))) {
++			*msg = "calloc";
++			return -1;
++		}
++		r.t = NULL;
++		r.tlen = 0;
++		for (p = g->filter.expression, i = 0; t = bex_gettoken_r(p, &p, &r); i++) {
++			bex[i].type = *t;
++			bex[i].id = 0;
++			switch (*t) {
++			case '"':
++				if (id = wam_name2id(w, g->rdir, t + 1)) {
++					bex[i].id = id;
++				}
++				else {
++					bex[i].type = 'F';
++				}
++				break;
++			case '&':
++			case '|':
++			case '!':
++			case '(':
++			case ')':
++			case 'T':
++			case 'F':
++				break;
++			default:
++				*msg = "internal error ~B";
++				free(r.t);
++				return -1;
++			}
++		}
++		free(r.t);
++		if (!(*a = bex_wsh(g->it, g->ns.niwords, w, g->rdir, type, na, &g->atotal, bex, i, NULL))) {
++			*msg = "bex_wsh failed (BAS)";
++			return -1;
++		}
++		if (!*na) {
++			*msg = "empty result (BAS)";
++			return -2;
++		}
++		free(bex);
++	}
++	else if (g->filter.fss.query) {
++		if (!g->ns.niwords) {
++			*msg = "empty IW (XAS)";
++			return -2;
++		}
++		if (!(*a = fss_wsh(g->it, g->ns.niwords, w, g->rdir, type, na, &g->atotal, NULL, &g->filter.fss))) {
++			*msg = "wsh failed (XAS_FSS)";
++			return -1;
++		}
++		if (!*na) {
++			*msg = "empty result (XAS)";
++			return -2;
++		}
++	}
++	else {
++#ifdef ASSV_DBGINFO
++		struct assv_scookie sc;
++#endif
++
++		if (!g->ns.niwords) {
++			*msg = "empty IW (AS)";
++			return -2;
++		}
++#ifdef ASSV_DBGINFO
++		bzero(&sc, sizeof sc);
++		if (g->gen_assv_dbginfo) {
++			sc.flag = ASSV_SC_DBGINFO;
++		}
++#endif
++		if (!(*a = wsh(g->it, g->ns.niwords, w, g->rdir, type, na, &g->atotal, 
++#ifdef ASSV_DBGINFO
++			&sc
++#else
++			NULL
++#endif
++			))) {
++			*msg = "wsh failed (AS)";
++			return -1;
++		}
++		if (!*na) {
++			*msg = "empty result (AS)";
++			return -2;
++		}
++	}
#endif
#else /* ! ENABLE_GETA */
--	if (g->fss.query) {
--		FSS *f;
--		struct fss_simple_query p, n;
--		if (!(f = 
--			fss_open(getaroot, g->target)
--)) {
--			syslog(LOG_DEBUG, "fss_open failed 1");
--			*msg = "fss_open";
--			return -1;
--		}
--		if (g->xfss_downsample) {
--			if (*na <= 0) {
--				*msg = "parameter out of range (FSS)";
--				return -1;
--			}
--			p.narts = -1;
--		}
--		else {
--			p.narts = *na;
--		}
--		n.narts = 0;
--		if (
--			xfss
--			(f, &g->fss, &p, &n) == -1) {
--			*msg = "xfss failed";
--			return -1;
--		}
--/* p.arts and n.arts are alias. do not free */
--		if (!p.narts) {
--			*msg = "empty result (FSS)";
--			return -2;
--		}
--		if (g->xfss_downsample && p.narts > *na) {
--			downsample(p.arts, p.narts, *na, sizeof *p.arts);
--			p.narts = *na;
--		}
--		/* XXX n wo tukatte, hosyuugou mo kensaku dekiru youni suru? */
--		*na = p.narts;
--		if (!(*a = arts2syminfo(p.arts, p.narts))) {
--			*msg = "arts2syminfo failed";
--			return -1;
--		}
--		g->atotal = p.total;
--		fss_close(f);
--	}
--	else if (g->filter.expression) {
--		char *p, *t;
--		struct bex_gettoken_t r;
--		struct syminfo *bex;	/* XXX */
--		ssize_t tmp;
--		df_t i;
--		size_t n;
--		idx_t id;
--		if (!g->ns.niwords) {
--			*msg = "empty IW (BAS)";
--			return -2;
--		}
--
--		r.t = NULL;
--		r.tlen = 0;
--		for (p = g->filter.expression, n = 0; t = bex_gettoken_r(p, &p, &r); n++) ;
--		free(r.t);
--		if (!p || *p != '\0') {
--			*msg = "bool-expression: parse error";
--			return -1;
--		}
--		if (!(bex = calloc(n, sizeof *bex))) {
--			*msg = "calloc";
--			return -1;
--		}
--		r.t = NULL;
--		r.tlen = 0;
--		for (p = g->filter.expression, i = 0; t = bex_gettoken_r(p, &p, &r); i++) {
--			switch (*t) {
--			case '"':
--				if (id = wam_name2id(w, g->rdir, t + 1)) {
--					bex[i].id = id;
--					bex[i].attr = 0;
--				}
--				else {
--					bex[i].id = 0;
--					bex[i].attr = 'F';
--				}
--				break;
--			case '&':
--			case '|':
--			case '!':
--			case '(':
--			case ')':
--			case 'T':
--			case 'F':
--				bex[i].id = 0;
--				bex[i].attr = *t;
--				break;
--			default:
--				*msg = "internal error ~B";
--				free(r.t);
--				return -1;
--			}
--		}
--		free(r.t);
--		tmp = *na;
--		if (!(*a = bex_wsh(g->it, g->ns.niwords, w, g->rdir, type, &tmp, &g->atotal, bex, i))) {
--			*msg = "bex_wsh failed (BAS)";
--			return -1;
--		}
--		*na = tmp;
--		if (!*na) {
--			*msg = "empty result (BAS)";
--			return -2;
--		}
--		free(bex);
--	}
--	else if (g->filter.fss.query) {
--		FSS *f;
--		struct fss_simple_query p, n;
--		idx_t *pm = NULL, *nm = NULL;
--		ssize_t tmp;
--		if (!g->ns.niwords) {
--			*msg = "empty IW (XAS)";
--			return -2;
--		}
--		if (!(f = fss_open(getaroot, g->target))) {
--			syslog(LOG_DEBUG, "fss_open failed 2");
--			*msg = "fss_open";
--			return -1;
--		}
--		p.narts = n.narts = -1;
--		if (xfss(f, &g->filter.fss, &p, &n) == -1) {
--			*msg = "xfss failed";
--			return -1;
--		}
--/* p.arts and n.arts are alias. do not free */
--		if (p.arts && !(pm = arts2idvec(p.arts, p.narts))) {
--			*msg = "arts2idvec p failed";
--			return -1;
--		}
--		if (n.arts && !(nm = arts2idvec(n.arts, n.narts))) {
--			*msg = "arts2idvec n failed";
--			return -1;
--		}
--		if (wam_setmask(w, g->rdir, pm, p.narts, nm, n.narts) == -1) {
--			*msg = "mam_setmask failed";
--			return -1;
--		}
--		free(pm);
--		free(nm);
--		fss_close(f);
--		tmp = *na;
--		if (!(*a = wsh(g->it, g->ns.niwords, w, g->rdir, type, &tmp, &g->atotal))) {
--			*msg = "wsh failed (XAS)";
--			return -1;
--		}
--		*na = tmp;
--/* fprintf(stderr, "a = %p, na = %"PRIdDF_T"\n", a, *na); */
--		if (wam_setmask(w, g->rdir, NULL, 0, NULL, 0) == -1) {
--			*msg = "mam_setmask failed";
--			return -1;
--		}
--		if (!*na) {
--			*msg = "empty result (XAS)";
--			return -2;
--		}
--	}
--	else {
--#ifdef ASSV_DBGINFO
--		struct assv_scookie sc;
--#endif
--		ssize_t tmp;
--
--		if (!g->ns.niwords) {
--			*msg = "empty IW (AS)";
--			return -2;
--		}
--		tmp = *na;
--		if (!(*a = wsh(g->it, g->ns.niwords, w, g->rdir, type, &tmp, &g->atotal))) {
--			*msg = "wsh failed (AS)";
--			return -1;
--		}
--		*na = tmp;
--		if (!*na) {
--			*msg = "empty result (AS)";
--			return -2;
--		}
--	}
#endif /* ! ENABLE_GETA */
	return 0;
}

static struct bxue_t *
expr2bex(w, dir, expression, msg, ip)
WAM *w;
char *expression;
char **msg;
df_t *ip;
{
	struct bxue_t *bex;
	char *p, *t;
	struct bex_gettoken_t r;
	df_t i;
	size_t n;
	idx_t id;

	r.t = NULL;
	r.tlen = 0;
	for (p = expression, n = 0; t = bex_gettoken_r(p, &p, &r); n++) ;
	free(r.t);
	if (!p || *p != '\0') {
		*msg = "bool-expression: parse error";
		return NULL;
	}
	if (!(bex = calloc(n, sizeof *bex))) {
		*msg = "calloc";
		return NULL;
	}
	r.t = NULL;
	r.tlen = 0;
	for (p = expression, i = 0; t = bex_gettoken_r(p, &p, &r); i++) {
		bex[i].type = *t;
		bex[i].id = 0;
		switch (*t) {
		case '"':
			if (id = wam_name2id(w, dir, t + 1)) {
				bex[i].id = id;
			}
			else {
				bex[i].type = 'F';
			}
			break;
		case '&':
		case '|':
		case '!':
		case '(':
		case ')':
		case 'T':
		case 'F':
			break;
		default:
			*msg = "internal error ~B";
			free(r.t);
			return NULL;
		}
	}
	free(r.t);
	*ip = i;
	return bex;
}

static char *
bex_gettoken_r(p, q, r)
char *p, **q;
struct bex_gettoken_t *r;
{
	size_t i;
	for (; *p && isspace(*p & 0xff); p++) ;
	if (!*p) {
		*q = p;
		return NULL;
	}
	switch (*p) {
	case '"':
		if (!r->t) {
			r->tlen = 32;
			if (!(r->t = malloc(r->tlen))) {
				r->tlen = 0;
				return NULL;
			}
		}
		r->t[0] = '"';
		for (i = 1, p++; *p && *p != '"'; p++) {
			if (*p == '\\' && !*++p) {
				return NULL;
			}
			if (r->tlen <= i + 1) {
				void *new;
				size_t newsize = i + 1;
				newsize = NA(newsize + 1, 32);
				if (!(new = realloc(r->t, newsize))) {
					return NULL;
				}
				r->tlen = newsize;
				r->t = new;
			}
			r->t[i++] = *p;
		}
		r->t[i] = '\0';
		if (*p == '"') {
			*q = ++p;
			return r->t;
		}
		return NULL;
	case '&':
	case '|':
	case '!':
	case '(':
	case ')':
	case 'T':
	case 'F':
		r->d[0] = *p++;
		r->d[1] = '\0';
		*q = p;
		return r->d;
	default:
		return NULL;
	}
}

static struct element *
assoc1_result(w, g)
WAM *w;
struct gss3_g *g;
{
	df_t i;
	union content **c;
	struct element *r;
	if (!(c = calloc(g->ns.niwords + 1, sizeof *c))) {
		return x_error("!calloc");
	}
	for (i = 0; i < g->ns.niwords + 1; i++) {
		c[i] = NULL;
	}
#define	ERR(e)	do { r = (e); goto err; } while (0)
	for (i = 0; i < g->ns.niwords; i++) {
		struct element *e;
		char const *name;
		char *nmp, *scp, *tfd;
		char score[32];
		char tf_d[32];
		if (!(name = wam_id2name(w, g->rdir, g->it[i].id))) {
			ERR(x_error("internal error ~N: %"PRIuIDX_T, g->it[i].id));
		}
		snprintf(score, sizeof score, "%e", g->it[i].weight);
		snprintf(tf_d, sizeof tf_d, "%"PRIdFREQ_T, g->it[i].TF_d);
		nmp = strdup(name);
		scp = strdup(score);
		tfd = strdup(tf_d);
		switch (g->dir) {
		case WAM_ROW:
			e = `keyword(name = ,nmp; score = ,scp; TFd = ,tfd;) { };
			break;
		case WAM_COL:
			e = `article(name = ,nmp; score = ,scp; TFd = ,tfd;) { };
			break;
		default:
			ERR(x_error("internal error ~dir"));
		}
		if (!nmp || !scp || !e) {
			free(nmp);
			free(scp);
			ERR(x_error("no memory"));
		}
		c[i] = (union content *)e;
	}
	c[i] = NULL;
	r = `result(status = "OK") {
		,@c;
	};
	if (!r) {
/* XXX */
		ERR(x_error("no mem"));
	};
	free(c);
	return r;
err:
#undef ERR
	for (i = 0; i < g->ns.niwords + 1; i++) {
		free_element(&c[i]->element);
	}
	free(c);
	return r;
}

/* XXX */
static struct element *
simv_result(w, g)
WAM *w;
struct gss3_g *g;
{
	g->wm;
	return NULL;
}

struct cs_elem *
csb1(r, nr, w, dir, ncp, cs_type, vt_type, wt_type)
WAM *w;
struct syminfo *r;
df_t nr, *ncp;
{
	df_t th, ckkn;
	struct cs_elem *cslst;
	df_t elemn;
	int bias;
#if defined ENABLE_GETA
	size_t tmp;
#endif

	if (*ncp > nr) {
		*ncp = nr;
	}

	if (*ncp <= 1) {
		goto ONE;
	}

	bias = 0;	/* bias info. (if unknown, use 0) */
	elemn = 256;
	/* nc = *ncp;	* cluster # requested */
	th = 1;		/* minimal element # of each cluster */
	ckkn = 128;	/* maximum length of cluster-featureing word vector */

#if ! defined ENABLE_GETA
	cslst = ncsb(r, nr, cs_type, w, dir, vt_type, elemn, ncp, wt_type, ckkn);
#else
	tmp = *ncp;
	cslst = csb(r, nr, bias, w, dir, cs_type, wt_type, elemn, &tmp, th, ckkn, NULL);
	*ncp = tmp;
#endif

	if (!cslst) { /* fail */
		struct cs_elem *e;
	ONE: /* no need for clustering */
		*ncp = 1;
		if (!(cslst = calloc(1, sizeof *cslst))) {
			syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
			return NULL;
		}
		e = &cslst[0];
		e->csa.s = bdup(r, nr * sizeof *r);
		e->csa.n = nr;
		e->csw.s = NULL;
		e->csw.n = 0;
	}

	return cslst;
}

static struct element *
result_page(w, g, ac, nac, atotal, kc, nkc, ktotal)
WAM *w;
struct gss3_g *g;
struct cs_elem *ac, *kc;
df_t nac, atotal, nkc, ktotal;
{
	double **am, **km, **aam, **kkm;
	struct props *ap;
	struct element *r;
	char **ps, *p0;
	char at[32], kt[32], *atp, *ktp;
	union content **al, **kl;

	snprintf(at, sizeof at, "%"PRIdDF_T, atotal);
	snprintf(kt, sizeof kt, "%"PRIdDF_T, ktotal);

	if (!(p0 = strdup(g->a_props))) {
		return x_error("strdup failed");
	}
	if (!(ps = splitprops(p0))) {
		free(p0);
		return x_error("invalid props specification");
	}

	if (!(ap = openprops(w, g->target, g->dir, ps))) {
		free(p0);
		free(ps);
		return x_error("openprops failed");
	}
	free(ps);
	free(p0);
	/* no need to check ap != NULL */
	/* see process_getprop */

	if (pull_nam(w, g->dir, ac, nac) == -1) {
		freeprops(ap);
		return x_error("pull_nam ROW failed");
	}
	if (pull_nam(w, g->rdir, kc, nkc) == -1) {
		freeprops(ap);
		return x_error("pull_nam COL failed");
	}
	am = g->xref_types & 1 ? g->m : NULL;
	km = g->xref_types & 2 ? g->m : NULL;
	aam = g->xref_types & 4 ? g->aam : NULL;
	kkm = g->xref_types & 8 ? g->kkm : NULL;
	al = clslst(w, g->dir, ac, nac, kc, nkc, ap, am, aam);
	kl = clslst(w, g->rdir, kc, nkc, ac, nac, NULL, km, kkm);

	atp = strdup(at);
	ktp = strdup(kt);

#if defined ASSV_DBGINFO
	r = `result(status = "OK") {
		articles(total = ,atp;) { ,@al; }
		keywords(total = ,ktp;) { ,@kl; }
		,@dbginfo_iw(w, g);
	};
#else
	r = `result(status = "OK") {
		articles(total = ,atp;) { ,@al; }
		keywords(total = ,ktp;) { ,@kl; }
	};
#endif

	if (!al || !kl || !atp || !ktp || !r) {
		int k;
		for (k = 0; al && al[k]; k++) free_element(&al[k]->element);
		for (k = 0; kl && kl[k]; k++) free_element(&kl[k]->element);
		free(al);
		free(kl);
		free(atp);
		free(ktp);
		freeprops(ap);
		return x_error("low memory");
	}
	free(al);
	free(kl);
	freeprops(ap);
	return r;
}

#if defined ASSV_DBGINFO
static union content **
dbginfo_iw(w, g)
WAM *w;
struct gss3_g *g;
{
	union content **cs;
	if (g->gen_assv_dbginfo) {
		char *s;
		if (!(s = dbginfo_iw_str(w, g))) {
			return NULL;
		}
		if (!(cs = calloc(3, sizeof *cs))) {
			return NULL;
		}
		cs[0] = (union content *)`dbginfo(iwords = ,s;) { };
		cs[1] = NULL;
	}
	else {
		if (!(cs = calloc(1, sizeof *cs))) {
			return NULL;
		}
		cs[0] = NULL;
	}
	return cs;
}

static char *
dbginfo_iw_str(w, g)
WAM *w;
struct gss3_g *g;
{
	df_t i;
	struct escapedq_t es;
	char *r;
	char const *name, *ename;
	size_t l = 0;
	char s[32];

	if (g->ns.niwords == 0) {
		return strdup("[]");
	}

	es.b = NULL;
	es.l = 0;

	for (i = 0; i < g->ns.niwords; i++) {
		if (!(name = wam_id2name(w, g->rdir, g->it[i].id))) {
			goto error;
		}
		if (!(ename = escapedq(name, &es))) {
			goto error;
		}
		snprintf(s, sizeof s, "\",%g", g->it[i].weight);
		l += strlen(ename) + strlen(s) + 4;
	}

	if (!(r = malloc(l + 2))) {
		goto error;
	}

	*r = '\0';

	for (i = 0; i < g->ns.niwords; i++) {
		if (!(name = wam_id2name(w, g->rdir, g->it[i].id))) {
			goto error;
		}
		if (!(ename = escapedq(name, &es))) {
			goto error;
		}
		snprintf(s, sizeof s, "\",%g", g->it[i].weight);
		strcat(r, ",[\"");
		strcat(r, ename);
		strcat(r, s);
		strcat(r, "]");
	}
	r[0] = '[';
	strcat(r, "]");
	assert(strlen(r) == l + 1);

	free(es.b);
	return r;
error:
	free(es.b);
	return strdup("");
}
#endif

static char **
splitprops(p0)
char *p0;
{
	int i;
	char **ps, *p;
	if (index(p0, '/')) {
		return NULL;
	}
/*
	if (!(p0 = strdup(p0))) {
		return NULL;
	}
*/
	for (i = 0, p = p0; p && *p; i++) {
		if (p = index(p, ',')) {
			p++;
		}
	}
	if (!(ps = calloc(i + 1, sizeof *ps))) {
		return NULL;
	}
	for (i = 0, p = p0; p && *p; i++) {
		ps[i] = p;
		if (p = index(p, ',')) {
			*p++ = '\0';
		}
	}
	ps[i] = NULL;
	return ps;
}

static struct props *
openprops(w, handle, dir, ps)
WAM *w;
char *handle, **ps;
{
	struct props *ap = NULL;
	size_t i;

	if (!(ap = calloc(1, sizeof *ap))) {
		return NULL;
	}
	ap->n = 0;

	for (i = 0; ps[i]; i++) ;

	ap->names = calloc(i, sizeof *ap->names);
	ap->ws = calloc(i, sizeof *ap->ws);
	ap->fss = NULL;

#define	ERR()	do { goto fail; } while (0)
	if (!ap->names || !ap->ws) {
		ERR();
	}

	for (i = 0; ps[i]; i++) {
		if (!strcmp(ps[i], "vec")) {
			ap->ws[ap->n] = NULL;
			if (!(ap->names[ap->n] = strdup(ps[i]))) {
				ERR();
			}
			ap->n++;
		}
#if ! defined ENABLE_GETA
		else if (!strncmp(ps[i], "fss.", 4)) {
			char *v, *end;
			unsigned int segid;
			v = ps[i] + 4;
			segid = strtoul(v, &end, 10);
			if (!(*v && !*end)) {
				ERR();
			}
			if (segid > MAXSEGID) {
				ERR();
			}
			if (ap->fss || (ap->fss = wam_fss_open(w))) {
				ap->ws[ap->n] = NULL;
				if (!(ap->names[ap->n] = strdup(ps[i]))) {
					ERR();
				}
				ap->n++;
			}
		}
#endif
		else {
#if ! defined ENABLE_GETA
			char const *iname;
			iname = ps[i];
			if (ap->ws[ap->n] = wam_prop_open(w, dir, iname)) {
#else
			char iname[MAXPATHLEN + 1];
			snprintf(iname, sizeof iname, "%si", ps[i]);
			if (ap->ws[ap->n] = wam_prop_open(handle, dir, iname)) {
#endif
				if (!(ap->names[ap->n] = strdup(ps[i]))) {
					ERR();
				}
				ap->n++;
			}
		}
	}
	return ap;
fail:
#undef ERR
	freeprops(ap);
	return NULL;
}

static void
freeprops(ap)
struct props *ap;
{
	int i;
	if (!ap) {
		return;
	}
	for (i = 0; i < ap->n; i++) {
		if (ap->ws[i]) {
			wam_close(ap->ws[i]);
		}
		free(ap->names[i]);
	}
	if (ap->fss) {
		wam_close(ap->fss); 	/* XXX OK?  (if refcount works well... maybe) */
	}
	free(ap->ws);
	free(ap->names);
	free(ap);
}

static int fn_pull_nam(struct cs_elem *, df_t, struct syminfo *, df_t, void *, df_t);

struct cookie_pull_nam {
	WAM *w;
	int dir;
};

static int
fn_pull_nam(c, i, e, j, cookie, k)
struct cs_elem *c;
struct syminfo *e;
void *cookie;
df_t i, j, k;
{
	struct cookie_pull_nam *o = cookie;
	if (!(e->u.d.pname = wam_id2name(o->w, o->dir, e->id))) {
/*		fprintf(stderr, "wam_id2name failed\n"); */
syslog(LOG_DEBUG, "wam_id2name failed: %p %d %"PRIuIDX_T, o->w, o->dir, e->id);
		return -1;
	}
	return 0;
}

static int
pull_nam(w, dir, c, nc)
WAM *w;
struct cs_elem *c;
df_t nc;
{
	struct cookie_pull_nam cookie;
	cookie.w = w;
	cookie.dir = dir;
	return mapcsa(c, nc, fn_pull_nam, &cookie);
}

static union content **
clslst(w, dir, c, nc, x, nx, p, m, mm)
WAM *w;
struct cs_elem *c, *x;
df_t nc, nx;
struct props *p;
double **m, **mm;
{
	size_t i, c_offset;
	union content **cs;
	if (!(cs = calloc(nc + 1, sizeof *cs))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}
	for (c_offset = i = 0; i < nc; c_offset += c[i].csa.n, i++) {
		if (!(cs[i] = (union content *)clsx(w, dir, &c[i], p, c, nc, x, nx, m, mm, c_offset))) {
			int k;
			for (k = 0; k < i; k++) {
				free_element(&cs[i]->element);
			}
			return NULL;
		}
	}
	cs[i] = NULL;
	return cs;
}

static struct element *
clsx(w, dir, c0, p, c, nc, x, nx, m, mm, c_offset)
WAM *w;
struct cs_elem *c0, *c, *x;
struct props *p;
df_t nc, nx;
double **m, **mm;
{
	struct element *e;
	int i;
	if (!(e = alloc_element())) {
		syslog(LOG_DEBUG, "alloc_element: %s", strerror(errno));
		return NULL;
	}
	e->name = strdup("cls");
	e->attributes = NULL;
	e->contents = calloc(c0->csa.n + 1, sizeof *e->contents);
	if (!e->name || !e->contents) {
		free(e->name);
		free(e->contents);
		free(e);
		syslog(LOG_DEBUG, "strdup/calloc: %s", strerror(errno));
		return NULL;
	}
	for (i = 0; i < c0->csa.n; i++) {
		if (!(e->contents[i] = (union content *)clse(w, dir, &c0->csa.s[i], p, c, nc, x, nx, m, mm, c_offset + i))) {
			int k;
			for (k = 0; k < i; k++) {
				free_element(&e->contents[k]->element);
			}
			free(e->name);
			free(e->contents);
			free(e);
			return NULL;
		}
	}
	return e;
}

struct cookie_clse_m {
	int mode;
	int dir;
	int a_index;
	char const *cpname;
	union content **cc;
	size_t cc_index;
	double **m;
};

static int
fn_clse_m(c, i, e, j, cookie, k)
struct cs_elem *c;
struct syminfo *e;
void *cookie;
df_t i, j, k;
{
	struct cookie_clse_m *o = cookie;
	struct element *xe;
	double s;
	if (o->mode == 0) {
		s = o->dir == WAM_ROW ? o->m[o->a_index][k] : o->m[k][o->a_index];
	}
	else {
		s = o->m[o->a_index][k];
	}
	if (!(xe = xlist(o->cpname, e->u.d.pname, s))) {
		int l;
		for (l = 0; l < o->cc_index; l++) {
			free_element(&o->cc[l]->element);
		}
		free_element(xe);
		free(o->cc);
		return -1;
	}
	o->cc[o->cc_index++] = (union content *)xe;
	return 0;
}

static struct element *
clse(w, dir, a, p, c, nc, x, nx, m, mm, a_index)
WAM *w;
struct syminfo *a;
struct props *p;
struct cs_elem *c, *x;
df_t nc, nx;
double **m, **mm;
{
	char score[32];
	struct element *e;
	struct attribute *ap;
	size_t n;
	int d;
#if defined ASSV_DBGINFO
	char *qwstr = NULL;
	int AX = 0;
#else
#define	AX	0
#endif

#if defined ASSV_DBGINFO
	if (a->u.d.qw.w) {
		AX = 1;
		qwstr = qw2str(a->u.d.qw.w, a->u.d.qw.n);
	}
#endif

	if (!(e = alloc_element())) {
		syslog(LOG_DEBUG, "alloc_element: %s", strerror(errno));
		return NULL;
	}
	n = 3 + (p ? p->n : 0);
	if (!(ap = calloc(n, sizeof *ap))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		free(e);
		return NULL;
	}
	e->attributes = ap;
	e->name = strdup(dir == WAM_ROW ? "article" : "keyword");
	ap[0].name = strdup("name");
	ap[0].value = strdup(a->u.d.pname);
	snprintf(score, sizeof score, "%e", a->weight);
	ap[1].name = strdup("score");
	ap[1].value = strdup(score);
	d = 0;
	if (p) {
		d = fill_as(w, dir, a->id, p, ap + 2, n - 2);
	}
	ap[n - 1].name = ap[n - 1].value = NULL;
	if (!e->name ||
	    !ap[0].name || !ap[0].value ||
	    !ap[1].name || !ap[1].value ||
	    d == -1) {
		free(ap[0].name); free(ap[0].value);
		free(ap[1].name); free(ap[1].value);
		free(ap);
		free(e->name);
		free(e);
		syslog(LOG_DEBUG, "clse: strdup: %s", strerror(errno));
		return NULL;
	}
	if (m || mm || AX) {
		struct cookie_clse_m cookie;
		df_t nnx, nnc;
		nnx = 0;
		nnc = 0;
		cookie.dir = dir;
		cookie.a_index = a_index;
		if (m) {
			nnx = csalen_sum(x, nx);
		}
		if (mm) {
			nnc = csalen_sum(c, nc);
		}
		if (!(cookie.cc = calloc(nnx + nnc + AX + 1, sizeof *cookie.cc))) {
			free_element(e);
			return NULL;
		}
		cookie.cc_index = 0;
		if (m) {
			cookie.mode = 0;
			cookie.cpname = dir == WAM_ROW ? "keyword" : "article";
			cookie.m = m;
			if (mapcsa(x, nx, fn_clse_m, &cookie) == -1) {
				return NULL;
			}
		}
		if (mm) {
			cookie.mode = 1;
			cookie.cpname = dir == WAM_ROW ? "article" : "keyword";
			cookie.m = mm;
			if (mapcsa(c, nc, fn_clse_m, &cookie) == -1) {
				return NULL;
			}
		}
#if defined ASSV_DBGINFO
		if (AX == 1) {
			struct element *ee;
			struct attribute *eeap;
			ee = alloc_element();
			if (!(eeap = calloc(2, sizeof *eeap))) {
				syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
				free(e);
				return NULL;
			}
			eeap[0].name = strdup("iw-weights");
			eeap[0].value = qwstr ? qwstr : strdup("");
			eeap[1].name = NULL;
			eeap[1].value = NULL;
			ee->name = strdup("dbginfo");
			ee->attributes = eeap;
			ee->contents = NULL;
			if (!ee->name || !eeap[0].name || !eeap[0].value) {
				free(e);
				return NULL;
			}
			cookie.cc[cookie.cc_index++] = (union content *)ee;
		}
#endif
		cookie.cc[cookie.cc_index] = NULL;
		assert(cookie.cc_index == nnx + nnc + AX);
		e->contents = cookie.cc;
	}
	else {
		e->contents = NULL;
	}
	return e;
}

#if defined ASSV_DBGINFO
static char *
qw2str(w, n)
double *w;
df_t n;
{
	df_t i;
	char *r, s[32];
	size_t l = 0;

	if (n == 0) {
		return strdup("[]");
	}

#define	FMT	(i == 0 ? "[%g" : ",%g")
	for (i = 0; i < n; i++) {
		snprintf(s, sizeof s, FMT, w[i]);
		l += strlen(s);
	}

	if (!(r = malloc(l + 2))) {
		return NULL;
	}
	*r = '\0';

	for (i = 0; i < n; i++) {
		snprintf(s, sizeof s, FMT, w[i]);
		strcat(r, s);
	}
	strcat(r, "]");
#undef FMT
	return r;
}
#endif

static int
fill_as(w, dir, id, p, as, n)
WAM *w;
idx_t id;
struct props *p;
struct attribute *as;
size_t n;
{
	struct attribute *asp;
	int rdir = WAM_REVERT_DIRECTION(dir);
	int i;

	if (p->n >= n) {
		return -1;
	}
#define	ERR()	do { goto err; } while (0)
	for (i = 0; i < n; i++) {
		asp = &as[i];
		asp->name = asp->value = NULL;
	}
	for (i = 0; i < p->n; i++) {
		char *s;
		asp = &as[i];
		asp->name = strdup(p->names[i]);
		if (!strcmp(p->names[i], "vec")) {
			struct xr_vec const *v;
			assert(!p->ws[i]);
			if (wam_get_vec(w, dir, id, &v) >= 0) {
				if (!(s = vec2jsvec(w, rdir, v))) {
					syslog(LOG_DEBUG, "cannot alloc memory");
					ERR();
				}
			}
			else {
				syslog(LOG_DEBUG, "getvec failed");
				ERR();
			}
		}
#if ! defined ENABLE_GETA
		else if (!strncmp(p->names[i], "fss.", 4)) {
			char *v;
			unsigned int segid;
			char const *sg;
			v = p->names[i] + 4;
			segid = strtoul(v, NULL, 10);
			if (wam_fss_gets(p->fss, id, segid, &sg) <= 0) {
#if defined NOPROPERROR
				sg = "";
#else
				syslog(LOG_DEBUG, "wam_fss_gets failed");
				ERR();
#endif
			}
			if (!(s = strdup(sg))) {
				syslog(LOG_DEBUG, "strdup: %s", strerror(errno));
				ERR();
			}
		}
#endif
		else {
			char const *pr;
			if (wam_prop_gets(p->ws[i], id, &pr) == -1) {
#if defined NOPROPERROR
				pr = "";
#else
				syslog(LOG_DEBUG, "wam_prop_gets failed");
				ERR();
#endif
			}
			if (!(s = strdup(pr))) {
				syslog(LOG_DEBUG, "strdup: %s", strerror(errno));
				ERR();
			}
		}
		asp->value = s;
	}
	asp = &as[p->n];
	asp->name = NULL; asp->value = NULL;
	return 0;
err:
#undef ERR
	for (i = 0; i < n; i++) {
		asp = &as[i];
		free(asp->name); free(asp->value);
	}
	return -1;
}

static struct element *
xlist(en, name, s)
char const *en;
char const *name;
double s;
{
	char score[32];
	struct element *e;
	struct attribute *ap;

	snprintf(score, sizeof score, "%e", s);
	e = alloc_element();
	ap = calloc(3, sizeof *ap);
	if (!e || !ap) {
		free(ap);
		free(e);
		return NULL;
	}
	e->name = strdup(en);
	ap[0].name = strdup("name");
	ap[0].value = strdup(name);
	ap[1].name = strdup("score");
	ap[1].value = strdup(score);
	ap[2].name = NULL;
	ap[2].value = NULL;
	if (!e->name ||
	    !ap[0].name || !ap[0].value ||
	    !ap[1].name || !ap[1].value) {
		free(ap[0].name); free(ap[0].value);
		free(ap[1].name); free(ap[1].value);
		free(ap);
		free(e->name);
		free(e);
		return NULL;
	}
	e->attributes = ap;
	e->contents = NULL;
	return e;
}

static struct element *
x_error(char *reason, ...)
{
	char buf[2048], *p;
	va_list ap;
	va_start(ap, reason);
	vsnprintf(buf, sizeof buf, reason, ap);
	va_end(ap);
	if (!(p = strdup(buf))) {
		return NULL;
	}
	return `result(status = "ERROR" reason = ,p;) { };
}

struct cookie_ak_sl0 {
	WAM *w;
	int dir;
	struct syminfole *sl0;
};

static int
fn_ak_sl0(c, i, e, j, cookie, k)
struct cs_elem *c;
struct syminfo *e;
void *cookie;
df_t i, j, k;
{
	struct cookie_ak_sl0 *o = cookie;
	struct syminfole *s;
	df_t n;
	struct xr_vec const *v;
	s = &o->sl0[k];
	if ((n = wam_get_vec(o->w, o->dir, e->id, &v)) == -1) {
		syslog(LOG_DEBUG, "wam_get_vec failed");
		return -1;
	}
	s->n = n;
	if (!(s->s = calloc(n, sizeof *s->s))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return -1;
	}
	for (j = 0; j < n; j++) {
		struct syminfo *ssj = &s->s[j];
		struct xr_elem const *vej = &v->elems[j];
		ssj->id = vej->id;
		ssj->TF_d = vej->freq;
		ssj->DF = wam_elem_num(o->w, WAM_COL, vej->id);
	}
	return 0;
}

struct cookie_ak_sl1 {
	struct syminfole *sl1;
};

static int
fn_ak_sl1(c, i, e, j, cookie, k)
struct cs_elem *c;
struct syminfo *e;
void *cookie;
df_t i, j, k;
{
	struct cookie_ak_sl1 *o = cookie;
	struct syminfole *s;
	df_t n;
	s = &o->sl1[k];
	n = 1;
	s->n = n;
	if (!(s->s = calloc(n, sizeof *s->s))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return -1;
	}
	j = 0; {
		struct syminfo *ssj = &s->s[j];
		*ssj = *e;
	}
	return 0;
}

static double **
make_akmatrix(c0, nc0, c1, nc1, w, dir, nsl0p)
struct cs_elem *c0, *c1;
df_t nc0, nc1;
WAM *w;
df_t *nsl0p;
{
	size_t k;
	struct syminfole *sl0 = NULL, *sl1 = NULL;
	df_t nsl0, nsl1;
	df_t N;
	freq_t TF;
	double **r = NULL;

#define	ERR()	do { goto end; } while (0)
	nsl0 = csalen_sum(c0, nc0);
	nsl1 = csalen_sum(c1, nc1);

	if (!(sl0 = calloc(nsl0, sizeof *sl0))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		ERR();
	}
	for (k = 0; k < nsl0; k++) sl0[k].s = NULL;
	if (!(sl1 = calloc(nsl1, sizeof *sl1))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		ERR();
	}
	for (k = 0; k < nsl1; k++) sl1[k].s = NULL;
	N = wam_size(w, dir);
	TF = wam_total_freq_sum(w, dir);
	{
		struct cookie_ak_sl0 cookie;
		cookie.w = w;
		cookie.dir = dir;
		cookie.sl0 = sl0;
		if (mapcsa(c0, nc0, fn_ak_sl0, &cookie) == -1) {
			ERR();
		}
	}
	{
		struct cookie_ak_sl1 cookie;
		cookie.sl1 = sl1;
		if (mapcsa(c1, nc1, fn_ak_sl1, &cookie) == -1) {
			ERR();
		}
	}

	r = smartmtrxQ(sl0, nsl0, sl1, nsl1, N, TF);

end:
#undef ERR

	if (sl0) {
		for (k = 0; k < nsl0; k++) free(sl0[k].s);
		free(sl0);
	}

	if (sl1) {
		for (k = 0; k < nsl1; k++) free(sl1[k].s);
		free(sl1);
	}

	*nsl0p = nsl0;
	return r;
}

static double **
make_aakkmatrix(c0, nc0, w, dir, nsl0p)
struct cs_elem *c0;
df_t nc0;
WAM *w;
df_t *nsl0p;
{
	size_t k;
	struct syminfole *sl0 = NULL, *sl1 = NULL;
	df_t nsl0, nsl1;
	df_t N;
	freq_t TF;
	double **r = NULL;

#define	ERR()	do { goto end; } while (0)
	nsl0 = nsl1 = csalen_sum(c0, nc0);

	if (!(sl0 = calloc(nsl0, sizeof *sl0))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		ERR();
	}
	for (k = 0; k < nsl0; k++) sl0[k].s = NULL;
	if (!(sl1 = calloc(nsl1, sizeof *sl1))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		ERR();
	}
	for (k = 0; k < nsl1; k++) sl1[k].s = NULL;
	N = wam_size(w, dir);
	TF = wam_total_freq_sum(w, dir);
	{
		struct cookie_ak_sl0 cookie;
		cookie.w = w;
		cookie.dir = dir;
		cookie.sl0 = sl0;
		if (mapcsa(c0, nc0, fn_ak_sl0, &cookie) == -1) {
			ERR();
		}
		cookie.sl0 = sl1;
		if (mapcsa(c0, nc0, fn_ak_sl0, &cookie) == -1) {
			ERR();
		}
	}

	r = smartmtrxQ(sl0, nsl0, sl1, nsl1, N, TF);

end:
#undef ERR

	if (sl0) {
		for (k = 0; k < nsl0; k++) free(sl0[k].s);
		free(sl0);
	}

	if (sl1) {
		for (k = 0; k < nsl1; k++) free(sl1[k].s);
		free(sl1);
	}

	*nsl0p = nsl0;
	return r;
}

static struct element *
empty_result(note)
char const *note;
{
	if (!(note = strdup(note))) {
		return NULL;
	}
	return `result(status = "OK" note = ,note;) {
		articles(total = "0") { }
		keywords(total = "0") { }
	};
/* XXX */
}

static df_t
cutoff(n)
df_t n;
{
	if (n <= 10000) return n;
	return pow(0.125, log10(n) - 4) * n;
}

static char *
cont2str(c)
union content **c;
{
	char *text = NULL;
	size_t ts = 0;
	union content **x;
	if (!c) {
		return NULL;
	}
	for (x = c; *x; x++) {
		if ((*x)->element.type == CString) {
			struct chardata *s = &(*x)->chardata;
			ts += s->len;
		}
	}
	if (!(text = malloc(ts + 1))) {
		return NULL;
	}
	for (ts = 0, x = c; *x; x++) {
		if ((*x)->element.type == CString) {
			struct chardata *s = &(*x)->chardata;
			memmove(text + ts, s->value, s->len);
			ts += s->len;
		}
	}
	text[ts] = '\0';
	return text;
}

static size_t
contlen(c)
union content **c;
{
	union content **p;
	if (!c) {
		return 0;
	}
	for (p = c; *p; p++) ;
	return p - c;
}

static unsigned int
eltype(el, names, nmemb)
const XML_Char *el;
struct elname *names;
size_t nmemb;
{
	struct elname key, *e;
	key.name = el;
	if (e = bsearch(&key, names, nmemb, sizeof *names, elcompar)) {
		return e->id;
	}
	return 0;
}

static int
elcompar(va, vb)
const void *va;
const void *vb;
{
	struct elname const *a = va;
	struct elname const *b = vb;

	return strcmp(a->name, b->name);
}

static int
decode_weight(name, dflt)
char const *name;
{
	int i;
	char const *wt[] = WEIGHT_TYPE_NAMES;
	if (!name) {
		return dflt;
	}
	for (i = 0; i < nelems(wt); i++) {
		if (!strcmp(wt[i], name)) {
			return i;
		}
	}
#if defined ENABLE_DLSIM
	if (name[0] == '.') {
		return find_dlsim(name);
	}
#endif
	return -1;
}

static int
decode_cstype(name, dflt)
char const *name;
{
	int i;
	char const *wt[] = CS_TYPE_NAMES;
	if (!name) {
		return dflt;
	}
	for (i = 0; i < nelems(wt); i++) {
		if (!strcmp(wt[i], name)) {
			return i;
		}
	}
	return -1;
}

static int
decode_xref_types(req)
char const *req;
{
	char *o, *p, *q;
	int r = 0;

	if (!req || !*req) {
		return 0;
	}
	if (!(o = strdup(req))) {
		return -1;
	}

	for (q = o; p = q; ) {
		if (q = index(p, ',')) {
			*q++ = '\0';
		}
		if (!strcmp(p, "aw")) {
			r |= 1;
		}
		else if (!strcmp(p, "wa")) {
			r |= 2;
		}
		else if (!strcmp(p, "aa")) {
			r |= 4;
		}
		else if (!strcmp(p, "ww")) {
			r |= 8;
		}
		else {
			free(o);
			return -1;
		}
	}
	free(o);
	return r;
}

static int
readfn_fread(cookie, buf, size)
void *cookie;
char *buf;
{
#if defined REC_REQ_RAW
	int e = fread(buf, 1, size, cookie);
	if (e > 0 && rec_req_raw_f) {
		fwrite(buf, 1, e, rec_req_raw_f);
	}
	return e;
#else
	return fread(buf, 1, size, cookie);
#endif
}

/* spro8 to onazi */
static int
scompar(va, vb)
const void *va, *vb;
{
	char const **a, **b;
	a = (char const **)va;
	b = (char const **)vb;
	return strcmp(*a, *b);
}

#if defined ENABLE_NEWLAYOUT
static int
read_rcfile(g, path)
struct gss3_g *g;
char const *path;
{
	FILE *f;
	char buf[1024];
#define	T_DF_T		1
#define	T_WEIGHT	2
#define	T_CS_TYPE	3
#define	T_STRING	4
	struct ss {
		char *name;
		int type;
		size_t offs;
	};
	struct ss ss[] = {
#define	EE(N, T, M) {N, T, offsetof(struct gss3_g, M)}
		EE("nacls", T_DF_T, ns_dflt.nacls),
		EE("nkcls", T_DF_T, ns_dflt.nkcls),
		EE("niwords", T_DF_T, ns_dflt.niwords),
		EE("yykn", T_DF_T, ns_dflt.yykn),
		EE("stage1-sim", T_WEIGHT, sim_dflt.stage1),
		EE("stage2-sim", T_WEIGHT, sim_dflt.stage2),
		EE("stage3-sim", T_WEIGHT, sim_dflt.stage3),
		EE("cs-type", T_CS_TYPE, sim_dflt.cs_type),
		EE("cs-vsim", T_WEIGHT, sim_dflt.cs_v),
		EE("cs-sim", T_WEIGHT, sim_dflt.cs),
		EE("stemmer", T_STRING, st_dflt.stemmer),
#undef EE
	};
	int i;

	if (!(f = fopen(path, "r"))) {
		return E_RCFILE;
	}

	while (fgets(buf, sizeof buf, f)) {
		char *p, *key, *value;
		if (p = strchr(buf, '\n')) {
			*p = '\0';
		}
		if (!*buf || *buf == '#') {
			continue;
		}
		if (!(p = strchr(buf, '='))) {
			return E_RCMALF;
		}
		*p++ = '\0';
		key = buf;
		value = p;
		for (i = 0; i < nelems(ss); i++) {
			struct ss *sp = &ss[i];
			if (!strcmp(sp->name, key)) {
				void *loc = (char *)g + sp->offs;
				switch (sp->type) {
					char *end;
					char *v;
					df_t d;
					int t;
				case T_DF_T:
					d = strtoul(value, &end, 10);
					if (!(*value && !*end)) {
						return E_RCMVAL;
					}
					*(df_t *)loc = d;
					break;
				case T_WEIGHT:
					if ((t = decode_weight(value, -1)) == -1) {
						return E_RCMVAL;
					}
					*(int *)loc = t;
					break;
				case T_CS_TYPE:
					if ((t = decode_cstype(value, -1)) == -1) {
						return E_RCMVAL;
					}
					*(int *)loc = t;
					break;
				case T_STRING:
					if (!(v = strdup(value))) {
						return E_RCMVAL;
					}
					*(char **)loc = v;
					break;
				default:
					abort();
					break;
				}
/*fprintf(stderr, "%s => %s\n", key, value);*/
				goto next;
			}
		}
		return E_RCMALF;
	next: ;
	}

	fclose(f);
	return 0;
}
#endif
