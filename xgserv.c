/*	$Id: xgserv.c,v 1.75 2011/09/14 03:06:09 nis Exp $	*/

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
static char rcsid[] = "$Id: xgserv.c,v 1.75 2011/09/14 03:06:09 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "nwam.h"
#include "util.h"
#include "pi.h"

#include "cxml.h"
#include "priq.h"

#include "common.h"
#include "print.h"

#include "assvP.h"
#include "fssP.h"
#include "nwamP.h"
#include "nrpc.h"
#include "xgserv.h"

#include <gifnoc.h>

/* #define DBG	1 /* */

#define BULKSZ	65536

#if ! defined LOG_PERROR
#define	LOG_PERROR	0
#endif

struct xs_t {
	struct nrpc_t *n;	/* n[0 .. nn[0]-1]   -- row
				   n[nn[0] .. ns-1]  -- col */
	size_t nn[2], ns;	/* ns == nn[0] + nn[1] */
};

struct handle_rootdir_t {
	char const *handle;
	char const *rootdir;
};

struct from_to_rootdir_t {
	char const *from;
	char const *to;
	char const *rootdir;
};

struct dir_key_t {
	int dir;
	char const *key;
};

struct dir_idx_t {
	int dir;
	idx_t id;
};

struct key_idx_t {
	char const *key;
	idx_t id;
};

struct idx_segid_t {
	idx_t id;
	unsigned int segid;
};

struct assv_args_t {
	struct s_syminfo *q;
	df_t nq, nd;
	int qdir, type;
	struct assv_scookie *scookie;
};

struct xfss_args_t {
	df_t np, nn;
	struct fss_query *query;
};

static int xgserv_init(struct nrpc_t *, int, char const *, char const *);
static void *serv_exit(struct nrpc_t *, void *);
static void *serv_distconf(struct nrpc_t *, void *);
static void *serv_wam_update(struct nrpc_t *, void *);
static void *serv_wam_open(struct nrpc_t *, void *);
static void *serv_wam_close(struct nrpc_t *, void *);
static void *serv_wam_drop(struct nrpc_t *, void *);
static void *serv_wam_rename(struct nrpc_t *, void *);
static void *serv_wam_prop_open(struct nrpc_t *, void *);
static void *serv_wam_prop_close(struct nrpc_t *, void *);
static void *serv_wam_fss_open(struct nrpc_t *, void *);
static void *serv_wam_fss_close(struct nrpc_t *, void *);
static void *serv_wam_get_vec(struct nrpc_t *, void *);
static void *serv_wam_prop_gets(struct nrpc_t *, void *);
static void *serv_assv(struct nrpc_t *, void *);
static void *serv_xfss(struct nrpc_t *, void *);
static void *serv_wam_fss_gets(struct nrpc_t *, void *);

static struct syminfo *merge_arl(struct assv_result_t **, size_t, df_t *, df_t *, int *, struct diststat *);
/*static int sscompar(void const *, void const *);*/
/*static int merge_xrl(struct xfss_result_t **, size_t, struct fss_simple_query *, struct fss_simple_query *);*/
static int merge_xrl_1(struct fss_simple_query **, size_t, struct fss_simple_query *, int *);

static int mar_pint(struct mar_t *, void *);
static int mar_pidx(struct mar_t *, void *);
static int mar_string(struct mar_t *, void *);
static int mar_distconf(struct mar_t *, void *);
static int mar_handle_rootdir(struct mar_t *, void *);
static int mar_from_to_rootdir(struct mar_t *, void *);
static int mar_dir_key(struct mar_t *, void *);
static int mar_dir_idx(struct mar_t *, void *);
static int mar_key_idx(struct mar_t *, void *);
static int mar_idx_segid(struct mar_t *, void *);
static int mar_xr_vec(struct mar_t *, void *);
static int mar_raw_vec(struct mar_t *, void *);
static int mar_wam_update_args(struct mar_t *, void *);
static int mar_assv_args(struct mar_t *, void *);
static int mar_assv_result(struct mar_t *, void *);
static int mar_xfss_args(struct mar_t *, void *);
static int mar_xfss_result(struct mar_t *, void *);

static struct s_syminfo *pack_syminfo(struct syminfo *, df_t);
static struct syminfo *unpack_syminfo(struct s_syminfo *, df_t);

#if defined DBG
static void *iov_flatten(struct iovec *, int);
static void log_wam_update_args(struct wam_update_args *);
static void log_raw_vec(struct raw_vec *);
static void print_assv_args(struct assv_args_t *);
static void print_assv_result(size_t, struct assv_result_t *);
static void print_xfss_args(struct xfss_args_t *);
static void print_xfss_result(struct xfss_result_t *);
#endif

static char *handle_base(char const *, char const *);
static size_t safe_request_num(size_t, double, int, size_t);

static struct nrpc_fnreg_t fnreg[] = {
/* 0: EXIT */ { mar_pint, serv_exit, NULL },
/* 1: DISTCONF */ { mar_distconf, serv_distconf, NULL },
/* 2: WAM_UPDATE */ { mar_wam_update_args, serv_wam_update, mar_pint },
/* 3: RAWVEC */ { mar_raw_vec, NULL, NULL },
/* 4: RAWVEC_END */ { NULL, NULL, NULL },
/* 5: WAM_UPDATE_END */ { NULL, NULL, mar_pint },
/* 6: WAM_OPEN */ { mar_handle_rootdir, serv_wam_open, mar_pint },
/* 7: WAM_PROP_OPEN */ { mar_dir_key, serv_wam_prop_open, mar_pint },
/* 8: WAM_FSS_OPEN */ { NULL, serv_wam_fss_open, mar_pint },
/* 9: WAM_CLOSE */ { NULL, serv_wam_close, mar_pint },
/* 10: WAM_PROP_CLOSE */ { mar_dir_key, serv_wam_prop_close, mar_pint },
/* 11: WAM_FSS_CLOSE */ { NULL, serv_wam_fss_close, mar_pint },
/* 12: WAM_DROP */ { mar_handle_rootdir, serv_wam_drop, mar_pint },
/* 13: WAM_RENAME */ { mar_from_to_rootdir, serv_wam_rename, mar_pint },
/* 14: WAM_GET_VEC */ { mar_dir_idx, serv_wam_get_vec, mar_xr_vec },
/* 15: WAM_PROP_GETS */ { mar_key_idx, serv_wam_prop_gets, mar_string },
/* 16: WAM_FSS_GETS */ { mar_idx_segid, serv_wam_fss_gets, mar_string },
/* 17: ASSV */ { mar_assv_args, serv_assv, mar_assv_result },
/* 18: XFSS */ { mar_xfss_args, serv_xfss, mar_xfss_result },
/* 19: SETMASK */ { NULL, NULL, NULL },
};

static char **environ;

int
xgserv_loop(s, xgs_pwam_root, xgs_tmpdir, envp)
char const *xgs_pwam_root, *xgs_tmpdir;
char **envp;
{
	struct nrpc_t n;

	environ = envp;

	if (xgserv_init(&n, s, xgs_pwam_root, xgs_tmpdir) == -1) {
		fprintf(stderr, "xgserv_init 0 failed\n");
		return -1;
	}
	for (;;) {
		if (nrpc_serv_1(&n) == -1) {
			fprintf(stderr, "nrpc_serv failed\n");
			return -1;
		}
	}
	nrpc_clear(&n);

	return 0;
}

/* `s' should be a socket */
static int
xgserv_init(n, s, pwam_root, tmpdir)
struct nrpc_t *n;
char const *pwam_root, *tmpdir;
{
	n->serv_pwam_root = pwam_root;
	n->serv_tmpdir = tmpdir;
	if (nrpc_init(n, s, fnreg, nelems(fnreg)) == -1) {
		syslog(LOG_DEBUG, "nrpc_init %d failed", s);
		return -1;
	}
	n->u.w = NULL;
	n->u.e = 0;
	n->u.r.r = NULL;
	return 0;
}

static void *
serv_exit(n, v)
struct nrpc_t *n;
void *v;
{
	int *x = v;
	nrpc_clear(n);
#if defined ENABLE_MDBG
	mdbg_status(stderr, 1);
#endif
	_exit(*x);
	return NULL;
}

static void *
serv_distconf(n, v)
struct nrpc_t *n;
void *v;
{
	static char name[64];
	struct distconf_t *dc = v;
	n->u.dc = *dc;

	snprintf(name, sizeof name, "serv.%d.%"PRIdDF_T, n->u.dc.dir, n->u.dc.node_id);

	openlog(name, LOG_PID|LOG_PERROR, LOG_LOCAL0);
	return NULL;
}

static void *
serv_wam_update(n, v)
struct nrpc_t *n;
void *v;
{
	struct wam_update_args *a;
	char handle[MAXPATHLEN + 256];	/* XXX */
	struct nrpc_t nn0, *nn = &nn0;

	a = v;

	BEGIN();

	if (xgserv_init(nn, n->s, NULL, NULL) == -1) {
		fprintf(stderr, "xgserv_init 0 failed\n");
		syslog(LOG_DEBUG, "xgserv_init 0 failed");
		n->u.e = -1;
		goto end2;
	}

	snprintf(handle, sizeof handle, "%s/%d.%"PRIdDF_T"/%s", n->serv_pwam_root, n->u.dc.dir, n->u.dc.node_id, a->handle);
	a->handle = handle;
	a->dist.n = n;
	a->dist.nn = nn;
#if defined DBG
	log_wam_update_args(a);
#endif
	a->tmpdir = n->serv_tmpdir;

	assert(n->u.dc.role == NDWAM_ROLE_CHILD);
	a->dist.c = n->u.dc;
	n->u.e = 0;
	a->environ = environ;
	if (wam_update(a, NULL) == -1) {
		syslog(LOG_DEBUG, "wam_update failed");
		n->u.e = -1;
	}

	nrpc_clear(nn);
end2:
	n->ph.selector = NRPC_WAM_UPDATE_END;
	return &n->u.e;
}

static void *
serv_wam_open(n, v)
struct nrpc_t *n;
void *v;
{
	struct handle_rootdir_t *hr = v;
	char handle[MAXPATHLEN + 256];	/* XXX */
	n->u.e = 0;
	snprintf(handle, sizeof handle, "%s/%d.%"PRIdDF_T"/%s", n->serv_pwam_root, n->u.dc.dir, n->u.dc.node_id, hr->handle);
	if (!(n->u.w = wam_open_dc(handle, NULL, &n->u.dc))) {
		syslog(LOG_DEBUG, "serv_wam_open: %s failed", hr->handle);
		n->u.e = -1;
	}
	return &n->u.e;
}

static void *
serv_wam_close(n, v)
struct nrpc_t *n;
void *v;
{
	if (!n->u.w) {
		n->u.e = 0;
		return &n->u.e;
	}
	n->u.e = wam_close(n->u.w);
	n->u.w = NULL;
	return &n->u.e;
}

static void *
serv_wam_drop(n, v)
struct nrpc_t *n;
void *v;
{
	struct handle_rootdir_t *hr = v;
	char handle[MAXPATHLEN + 256];	/* XXX */
	n->u.e = 0;
	snprintf(handle, sizeof handle, "%s/%d.%"PRIdDF_T"/%s", n->serv_pwam_root, n->u.dc.dir, n->u.dc.node_id, hr->handle);
	if (wam_drop_dc(handle, NULL, &n->u.dc) == -1) {
		n->u.e = -1;
	}
	return &n->u.e;
}

static void *
serv_wam_rename(n, v)
struct nrpc_t *n;
void *v;
{
	struct from_to_rootdir_t *ftr = v;
	char from[MAXPATHLEN + 256];	/* XXX */
	char to[MAXPATHLEN + 256];	/* XXX */
	n->u.e = 0;
	snprintf(from, sizeof from, "%s/%d.%"PRIdDF_T"/%s", n->serv_pwam_root, n->u.dc.dir, n->u.dc.node_id, ftr->from);
	snprintf(to, sizeof to, "%s/%d.%"PRIdDF_T"/%s", n->serv_pwam_root, n->u.dc.dir, n->u.dc.node_id, ftr->to);
	if (wam_rename_dc(from, to, NULL, &n->u.dc) == -1) {
		n->u.e = -1;
	}
	return &n->u.e;
}

static void *
serv_wam_prop_open(n, v)
struct nrpc_t *n;
void *v;
{
	struct dir_key_t *dk = v;

	if (!n->u.w) {
		syslog(LOG_DEBUG, "serv_wam_prop_open: error: not open");
		return NULL;
	}
	if (!wam_prop_open(n->u.w, dk->dir, dk->key)) {
		syslog(LOG_DEBUG, "serv_wam_prop_open: failed");
		return NULL;
	}

	n->u.e = 0;
	return &n->u.e;
}

static void *
serv_wam_prop_close(n, v)
struct nrpc_t *n;
void *v;
{
	WAM *pr;
	struct dir_key_t *dk = v;

	if (!n->u.w) {
		syslog(LOG_DEBUG, "serv_wam_prop_close: %s: not open", dk->key);
		return NULL;
	}
	if (!(pr = get_registered_handle(n->u.w, dk->key))) {
		syslog(LOG_DEBUG, "serv_wam_prop_close: %s: not registerd", dk->key);
		return NULL;
	}
	n->u.e = wam_close(pr);
	return &n->u.e;
}

static void *
serv_wam_fss_open(n, v)
struct nrpc_t *n;
void *v;
{

	if (!n->u.w) {
		syslog(LOG_DEBUG, "serv_wam_fss_open: error: not open");
		return NULL;
	}
	if (!wam_fss_open(n->u.w)) {
		syslog(LOG_DEBUG, "serv_wam_fss_open: failed");
		return NULL;
	}

	n->u.e = 0;
	return &n->u.e;
}

static void *
serv_wam_fss_gets(n, v)
struct nrpc_t *n;
void *v;
{
	struct idx_segid_t *x = v;
	char const *s;
	WAM *pr;

	if (!n->u.w) {
		syslog(LOG_DEBUG, "serv_wam_prop_gets: " FSSKEY ": not open");
		return NULL;
	}
	if (!(pr = get_registered_handle(n->u.w, FSSKEY))) {
		syslog(LOG_DEBUG, "serv_wam_prop_gets: " FSSKEY ": not registerd");
		return NULL;
	}
	if (wam_fss_gets(pr, x->id, x->segid, &s) == -1) {
		syslog(LOG_DEBUG, "serv_wam_prop_gets: failed");
		return NULL;
	}
	return s;
}

static void *
serv_wam_fss_close(n, v)
struct nrpc_t *n;
void *v;
{
	WAM *fs;
	if (!n->u.w) {
		syslog(LOG_DEBUG, "serv_wam_fss_close: not open");
		return NULL;
	}
	if (!(fs = get_registered_handle(n->u.w, FSSKEY))) {
		syslog(LOG_DEBUG, "serv_wam_fss_close: not registerd");
		return NULL;
	}
	n->u.e = wam_close(fs);
	return &n->u.e;
}

static void *
serv_wam_get_vec(n, v)
struct nrpc_t *n;
void *v;
{
	struct dir_idx_t *x = v;
	struct xr_vec const *vec;

	if (!n->u.w) {
		syslog(LOG_DEBUG, "serv_wam_get_vec: not open");
		return NULL;
	}
	if (wam_get_vec(n->u.w, x->dir, x->id, &vec) == -1) {
		syslog(LOG_DEBUG, "serv_wam_get_vec: failed");
		return NULL;
	}

	return vec;
}

static void *
serv_wam_prop_gets(n, v)
struct nrpc_t *n;
void *v;
{
	struct key_idx_t *x = v;
	char const *s;
	WAM *pr;

	if (!n->u.w) {
		syslog(LOG_DEBUG, "serv_wam_prop_gets: %s: not open", x->key);
		return NULL;
	}
	if (!(pr = get_registered_handle(n->u.w, x->key))) {
		syslog(LOG_DEBUG, "serv_wam_prop_gets: %s: not registerd", x->key);
		return NULL;
	}
	if (wam_prop_gets(pr, x->id, &s) == -1) {
		syslog(LOG_DEBUG, "serv_wam_prop_gets: failed");
		return NULL;
	}
	return s;
}

static void *
serv_assv(n, v)
struct nrpc_t *n;
void *v;
{
	struct assv_args_t *x = v;
	struct syminfo *q, *r;

	if (!(q = unpack_syminfo(x->q, x->nq))) {
		syslog(LOG_DEBUG, "serv_assv: unpack_syminfo failed");
		return NULL;
	}
	n->u.r.nd = x->nd;
	r = assv(q, x->nq, n->u.w, x->qdir, x->type, &n->u.r.nd, &n->u.r.total, x->scookie);
/* q->v ha subete alias no hazu... */
	free(q);
	free(n->u.r.r);
	if (!(n->u.r.r = pack_syminfo(r, n->u.r.nd))) {
		syslog(LOG_DEBUG, "serv_assv: pack_syminfo failed");
		return NULL;
	}
	return &n->u.r.r;
}

static void *
serv_xfss(n, v)
struct nrpc_t *n;
void *v;
{
	struct xfss_args_t *x = v;
	WAM *fs;

	if (!n->u.w) {
		syslog(LOG_DEBUG, "serv_xfss: error: not open");
		return NULL;
	}
	if (!(fs = get_registered_handle(n->u.w, FSSKEY))) {
		syslog(LOG_DEBUG, "serv_xfss: error: not registerd");
		return NULL;
	}

	n->u.xr.rp.narts = x->np;
	n->u.xr.rn.narts = x->nn;
	if (wam_xfss(fs, x->query, &n->u.xr.rp, &n->u.xr.rn) == -1) {
		syslog(LOG_DEBUG, "serv_xfss: failed");
		return NULL;
	}
	return &n->u.xr;
}

int
cb_wam_update_send_ack(n, e)
struct nrpc_t *n;
{
	struct mar_t *m;
	struct iovec *v;
	int iovcnt;
	if (!(m = mar_create())) {
		syslog(LOG_DEBUG, "mar_create failed");
		return -1;
	}
	mar_reset(m);
	if (mar_pint(m, &e) == -1) {
		syslog(LOG_DEBUG, "mar_pint failed");
		return -1;
	}
	if (!(v = mar_finish(m, &iovcnt))) {
		syslog(LOG_DEBUG, "mar_finish failed");
		return -1;
	}
	if (nrpc_sendpkt(n, v, iovcnt, n->ph.selector + NRPC_ACK) == -1) {
		syslog(LOG_DEBUG, "nrpc_sendpkt failed");
		return -1;
	}
	mar_destroy(m);
	m = NULL;
	return 0;
}

int
cb_wam_update_read_raw_vec(n, r)
struct nrpc_t *n;
struct raw_vec **r;
{
	if (nrpc_recvpkt(n) == -1) {
		syslog(LOG_DEBUG, "cb_wam_update_read_raw_vec: nrpc_recvpkt failed");
		return -1;
	}
	switch (n->ph.selector) {
	case NRPC_RAWVEC:
		if (!r) return -1;
		*r = unmar(n->recvbuf.pkt);
		return 0;
	case NRPC_RAWVEC_END:
		return 1;
	case NRPC_WAM_UPDATE_END:
		return 2;
	default:
		syslog(LOG_DEBUG, "cb_wam_update_read_raw_vec: unknown selector: %d", n->ph.selector);
		return -1;
	}
}

int
cb_wam_openall(xs, handle, rootdir)
struct xs_t *xs;
char const *handle, *rootdir;
{
	struct handle_rootdir_t hr;
	hr.handle = handle_base(handle, rootdir);
	hr.rootdir = rootdir;

	if (nrpc_call_mux(xs->n, xs->ns, NRPC_WAM_OPEN, &hr) == -1) {
		syslog(LOG_DEBUG, "nrpc_call failed");
		return -1;
	}
	return 0;
}

int
cb_wam_dropall(xs, handle, rootdir)
struct xs_t *xs;
char const *handle, *rootdir;
{
	struct handle_rootdir_t hr;
	hr.handle = handle_base(handle, rootdir);
	hr.rootdir = rootdir;

	if (nrpc_call_mux(xs->n, xs->ns, NRPC_WAM_DROP, &hr) == -1) {
		syslog(LOG_DEBUG, "nrpc_call failed");
		return -1;
	}
	return 0;
}

int
cb_wam_renameall(xs, from, to, rootdir)
struct xs_t *xs;
char const *from, *to, *rootdir;
{
	struct from_to_rootdir_t ftr;
	ftr.from = handle_base(from, rootdir);
	ftr.to = handle_base(to, rootdir);
	ftr.rootdir = rootdir;

	if (nrpc_call_mux(xs->n, xs->ns, NRPC_WAM_RENAME, &ftr) == -1) {
		syslog(LOG_DEBUG, "nrpc_call failed");
		return -1;
	}
	return 0;
}

int
cb_wam_prop_openall(xs, dir, key)
struct xs_t *xs;
char const *key;
{
	struct dir_key_t dk;
	dk.dir = dir;
	dk.key = key;

	if (nrpc_call_mux(xs->n, xs->nn[0], NRPC_WAM_PROP_OPEN, &dk) == -1) {
		syslog(LOG_DEBUG, "nrpc_call failed");
		return -1;
	}
	return 0;
}

int
cb_wam_fss_openall(xs)
struct xs_t *xs;
{
	if (nrpc_call_mux(xs->n, xs->nn[0], NRPC_WAM_FSS_OPEN, NULL) == -1) {
		syslog(LOG_DEBUG, "nrpc_call failed");
		return -1;
	}
	return 0;
}

int
cb_wam_closeall(xs)
struct xs_t *xs;
{
	if (nrpc_call_mux(xs->n, xs->ns, NRPC_WAM_CLOSE, NULL) == -1) {
		syslog(LOG_DEBUG, "nrpc_call failed");
		return -1;
	}
	return 0;
}

int
cb_wam_prop_closeall(xs, dir, key)
struct xs_t *xs;
char const *key;
{
	struct dir_key_t dk;
	dk.dir = dir;
	dk.key = key;

	if (nrpc_call_mux(xs->n, xs->nn[0], NRPC_WAM_PROP_CLOSE, &dk) == -1) {
		syslog(LOG_DEBUG, "nrpc_call failed");
		return -1;
	}
	return 0;
}

int
cb_wam_fss_closeall(xs)
struct xs_t *xs;
{
	if (nrpc_call_mux(xs->n, xs->nn[0], NRPC_WAM_FSS_CLOSE, NULL) == -1) {
		syslog(LOG_DEBUG, "nrpc_call failed");
		return -1;
	}
	return 0;
}

void *
cb_wam_get_vec(xs, dir, id)
struct xs_t *xs;
idx_t id;
{
	size_t j;
	struct dir_idx_t di;
	di.dir = dir;
	di.id = id;
	j = XPART(id, xs->nn[dir]);
	switch (dir) {
	case WAM_ROW:
		break;
	case WAM_COL:
		j += xs->nn[0];
		break;
	}
	if (nrpc_call_1(&xs->n[j], NRPC_WAM_GET_VEC, &di) == -1) {
		fprintf(stderr, "nrpc_call failed\n");
		return NULL;
	}
	return unmar(xs->n[j].recvbuf.pkt);
}

void *
cb_wam_prop_gets(xs, key, id)
struct xs_t *xs;
char const *key;
idx_t id;
{
	size_t j;
	struct key_idx_t ki;
	ki.key = key;
	ki.id = id;
	j = XPART(id, xs->nn[WAM_ROW]);	/*XXX XXX XXX*/
	if (nrpc_call_1(&xs->n[j], NRPC_WAM_PROP_GETS, &ki) == -1) {
		fprintf(stderr, "nrpc_call failed\n");
		return NULL;
	}
	return unmar(xs->n[j].recvbuf.pkt);
}

void *
cb_wam_fss_gets(xs, id, segid)
struct xs_t *xs;
idx_t id;
unsigned int segid;
{
	size_t j;
	struct idx_segid_t is;
	is.id = id;
	is.segid = segid;
	j = XPART(id, xs->nn[WAM_ROW]);
	if (nrpc_call_1(&xs->n[j], NRPC_WAM_FSS_GETS, &is) == -1) {
		fprintf(stderr, "nrpc_call failed\n");
		return NULL;
	}
	return unmar(xs->n[j].recvbuf.pkt);
}

void *
cb_assv(xs, q, nq, qdir, type, nd, totalp, scookie, allowerror, may_incomplete, diststatp)
struct xs_t *xs;
struct syminfo *q;
df_t nq, *nd;
df_t *totalp;
struct assv_scookie *scookie;
unsigned int allowerror;
int *may_incomplete;
struct diststat **diststatp;
{
	struct assv_args_t aa;
	struct assv_result_t **ar;
	struct nrpc_t *n;
	size_t nn, i;
	struct syminfo *r;
	int mi;
#if defined DBG
	char s[8192];
#endif

	if (qdir == WAM_ROW) {
		n = &xs->n[xs->nn[0]];
		nn = xs->nn[1];
	}
	else {
		n = &xs->n[0];
		nn = xs->nn[0];
	}

	if (!(aa.q = pack_syminfo(q, nq))) {
		syslog(LOG_DEBUG, "cb_assv: pack_syminfo failed");
		return NULL;
	}
	aa.nq = nq;
	aa.qdir = qdir;
	aa.type = type;
	if (*nd == 0 || !allowerror) {
		aa.nd = *nd;
	}
	else {
		aa.nd = safe_request_num(*nd, 1.0 / nn, allowerror, nn);
	}
	aa.scookie = scookie;

	if (nrpc_call_mux(n, nn, NRPC_ASSV, &aa) == -1) {
		syslog(LOG_DEBUG, "nrpc_call failed");
		return NULL;
	}
	free(aa.q);

	if (!(ar = calloc(nn, sizeof *ar))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}
	for (i = 0; i < nn; i++) {
		ar[i] = unmar(n[i].recvbuf.pkt);
	}
	if (diststatp) {
		free(*diststatp);
		if (!(*diststatp = calloc(nn, sizeof **diststatp))) {
			syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
			return NULL;
		}
	}

	r = merge_arl(ar, nn, nd, totalp, &mi, diststatp ? *diststatp : NULL);

	if (may_incomplete) {
		*may_incomplete = mi;
	}

	free(ar);
#if defined DBG
snprint_syminfo(r, *nd, 0, 128, NULL, 0, s, sizeof s);
printf("cb_assv: result: %s\n", s);
syslog(LOG_DEBUG, "cb_assv: result: %s", s);
#endif
	return r;
}

struct cont_arl {
	struct assv_result_t *a;
	df_t j;
};

static priq_prop_t prop_f_psscompar;
static priq_prop_t prop_b_psscompar;

static priq_prop_t prop_f_acompar;
static priq_prop_t prop_b_acompar;

static struct syminfo *
merge_arl(ar, nn, nd, totalp, may_incomplete, diststat)
struct assv_result_t **ar;
size_t nn;
df_t *nd, *totalp;
int *may_incomplete;
struct diststat *diststat;
{
	int merge_arl_incomplete = 0;
	size_t i;
	struct syminfo *r;
	df_t ttl;
	struct priq *a;
	df_t k;
	struct cont_arl *c;

	if (!(c = calloc(nn, sizeof *c))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}

	if (!(a = priq_creat_p(sizeof (struct cont_arl *), NULL, prop_f_psscompar, prop_b_psscompar, NULL))) {
		syslog(LOG_DEBUG, "PRIQ_CREAT");
		return NULL;
	}

	if (!(r = calloc(*nd, sizeof *r))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}

	ttl = 0;

	for (i = 0; i < nn; i++) {
		struct cont_arl *t = &c[i];
		t->a = ar[i];
		t->j = 0;
		if (t->a->nd) {
			if (priq_enq(a, &t) == -1) {
				syslog(LOG_DEBUG, "priq_enq");
				return NULL;
			}
		}
		ttl += ar[i]->total;
	}
	k = 0;
	while (!PRIQ_EMPTYP(a)) {
		struct cont_arl *t = *(struct cont_arl **)PRIQ_TOP(a);
		*(struct s_syminfo *)&r[k++] = t->a->r[t->j++];
		if (k == *nd) break;
		if (t->j < t->a->nd) {
			priq_rplatop(a, &t);
		}
		else {
			if (t->a->nd != t->a->total) {
				merge_arl_incomplete = 1;
				/* NOTE: can be false positive */
			}
			priq_deq(a);
		}
	}

	if (diststat) {
		for (i = 0; i < nn; i++) {
			struct cont_arl *t = &c[i];
			diststat[i].j = t->j;
			diststat[i].nd = t->a->nd;
			diststat[i].total = t->a->total;
		}
	}
#if defined DBG_DIST
if (merge_arl_incomplete) {
	for (i = 0; i < nn; i++) {
		struct cont_arl *t = &c[i];
		size_t j;
if (t->j == t->a->nd && t->a->nd < t->a->total) fprintf(stderr, "*"); else fprintf(stderr, "#");
		fprintf(stderr, "%02"PRIuSIZE_T" (%"PRIdDF_T" %"PRIdDF_T" %"PRIdDF_T")", (pri_size_t)i, t->j, t->a->nd, t->a->total);
		for (j = 0; j < t->j; j++) {
			fprintf(stderr, " %6"PRIuIDX_T" %e", t->a->r[j].id, t->a->r[j].weight);
		}

		fprintf(stderr, "\n");
	}
}
#endif

	priq_free(a);
	free(c);

	*nd = k;
	if (totalp) {
		*totalp = ttl;
	}
	*may_incomplete = merge_arl_incomplete;
	return r;
}

#if 0
static int
sscompar(va, vb)
void const *va, *vb;
{
	struct s_syminfo const *a, *b;
	a = va;
	b = vb;
	if (fequal(a->weight, b->weight)) {
		if (a->id == b->id) {
			return 0;
		}
		else if (a->id < b->id) {
			return 1;
		}
		else {
			return -1;
		}
	}
	if (a->weight < b->weight) {
		return 1;
	}
	else if (a->weight > b->weight) {
		return -1;
	}
	return 0;
}
#endif

#define compar_psscompar(e, A, B, q) \
{ \
	struct cont_arl * const *va; \
	struct cont_arl * const *vb; \
	struct s_syminfo *x, *y; \
	va = (A); \
	vb = (B); \
	x = &(*va)->a->r[(*va)->j]; \
	y = &(*vb)->a->r[(*vb)->j]; \
	if (fequal(x->weight, y->weight)) { \
		if (x->id == y->id) { \
			e = 0; \
		} \
		else if (x->id < y->id) { \
			e = -1; \
		} \
		else { \
			e = 1; \
		} \
	} \
	else if (x->weight < y->weight) { \
		e = 1; \
	} \
	else if (x->weight > y->weight) { \
		e = -1; \
	} \
	else e = 0; \
}

static PROP_F_TYPED(prop_f_psscompar, compar_psscompar, struct cont_arl **)
static PROP_B_TYPED(prop_b_psscompar, compar_psscompar, struct cont_arl **)

struct cont_xrl {
	struct fss_simple_query *a;
	df_t j;
};

int
cb_xfss(xs, query, rp, rn, allowerror, may_incomplete)
struct xs_t *xs;
struct fss_query *query;
struct fss_simple_query *rp, *rn;
unsigned int allowerror;
int *may_incomplete;
{
	int e = 0;
	struct xfss_args_t xa;
	struct fss_simple_query **xrp, **xrn;
	struct nrpc_t *n;
	size_t nn, i;
	int ip, in;

	n = &xs->n[0];
	nn = xs->nn[0];

	xa.query = query;
	if (rp->narts <= 0 || !allowerror) {
		xa.np = rp->narts;
	}
	else {
		xa.np = safe_request_num(rp->narts, 1.0 / nn, allowerror, nn);
	}
	if (rn->narts <= 0 || !allowerror) {
		xa.nn = rn->narts;
	}
	else {
		xa.nn = safe_request_num(rn->narts, 1.0 / nn, allowerror, nn);
	}

	if (nrpc_call_mux(n, nn, NRPC_XFSS, &xa) == -1) {
		syslog(LOG_DEBUG, "nrpc_call failed");
		return -1;
	}

	if (!(xrp = calloc(nn, sizeof *xrp))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return -1;
	}
	if (!(xrn = calloc(nn, sizeof *xrn))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return -1;
	}
	for (i = 0; i < nn; i++) {
		struct xfss_result_t *xr;
		xr = unmar(n[i].recvbuf.pkt);
		xrp[i] = &xr->rp;
		xrn[i] = &xr->rn;
	}
	if (merge_xrl_1(xrp, nn, rp, &ip) == -1) {
		syslog(LOG_DEBUG, "merge_xrl_1 P failed");
		e = -1;
	}
	if (merge_xrl_1(xrn, nn, rn, &in) == -1) {
		syslog(LOG_DEBUG, "merge_xrl_1 N failed");
		e = -1;
	}

	if (may_incomplete) {
		*may_incomplete = ip || in;
	}

	free(xrp);
	free(xrn);

	return e;
}

static int
merge_xrl_1(xrp, nn, rp, may_incomplete)
struct fss_simple_query **xrp;
size_t nn;
struct fss_simple_query *rp;
int *may_incomplete;
{
	size_t i;
	df_t tnarts, ttotal, k;
	struct priq *a;
	struct cont_xrl *c;
	int merge_xrl_1_incomplete = 0;

	tnarts = ttotal = 0;
	for (i = 0; i < nn; i++) {
		tnarts += xrp[i]->narts;
		ttotal += xrp[i]->total;
	}
	if (rp->narts != -1) {
		tnarts = MIN(tnarts, rp->narts);
	}
	*rp = *xrp[0];	/* copy! */
	rp->pattern = NULL;
	rp->arts = NULL;
	rp->narts = tnarts;
	rp->total = ttotal;
	if (rp->narts == 0) {
		return 0;
	}

	if (!(rp->arts = calloc(rp->narts, sizeof *rp->arts))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return -1;
	}

	if (!(c = calloc(nn, sizeof *c))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return -1;
	}

	if (!(a = priq_creat_p(sizeof (struct cont_xrl *), NULL, prop_f_acompar, prop_b_acompar, NULL))) {
		syslog(LOG_DEBUG, "PRIQ_CREAT");
		return -1;
	}

	for (i = 0; i < nn; i++) {
		struct cont_xrl *t = &c[i];
		t->a = xrp[i];
		t->j = 0;
		if (t->a->narts) {
			if (priq_enq(a, &t) == -1) {
				syslog(LOG_DEBUG, "priq_enq");
				return -1;
			}
		}
	}
	k = 0;
	while (!PRIQ_EMPTYP(a)) {
		struct cont_xrl *t = *(struct cont_xrl **)PRIQ_TOP(a);
		rp->arts[k++] = t->a->arts[t->j++];
		if (k == rp->narts) break;
		if (t->j < t->a->narts) {
			priq_rplatop(a, &t);
		}
		else {
			if (t->a->narts != t->a->total) {
				merge_xrl_1_incomplete = 1;
			}
			priq_deq(a);
		}
	}

	priq_free(a);
	free(c);

	assert(rp->narts == k);

	*may_incomplete = merge_xrl_1_incomplete;
	return 0;
}

#define compar_acompar(e, A, B, q) \
{ \
	struct cont_xrl * const *va; \
	struct cont_xrl * const *vb; \
	struct art *x, *y; \
	va = (A); \
	vb = (B); \
	x = &(*va)->a->arts[(*va)->j]; \
	y = &(*vb)->a->arts[(*vb)->j]; \
	if (x->id < y->id) { \
		e = -1; \
	} \
	else if (x->id > y->id) { \
		e = 1; \
	} \
	else if (x->segid < y->segid) { \
		e = -1; \
	} \
	else if (x->segid > y->segid) { \
		e = 1; \
	} \
	else if (x->disp < y->disp) { \
		e = -1; \
	} \
	else if (x->disp > y->disp) { \
		e = 1; \
	} \
	else e = 0; \
}

static PROP_F_TYPED(prop_f_acompar, compar_acompar, struct cont_xrl **)
static PROP_B_TYPED(prop_b_acompar, compar_acompar, struct cont_xrl **)

static int
mar_pint(m, v)
struct mar_t *m;
void *v;
{
	int *x = v;

	if (mar_append(m, NULL, x, sizeof *x) == -1) {
		syslog(LOG_DEBUG, "mar_pint: mar_append failed");
		return -1;
	}
	return 0;
}

static int
mar_pidx(m, v)
struct mar_t *m;
void *v;
{
	idx_t *x = v;

	if (mar_append(m, NULL, x, sizeof *x) == -1) {
		syslog(LOG_DEBUG, "mar_pidx: mar_append failed");
		return -1;
	}
	return 0;
}

static int
mar_distconf(m, v)
struct mar_t *m;
void *v;
{
	struct distconf_t *x = v;

	if (mar_append(m, NULL, x, sizeof *x) == -1) {
		syslog(LOG_DEBUG, "mar_distconf: mar_append failed");
		return -1;
	}
	return 0;
}

static int
mar_string(m, v)
struct mar_t *m;
void *v;
{
	char *x = v;

	if (!x) {
		syslog(LOG_DEBUG, "mar_string: !x");
		return -1;
	}
	if (mar_append(m, NULL, x, strlen(x) + 1) == -1) {
		syslog(LOG_DEBUG, "mar_string: mar_append failed");
		return -1;
	}
	return 0;
}

static int
mar_handle_rootdir(m, v)
struct mar_t *m;
void *v;
{
	struct handle_rootdir_t *x = v;

	if (mar_append(m, NULL, x, sizeof *x) == -1) {
		syslog(LOG_DEBUG, "mar_handle_rootdir: mar_append failed");
		return -1;
	}
	if (!x->handle) {
		syslog(LOG_DEBUG, "mar_handle_rootdir: !x->handle");
		return -1;
	}
	if (mar_append(m, x, x->handle, strlen(x->handle) + 1) == -1) {
		syslog(LOG_DEBUG, "mar_handle_rootdir: mar_append failed");
		return -1;
	}
	if (x->rootdir) {
		if (mar_append(m, x, x->rootdir, strlen(x->rootdir) + 1) == -1) {
			syslog(LOG_DEBUG, "mar_handle_rootdir: mar_append failed");
			return -1;
		}
	}

	return 0;
}

static int
mar_from_to_rootdir(m, v)
struct mar_t *m;
void *v;
{
	struct from_to_rootdir_t *x = v;

	if (mar_append(m, NULL, x, sizeof *x) == -1) {
		syslog(LOG_DEBUG, "mar_from_to_rootdir: mar_append failed");
		return -1;
	}
	if (!x->from) {
		syslog(LOG_DEBUG, "mar_from_to_rootdir: !x->from");
		return -1;
	}
	if (mar_append(m, x, x->from, strlen(x->from) + 1) == -1) {
		syslog(LOG_DEBUG, "mar_from_to_rootdir: mar_append failed");
		return -1;
	}
	if (!x->to) {
		syslog(LOG_DEBUG, "mar_from_to_rootdir: !x->to");
		return -1;
	}
	if (mar_append(m, x, x->to, strlen(x->to) + 1) == -1) {
		syslog(LOG_DEBUG, "mar_from_to_rootdir: mar_append failed");
		return -1;
	}
	if (x->rootdir) {
		if (mar_append(m, x, x->rootdir, strlen(x->rootdir) + 1) == -1) {
			syslog(LOG_DEBUG, "mar_from_to_rootdir: mar_append failed");
			return -1;
		}
	}

	return 0;
}

static int
mar_dir_key(m, v)
struct mar_t *m;
void *v;
{
	struct dir_key_t *x = v;

	if (mar_append(m, NULL, x, sizeof *x) == -1) {
		syslog(LOG_DEBUG, "mar_dir_key: mar_append failed");
		return -1;
	}
	if (!x->key) {
		syslog(LOG_DEBUG, "mar_dir_key: !x->key");
		return -1;
	}
	if (mar_append(m, x, x->key, strlen(x->key) + 1) == -1) {
		syslog(LOG_DEBUG, "mar_dir_key: mar_append failed");
		return -1;
	}

	return 0;
}

static int
mar_dir_idx(m, v)
struct mar_t *m;
void *v;
{
	struct dir_idx_t *x = v;

	if (mar_append(m, NULL, x, sizeof *x) == -1) {
		syslog(LOG_DEBUG, "mar_dir_idx: mar_append failed");
		return -1;
	}
	return 0;
}

static int
mar_key_idx(m, v)
struct mar_t *m;
void *v;
{
	struct key_idx_t *x = v;

	if (mar_append(m, NULL, x, sizeof *x) == -1) {
		syslog(LOG_DEBUG, "mar_key_idx: mar_append failed");
		return -1;
	}
	if (!x->key) {
		syslog(LOG_DEBUG, "mar_key_idx: !x->key");
		return -1;
	}
	if (mar_append(m, x, x->key, strlen(x->key) + 1) == -1) {
		syslog(LOG_DEBUG, "mar_key_idx: mar_append failed");
		return -1;
	}

	return 0;
}

static int
mar_idx_segid(m, v)
struct mar_t *m;
void *v;
{
	struct dir_idx_t *x = v;

	if (mar_append(m, NULL, x, sizeof *x) == -1) {
		syslog(LOG_DEBUG, "mar_idx_segid: mar_append failed");
		return -1;
	}
	return 0;
}

static int
mar_xr_vec(m, v)
struct mar_t *m;
void *v;
{
	struct xr_vec *x = v;

	if (mar_append(m, NULL, x, sizeof_dxr_vec(x->elem_num)) == -1) {
		syslog(LOG_DEBUG, "mar_xr_vec: mar_append failed");
		return -1;
	}
	return 0;
}

static int
mar_raw_vec(m, v)
struct mar_t *m;
void *v;
{
	struct raw_vec *r = v;
	size_t i;

	if (mar_append(m, NULL, r, sizeof *r) == -1) {
		syslog(LOG_DEBUG, "mar_raw_vec: mar_append failed");
		return -1;
	}

	if (r->p) {
		if (mar_append(m, r, r->p, strlen(r->p) + 1) == -1) {
			syslog(LOG_DEBUG, "mar_raw_vec: mar_append failed");
			return -1;
		}
	}

	if (r->name) {
		if (mar_append(m, r, r->name, strlen(r->name) + 1) == -1) {
			syslog(LOG_DEBUG, "mar_raw_vec: mar_append failed");
			return -1;
		}
	}

	if (r->elem_num) {
		if (mar_append(m, r, r->elems, r->elem_num * sizeof *r->elems) == -1) {
			syslog(LOG_DEBUG, "mar_raw_vec: mar_append failed");
			return -1;
		}
		for (i = 0; i < r->elem_num; i++) {
			if (!r->elems[i].name) {
				syslog(LOG_DEBUG, "mar_raw_vec: !r->elems[i].name");
				return -1;
			}
			if (mar_append(m, r->elems, r->elems[i].name, strlen(r->elems[i].name) + 1) == -1) {
				syslog(LOG_DEBUG, "mar_raw_vec: mar_append failed");
				return -1;
			}
		}
	}

	if (r->nprops) {
		if (mar_append(m, r, r->props, r->nprops * sizeof *r->props) == -1) {
			syslog(LOG_DEBUG, "mar_raw_vec: mar_append failed");
			return -1;
		}
		for (i = 0; i < r->nprops; i++) {
			if (!r->props[i].key) {
				syslog(LOG_DEBUG, "mar_raw_vec: !r->props[i].key");
				return -1;
			}
			if (mar_append(m, r->props, r->props[i].key, strlen(r->props[i].key) + 1) == -1) {
				syslog(LOG_DEBUG, "mar_raw_vec: mar_append failed");
				return -1;
			}
			if (r->props[i].value && mar_append(m, r->props, r->props[i].value, strlen(r->props[i].value) + 1) == -1) {
				syslog(LOG_DEBUG, "mar_raw_vec: mar_append failed");
				return -1;
			}
		}
	}

	for (i = 0; i < r->nsegms; i++) {
		if (!r->segms[i].value) {
			syslog(LOG_DEBUG, "mar_raw_vec: !r->segms[i].value");
			return -1;
		}
		if (mar_append(m, r, r->segms[i].value, strlen(r->segms[i].value) + 1) == -1) {
			syslog(LOG_DEBUG, "mar_raw_vec: mar_append failed");
			return -1;
		}
	}
	return 0;
}

static int
mar_wam_update_args(m, v)
struct mar_t *m;
void *v;
{
/** XXX s.{host,serv,sock,path} will not be transferred! */
	struct wam_update_args *a = v;
	size_t i;

	if (mar_append(m, NULL, a, sizeof *a) == -1) {
		syslog(LOG_DEBUG, "mar_wam_update_args: mar_append failed");
		return -1;
	}

	if (!a->handle) {
		syslog(LOG_DEBUG, "mar_wam_update_args: !a->handle");
		return -1;
	}
	if (mar_append(m, a, a->handle, strlen(a->handle) + 1) == -1) {
		syslog(LOG_DEBUG, "mar_wam_update_args: mar_append failed");
		return -1;
	}

	if (!a->tmpdir) {
		syslog(LOG_DEBUG, "mar_wam_update_args: !a->tmpdir");
		return -1;
	}
	if (mar_append(m, a, a->tmpdir, strlen(a->tmpdir) + 1) == -1) {
		syslog(LOG_DEBUG, "mar_wam_update_args: mar_append failed");
		return -1;
	}

	if (a->command == WAM_UPDATE_FROM_ITB) {
		size_t k;
		for (k = 0; a->i.itbs[k]; k++) ;
		if (mar_append(m, a, a->i.itbs, (k + 1) * sizeof *a->i.itbs) == -1) {
			syslog(LOG_DEBUG, "mar_wam_update_args: mar_append failed");
			return -1;
		}
		for (i = 0; i < k; i++) {
			if (!a->i.itbs[i]) {
				syslog(LOG_DEBUG, "mar_wam_update_args: !a->i.itbs[i]");
				return -1;
			}
			if (mar_append(m, a->i.itbs, a->i.itbs[i], strlen(a->i.itbs[i]) + 1) == -1) {
				syslog(LOG_DEBUG, "mar_wam_update_args: mar_append failed");
				return -1;
			}
		}
		if (!a->i.s.stemmer) {
			syslog(LOG_DEBUG, "mar_wam_update_args: !a->i.s.stemmer");
			return -1;
		}
		if (mar_append(m, a, a->i.s.stemmer, strlen(a->i.s.stemmer) + 1) == -1) {
			syslog(LOG_DEBUG, "mar_wam_update_args: mar_append failed");
			return -1;
		}
		for (k = 0; a->i.keys[k]; k++) ;
		if (mar_append(m, a, a->i.keys, (k + 1) * sizeof *a->i.keys) == -1) {
			syslog(LOG_DEBUG, "mar_wam_update_args: mar_append failed");
			return -1;
		}
		for (i = 0; i < k; i++) {
			if (!a->i.keys[i]) {
				syslog(LOG_DEBUG, "mar_wam_update_args: !a->i.keys[i]");
				return -1;
			}
			if (mar_append(m, a->i.keys, a->i.keys[i], strlen(a->i.keys[i]) + 1) == -1) {
				syslog(LOG_DEBUG, "mar_wam_update_args: mar_append failed");
				return -1;
			}
		}
	}

	if (a->command == WAM_UPDATE_DELIDL) {
		if (mar_append(m, a, a->d.id, a->d.nid * sizeof *a->d.id) == -1) {
			syslog(LOG_DEBUG, "mar_wam_update_args: mar_append failed");
			return -1;
		}
		if (mar_append(m, a, a->d.vecs, a->d.nid * sizeof *a->d.vecs) == -1) {
			syslog(LOG_DEBUG, "mar_wam_update_args: mar_append failed");
			return -1;
		}
		for (i = 0; i < a->d.nid; i++) {
			if (mar_append(m, a->d.vecs, a->d.vecs[i], sizeof_dxr_vec(a->d.vecs[i]->elem_num)) == -1) {
				syslog(LOG_DEBUG, "mar_wam_update_args: mar_append failed");
				return -1;
			}
		}
	}

	assert(!a->dist.n);
	assert(!a->dist.nn);
	return 0;
}

static int
mar_assv_args(m, v)
struct mar_t *m;
void *v;
{
	struct assv_args_t *x = v;
	size_t i;

	if (mar_append(m, NULL, x, sizeof *x) == -1) {
		syslog(LOG_DEBUG, "mar_assv_args: mar_append failed");
		return -1;
	}

	if (!x->q) {
		syslog(LOG_DEBUG, "mar_assv_args: !x->q");
		return -1;
	}
	if (mar_append(m, x, x->q, x->nq * sizeof *x->q) == -1) {
		syslog(LOG_DEBUG, "mar_assv_args: mar_append failed");
		return -1;
	}
	for (i = 0; i < x->nq; i++) {
		if (x->q[i].v) {
			if (mar_append(m, x->q, x->q[i].v, sizeof_dxr_vec(x->q[i].v->elem_num)) == -1) {
				syslog(LOG_DEBUG, "mar_assv_args: mar_append failed");
				return -1;
			}
		}
	}
	if (x->scookie) {
		if (mar_append(m, x, x->scookie, sizeof *x->scookie) == -1) {
			syslog(LOG_DEBUG, "mar_assv_args: mar_append failed");
			return -1;
		}
		if (x->scookie->flag & ASSV_SC_CUTOFF_DF_LIST) {
			if (mar_append(m, x->scookie, x->scookie->cutoff_df_list, x->nq * sizeof *x->scookie->cutoff_df_list) == -1) {
				syslog(LOG_DEBUG, "mar_assv_args: mar_append failed");
				return -1;
			}
		}
		if (x->scookie->flag & ASSV_SC_BX) {
			if (mar_append(m, x->scookie, x->scookie->bx.b, x->scookie->bx.blen * sizeof *x->scookie->bx.b) == -1) {
				syslog(LOG_DEBUG, "mar_assv_args: mar_append failed");
				return -1;
			}
		}
		if (x->scookie->flag & ASSV_SC_MASK) {
			if (x->scookie->mask.p.m) {
				if (mar_append(m, x->scookie, x->scookie->mask.p.m, x->scookie->mask.p.len * sizeof *x->scookie->mask.p.m) == -1) {
					syslog(LOG_DEBUG, "mar_assv_args: mar_append failed");
					return -1;
				}
			}
			if (x->scookie->mask.n.m) {
				if (mar_append(m, x->scookie, x->scookie->mask.n.m, x->scookie->mask.n.len * sizeof *x->scookie->mask.n.m) == -1) {
					syslog(LOG_DEBUG, "mar_assv_args: mar_append failed");
					return -1;
				}
			}
		}
		if (x->scookie->flag & ASSV_SC_FSS) {
			if (mar_append(m, x->scookie, x->scookie->fssq, sizeof *x->scookie->fssq) == -1) {
				syslog(LOG_DEBUG, "mar_assv_args: mar_append failed");
				return -1;
			}
			if (mar_append(m, x->scookie->fssq, x->scookie->fssq->query, x->scookie->fssq->nq * sizeof *x->scookie->fssq->query) == -1) {
				syslog(LOG_DEBUG, "mar_assv_args: mar_append failed");
				return -1;
			}
			for (i = 0; i < x->scookie->fssq->nq; i++) {
				size_t j;

				if (mar_append(m, x->scookie->fssq->query, x->scookie->fssq->query[i].q, x->scookie->fssq->query[i].n * sizeof *x->scookie->fssq->query[i].q)) {
					syslog(LOG_DEBUG, "mar_assv_args: mar_append failed");
					return -1;
				}
				for (j = 0; j < x->scookie->fssq->query[i].n; j++) {
					if (mar_append(m, x->scookie->fssq->query[i].q, x->scookie->fssq->query[i].q[j].pattern, strlen(x->scookie->fssq->query[i].q[j].pattern) + 1)) {
						syslog(LOG_DEBUG, "mar_assv_args: mar_append failed");
						return -1;
					}
					assert(x->scookie->fssq->query[i].q[j].arts == NULL);
				}
			}

		}

	}
	return 0;
}

static int
mar_assv_result(m, v)
struct mar_t *m;
void *v;
{
	struct assv_result_t *x = v;

	if (mar_append(m, NULL, x, sizeof *x) == -1) {
		syslog(LOG_DEBUG, "mar_assv_result: mar_append failed");
		return -1;
	}
	if (x->r) {
		size_t i;
		if (mar_append(m, x, x->r, x->nd * sizeof *x->r) == -1) {
			syslog(LOG_DEBUG, "mar_assv_result: mar_append failed");
			return -1;
		}
		for (i = 0; i < x->nd; i++) {
			if (x->r[i].v) {
				syslog(LOG_DEBUG, "mar_assv_result: not implemented");
				return -1;
			}
		}
	}

	return 0;
}

static int
mar_xfss_args(m, v)
struct mar_t *m;
void *v;
{
	struct xfss_args_t *x = v;
	size_t i;

	if (mar_append(m, NULL, x, sizeof *x) == -1) {
		syslog(LOG_DEBUG, "mar_xfss_args: mar_append failed");
		return -1;
	}

	if (!x->query) {
		syslog(LOG_DEBUG, "mar_xfss_args: !x->query");
		return -1;
	}

	if (mar_append(m, x, x->query, sizeof *x->query) == -1) {
		syslog(LOG_DEBUG, "mar_xfss_args: mar_append failed");
		return -1;
	}

	if (mar_append(m, x->query, x->query->query, x->query->nq * sizeof *x->query->query) == -1) {
		syslog(LOG_DEBUG, "mar_xfss_args: mar_append failed");
		return -1;
	}
	for (i = 0; i < x->query->nq; i++) {
		size_t j;
		if (mar_append(m, x->query->query, x->query->query[i].q, x->query->query[i].n * sizeof *x->query->query[i].q)) {
			syslog(LOG_DEBUG, "mar_xfss_args: mar_append failed");
			return -1;
		}
		for (j = 0; j < x->query->query[i].n; j++) {
			if (mar_append(m, x->query->query[i].q, x->query->query[i].q[j].pattern, strlen(x->query->query[i].q[j].pattern) + 1)) {
				syslog(LOG_DEBUG, "mar_xfss_args: mar_append failed");
				return -1;
			}
			assert(x->query->query[i].q[j].arts == NULL);
		}
	}

	return 0;
}

static int
mar_xfss_result(m, v)
struct mar_t *m;
void *v;
{
	struct xfss_result_t *x = v;
	void *save;
/* WARNING: x->r[pn]->arts[*].q will not be transferred */

	save = x->rp.pattern;
	x->rp.pattern = NULL;
	if (mar_append(m, NULL, x, sizeof *x) == -1) {
		syslog(LOG_DEBUG, "mar_xfss_result: mar_append failed");
		return -1;
	}
	x->rp.pattern = save;
	if (x->rp.narts) {
		if (mar_append(m, x, x->rp.arts, x->rp.narts * sizeof *x->rp.arts) == -1) {
			syslog(LOG_DEBUG, "mar_xfss_result: mar_append failed");
			return -1;
		}
	}
	if (x->rn.pattern) {
		if (mar_append(m, x, x->rn.pattern, strlen(x->rn.pattern) + 1) == -1) {
			syslog(LOG_DEBUG, "mar_xfss_result: mar_append failed");
			return -1;
		}
	}
	if (x->rn.narts) {
		if (mar_append(m, x, x->rn.arts, x->rn.narts * sizeof *x->rn.arts) == -1) {
			syslog(LOG_DEBUG, "mar_xfss_result: mar_append failed");
			return -1;
		}
	}

	return 0;
}

struct servlist_t *
xgwam_servlist(dc, server_conf_paths)
struct distconf_t *dc;
char const **server_conf_paths;
{
	struct servlist_t *ss;
	size_t j;
	int d;
	if (!(ss = calloc(dc->nn[0] + dc->nn[1] + 1, sizeof *ss))) {
		syslog(LOG_DEBUG, "calloc");
		return NULL;
	}
	for (d = j = 0; d < 2; d++) {
		size_t i;
		FILE *f;
		char buf[MAXHOSTNAMELEN * 3 + 4];
		if (!(f = fopen(server_conf_paths[d], "r"))) {
			syslog(LOG_DEBUG, "xgwam_servlist: %s: %s", server_conf_paths[d], strerror(errno));
			return NULL;
		}
		for (i = 0; i < dc->nn[d]; i++, j++) {
			char *v[4], *p;
			size_t ni;
			if (!fgets(buf, sizeof buf, f)) {
				rewind(f);
				if (!fgets(buf, sizeof buf, f)) {
					syslog(LOG_DEBUG, "fgets: %s", strerror(errno));
					return NULL;
				}
			}
			if (p = rindex(buf, '\n')) *p = '\0';
			if (p = rindex(buf, '\r')) *p = '\0';
			ni = splitv(buf, SSEP, v, nelems(v), 0);
			if (ni != 3 && ni != 2) {
				syslog(LOG_DEBUG, "splitv: invalid field number: %"PRIuSIZE_T, (pri_size_t)ni);
				return NULL;
			}
			ss[j].host = ss[j].port = ss[j].localsocket = NULL;
			if (*v[0] && !(ss[j].host = strdup(v[0]))) return NULL;
			if (*v[1] && !(ss[j].port = strdup(v[1]))) return NULL;
			if (ni == 3 && *v[2] && !(ss[j].localsocket = strdup(v[2]))) return NULL;
			if (!ss[j].host && !ss[j].localsocket) {
				syslog(LOG_DEBUG, "malformed conf");
				return NULL;
			}
		}
		fclose(f);
	}
	assert(j == dc->nn[0] + dc->nn[1]);
	ss[j].host = ss[j].port = ss[j].localsocket = NULL;
	return ss;
}

struct xs_t *
xgwam_connect(dc, ss)
struct distconf_t *dc;
struct servlist_t *ss;
{
	struct xs_t *xs;
#if defined TCP_NODELAY
	int one = 1;
#endif
	size_t j;
	int d, s;
	struct distconf_t distconf;

	assert(dc->role == NDWAM_ROLE_PARENT);
	assert(dc->dir == 0);
	assert(dc->node_id == 0);

	if (!(xs = calloc(1, sizeof *xs))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}
	xs->nn[0] = dc->nn[0];
	xs->nn[1] = dc->nn[1];
	xs->ns = xs->nn[0] + xs->nn[1];

	if (!(xs->n = calloc(xs->ns, sizeof *xs->n))) {
		syslog(LOG_DEBUG, "calloc(%"PRIuSIZE_T"): %s", (pri_size_t)xs->ns, strerror(errno));
		return NULL;
	}
	distconf = *dc;
	distconf.role = NDWAM_ROLE_CHILD;

	for (d = j = 0; d < 2; d++) {
		size_t i;
		distconf.dir = d;	/* overwrite */
		for (i = 0; i < xs->nn[d]; i++, j++) {
			distconf.node_id = i;	/* overwrite */

if (!ss[j].host && !ss[j].localsocket) {
	syslog(LOG_DEBUG, "xgwam_connect: internal error");
	return NULL;
}
			if ((s = ga_nnet_cli(ss[j].host, ss[j].port, ss[j].localsocket)) == -1) {
				syslog(LOG_DEBUG, "ga_nnet_cli: %s:%s:%s %s", ss[j].host ? ss[j].host : "(null)", ss[j].port ? ss[j].port : "(null)", ss[j].localsocket ? ss[j].localsocket : "(null)", strerror(errno));
				return NULL;
			}
#if defined TCP_NODELAY
			(void)setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof one);
#endif
			if (xgserv_init(&xs->n[j], s, NULL, NULL) == -1) {
				syslog(LOG_DEBUG, "xgserv_init %d failed: %s", s, strerror(errno));
				return NULL;
			}
			if (nrpc_call_1(&xs->n[j], NRPC_DISTCONF, &distconf) == -1) {
				syslog(LOG_DEBUG, "nrpc_call failed");
				return NULL;
			}
		}
	}
	assert(j == xs->ns);
	return xs;
}

int
xgwam_disconnect(xs)
struct xs_t *xs;
{
	size_t i;
	int e = 0;

	if (nrpc_call_mux(xs->n, xs->ns, NRPC_EXIT, &e) == -1) {
		syslog(LOG_DEBUG, "nrpc_call failed");
		return -1;
	}
	for (i = 0; i < xs->ns; i++) {
		nrpc_clear(&xs->n[i]);
	}
	bzero(xs->n, xs->ns * sizeof *xs->n);
	free(xs->n);
	bzero(xs, sizeof *xs);
	free(xs);
	return 0;
}

int
xwamupdate_init(xs, g)
struct xs_t *xs;
struct wam_update_args *g;
{
	int d;
	size_t j;
	char const *save = g->handle;

	assert(!g->dist.n);
	assert(!g->dist.nn);

	g->handle = handle_base(g->handle, NULL);
	for (d = j = 0; d < 2; d++) {
		size_t i;
		for (i = 0; i < xs->nn[d]; i++, j++) {
			if (nrpc_call_1(&xs->n[j], NRPC_WAM_UPDATE, g) == -1) {
				syslog(LOG_DEBUG, "nrpc_call failed");
				return -1;
			}
#if defined BULKSZ
			syslog(LOG_DEBUG, "nrpc_setbulk(%"PRIuSIZE_T"): %"PRIuSIZE_T, (pri_size_t)j, (pri_size_t)BULKSZ);
			if (nrpc_setbulk(&xs->n[j], BULKSZ) == -1) {
				syslog(LOG_DEBUG, "xwamupdate_init: nrpc_setbulk failed");
				return -1;
			}
#endif
		}
	}
	g->handle = save;
	assert(j == xs->ns);
	return 0;
}

int
xwamupdate_push_rawvec(xs, r)
struct xs_t *xs;
struct raw_vec *r;
{
	if (nrpc_call_mux(xs->n, xs->ns, NRPC_RAWVEC, r) == -1) {
		syslog(LOG_DEBUG, "nrpc_call failed");
		return -1;
	}
	return 0;
}

int
xwamupdate_rawvec_end(xs)
struct xs_t *xs;
{
#if defined BULKSZ
	size_t i;
	for (i = 0; i < xs->ns; i++) {
		syslog(LOG_DEBUG, "nrpc_setbulk(%"PRIuSIZE_T"): 0", (pri_size_t)i);
		if (nrpc_setbulk(&xs->n[i], 0) == -1) {
			syslog(LOG_DEBUG, "xwamupdate_rawvec_end: nrpc_setbulk failed");
			return -1;
		}
	}
#endif
	if (nrpc_call_mux(xs->n, xs->ns, NRPC_RAWVEC_END, NULL) == -1) {
		syslog(LOG_DEBUG, "xwamupdate_rawvec_end: nrpc_call failed");
		return -1;
	}
	return 0;
}

int
xwamupdate_end(xs)
struct xs_t *xs;
{
	size_t i;

	if (nrpc_call_mux(xs->n, xs->ns, NRPC_WAM_UPDATE_END, NULL) == -1) {
		syslog(LOG_DEBUG, "xwamupdate_end: nrpc_call failed");
		return -1;
	}
	for (i = 0; i < xs->ns; i++) {
/*
		fprintf(stderr, "%"PRIuSIZE_T":%p ", (pri_size_t)i, xs->n[i].recvbuf.pkt);
*/
	}
	return 0;
}

static struct s_syminfo *
pack_syminfo(q, n)
struct syminfo *q;
df_t n;
{
	df_t i;
	struct s_syminfo *s;
	if (!(s = calloc(n, sizeof *s))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}
	for (i = 0; i < n; i++) {
		s[i] = *(struct s_syminfo *)&q[i];
	}
	return s;
}

static struct syminfo *
unpack_syminfo(s, n)
struct s_syminfo *s;
df_t n;
{
	df_t i;
	struct syminfo *q;
	if (!(q = calloc(n, sizeof *q))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}
	for (i = 0; i < n; i++) {
		*(struct s_syminfo *)&q[i] = s[i];
	}
	return q;
}

#if defined DBG

static void *
iov_flatten(v, iovcnt)
struct iovec *v;
{
	size_t size;
	int i;
	void *p;

	for (size = 0, i = 0; i < iovcnt; i++) {
		size += v[i].iov_len;
	}
	if (!(p = malloc(size))) {
		syslog(LOG_DEBUG, "malloc: %s", strerror(errno));
		return NULL;
	}
	for (size = 0, i = 0; i < iovcnt; i++) {
		memcpy((char *)p + size, v[i].iov_base, v[i].iov_len);
		size += v[i].iov_len;
	}
	return p;
}

static void
log_wam_update_args(a)
struct wam_update_args *a;
{
	size_t i;
	syslog(LOG_DEBUG, "wam_update_args:");
	syslog(LOG_DEBUG, "command: %d", a->command);
	syslog(LOG_DEBUG, "handle: %s", a->handle);
	syslog(LOG_DEBUG, "tmpdir: %s", a->tmpdir);
	if (a->command == WAM_UPDATE_FROM_ITB) {
		for (i = 0; a->i.itbs[i]; i++) {
			syslog(LOG_DEBUG, "itbs[%"PRIuSIZE_T"]: %s", (pri_size_t)i, a->i.itbs[i]);
		}
		syslog(LOG_DEBUG, "stemmer: %s", a->i.s.stemmer);
		for (i = 0; a->i.keys[i]; i++) {
			syslog(LOG_DEBUG, "keys[%"PRIuSIZE_T"]: %s", (pri_size_t)i, a->i.keys[i]);
		}
		syslog(LOG_DEBUG, "stem_a_no: %d", a->i.s.stem_a_no);
		syslog(LOG_DEBUG, "stem_n_no: %d", a->i.s.stem_n_no);
		syslog(LOG_DEBUG, "mkri_ncpu: %d", a->i.mkri_ncpu);
	}
	if (a->command == WAM_UPDATE_DELIDL) {
		for (i = 0; i < a->d.nid; i++) {
			syslog(LOG_DEBUG, "id[%"PRIuSIZE_T"]: %"PRIuIDX_T, (pri_size_t)i, a->d.id[i]);
		}
		syslog(LOG_DEBUG, "nid: %"PRIuSIZE_T, (pri_size_t)a->d.nid);
		for (i = 0; i < a->d.nid; i++) {
			syslog(LOG_DEBUG, "vecs[%"PRIuSIZE_T"]: %p", (pri_size_t)i, a->d.vecs[i]);
		}
	}

	syslog(LOG_DEBUG, "dist.c.role: %d", a->dist.c.role);
	syslog(LOG_DEBUG, "dist.c.nn[0]: %"PRIdDF_T, a->dist.c.nn[0]);
	syslog(LOG_DEBUG, "dist.c.nn[1]: %"PRIdDF_T, a->dist.c.nn[1]);
	syslog(LOG_DEBUG, "dist.n: %p", a->dist.n);
	syslog(LOG_DEBUG, "dist.nn: %p", a->dist.nn);
	syslog(LOG_DEBUG, "dist.c.dir: %d", a->dist.c.dir);
	syslog(LOG_DEBUG, "dist.c.node_id: %"PRIdDF_T, a->dist.c.node_id);
}

static void
log_raw_vec(r)
struct raw_vec *r;
{
	size_t i;

	syslog(LOG_DEBUG, "elems_size: %"PRIuSIZE_T, (pri_size_t)r->st.elems_size);
	syslog(LOG_DEBUG, "name_size: %"PRIuSIZE_T, (pri_size_t)r->st.name_size);

	syslog(LOG_DEBUG, "name: %s", r->name);
#if defined USEIDFORIF
	syslog(LOG_DEBUG, "id: %"PRIuIDX_T, r->id);
#endif
	syslog(LOG_DEBUG, "elems: %p", r->elems);
	syslog(LOG_DEBUG, "elem_num: %"PRIdDF_T, r->elem_num);
	for (i = 0; i < r->elem_num; i++) {
		struct raw_elem *e = &r->elems[i];
		syslog(LOG_DEBUG, "e[%"PRIuSIZE_T"].name: %s", (pri_size_t)i, e->name);
#if defined USEIDFORIF
		syslog(LOG_DEBUG, "e[%"PRIuSIZE_T"].id: %"PRIuIDX_T, (pri_size_t)i, e->id);
#endif
		syslog(LOG_DEBUG, "e[%"PRIuSIZE_T"].name_size: %"PRIuSIZE_T, (pri_size_t)i, (pri_size_t)e->name_size);
		syslog(LOG_DEBUG, "e[%"PRIuSIZE_T"].tf: %"PRIdFREQ_T, (pri_size_t)i, e->tf);
	}
	syslog(LOG_DEBUG, "props: %p", r->props);
	syslog(LOG_DEBUG, "nprops: %"PRIuSIZE_T, (pri_size_t)r->nprops);
	for (i = 0; i < r->nprops; i++) {
		struct raw_prop *e = &r->props[i];
		syslog(LOG_DEBUG, "p[%"PRIuSIZE_T"].key: %s", (pri_size_t)i, e->key);
		syslog(LOG_DEBUG, "p[%"PRIuSIZE_T"].value_size: %"PRIuSIZE_T, (pri_size_t)i, (pri_size_t)e->value_size);
		syslog(LOG_DEBUG, "p[%"PRIuSIZE_T"].value: %p (%s)", (pri_size_t)i, e->value, e->value ? e->value : "");
	}
	syslog(LOG_DEBUG, "nsegms: %"PRIuIDX_T, r->nsegms);
	for (i = 0; i < r->nsegms; i++) {
		struct raw_prop *e = &r->segms[i];
		syslog(LOG_DEBUG, "s[%"PRIuSIZE_T"].value_size: %"PRIuSIZE_T, (pri_size_t)i, (pri_size_t)e->value_size);
		syslog(LOG_DEBUG, "s[%"PRIuSIZE_T"].value: %p (%s)", (pri_size_t)i, e->value, e->value ? e->value : "");
	}
}

static void
print_assv_args(a)
struct assv_args_t *a;
{

	syslog(LOG_DEBUG, "a->q = %p", a->q);
	syslog(LOG_DEBUG, "a->nq = %"PRIdDF_T, a->nq);
	syslog(LOG_DEBUG, "a->nd = %"PRIdDF_T, a->nd);
	syslog(LOG_DEBUG, "a->qdir = %d", a->qdir);
	syslog(LOG_DEBUG, "a->type = %d", a->type);
	syslog(LOG_DEBUG, "a->scookie = %p", a->scookie);
}

static void
print_assv_result(i, r)
size_t i;
struct assv_result_t *r;
{
	char s[2048];
	syslog(LOG_DEBUG, "%"PRIuSIZE_T": r->r = %p", (pri_size_t)i, r->r);
	syslog(LOG_DEBUG, "%"PRIuSIZE_T": ", (pri_size_t)i);
	snprint_s_syminfo(r->r, r->nd, r->total, 12, NULL, 0, s, sizeof s);
	syslog(LOG_DEBUG, "%s", s);
	syslog(LOG_DEBUG, "%"PRIuSIZE_T": r->nd = %"PRIdDF_T, (pri_size_t)i, r->nd);
	syslog(LOG_DEBUG, "%"PRIuSIZE_T": r->total = %"PRIdDF_T, (pri_size_t)i, r->total);
}

static void
print_xfss_args(a)
struct xfss_args_t *a;
{
	size_t i;

	syslog(LOG_DEBUG, "a->np = %"PRIdDF_T, a->np);
	syslog(LOG_DEBUG, "a->nn = %"PRIdDF_T, a->nn);
	syslog(LOG_DEBUG, "a->query = %p", a->query);
	syslog(LOG_DEBUG, "a->query->nq = %"PRIdDF_T, a->query->nq);

	for (i = 0; i < a->query->nq; i++) {
		size_t j;
		syslog(LOG_DEBUG, "a->query->query[%"PRIuSIZE_T"].n = %"PRIdDF_T, (pri_size_t)i, a->query->query[i].n);
		for (j = 0; j < a->query->query[i].n; j++) {
			syslog(LOG_DEBUG, "a->query->query[%d].pattern = %s", (int)j, a->query->query[i].q[j].pattern);
			assert(a->query->query[i].q[j].arts == NULL);
		}
	}
}

static void
print_xfss_result(r)
struct xfss_result_t *r;
{
	size_t i;
	syslog(LOG_DEBUG, "r->rp.narts = %"PRIdDF_T, r->rp.narts);
	syslog(LOG_DEBUG, "r->rp.total = %"PRIdDF_T, r->rp.total);
	syslog(LOG_DEBUG, "r->rp.arts = %p", r->rp.arts);
	for (i = 0; i < r->rp.narts; i++) {
		if (i >= 4) { syslog(LOG_DEBUG, "..."); break; }
		syslog(LOG_DEBUG, "r->rp.arts[%"PRIuSIZE_T"].id = %"PRIuIDX_T, (pri_size_t)i, r->rp.arts[i].id);
	}
	syslog(LOG_DEBUG, "r->rn.narts = %"PRIdDF_T, r->rn.narts);
	syslog(LOG_DEBUG, "r->rn.total = %"PRIdDF_T, r->rn.total);
	syslog(LOG_DEBUG, "r->rn.arts = %p", r->rn.arts);
}

#endif

static char *
handle_base(handle, rootdir)
char const *handle, *rootdir;
{
	char *p;
	if (p = rindex(handle, '/')) {
		return p + 1;
	}
	return handle;
}

/*
 * n: number of result
 * p: ratio of population of objects of the host: useally 1/l
 * b: 1/b == average error rate to achieve 
 * l: number of partitions
 * RETURN VALUE: number of request
 */
static size_t
safe_request_num(n, p, b, l)
size_t n, l;
double p;
{
	double a, q, s, t, r;
	size_t k;

	a = 1 - 1.0 / b;
	r = 1.0 / b;
	q = 1 - p;
	s = t = pow(q, n);
	if (s == 0) {
		return n;
	}
	for (k = 0; k < n && 1 - s > r / l; k++) {
		t *= (n - k) * p / ((k + 1) * q);
		s += t;
	}
	return k;
}
