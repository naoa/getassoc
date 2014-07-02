/*	$Id: nwam.c,v 1.179 2011/09/14 03:06:09 nis Exp $	*/

/*-
 * Copyright (c) 2008 Shingo Nishioka.
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
static char rcsid[] = "$Id: nwam.c,v 1.179 2011/09/14 03:06:09 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <unistd.h>

#include <unicode/utf.h>
#include <unicode/utf8.h>

#include "nwam.h"
#include "util.h"
#include "pi.h"

#include "priq.h"
#include "xstem.h"

#include "common.h"
#include "print.h"

#include "fssP.h"
#include "nwamP.h"
#include "xgserv.h"

#include <gifnoc.h>

static void nw_setbase(WAM *, char const *, char const *);
static int wam_drop_l(char const *, char const *, int);
static int wam_drop_0(WAM *);
static int wam_rename_0(WAM *, char const *, char const *);
static WAM *wam_open_rw(char const *, char const *, mode_t, struct distconf_t *, int, char const **, int);
static WAM *wam_prop_open_rw(char const *, int, char const *, char const *, mode_t, int);
static int wam_prop_drop(WAM *, int, char const *);
static WAM *wam_fss_open_rw(char const *, char const *, mode_t, int);
static int wam_fss_drop(WAM *);
static int wam_unlock_local(WAM *);
static int fixate_cw(WAM *, int);
static int fixate_xr(WAM *, int);
static void sigint(int);
#if defined SIGPIPE
static void sigpipe(int);
#endif
static void emergency_cleanup(void);
static void cleanup_tmp(void);
static char *check_keys(char const **);
static int wam_delidl_fill_v(struct rvst *, struct wam_update_args *);
static void wam_delidl_cleanup(struct rvst *, struct wam_update_args *);
static int wam_delidl_v(struct rvst *, struct wam_update_args *);
static int del_vec(idx_t, struct xr_vec const *, struct rvst *);
static int pass3(struct rvst *, struct distconf_t *);
static int p3_eject_vec(idx_t, df_t, freq_t, struct rvst *, struct distconf_t *);
static int minep(idx_t, struct distconf_t *);
static int load_ci(WAM *, struct corpus_i *);
static int sync_ci(WAM *, struct corpus_i *);
static void free_ci(struct corpus_i *);
static int add_ci(struct corpus_i *, char *, int);
static int add_ci_pair(struct corpus_i *, char const *, char const *, int);
static int extend_cprop(struct corpus_i *);
static char *lookup_ci(struct corpus_i *, char const *);
static int sync_ns_nw(WAM *);
static int load_ns_nw(WAM *);
static int sync_ns_pr(WAM *);
static int load_ns_pr(WAM *);
static int sync_ns_fs(WAM *);
static int load_ns_fs(WAM *);
static int dump_cw(NWDB *, FILE *);
static int dump_cr(NWDB *, FILE *);
static int dump_xr(NWDB *, int, WAM *, FILE *);
static int dump_pr(NWDB *, int, WAM *, FILE *);
static int p1_eject_vec(struct raw_vec *, struct corpus_i *, struct rvst *);
static idx_t insert_cw(char *, df_t, freq_t, NWDB *, idx_t *, NWDB *, struct rvst *);
static int dec_cw(idx_t, df_t, freq_t, int, struct rvst *);
static int delete_cw(idx_t, char *, NWDB *, NWDB *);
static int insert_xr(idx_t, struct xr_vec *, NWDB *);
static int retrieve_xr(idx_t, struct xr_vec **, NWDB *);
static int delete_xr(idx_t, NWDB *);
static int delete_pr(idx_t, NWDB *);
static int regenerate_max(struct rvst *);
static int p2_eject_vec(struct raw_vec *, struct rvst *, struct distconf_t *);
static int p2_eject_pair(struct rc_pair *, struct rvst *);
static struct raw_vec *new_raw_vec(char const **);
static int read_raw_vec(struct raw_vec *, struct rvst *
#if defined ENABLE_RAWVECLEN_LIMIT
	, df_t
#endif
);
static int itb_prochdr(struct raw_vec *, struct rvst *, char *);
static int itb_procline(struct raw_vec *, struct rvst *, char *);
static char *rs_readln(struct rvst *);
static int rs_unread(struct rvst *);
static void sort_uniq_rvec(struct raw_vec *);
static void chop(char *);
static void free_raw_vec(struct raw_vec *);
static void setint32_t(int32_t, char [INT32_TWID]);
static void setdf_t(df_t, char [DF_TWID]);
static void setidx_t(idx_t, char [IDX_TWID]);
static void setfreq_t(freq_t, char [FREQ_TWID]);
static int icompar(const void *, const void *);
static int pcompar(const void *, const void *);
static int rcompar(const void *, const void *);
static int ecompar(const void *, const void *);
static int xcompar(const void *, const void *);
static int extend_elems(struct raw_vec *);
static int xstrdup(char **, size_t *, char const *);
static int xappend(char **, size_t *, char const *);
static int setup_pstem(struct stm_d *, struct wam_update_args *, int);
static int start_stmd(struct stm_d *, struct stm_x *, int, char **);
static int start_reader(struct stm_d *, char const *, char const **);
static void *start_reader0(void *);
static int relay_aa(struct stm_d *, char *);
static int relay_nn(struct stm_d *, char *);
static char *ss_readln(struct stm_d_s *);
static int cleanup_pstem(struct stm_d *);
static int proc_body(struct raw_vec *, struct rvst *, char *);
static int proc_body_eject(struct raw_vec *, char *);
static int proc_flwd(struct raw_vec *, struct rvst *, char *);
static int sort_pair_file(char const *);
static int next_pair(struct pair_stream *);
static int insert_pr(idx_t, struct raw_prop *, NWDB *);
static int merge_vec(struct rvst *, struct xr_vec const *);
static void uniq_vec(struct rvst *);
static int setmask0(struct mask_t *, idx_t *, df_t);
static void mask_vec(struct xr_vec *, idx_t const *, df_t, idx_t const *, df_t);
static struct syminfo *wsh_assv(struct syminfo const *, df_t, WAM *, int, int, df_t *, df_t *, struct assv_scookie *);
static char const *basnam(char const *);
#if ! defined NO_F_SETLKW
static int setlkw(int, char const *);
static int unsetlk(int);
#endif
static void fdcloseexec(void);
static int verify_utf8_string(char const *, size_t *);
#if defined FLWD_UTF32
static ssize_t fwputs(char const *, FILE *);
#endif
static int register_handle(WAM *, WAM *, char const *);
static int unregister_handle(WAM *, WAM *);
static void close_registered_handles(WAM *);
static int reserved_prop(char const *);
static struct servlist_t *get_server_list(WAM *, char const **);
static char *encode_servers(struct servlist_t *);
static struct servlist_t *decode_servers(char const *);
static char *strldup(char const *, size_t);

#define	BOM	"\357\273\277"

#define \
compar_ycompar(e, a, b, q) { \
	struct pair_stream **va; \
	struct pair_stream **vb; \
	va = a; \
	vb = b; \
	if ((*va)->p.c_id < (*vb)->p.c_id) { \
		e = -1; \
	} \
	else if ((*va)->p.c_id > (*vb)->p.c_id) { \
		e = 1; \
	} \
	else if ((*va)->p.r_id < (*vb)->p.r_id) { \
		e = -1; \
	} \
	else if ((*va)->p.r_id > (*vb)->p.r_id) { \
		e = 1; \
	} \
	else { \
		e = 0; \
	} \
}

static priq_prop_t prop_f_ycompar;
static priq_prop_t prop_b_ycompar;

static PROP_F_TYPED(prop_f_ycompar, compar_ycompar, struct pair_stream **)
static PROP_B_TYPED(prop_b_ycompar, compar_ycompar, struct pair_stream **)

static char const *getaroot = NULL;

static int need_cleanup = 1;

int
wam_init(rootdir)
char const *rootdir;
{
	BEGIN();
	getaroot = rootdir;
	return 0;
}

static void
nw_setbase(nw, handle, rootdir)
WAM *nw;
char const *handle, *rootdir;
{
	if (rootdir) nw->rootdir = strdup(rootdir);
	else nw->rootdir = NULL;
	strncpy(nw->handle, handle, sizeof nw->handle);
	if (rootdir) {
		snprintf(nw->base, sizeof nw->base, "%s" DIRECTORY_DELIMITER NWAMDIR DIRECTORY_DELIMITER "%s", rootdir, nw->handle);
	}
	else {
		snprintf(nw->base, sizeof nw->base, "%s", nw->handle);
	}
	snprintf(nw->lock.path, sizeof nw->lock.path, "%s" DIRECTORY_DELIMITER LOCKKEY, nw->base);
}

int
wam_drop_dc(handle, rootdir, dc)
char const *handle, *rootdir;
struct distconf_t *dc;
{
	WAM *w;
	if (!(w = wam_open_rw(handle, rootdir, O_RDWR, dc, 1, NULL, 1))) {
		syslog(LOG_DEBUG, "wam_drop_dc: wam_open_rw failed: %s", handle);
		return -1;
	}

	return wam_drop_0(w);
}

int
wam_drop(char const *handle, ...)
{
	char const *rootdir;

	if (!(rootdir = getaroot)) {
		va_list ap;
		va_start(ap, handle);
		rootdir = va_arg(ap, char *);
		va_end(ap);
	}

	return wam_drop_l(handle, rootdir, 0);
}

static int
wam_drop_l(handle, rootdir, no_lock)
char const *handle, *rootdir;
{
	struct distconf_t dc;
	WAM *w;

	bzero(&dc, sizeof dc);
	dc.role = NDWAM_INHERIT;

	if (!(w = wam_open_rw(handle, rootdir, O_RDWR, &dc, 1, NULL, no_lock))) {
		syslog(LOG_DEBUG, "wam_drop: wam_open_rw failed: %s", handle);
		return -1;
	}

	if (w->u.w.dist.role == NDWAM_ROLE_PARENT) {
		if (cb_wam_dropall(w->u.w.xs, handle, w->rootdir) == -1) {
			syslog(LOG_DEBUG, "wam_drop: cb_wam_dropall failed");
			return -1;
		}
	}

	return wam_drop_0(w);
}

static int
wam_drop_0(w)
WAM *w;
{
	int d;
	char *ks;
	char const **kk;
	size_t nkk, nkk0;
	size_t i;
	int e = 0;

	if (!(ks = lookup_ci(&w->u.w.ci, "properties"))) {
		syslog(LOG_DEBUG, "`properties' not found in ci. why?");
		return -1;
	}
	nkk0 = count_char(ks, ',') + 2;
	if (!(kk = calloc(nkk0, sizeof *kk))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return -1;
	}
	nkk = splitv(ks, ',', kk, nkk0 - 1, 1);

	for (i = 0; i < nkk; i++) {
		if (!strcmp(kk[i], FSSKEY)) {
			wam_fss_drop(w);
			wam_prop_drop(w, WAM_ROW, RMAPKEY);
		}
		else {
			if (wam_prop_drop(w, WAM_ROW, kk[i]) == -1) {
				syslog(LOG_DEBUG, "wam_prop_drop failed");
				return -1;
			}
		}
	}

	for (d = 0; d < 2; d++) {
		w->u.w.d[d].cw = NULL;
		w->u.w.d[d].cr = NULL;
		w->u.w.d[d].xr = NULL;
		if (nwdb_drop(w->u.w.d[d].cwp, NWDB_NCW) == -1) {
			syslog(LOG_DEBUG, "warning: ncw: %s", strerror(errno));
			e = -1;
		}
		if (nwdb_drop(w->u.w.d[d].crp, NWDB_NCR) == -1) {
			syslog(LOG_DEBUG, "warning: crp: %s", strerror(errno));
			e = -1;
		}
		if (nwdb_drop(w->u.w.d[d].xrp, NWDB_NXR) == -1) {
			syslog(LOG_DEBUG, "warning: xrp: %s", strerror(errno));
			e = -1;
		}
	}

	if (unlink(w->u.w.inf) == -1) {
		syslog(LOG_DEBUG, "warning: %s: %s", w->u.w.inf, strerror(errno));
		e = -1;
	}
	if (unlink(w->sche) == -1) {
		syslog(LOG_DEBUG, "warning: %s: %s", w->sche, strerror(errno));
		e = -1;
	}

	if (unlink(w->lock.path) == -1) {
		syslog(LOG_DEBUG, "warning: %s: %s", w->lock.path, strerror(errno));
		e = -1;
	}
	if (rmdir(w->base) == -1) {
		syslog(LOG_DEBUG, "warning: %s: %s", w->base, strerror(errno));
		e = -1;
	}
	wam_close(w);
	free(kk);
	free(ks);
	return e;
}

int
wam_rename_dc(from, to, rootdir, dc)
char const *from, *to, *rootdir;
struct distconf_t *dc;
{
	WAM *w;
	if (!(w = wam_open_rw(from, rootdir, O_RDWR, dc, 1, NULL, 1))) {
		return -1;
	}

	return wam_rename_0(w, to, rootdir);
}

int
wam_rename(char const *from, char const *to, ...)
{
	char const *rootdir;
	struct distconf_t dc;
	WAM *w;

	if (!(rootdir = getaroot)) {
		va_list ap;
		va_start(ap, to);
		rootdir = va_arg(ap, char *);
		va_end(ap);
	}

	bzero(&dc, sizeof dc);
	dc.role = NDWAM_INHERIT;

	if (!(w = wam_open_rw(from, rootdir, O_RDWR, &dc, 1, NULL, 0))) {
		return -1;
	}

	if (w->u.w.dist.role == NDWAM_ROLE_PARENT) {
		if (cb_wam_renameall(w->u.w.xs, from, to, w->rootdir) == -1) {
			return -1;
		}
	}

	return wam_rename_0(w, to, rootdir);
}

static int
wam_rename_0(w, to, rootdir)
WAM *w;
char const *to, *rootdir;
{
	WAM nw;
	int e = 0;

	nw_setbase(&nw, to, rootdir);
	wam_unlock_local(w);
	if (rename(w->base, nw.base) == -1) {
		syslog(LOG_DEBUG, "rename %s %s failed", w->base, nw.base);
		e = -1;
	}
	wam_close(w);
	free(nw.rootdir);
	return e;
}

WAM *
wam_open_dc(handle, rootdir, dc)
char const *handle, *rootdir;
struct distconf_t *dc;
{
	return wam_open_rw(handle, rootdir, O_RDONLY, dc, 0, NULL, 1);
}

WAM *
wam_open(char const *handle, ...)
{
	char const *rootdir;
	struct distconf_t dc;
	WAM *w;

	if (!(rootdir = getaroot)) {
		va_list ap;
		va_start(ap, handle);
		rootdir = va_arg(ap, char *);
		va_end(ap);
	}

	bzero(&dc, sizeof dc);
	dc.role = NDWAM_INHERIT;

	if (!(w = wam_open_rw(handle, rootdir, O_RDONLY, &dc, 0, NULL, 0))) {
		return NULL;
	}

	if (w->u.w.dist.role == NDWAM_ROLE_PARENT) {
		if (cb_wam_openall(w->u.w.xs, handle, w->rootdir) == -1) {
			return NULL;
		}
	}

	return w;
}

static WAM *
wam_open_rw(handle, rootdir, flags, dc, nodbm, server_conf_paths, no_lock)
char const *handle, *rootdir;
mode_t flags;
struct distconf_t *dc;
char const **server_conf_paths;
{
	struct stat sb;
	WAM *nw = NULL;
	int d;

	if (!(nw = calloc(1, sizeof *nw))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		goto error;
	}
	for (d = 0; d < 2; d++) {
		nw->u.w.d[d].cw = nw->u.w.d[d].cr = nw->u.w.d[d].xr = NULL;
	}

	nw->lock.d = -1;
	nw->type = NWAM_D_TYPE_WAM;
	nw_setbase(nw, handle, rootdir);
	snprintf(nw->sche, sizeof nw->sche, "%s" DIRECTORY_DELIMITER SCHEKEY_NW, nw->base);
	snprintf(nw->u.w.inf, sizeof nw->u.w.inf, "%s" DIRECTORY_DELIMITER INFKEY, nw->base);

	if (stat(nw->base, &sb) == -1) {
		if (!(flags & O_CREAT) || mkdir(nw->base, 0777) == -1) {
			syslog(LOG_DEBUG, "wam_open_rw: %s: %s", nw->base, strerror(errno));
			goto error;
		}
	}

	nw->lock.d = -1;
	if (!no_lock) {
		if ((nw->lock.d = setlkw(flags == O_RDONLY ? F_RDLCK : F_WRLCK, nw->lock.path)) == -1) {
			syslog(LOG_DEBUG, "%s: %s", nw->lock.path, strerror(errno));
			goto error;
		}
	}

	nw->u.w.total_elem_num = nw->u.w.total_freq_sum = 0;
	nw->u.w.tv.ptr = NULL;
	nw->u.w.tv.size = 0;
	for (d = 0; d < 2; d++) {
		nw->u.w.d[d].nentries = nw->u.w.d[d].max_elem_num = 0;
		nw->u.w.d[d].max_freq_sum = 0;
		nw->u.w.d[d].maxid = 0;
		nw->u.w.d[d].compress_mode = 0;
		snprintf(nw->u.w.d[d].cwp, sizeof nw->u.w.d[d].cwp, "%s" DIRECTORY_DELIMITER "ncw%c.db", nw->base, "rc"[d]);
		snprintf(nw->u.w.d[d].crp, sizeof nw->u.w.d[d].crp, "%s" DIRECTORY_DELIMITER "ncr%c.db", nw->base, "rc"[d]);
		snprintf(nw->u.w.d[d].xrp, sizeof nw->u.w.d[d].xrp, "%s" DIRECTORY_DELIMITER "nxr%c.db", nw->base, "rc"[d]);
		nw->u.w.d[d].cw = NULL;
		nw->u.w.d[d].cr = NULL;
		nw->u.w.d[d].xr = NULL;
		if (!nodbm) {
			if (!(nw->u.w.d[d].cw = nwdb_open(nw->u.w.d[d].cwp, NWDB_NCW, flags))) {
				syslog(LOG_DEBUG, "%s: %s", nw->u.w.d[d].cwp, strerror(errno));
				goto error;
			}
			if (!(nw->u.w.d[d].cr = nwdb_open(nw->u.w.d[d].crp, NWDB_NCR, flags))) {
				syslog(LOG_DEBUG, "%s: %s", nw->u.w.d[d].crp, strerror(errno));
				goto error;
			}
			if (!(nw->u.w.d[d].xr = nwdb_open(nw->u.w.d[d].xrp, NWDB_NXR, flags))) {
				syslog(LOG_DEBUG, "%s: %s", nw->u.w.d[d].xrp, strerror(errno));
				goto error;
			}
		}
		nw->u.w.d[d].pmask.buf = nw->u.w.d[d].nmask.buf = NULL;
		nw->u.w.d[d].pmask.size = nw->u.w.d[d].nmask.size = 0;
		nw->u.w.d[d].pmask.mask = nw->u.w.d[d].nmask.mask = NULL;
		nw->u.w.d[d].pmask.len = nw->u.w.d[d].nmask.len = 0;
		nw->u.w.d[d].mask_set = 0;
	}
	nw->u.w.dist.role = 0;	/* don't care */
	nw->u.w.dist.dir = 0;	/* don't care */
	nw->u.w.dist.node_id = 0;	/* don't care */
	nw->u.w.dist.nn[0] = 0;	/* don't care */
	nw->u.w.dist.nn[1] = 0;	/* don't care */

	if (access(nw->sche, flags == O_RDONLY ? R_OK : R_OK|W_OK) != -1) {
		if (load_ns_nw(nw) == -1) {
			syslog(LOG_DEBUG, "load_ns_nw failed: %s", nw->sche);
			goto error;
		}
		if (dc->role == NDWAM_INHERIT) {
			dc->role = nw->u.w.dist.role;
			dc->dir = nw->u.w.dist.dir;
			dc->node_id = nw->u.w.dist.node_id;
			dc->nn[0] = nw->u.w.dist.nn[0];
			dc->nn[1] = nw->u.w.dist.nn[1];
		}
		else if (nw->u.w.dist.role != dc->role) {
			syslog(LOG_DEBUG, "wam_open_rw: role changed: %d -> %d", nw->u.w.dist.role, dc->role);
			goto error;
		}
		else if (dc->role != NDWAM_MONOLITHIC &&
			(nw->u.w.dist.dir != dc->dir ||
			 nw->u.w.dist.node_id != dc->node_id ||
			 nw->u.w.dist.nn[0] != dc->nn[0] ||
			 nw->u.w.dist.nn[1] != dc->nn[1])) {
			syslog(LOG_DEBUG, "wam_open_rw: some dist parameter changed");
			goto error;
		}
	}
	else if (flags == O_RDONLY) {
		syslog(LOG_DEBUG, "load_ns_nw failed: %s", nw->sche);
		goto error;
	}
	else {
		if (dc->role == NDWAM_INHERIT) {
			dc->role = NDWAM_MONOLITHIC;
		}
		nw->u.w.dist.role = dc->role;
		nw->u.w.dist.dir = dc->dir;
		nw->u.w.dist.node_id = dc->node_id;
		nw->u.w.dist.nn[0] = dc->nn[0];
		nw->u.w.dist.nn[1] = dc->nn[1];
		if (sync_ns_nw(nw) == -1) {
			syslog(LOG_DEBUG, "sync_ns_nw failed (1)");
			goto error;
		}
	}

	if (load_ci(nw, &nw->u.w.ci) == -1) {
		syslog(LOG_DEBUG, "load_ci failed");
		goto error;
	}

	if (nw->u.w.dist.role == NDWAM_ROLE_PARENT) {
		struct servlist_t *ss;
		size_t i;
		if (server_conf_paths) {
			syslog(LOG_DEBUG, "reading server conf: %s, %s", server_conf_paths[0], server_conf_paths[1]);
		}
		if (!(ss = get_server_list(nw, server_conf_paths))) {
			syslog(LOG_DEBUG, "get_server_list failed");
			goto error;
		}
		if (!(nw->u.w.xs = xgwam_connect(&nw->u.w.dist, ss))) {
			syslog(LOG_DEBUG, "xgwam_connect failed");
			goto error;
		}
		for (i = 0; ss[i].host || ss[i].localsocket; i++) {
			free(ss[i].host);
			free(ss[i].port);
			free(ss[i].localsocket);
		}
		free(ss);
	}
	else {
		nw->u.w.xs = NULL;
	}
	nw->u.w.slave = NULL;
	nw->u.w.slave_size = 0;
#if defined ALLOW_ERROR
	nw->u.w.allowerror = ALLOW_ERROR;
#else
	nw->u.w.allowerror = 0;
#endif
	nw->u.w.may_incomplete = 0;
	nw->u.w.need_diststat = 0;
	nw->u.w.diststat = NULL;
	return nw;

error:
	if (nw) {
		for (d = 0; d < 2; d++) {
			free(nw->u.w.d[d].cw);
			free(nw->u.w.d[d].cr);
			free(nw->u.w.d[d].xr);
		}
		/* no need to free nw->u.w.tv.ptr */
		if (nw->lock.d != -1) {
			unsetlk(nw->lock.d);
			close(nw->lock.d);
		}
	}
	free(nw);
	return NULL;
}

WAM *
wam_prop_open(master, d, key)
WAM *master;
char const *key;
{
	WAM *pr;

	if (!master) {
		return NULL;
	}
	if (d == WAM_COL) {
		return NULL;
	}
	if (pr = get_registered_handle(master, key)) {
		pr->u.p.open_count++;
		return pr;
	}
	if (!(pr = wam_prop_open_rw(master->handle, d, key, master->rootdir, O_RDONLY, 0))) {
		return NULL;
	}

	pr->u.p.master = master;
	register_handle(master, pr, key);

	if (master->u.w.dist.role == NDWAM_ROLE_PARENT) {
		if (cb_wam_prop_openall(master->u.w.xs, d, key) == -1) {
			return NULL;
		}
	}

	return pr;
}

static WAM *
wam_prop_open_rw(handle, d, key, rootdir, flags, nodbm)
char const *handle, *key, *rootdir;
mode_t flags;
{
	WAM *pr;

	if (!(pr = calloc(1, sizeof *pr))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}

	pr->u.p.master = NULL;
	pr->u.p.open_count = 1;
	pr->lock.d = -1;
	pr->type = NWAM_D_TYPE_PROP_FILE;
	if (!(pr->u.p.key = strdup(key))) {
		return NULL;
	}

	nw_setbase(pr, handle, rootdir);
	snprintf(pr->sche, sizeof pr->sche, "%s" DIRECTORY_DELIMITER "nprr.%s" SCHE_EXT_PR, pr->base, key);
	snprintf(pr->u.p.prp, sizeof pr->u.p.prp, "%s" DIRECTORY_DELIMITER "nprr.%s.db", pr->base, key);

	if (flags == O_RDONLY) {
		if ((pr->lock.d = setlkw(F_RDLCK, pr->lock.path)) == -1) {
			syslog(LOG_DEBUG, "%s: %s", pr->lock.path, strerror(errno));
			goto error;
		}
	}
	else {
		/* special case!!! */
		/* in this case, wam_update() should have aquired write-lock,
		   so do not try to aquire write lock */
		pr->lock.d = -1;
	}

	if (access(pr->sche, R_OK) != -1) {
		if (load_ns_pr(pr) == -1) {
			syslog(LOG_DEBUG, "load_ns_pr failed: %s", pr->sche);
			goto error;
		}
	}
	else if (flags == O_RDONLY) {
		syslog(LOG_DEBUG, "load_ns_pr failed: %s", pr->sche);
		goto error;
	}
	else if (sync_ns_pr(pr) == -1) {
		syslog(LOG_DEBUG, "sync_ns_pr failed");
		goto error;
	}

	pr->u.p.pr = NULL;

	if (!nodbm) {
		if (!(pr->u.p.pr = nwdb_open(pr->u.p.prp, NWDB_NPR, flags))) {
			syslog(LOG_DEBUG, "%s: %s", pr->u.p.prp, strerror(errno));
			goto error;
		}
	}
	pr->u.p.dir = d;
	return pr;

error:
	if (pr->lock.d != -1) {
		unsetlk(pr->lock.d);
		close(pr->lock.d);
	}
	free(pr);
	return NULL;
}

static int
wam_prop_drop(w, d, key)
WAM *w;
char const *key;
{
	WAM *pr;
	int e = 0;
/* F_WRLCK sarete iru hazunanode, open_count ha kini sinai */
	if (!(pr = wam_prop_open_rw(w->handle, d, key, w->rootdir, O_RDWR, 1))) {
		return -1;
	}

	if (nwdb_drop(pr->u.p.prp, NWDB_NPR) == -1) {
		e = -1;
	}
	if (unlink(pr->sche) == -1) {
		syslog(LOG_DEBUG, "warning: %s: %s", pr->u.p.prp, strerror(errno));
		e = -1;
	}

	wam_close(pr);
	return e;
}

WAM *
wam_fss_open(master)
WAM *master;
{
	WAM *fs;

	if (!master) {
		return NULL;
	}
	if (fs = get_registered_handle(master, FSSKEY)) {
		fs->u.fss.open_count++;
		return fs;
	}
	if (!(fs = wam_fss_open_rw(master->handle, master->rootdir, O_RDONLY, 0))) {
		return NULL;
	}

	fs->u.fss.master = master;
	register_handle(master, fs, FSSKEY);

	if (master->u.w.dist.role == NDWAM_ROLE_PARENT) {
		if (cb_wam_fss_openall(master->u.w.xs) == -1) {
			return NULL;
		}
	}

	return fs;
}

static WAM *
wam_fss_open_rw(handle, rootdir, flags, nofile)
char const *handle, *rootdir;
mode_t flags;
{
	WAM *fs;
	size_t m;

	if (!(fs = calloc(1, sizeof *fs))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}
	fs->u.fss.paths = NULL;

	fs->u.fss.master = NULL;
	fs->u.fss.open_count = 1;
	fs->lock.d = -1;
	fs->type = NWAM_D_TYPE_FSS;
	nw_setbase(fs, handle, rootdir);
	snprintf(fs->sche, sizeof fs->sche, "%s" DIRECTORY_DELIMITER SCHEKEY_FS, fs->base);

	if (!nofile) {
		if ((fs->lock.d = setlkw(F_RDLCK, fs->lock.path)) == -1) {
			syslog(LOG_DEBUG, "%s: %s", fs->lock.path, strerror(errno));
			goto error;
		}
	}
	else {
		/* special case!!! */
		/* in this case, wam_update() should have aquired write-lock,
		   so do not try to aquire write lock */
		fs->lock.d = -1;
	}

	if (access(fs->sche, R_OK) != -1) {
		if (load_ns_fs(fs) == -1) {
			syslog(LOG_DEBUG, "load_ns_fs failed: %s", fs->sche);
			goto error;
		}
	}
	else if (flags == O_RDONLY) {
		syslog(LOG_DEBUG, "load_ns_fs failed: %s", fs->sche);
		goto error;
	}
	else if (sync_ns_fs(fs) == -1) {
		syslog(LOG_DEBUG, "sync_ns_fs failed");
		goto error;
	}

	if (!(fs->u.fss.paths = calloc(MAXNFSSFRG, sizeof *fs->u.fss.paths))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		goto error;
	}

	for (m = 0; m < MAXNFSSFRG; m++) {
		struct fss_paths *p = &fs->u.fss.paths[m];

		snprintf(p->flwd, sizeof p->flwd, "%s" DIRECTORY_DELIMITER FLWDKEY ".%"PRIuSIZE_T, fs->base, (pri_size_t)m);
		snprintf(p->idx, sizeof p->idx, "%s" DIRECTORY_DELIMITER IDXKEY ".%"PRIuSIZE_T, fs->base, (pri_size_t)m);
		snprintf(p->bitmap, sizeof p->bitmap, "%s" DIRECTORY_DELIMITER MAPKEY ".%"PRIuSIZE_T, fs->base, (pri_size_t)m);
	}

	if (!nofile) {
		if (!(fs->u.fss.f = fss_open(fs->u.fss.paths, fs->u.fss.j))) {
			free(fs);
			goto error;
		}
	}
	else {
		/* special case!!! */
		/* if flags != O_RDONLY, this function call is dummy.
		   the main aim is generate paths[*].* for wam_update.
		   so we do not need to open actual fss here.
		*/
		fs->u.fss.f = NULL;
	}
	return fs;

error:
	free(fs->u.fss.paths);
	if (fs->lock.d != -1) {
		unsetlk(fs->lock.d);
		close(fs->lock.d);
	}
	free(fs);
	return NULL;
}

static int
wam_fss_drop(w)
WAM *w;
{
	WAM *fs;
	size_t m;
	int e = 0;

/* F_WRLCK sarete iru hazunanode, open_count ha kini sinai */
	if (!(fs = wam_fss_open_rw(w->handle, w->rootdir, O_RDWR, 1))) {
		return -1;
	}

	for (m = 0; m < MAXNFSSFRG; m++) {
		struct fss_paths *p = &fs->u.fss.paths[m];
		if (unlink(p->flwd) == -1) {
			/*syslog(LOG_DEBUG, "warning: %s: %s", p->flwd, strerror(errno));*/
			e = -1;
		}
		if (unlink(p->idx) == -1) {
			/*syslog(LOG_DEBUG, "warning: %s: %s", p->idx, strerror(errno));*/
			e = -1;
		}
		if (unlink(p->bitmap) == -1) {
			/*syslog(LOG_DEBUG, "warning: %s: %s", p->bitmap, strerror(errno));*/
			e = -1;
		}
	}
	if (unlink(fs->sche) == -1) {
		syslog(LOG_DEBUG, "warning: %s: %s", fs->sche, strerror(errno));
		e = -1;
	}

	wam_close(fs);
	return e;
}

/* XXX WARNING: 
  wam_close does not call sync_ns_*.
  (unlike wam_open_rw and wam_fss_open_rw_* call load_ns_* or sync_ns automatically)
  in case sync_ns required, you must call explicitly, before calling wam_close. */
int
wam_close(nw)
WAM *nw;
{
	int d, e = 0;

	switch (nw->type) {
	case NWAM_D_TYPE_WAM:
		close_registered_handles(nw);
		free_ci(&nw->u.w.ci);
		if (nw->u.w.dist.role == NDWAM_ROLE_PARENT) {
			if (cb_wam_closeall(nw->u.w.xs) == -1) {
				return -1;
			}
			if (xgwam_disconnect(nw->u.w.xs) == -1) {
				return -1;
			}
		}
		for (d = 0; d < 2; d++) {
			if (nw->u.w.d[d].cw && nwdb_close(nw->u.w.d[d].cw) == -1) {
				e = -1;
			}
			if (nw->u.w.d[d].cr && nwdb_close(nw->u.w.d[d].cr) == -1) {
				e = -1;
			}
			if (nw->u.w.d[d].xr && nwdb_close(nw->u.w.d[d].xr) == -1) {
				e = -1;
			}
			free(nw->u.w.d[d].pmask.buf);
			free(nw->u.w.d[d].nmask.buf);
		}
		free(nw->u.w.tv.ptr);
		free(nw->u.w.diststat);
		break;
	case NWAM_D_TYPE_PROP_FILE:
		if (--nw->u.p.open_count < 1) {
			WAM *master = nw->u.p.master;
			if (master && master->u.w.dist.role == NDWAM_ROLE_PARENT) {
				if (cb_wam_prop_closeall(master->u.w.xs, WAM_ROW, nw->u.p.key) == -1) {
					return -1;
				}
			}
			unregister_handle(master, nw);
			if (nw->u.p.pr && nwdb_close(nw->u.p.pr) == -1) {
				e = -1;
			}
		}
		free(nw->u.p.key);
		break;
	case NWAM_D_TYPE_FSS:
		if (--nw->u.fss.open_count < 1) {
			WAM *master = nw->u.fss.master;
			if (master && master->u.w.dist.role == NDWAM_ROLE_PARENT) {
				if (cb_wam_fss_closeall(master->u.w.xs) == -1) {
					return -1;
				}
			}
			unregister_handle(master, nw);
			if (nw->u.fss.f && fss_close(nw->u.fss.f) == -1) {
				e = -1;
			}
			free(nw->u.fss.paths);
		}
		break;
	}
	if (nw->lock.d != -1) {
		unsetlk(nw->lock.d);
		close(nw->lock.d);
	}
if (nw->rootdir) *nw->rootdir = '\0';
	free(nw->rootdir);
bzero(nw, sizeof *nw);
	free(nw);
	return e;
}

static int
wam_unlock_local(w)
WAM *w;
{
	if (w->lock.d != -1) {
		unsetlk(w->lock.d);
		close(w->lock.d);
		w->lock.d = -1;
	}
	return 0;
}

static int
fixate_cw(nw, d)
WAM *nw;
{
	if (nwdb_close(nw->u.w.d[d].cw) == -1) {
		syslog(LOG_DEBUG, "%s: %s", nw->u.w.d[d].cwp, strerror(errno));
		return -1;
	}
	if (nwdb_close(nw->u.w.d[d].cr) == -1) {
		syslog(LOG_DEBUG, "%s: %s", nw->u.w.d[d].crp, strerror(errno));
		return -1;
	}
	if (!(nw->u.w.d[d].cw = nwdb_open(nw->u.w.d[d].cwp, NWDB_NCW, O_RDWR))) {
		syslog(LOG_DEBUG, "%s: %s", nw->u.w.d[d].cwp, strerror(errno));
		return -1;
	}
	if (!(nw->u.w.d[d].cr = nwdb_open(nw->u.w.d[d].crp, NWDB_NCR, O_RDWR))) {
		syslog(LOG_DEBUG, "%s: %s", nw->u.w.d[d].crp, strerror(errno));
		return -1;
	}
	return 0;
}

static int
fixate_xr(nw, d)
WAM *nw;
{
	if (nwdb_close(nw->u.w.d[d].xr) == -1) {
		syslog(LOG_DEBUG, "%s: %s", nw->u.w.d[d].xrp, strerror(errno));
		return -1;
	}
	if (!(nw->u.w.d[d].xr = nwdb_open(nw->u.w.d[d].xrp, NWDB_NXR, O_RDWR))) {
		syslog(LOG_DEBUG, "%s: %s", nw->u.w.d[d].xrp, strerror(errno));
		return -1;
	}
	return 0;
}

struct exargs {
	char *handle, *rootdir;
};

struct exargs exargs = { NULL, NULL };

int
wam_update(struct wam_update_args *g, ...)
{
	char *ks = NULL;
	WAM *nw;
	struct raw_vec *r;
	size_t i;
	struct rvst rs0, *rs = &rs0;
	int d;
	char const *rootdir;
#if defined SIGPIPE
	void (*osigpipe)(int);
#endif
	void (*osigint)(int);
#if defined SIGHUP
	void (*osighup)(int);
#endif
	void (*osigterm)(int);
	char sts[FREQ_TWID * 4 + 5];

#if defined SIGPIPE
	osigpipe = signal(SIGPIPE, sigpipe);
	sigflg(SIGPIPE, SA_RESTART, SA_RESETHAND);
#endif

	osigint = signal(SIGINT, sigint);
#if defined SIGHUP
	osighup = signal(SIGHUP, sigint);
#endif
	osigterm = signal(SIGTERM, sigint);

	PRINT_ELAPSED("wam_update", stderr);
	rs->flags = 0;
	rs->pass = 0;

syslog(LOG_DEBUG, "wam_update: start");

	if (!(rootdir = getaroot)) {
		va_list ap;
		va_start(ap, g);
		rootdir = va_arg(ap, char *);
		va_end(ap);
	}

syslog(LOG_DEBUG, "rootdir: %s", rootdir ? rootdir : "(null)");

syslog(LOG_DEBUG, "g->handle: %s", g->handle);
	if (!(nw = wam_open_rw(g->handle, rootdir, O_RDWR|O_CREAT, &g->dist.c, 0, g->dist.server_conf_paths, 0))) {
		syslog(LOG_DEBUG, "wam_open_rw failed: %s", g->handle);
		goto error;
	}
	switch (g->command) {
	case WAM_UPDATE_FROM_ITB:
		exargs.handle = g->handle;
		exargs.rootdir = rootdir;
		nwam_add_tmp(nw->base, NWAM_D);
		break;
	}

	rs->nw = nw;

	if (g->tmpdir) {
		rs->tmpdir = g->tmpdir;
	}
	else if (!(rs->tmpdir = getenv("TMPDIR"))) {
		rs->tmpdir = "/tmp";
	}

	rs->path = NULL;
	rs->pos = 0;
	rs->lineno = 0;
	rs->last_l = 0;
	rs->relay_out = NULL;

	rs->ifrg.n = rs->ifrg.j = 0;
	rs->ifrg.stream = NULL;
	rs->ifrg.ej_size = 0;

	rs->pfrg.n = rs->pfrg.j = 0;
	rs->pfrg.stream = NULL;
	rs->pfrg.ej_size = 0;

	rs->stream = NULL;

	rs->fss.flwd = NULL;
	rs->fss.no_last_segid = 1;		/* will be overrided later... */
	rs->fss.ioptions = NULL;

	rs->make_fssi = 0;

	rs->prs = NULL;

	rs->s.ncre = NULL;
	rs->s.ncre_size = 0;

	rs->v_size = 0;
	if (!(rs->v = malloc(sizeof_dxr_vec(rs->v_size)))) {
		syslog(LOG_DEBUG, "malloc(%"PRIuSIZE_T"): %s", (pri_size_t)sizeof_dxr_vec(rs->v_size), strerror(errno));
		goto error;
	}

	switch (g->command) {
		char const **kk;
		size_t nkk, nkk0;
		char *s;
	case WAM_UPDATE_FROM_ITB:
		if (add_ci_pair(&nw->u.w.ci, "stemmer", g->i.s.stemmer, 0) != 0) {
			goto error;
		}
		if (ks = lookup_ci(&nw->u.w.ci, "properties")) {
			nkk0 = count_char(ks, ',') + 2;
			if (!(kk = calloc(nkk0, sizeof *kk))) {
				syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
				goto error;
			}
			nkk = splitv(ks, ',', kk, nkk0 - 1, 1);
			assert(nkk <= nkk0 - 1);
			g->i.keys = kk;
			if (!(s = check_keys(g->i.keys))) {
				goto error;
			}
			free(s);
		}
		else {
			if (!(s = check_keys(g->i.keys))) {
				goto error;
			}
			if (add_ci_pair(&nw->u.w.ci, "properties", s, 0) != 0) {
				goto error;
			}
			free(s);
		}
		sync_ci(nw, &nw->u.w.ci);
		break;

	case WAM_UPDATE_DELIDL:
		if (!(ks = lookup_ci(&nw->u.w.ci, "properties"))) {
			syslog(LOG_DEBUG, "`properties' not found in ci. corrupt DB?");
			goto error;
		}
		nkk0 = count_char(ks, ',') + 2;
		if (!(kk = calloc(nkk0, sizeof *kk))) {
			syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
			goto error;
		}
		nkk = splitv(ks, ',', kk, nkk0 - 1, 1);
		assert(nkk <= nkk0 - 1);
		g->i.keys = kk;
		if (!(s = check_keys(g->i.keys))) {
			goto error;
		}
		free(s);
		break;
	}

	for (i = 0; g->i.keys[i]; i++) {
		if (!strcmp(g->i.keys[i], FSSKEY)) {
			rs->make_fssi = 1;
			syslog(LOG_DEBUG, "FSS = 1");
			break;
		}
	}

	if (!(r = new_raw_vec(g->i.keys))) {
		syslog(LOG_DEBUG, "new_raw_vec: %s", strerror(errno));
		goto error;
	}

	rs->nprs = r->nprops;
	if (!(rs->prs = calloc(rs->nprs, sizeof *rs->prs))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
	}
	for (i = 0; i < r->nprops; i++) {
		WAM *pr;
/* F_WRLCK sarete iru hazunanode, open_count ha kini sinai */
		if (!(pr = wam_prop_open_rw(nw->handle, WAM_ROW, r->props[i].key, rootdir, O_RDWR|O_CREAT, 0))) {
			syslog(LOG_DEBUG, "%s: %s", nw->handle, strerror(errno));
			goto error;
		}
		rs->prs[i] = pr;
	}

	if (rs->make_fssi) {
/* F_WRLCK sarete iru hazunanode, open_count ha kini sinai */
		if (!(rs->fss.f = wam_fss_open_rw(nw->handle, rootdir, O_RDWR|O_CREAT, 1))) {
			goto error;
		}
/* F_WRLCK sarete iru hazunanode, open_count ha kini sinai */
		if (!(rs->fss.rmap = wam_prop_open_rw(nw->handle, WAM_ROW, RMAPKEY, rootdir, O_RDWR|O_CREAT, 0))) {
			syslog(LOG_DEBUG, "%s: %s", nw->handle, strerror(errno));
			goto error;
		}
		rs->fss.last_segid = rs->fss.f->u.fss.nsegms;	/*XXX*/
		rs->fss.no_last_segid = rs->fss.f->u.fss.j == 0;	/*XXX*/
	}

	switch (nw->u.w.dist.role) {
	case NDWAM_MONOLITHIC:
		if (g->command == WAM_UPDATE_DELIDL) {
			if (wam_delidl_fill_v(rs, g) == -1) {
				syslog(LOG_DEBUG, "wam_delidl_fill_v failed");
				goto error;
			}
			if (wam_delidl_v(rs, g) == -1) {
				syslog(LOG_DEBUG, "wam_delidl failed");
				goto error;
			}
			wam_delidl_cleanup(rs, g);
			goto finalize;
		}
		break;
	case NDWAM_ROLE_PARENT:
		if (g->command == WAM_UPDATE_DELIDL) {
			if (cb_wam_openall(nw->u.w.xs, nw->handle, nw->rootdir) == -1) {
				goto error;
			}
			if (wam_delidl_fill_v(rs, g) == -1) {
				syslog(LOG_DEBUG, "wam_delidl_fill_v failed");
				goto error;
			}
			if (cb_wam_closeall(nw->u.w.xs) == -1) {
				goto error;
			}
			g->command = WAM_UPDATE_DELIDL;
		}

		if (xwamupdate_init(nw->u.w.xs, g) == -1) {
			syslog(LOG_DEBUG, "xwamupdate_init failed");
			goto error;
		}
		if (g->command == WAM_UPDATE_DELIDL) {
			if (wam_delidl_v(rs, g) == -1) {
				syslog(LOG_DEBUG, "wam_delidl failed");
				goto error;
			}
			wam_delidl_cleanup(rs, g);
			goto finalize;
		}
		break;
	case NDWAM_ROLE_CHILD:
		assert(g->dist.n);
		assert(g->dist.nn);
		cb_wam_update_send_ack(g->dist.n, 0);

		if (g->command == WAM_UPDATE_DELIDL) {
			if (wam_delidl_v(rs, g) == -1) {
				syslog(LOG_DEBUG, "wam_delidl failed");
				goto error;
			}
			goto finalize;
		}

		break;
	default:
		syslog(LOG_DEBUG, "unknown role");
		goto error;
	}

	rs->pass = 1;
	PRINT_ELAPSED("pass 1", stderr);
	/* pass 1 */
	if (nw->u.w.dist.role != NDWAM_ROLE_CHILD) {
		if (setup_pstem(&rs->ps, g, rs->make_fssi) == -1) {
			syslog(LOG_DEBUG, "setup_pstem: %s", strerror(errno));
			goto error;
		}
		if (!(rs->relay_out = nfopen(rs->ps.relay_out))) {
			syslog(LOG_DEBUG, "nfopen: %s", strerror(errno));
			goto error;
		}
	}
	pi_init(&rs->pi.pi, 0, '(', ')', 0, stderr);
	rs->pi.read = 0;

	switch (nw->u.w.dist.role) {
	case NDWAM_MONOLITHIC:
		for (; read_raw_vec(r, rs
#if defined ENABLE_RAWVECLEN_LIMIT
			, g->i.rawveclen_limit
#endif
			) >= 0; ) {
			if (p1_eject_vec(r, &nw->u.w.ci, rs) == -1) {
				goto error;
			}
		}
		break;
	case NDWAM_ROLE_PARENT:
		for (; read_raw_vec(r, rs
#if defined ENABLE_RAWVECLEN_LIMIT
			, g->i.rawveclen_limit
#endif
			) >= 0; ) {
			if (xwamupdate_push_rawvec(nw->u.w.xs, r) == -1) {
				syslog(LOG_DEBUG, "xwamupdate_push_rawvec failed");
				goto error;
			}
			if (p1_eject_vec(r, &nw->u.w.ci, rs) == -1) {
				goto error;
			}
		}
		break;
	case NDWAM_ROLE_CHILD:
		for (;;) {
			struct raw_vec *r;
			switch (cb_wam_update_read_raw_vec(g->dist.nn, &r)) {
			case 0:
				if (p1_eject_vec(r, &nw->u.w.ci, rs) == -1) {
					goto error;
				}
				break;
			case 1:
				goto end;
			default:
				goto error;
			}
		}
	end:
		break;
	default:
		goto error;
	}
	fprintf(stderr, "((%"PRIuSIZE_T"))", (pri_size_t)rs->pi.read);
	if (nw->u.w.dist.role != NDWAM_ROLE_CHILD) {
		nfclose(rs->relay_out);
		rs->ps.relay_out = NULL;
		rs->relay_out = NULL;
		if (cleanup_pstem(&rs->ps) == -1) {
			syslog(LOG_DEBUG, "cleanup_pstem: %s", strerror(errno));
			goto error;
		}
	}

	if (rs->ifrg.stream) {
		fclose(rs->ifrg.stream);
		fprintf(stderr, "$>");
		rs->ifrg.stream = NULL;
	}

	for (d = 0; d < 2; d++) {
		if (fixate_cw(nw, d) == -1) {
			syslog(LOG_DEBUG, "fixate_cw: %s", strerror(errno));
			goto error;
		}
	}

	switch (nw->u.w.dist.role) {
	case NDWAM_ROLE_PARENT:
		xwamupdate_rawvec_end(nw->u.w.xs);
		break;
	}

	rs->pass = 2;
	PRINT_ELAPSED("pass 2", stderr);
	/* pass 2 */

	if (rs->make_fssi) {
		struct fss_paths *p;
		size_t ej_size;
		if (rs->fss.f->u.fss.j == 0) {
			rs->fss.j = 0;
			ej_size = 0;
		}
		else {
			rs->fss.j = rs->fss.f->u.fss.j - 1;
			ej_size = 0;
		}
		p = &rs->fss.f->u.fss.paths[rs->fss.j];
		if (!(rs->fss.flwd = fopen(p->flwd, "ab"))) {
			syslog(LOG_DEBUG, "fss.flwd %s: %s", p->flwd, strerror(errno));
			goto error;
		}
		rs->fss.j++;
		rs->fss.ej_size = ej_size;
	}

	rs->ifrg.j = 0;

pi_init(&rs->pi.pi, 0, '(', ')', 0, stderr);
rs->pi.read = 0;
	for (; read_raw_vec(r, rs
#if defined ENABLE_RAWVECLEN_LIMIT
		, 0
#endif
		) >= 0; ) {
		if (p2_eject_vec(r, rs, &nw->u.w.dist) == -1) {
			goto error;
		}
	}
	fprintf(stderr, "((%"PRIuSIZE_T"))", (pri_size_t)rs->pi.read);

	if (rs->pfrg.stream) {
		fclose(rs->pfrg.stream);
		fprintf(stderr, "$)");
		rs->pfrg.stream = NULL;
	}

	for (i = 0; i < rs->ifrg.n; i++) {
		if (truncate(rs->ifrg.paths[i], 0) == -1) {
			syslog(LOG_DEBUG, "warning: %s: %s", rs->ifrg.paths[i], strerror(errno));
		}
		free(rs->ifrg.paths[i]);
	}
	rs->ifrg.n = 0;

	if (rs->make_fssi) {
		fclose(rs->fss.flwd);
		rs->fss.flwd = NULL;
		if (nwdb_close(rs->fss.rmap->u.p.pr) == -1) {
			syslog(LOG_DEBUG, "nwdb_close %s: %s", rs->fss.rmap->u.p.prp, strerror(errno));
			goto error;
		}
		free(rs->fss.rmap->u.p.key);
		free(rs->fss.rmap);
	}

	for (i = 0; i < rs->nprs; i++) {
		WAM *pr = rs->prs[i];
		sync_ns_pr(pr);
		if (nwdb_close(pr->u.p.pr) == -1) {
			syslog(LOG_DEBUG, "nwdb_close %s: %s", pr->u.p.prp, strerror(errno));
			goto error;
		}
		free(pr->u.p.key);
		free(pr);
	}

	if (fixate_xr(nw, 0) == -1) {
		syslog(LOG_DEBUG, "fixate_xr 0: %s", strerror(errno));
		goto error;
	}

	rs->pass = 3;
	PRINT_ELAPSED("pass 3", stderr);
	/* pass 3 */

	for (i = 0; i < rs->pfrg.n; i++) {
		if (sort_pair_file(rs->pfrg.paths[i]) == -1) {
			goto error;
		}
	}

	if (pass3(rs, &nw->u.w.dist) == -1) {
		goto error;
	}

	for (i = 0; i < rs->pfrg.n; i++) {
		if (truncate(rs->pfrg.paths[i], 0) == -1) {
			syslog(LOG_DEBUG, "warning: %s: %s", rs->pfrg.paths[i], strerror(errno));
		}
		free(rs->pfrg.paths[i]);
	}
	rs->pfrg.n = 0;

	if (fixate_xr(nw, 1) == -1) {
		syslog(LOG_DEBUG, "fixate_xr 1: %s", strerror(errno));
		goto error;
	}

	if (rs->make_fssi) {
		char *ioptions;
		ioptions = rs->fss.ioptions ? rs->fss.ioptions : "";
		fprintf(stderr, "ioptions = [%s]\n", ioptions);

		rs->fss.f->u.fss.j = rs->fss.j;
		rs->fss.f->u.fss.nsegms = rs->fss.last_segid;
		for (i = 0; i < rs->fss.j; i++) {
			struct fss_paths *p = &rs->fss.f->u.fss.paths[i];
			fprintf(stderr, "RI[%"PRIuSIZE_T"/%"PRIuSIZE_T"]\n", (pri_size_t)i, (pri_size_t)rs->fss.j);
			if (mkri(nw, p->flwd, p->idx, p->bitmap, ioptions, rs->tmpdir, g->i.mkri_ncpu) == -1) {
				goto error;
			}
		}
		free(rs->fss.ioptions);
	}

finalize:
	PRINT_ELAPSED("finalize", stderr);
	/* finalize */

	if (rs->flags & RVST_LOST_MAX) {
		if (regenerate_max(rs) == -1) {
			goto error;
		}
		rs->flags &= ~RVST_LOST_MAX;
	}

	free(rs->prs);
	free_raw_vec(r);

	snprintf(sts, sizeof sts, "%"PRIdDF_T, nw->u.w.d[0].nentries);
	if (add_ci_pair(&nw->u.w.ci, "stat.row_num", sts, 1) != 0) {
		goto error;
	}
	snprintf(sts, sizeof sts, "%"PRIdDF_T, nw->u.w.d[1].nentries);
	if (add_ci_pair(&nw->u.w.ci, "stat.col_num", sts, 1) != 0) {
		goto error;
	}
	snprintf(sts, sizeof sts, "%"PRIdFREQ_T, nw->u.w.total_elem_num);
	if (add_ci_pair(&nw->u.w.ci, "stat.total_elem_num", sts, 1) != 0) {
		goto error;
	}
	snprintf(sts, sizeof sts, "%"PRIdFREQ_T, nw->u.w.total_freq_sum);
	if (add_ci_pair(&nw->u.w.ci, "stat.total_freq_sum", sts, 1) != 0) {
		goto error;
	}

	if (sync_ci(nw, &nw->u.w.ci) == -1) {
		goto error;
	}

	if (rs->make_fssi) {
		if (sync_ns_fs(rs->fss.f) == -1) {
			goto error;
		}
		wam_close(rs->fss.f);
	}

	if (sync_ns_nw(nw) == -1) {
		syslog(LOG_DEBUG, "sync_ns_nw failed (2)");
		goto error;
	}

	free(rs->v);
	free(rs->s.ncre);

	switch (nw->u.w.dist.role) {
	case NDWAM_MONOLITHIC:
		break;
	case NDWAM_ROLE_PARENT:
		if (xwamupdate_end(nw->u.w.xs) == -1) {
			goto error;
		}
		break;
	case NDWAM_ROLE_CHILD:
		if (cb_wam_update_read_raw_vec(g->dist.nn, NULL) != 2) {
			goto error;
		}
		break;
	default:
		goto error;
	}

	free(ks);
	wam_close(nw);

	PRINT_ELAPSED("end", stderr);
	cleanup_tmp();
#if defined SIGPIPE
	signal(SIGPIPE, osigpipe);
#endif
	signal(SIGINT, osigint);
#if defined SIGHUP
	signal(SIGHUP, osighup);
#endif
	signal(SIGTERM, osigterm);
	return 0;
error:
	emergency_cleanup();
#if defined SIGPIPE
	signal(SIGPIPE, osigpipe);
#endif
	signal(SIGINT, osigint);
#if defined SIGHUP
	signal(SIGHUP, osighup);
#endif
	signal(SIGTERM, osigterm);
	return -1;
}

static void
sigint(n)
{
	emergency_cleanup();
	syslog(LOG_DEBUG, "_exit 1");
	_exit(1);
}

#if defined SIGPIPE
static void
sigpipe(n)
{
	syslog(LOG_DEBUG, "SIGPIPE");
	emergency_cleanup();
	syslog(LOG_DEBUG, "_exit 1");
	_exit(1);
}
#endif

static void
emergency_cleanup()
{
	if (!need_cleanup) {
		return;
	}
	syslog(LOG_DEBUG, "emergency cleanup");
	cleanup_tmp();
	syslog(LOG_DEBUG, "wam_drop_l");
	if (exargs.handle) {
		wam_drop_l(exargs.handle, exargs.rootdir, 1);
	}
	syslog(LOG_DEBUG, "cleanup exargs");

	exargs.handle = NULL;
	exargs.rootdir = NULL;
}

struct tmps {
	char *path;
	int type;
};

static struct tmps *tmps = NULL;
static size_t ntmps = 0;
static size_t tmps_size = 0;

static void
cleanup_tmp()
{
	size_t i;
	for (i = 0; i < ntmps; i++) {
		if (tmps[i].type == NWAM_D) {
			rmdir(tmps[i].path);
		}
		else {
			unlink(tmps[i].path);
		}
		free(tmps[i].path);
	}
	free(tmps);
	tmps = NULL;
	ntmps = 0;
	tmps_size = 0;
}

void
nwam_add_tmp(path, type)
char const *path;
{
	if (tmps_size <= ntmps) {
		void *new;
		size_t newsize = ntmps;
		newsize = NA(newsize + 1, 128);
		if (!(new = realloc(tmps, newsize * sizeof *tmps))) {
			/* fatal error */
			syslog(LOG_DEBUG, "fatal error: please cleanup intermediate files by hand.");
		}
		tmps_size = newsize;
		tmps = new;
	}

	if (!(tmps[ntmps].path = strdup(path))) {
		/* fatal error */
		syslog(LOG_DEBUG, "fatal error: please cleanup intermediate files by hand.");
	}
	tmps[ntmps].type = type;
	ntmps++;
}

static char *
check_keys(keys)
char const **keys;
{
	char *s;
	size_t i, l;
	for (i = l = 0; keys[i]; i++) {
		size_t j;
		l += strlen(keys[i]) + 1;
		for (j = i + 1; keys[j]; j++) {
			if (!strcmp(keys[i], keys[j])) {
				fprintf(stderr, "duplicate key: %s\n", keys[i]);
				return NULL;
			}
		}
		syslog(LOG_DEBUG, "key[%"PRIuSIZE_T"] = %s", (pri_size_t)i, keys[i]);
	}
	if (!(s = malloc(l + 1))) {
		syslog(LOG_DEBUG, "malloc(%"PRIuSIZE_T"): %s", (pri_size_t)(l + 1), strerror(errno));
		return NULL;
	}
	s[0] = '\0';
	for (i = l = 0; keys[i]; i++) {
		size_t ll = strlen(keys[i]);
		if (i) {
			s[l++] = ',';
		}
		memmove(s + l, keys[i], ll);
		l += ll;
		s[l] = '\0';
	}
	return s;
}

static int
wam_delidl_fill_v(rs, g)
struct rvst *rs;
struct wam_update_args *g;
{
	size_t i;
	int e = 0;

	if (!(g->d.vecs = calloc(g->d.nid, sizeof *g->d.vecs))) {
		syslog(LOG_DEBUG, "wam_delidl_fill_v: calloc: %s", strerror(errno));
		return -1;
	}

	for (i = 0; i < g->d.nid; i++) {
		struct xr_vec const *v;
		if (wam_get_vec(rs->nw, WAM_ROW, g->d.id[i], &v) == -1) {
			e = -1;
			syslog(LOG_DEBUG, "wam_delidl_fill_v: wam_get_vec failed");
			return -1;
		}
		
		if (!(g->d.vecs[i] = dxr_dup(v))) {
			syslog(LOG_DEBUG, "wam_delidl_fill_v: dxr_dup: %s", strerror(errno));
			e = -1;
			return -1;
		}
	}

	return 0;
}

static void
wam_delidl_cleanup(rs, g)
struct rvst *rs;
struct wam_update_args *g;
{
	size_t j;

	for (j = 0; j < g->d.nid; j++) {
		free(g->d.vecs[j]);
	}
	free(g->d.vecs);
	g->d.vecs = NULL;
}

static int
wam_delidl_v(rs, g)
struct rvst *rs;
struct wam_update_args *g;
{
	df_t i;
	for (i = 0; i < g->d.nid; i++) {
		if (del_vec(g->d.id[i], g->d.vecs[i], rs) == -1) {
			syslog(LOG_DEBUG, "wam_delidl_v: del_vec failed");
			return -1;
		}
	}
	return 0;
}

static int
del_vec(id, v, rs)
idx_t id;
struct xr_vec const *v;
struct rvst *rs;
{
	df_t i, j;
	struct xr_vec *v0;
	int myxr, mypi, rowxr;

	for (i = 0; i < v->elem_num; i++)  {
		struct xr_elem const *e = &v->elems[i];
		switch (dec_cw(e->id, 1, e->freq, 1, rs)) {
		default:
			fprintf(stderr, "assertion failed 1\n");
			/* FALLTHRU */
		case -1:
			syslog(LOG_DEBUG, "del_vec: dec_cw %"PRIuIDX_T" failed", e->id);
			return -1;
		case 0:
			break;
		}
	}
	switch (dec_cw(id, v->elem_num, v->freq_sum, 0, rs)) {
	default:
		fprintf(stderr, "assertion failed 4\n");
		/* FALLTHRU */
	case -1:
		syslog(LOG_DEBUG, "del_vec: dec_cw %"PRIuIDX_T" failed", id);
		return -1;
	case 0:
		break;
	}

	switch (rs->nw->u.w.dist.role) {
	case NDWAM_MONOLITHIC:
		myxr = 1, mypi = 1, rowxr = 0;
		break;
	case NDWAM_ROLE_PARENT:
		myxr = 0, mypi = 0, rowxr = 0;
		break;
	case NDWAM_ROLE_CHILD:
		switch (rs->nw->u.w.dist.dir) {
		case WAM_COL:
			reduce_vec(v, &rs->nw->u.w.dist);
			myxr = 1, mypi = 0, rowxr = 0;
			break;
		case WAM_ROW:
			myxr = mypi = minep(id, &rs->nw->u.w.dist);
			rowxr = 1;
			break;
		default:
			return -1;
		}
		break;
	default:
		return -1;
	}

	if (myxr || rowxr) {
		for (i = 0; i < v->elem_num; i++)  {
			struct xr_elem const *e = &v->elems[i];
			struct xr_vec *v1;
			switch (retrieve_xr(e->id, &v0, rs->nw->u.w.d[1].xr)) {
			default:
				syslog(LOG_DEBUG, "del_vec: retrieve_xr %"PRIuIDX_T" failed", e->id);
				return -1;
			case 0:
				break;
			}
			if (!(v1 = dxr_dup(v0))) {
				syslog(LOG_DEBUG, "del_vec: dxr_dup: %s", strerror(errno));
				return -1;
			}
			for (j = 0; j < v1->elem_num; j++) {
				if (v1->elems[j].id == id) break;
			}
		if (myxr) {
			if (j == v1->elem_num) {
				free(v1);
				syslog(LOG_DEBUG, "del_vec: assertion failed 3");
				return -1;
			}
			assert(v1->elems[j].freq == e->freq);
			memmove(&v1->elems[j], &v1->elems[j + 1], sizeof v1->elems[j] * (v1->elem_num - 1 - j));
			v1->elem_num--;
		}
		else if (rowxr) {
			if(j != v1->elem_num) {
				syslog(LOG_DEBUG, "del_vec: assertion failed 2");
				return -1;
			}
		}
		else {
			syslog(LOG_DEBUG, "del_vec: internal error");
			return -1;
		}
			v1->freq_sum -= e->freq;
			if (insert_xr(e->id, v1, rs->nw->u.w.d[1].xr) == -1) {
				free(v1);
				syslog(LOG_DEBUG, "del_vec: insert_xr %"PRIuIDX_T" failed", e->id);
				return -1;
			}
			free(v1);

		}
	}

	if (myxr) {

		switch (retrieve_xr(id, &v0, rs->nw->u.w.d[0].xr)) {
		default:
			syslog(LOG_DEBUG, "del_vec: retrieve_xr %"PRIuIDX_T" failed", id);
			return -1;
		case 0:
			break;
		}
		assert(v0->elem_num == v->elem_num);
		assert(v0->freq_sum == v->freq_sum);
		if (delete_xr(id, rs->nw->u.w.d[0].xr) == -1) {
			syslog(LOG_DEBUG, "del_vec: delete_xr %"PRIuIDX_T" failed", id);
			return -1;
		}
	}

	if (mypi) {

		for (i = 0; i < rs->nprs; i++) {
			WAM *pr = rs->prs[i];
			if (delete_pr(id, pr->u.p.pr) == -1) {
				syslog(LOG_DEBUG, "del_vec: delete_pr %"PRIuIDX_T" failed", id);
				return -1;
			}
		}

		if (rs->make_fssi) {
			struct flwd_idx idx;
			size_t size;
			struct fss_rmap_ent_t rmap_e0, *rmap_e;
			FILE *f;
			struct fss_paths *p;

			switch (nwdb_get(rs->fss.rmap->u.p.pr, &id, &rmap_e, &size)) {
			case -1:
			case 1:
				syslog(LOG_DEBUG, "nwdb_get " RMAPKEY " failed");
				return -1;
			case 0:
				break;
			}
			rmap_e0 = *rmap_e;
			if (nwdb_del(rs->fss.rmap->u.p.pr, &id) != 0) {
				syslog(LOG_DEBUG, "nwdb_del fss.rmap failed");
				return -1;
			}

	fprintf(stderr, "delete fss: part = %"PRIuSIZE_T", offset = %"PRIuSIZE_T"\n", (pri_size_t)rmap_e0.j, (pri_size_t)rmap_e0.offset);

			p = &rs->fss.f->u.fss.paths[rmap_e0.j];
			if (!(f = fopen(p->flwd, "r+b"))) {
				syslog(LOG_DEBUG, "del_vec: %s: %s", p->flwd, strerror(errno));
				return -1;
			}
			if (fseek(f, rmap_e0.offset, SEEK_SET) == -1) {	/* XXX */
				syslog(LOG_DEBUG, "del_vec: fseek: %s", strerror(errno));
				return -1;
			}
			for (;;) {
#if defined FLWD_UTF32
				unsigned int u;
#else
				unsigned char c;
				size_t sz;
#endif

				if (fread(&idx, sizeof idx, 1, f) != 1) {
					break;
				}
				if (idx.id != id) {
					break;
				}
				if (fseek(f, -(long)(sizeof idx), SEEK_CUR) == -1) {
					syslog(LOG_DEBUG, "del_vec: fseek: %s", strerror(errno));
					return -1;
				}
	fprintf(stderr, "erase fss: id = %"PRIuIDX_T", segid = %u\n", idx.id, idx.segid);
				idx.id = 0;
				idx.segid = 0;
				if (fwrite(&idx, sizeof idx, 1, f) != 1) {
					syslog(LOG_DEBUG, "del_vec: fwrite: %s", strerror(errno));
					return -1;
				}

#if defined FLWD_UTF32
				if (fread(&u, sizeof u, 1, f) != 1) {
					syslog(LOG_DEBUG, "del_vec: unexpected EOF");
					return -1;
				}
				assert(u == 0);
				for (; fread(&u, 4, 1, f) == 1; ) {
					if (!u) break;
				}
#else
				if (fread(&c, 1, 1, f) != 1) {
					syslog(LOG_DEBUG, "del_vec: unexpected EOF");
					return -1;
				}
				assert(c == 0);
				for (sz = 1; fread(&c, sizeof c, 1, f) == 1; sz++) {
					if (!c) break;
				}
				sz++;
				if (sz % FWIDXALIGN) {
					size_t pad = FWIDXALIGN - sz % FWIDXALIGN;
					for (; pad && fread(&c, 1, 1, f) == 1; pad--) {
						if (c != FWPADCHR) {
							syslog(LOG_DEBUG, "del_vec: aligment error");
							return -1;
						}
					}
					if (pad != 0) {
						syslog(LOG_DEBUG, "del_vec: aligment error");
						return -1;
					}
				}
#endif
			}
			fclose(f);
		}
	}

	return 0;
}

static int
pass3(rs, dc)
struct rvst *rs;
struct distconf_t *dc;
{
	struct priq *a;
	idx_t prev_id;
	df_t elem_num;
	freq_t freq_sum;
	struct pair_stream *pstrs;
	size_t i;

	if (!(a = priq_creat_p(sizeof (struct pair_stream *), NULL, prop_f_ycompar, prop_b_ycompar, NULL))) {
		return -1;
	}
	if (!(pstrs = calloc(rs->pfrg.n, sizeof *pstrs))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return -1;
	}
	for (i = 0; i < rs->pfrg.n; i++) {
		struct pair_stream *b = &pstrs[i];
		if (!(b->stream = fopen(rs->pfrg.paths[i], "rb"))) {
			syslog(LOG_DEBUG, "%s: %s", rs->pfrg.paths[i], strerror(errno));
			return -1;
		}
		if (next_pair(b) != EOF) {
			if (priq_enq(a, &b) == -1) {
				return -1;
			}
		}
		else {
			fclose(b->stream);
		}
	}

	prev_id = 0;
	elem_num = 0;
	freq_sum = 0;

pi_init(&rs->pi.pi, 0, '[', ']', 0, stderr);
rs->pi.read = 0;
	while (!PRIQ_EMPTYP(a)) {
		struct pair_stream *b = *(struct pair_stream **)PRIQ_TOP(a);
		struct xr_elem *xe;

		if (b->p.c_id != prev_id) {
			if (prev_id) {
				if (p3_eject_vec(prev_id, elem_num, freq_sum, rs, dc) == -1) {
					return -1;
				}
			}
			prev_id = b->p.c_id;
			elem_num = 0;
			freq_sum = 0;
		}

		if (rs->v_size <= elem_num) {
			void *new;
			size_t newsize = elem_num;
			newsize = NA(newsize + 1, 128);
			if (!(new = realloc(rs->v, sizeof_dxr_vec(newsize)))) {
				free(rs->v);
				rs->v = NULL;
				rs->v_size = 0;
				return -1;
			}
			rs->v_size = newsize;
			rs->v = new;
		}

		xe = &rs->v->elems[elem_num++];
		xe->id = b->p.r_id;
		xe->freq = b->p.tf;
		freq_sum += b->p.tf;
		if (elem_num > 1) {
			struct xr_elem *pe;
			pe = xe - 1;
			if (xe->id < pe->id) {
				fprintf(stderr, "!!!! %"PRIuIDX_T" %"PRIuIDX_T"\n", xe->id, pe->id);
				abort();
			}
		}

		if (next_pair(b) != EOF) {
			priq_rplatop(a, &b);
		}
		else {
			fclose(b->stream);
			priq_deq(a);
		}
	}
	if (prev_id) {
		if (p3_eject_vec(prev_id, elem_num, freq_sum, rs, dc) == -1) {
			return -1;
		}
	}
	fprintf(stderr, "[[%"PRIuSIZE_T"]]", (pri_size_t)rs->pi.read);

	priq_free(a);

	free(pstrs);
	return 0;
}

static int
p3_eject_vec(id, elem_num, freq_sum, rs, dc)
idx_t id;
df_t elem_num;
freq_t freq_sum;
struct rvst *rs;
struct distconf_t *dc;
{
	struct xr_vec *v0;
	int myxr;

++rs->pi.read;
PI_CHECK(&rs->pi.pi, rs->pi.read);

	rs->v->elem_num = elem_num;
	rs->v->freq_sum = freq_sum;
	switch (retrieve_xr(id, &v0, rs->nw->u.w.d[1].xr)) {
	case -1:
		return -1;
	case 1:
		v0 = NULL;
		break;
	case 0:
		if (merge_vec(rs, v0) == -1) {
			return -1;
		}
	}

	uniq_vec(rs);
	switch (dc->role) {
	case NDWAM_MONOLITHIC:
		myxr = 1;
		break;
	case NDWAM_ROLE_PARENT:
		myxr = 0;
		break;
	case NDWAM_ROLE_CHILD:
		switch (dc->dir) {
		case WAM_ROW:
			reduce_vec(rs->v, dc);
			myxr = 1;
			break;
		case WAM_COL:
			myxr = minep(id, dc);
			break;
		default:
			return -1;
		}
		break;
	default:
		return -1;
	}
	if (myxr) {
		if (insert_xr(id, rs->v, rs->nw->u.w.d[1].xr) == -1) {
			return -1;
		}
	}
	if (rs->nw->u.w.d[1].max_elem_num < elem_num) {
		rs->nw->u.w.d[1].max_elem_num = elem_num;
	}
	if (rs->nw->u.w.d[1].max_freq_sum < freq_sum) {
		rs->nw->u.w.d[1].max_freq_sum = freq_sum;
	}
	if (!v0) {
		rs->nw->u.w.d[1].nentries++;
	}
	return 0;
}

static int
minep(id, dc)
idx_t id;
struct distconf_t *dc;
{
	return XPART(id, dc->nn[dc->dir]) == dc->node_id;
}

void
reduce_vec(v, dc)
struct xr_vec *v;
struct distconf_t *dc;
{
	df_t i, j;
	for (i = j = 0; i < v->elem_num; i++) {
		if (minep(v->elems[i].id, dc)) {
			v->elems[j++] = v->elems[i];
		}
	}
	v->elem_num = j;
}

static int
load_ci(nw, ci)
WAM *nw;
struct corpus_i *ci;
{
	FILE *f;
	NFILE *g;
	char *p;

	ci->ncprop = 0;
	ci->cprop_size = 0;
	ci->cprop = NULL;

	if (!(f = fopen(nw->u.w.inf, "r"))) {
		return 0;
	}
	if (!(g = nfopen(f))) {
		syslog(LOG_DEBUG, "nfopen: %s", strerror(errno));
		return -1;
	}
	while ((p = readln(g)) && *p) {
		chop(p);
		if (*p == '@' && add_ci(ci, p, 0) != 0) {
			fprintf(stderr, "add_ci failed\n");
			return -1;
		}
	}
	nfclose(g);
	return 0;
}

static int
sync_ci(nw, ci)
WAM *nw;
struct corpus_i *ci;
{
	FILE *f;
	size_t i;

	if (!(f = fopen(nw->u.w.inf, "w"))) {
		syslog(LOG_DEBUG, "%s: %s", nw->u.w.inf, strerror(errno));
		return -1;
	}
	for (i = 0; i < ci->ncprop; i++) {
		struct corpus_prop *cp = &ci->cprop[i];
		fprintf(f, "@%s=%s\n", cp->key, cp->value);
	}
	fclose(f);
	return 0;
}

static void
free_ci(ci)
struct corpus_i *ci;
{
	size_t i;

	for (i = 0; i < ci->ncprop; i++) {
		struct corpus_prop *cp = &ci->cprop[i];
		free(cp->key);
		free(cp->value);
	}
	free(ci->cprop);
}

char *
lookup_ci(ci, key)
struct corpus_i *ci;
char const *key;
{
	size_t i;
	for (i = 0; i < ci->ncprop; i++) {
		struct corpus_prop *cp = &ci->cprop[i];
		if (!strcmp(cp->key, key)) {
			char *r;
			if (!(r = strdup(cp->value))) {
				syslog(LOG_DEBUG, "strdup: %s", strerror(errno));
				return NULL;
			}
			return r;
		}
	}
	return NULL;
}

static int
add_ci(ci, p, no_warn)
struct corpus_i *ci;
char *p;
{
	char *key, *value, *q;

	if (*p != '@') return -1;
	key = p + 1;
	if (!(q = index(key, '='))) {
		return 1;
	}
	*q++ = '\0';
	value = q;

	return add_ci_pair(ci, key, value, no_warn);
}

static int
add_ci_pair(ci, key, value, no_warn)
struct corpus_i *ci;
char const *key, *value;
{
	struct corpus_prop *cp;
	size_t i;

	for (i = 0; i < ci->ncprop; i++) {
		struct corpus_prop *xp = &ci->cprop[i];
		if (!strcmp(key, xp->key)) {
			if (strcmp(xp->value, value)) {
				fprintf(stderr, "note: redefined ci_entry: key=%s, old-value=%s, new-value=%s\n", key, xp->value, value);
			}
			free(xp->value);
			if (!(xp->value = strdup(value))) {
				xp->value = NULL;
				return 2;
			}
			return 0;
		}
	}

	if (extend_cprop(ci) == -1) {
		return 2;
	}
	assert(ci->ncprop < ci->cprop_size);
	cp = &ci->cprop[ci->ncprop];
	if (!(cp->key = strdup(key))) {
		return 2;
	}
	if (!(cp->value = strdup(value))) {
		free(cp->key);
		return 2;
	}
	ci->ncprop++;
	return 0;
}

static int
extend_cprop(ci)
struct corpus_i *ci;
{
	if (ci->cprop_size <= ci->ncprop) {
		void *new;
		size_t newsize = ci->ncprop;
		newsize = NA(newsize + 1, 16);
		if (!(new = realloc(ci->cprop, newsize * sizeof *ci->cprop))) {
			return -1;
		}
		ci->cprop_size = newsize;
		ci->cprop = new;
	}
	return 0;
}

#define	SCHE_MAGIC_NW	"#NDWAM\r\n"
static char *sche_kwds_nw[] = {
	"- row \r\n",
	"- col \r\n",
	"- com \r\n",
	"- dis \r\n",
	};

static int
sync_ns_nw(nw)
WAM *nw;
{
	FILE *f;
	struct nsche_nw ns0, *ns = &ns0;
	int d, dist;

	if (!nw) {
		fprintf(stderr, "internal error: nw == NULL\n");
		return -1;
	}

	if (nw->type != NWAM_D_TYPE_WAM) {
		fprintf(stderr, "internal error: !WAM\n");
		return -1;
	}

	memmove(ns->magic, SCHE_MAGIC_NW, 8);	/* safe */

	for (d = 0; d < 2; d++) {
		memmove(ns->d[d].pad0, sche_kwds_nw[d], 8);	/* safe */
		setdf_t(nw->u.w.d[d].nentries, ns->d[d].nentries);
		setdf_t(nw->u.w.d[d].max_elem_num, ns->d[d].max_elem_num);
		setfreq_t(nw->u.w.d[d].max_freq_sum, ns->d[d].max_freq_sum);
		setidx_t(nw->u.w.d[d].maxid, ns->d[d].maxid);
		setint32_t(nw->u.w.d[d].compress_mode, ns->d[d].compress_mode);
	}

	memmove(ns->pad1, sche_kwds_nw[2], 8);	/* safe */
	setfreq_t(nw->u.w.total_elem_num, ns->total_elem_num);
	setfreq_t(nw->u.w.total_freq_sum, ns->total_freq_sum);
	switch (nw->u.w.dist.role) {
	default:
		fprintf(stderr, "internal error: nw->u.w.dist.role = %d\n", nw->u.w.dist.role);
		return -1;
	case NDWAM_MONOLITHIC:
		dist = 0;
		break;
	case NDWAM_ROLE_PARENT:
	case NDWAM_ROLE_CHILD:
		memmove(ns->dist.pad2, sche_kwds_nw[3], 8);	/* safe */
		setint32_t(nw->u.w.dist.role, ns->dist.role);
		setint32_t(nw->u.w.dist.dir, ns->dist.dir);
		setdf_t(nw->u.w.dist.node_id, ns->dist.node_id);
		setdf_t(nw->u.w.dist.nn[0], ns->dist.nn[0]);
		setdf_t(nw->u.w.dist.nn[1], ns->dist.nn[1]);
		dist = 1;
	}

	if (!(f = fopen(nw->sche, "wb"))) {
		syslog(LOG_DEBUG, "%s: %s", nw->sche, strerror(errno));
		return -1;
	}
	if (fwrite(ns, dist ? sizeof *ns : offsetof(struct nsche_nw, dist), 1, f) != 1) {
		syslog(LOG_DEBUG, "%s: %s", nw->sche, strerror(errno));
		fclose(f);
		return -1;
	}
	fclose(f);
	return 0;
}

static int
load_ns_nw(nw)
WAM *nw;
{
	FILE *f;
	struct nsche_nw ns0, *ns = &ns0;
	int d;
	int dist;

	if (nw->type != NWAM_D_TYPE_WAM) {
		syslog(LOG_DEBUG, "internal error: !WAM");
		return -1;
	}

	if (!(f = fopen(nw->sche, "rb"))) {
		syslog(LOG_DEBUG, "%s: %s", nw->sche, strerror(errno));
		return -1;
	}

	if (fseek(f, 0L, SEEK_END) == -1) {
		syslog(LOG_DEBUG, "%s: %s", nw->sche, strerror(errno));
		return -1;
	}

	switch (ftell(f)) {
	case sizeof *ns:
		dist = 1;
		break;
	case offsetof(struct nsche_nw, dist):
		dist = 0;
		break;
	default:
		syslog(LOG_DEBUG, "ftell failed");
		return -1;
	}

	rewind(f);

	if (fread(ns, dist ? sizeof *ns : offsetof(struct nsche_nw, dist), 1, f) != 1) {
		syslog(LOG_DEBUG, "%s: %s", nw->sche, strerror(errno));
		fclose(f);
		return -1;
	}
	fclose(f);

	if (strncmp(ns->magic, SCHE_MAGIC_NW, 8)) {
		syslog(LOG_DEBUG, "!magic");
		return -1;
	}

	for (d = 0; d < 2; d++) {
		if (strncmp(ns->d[d].pad0, sche_kwds_nw[d], 8)) {
			syslog(LOG_DEBUG, "malformed nsche_nw");
			return -1;
		}
		nw->u.w.d[d].nentries = strtoul(ns->d[d].nentries, NULL, 10);
		nw->u.w.d[d].max_elem_num = strtoul(ns->d[d].max_elem_num, NULL, 10);
		nw->u.w.d[d].max_freq_sum = strtoul(ns->d[d].max_freq_sum, NULL, 10);	/*XXX*/
		nw->u.w.d[d].maxid = strtoul(ns->d[d].maxid, NULL, 10);
		nw->u.w.d[d].compress_mode = strtoul(ns->d[d].compress_mode, NULL, 10);
	}

	if (strncmp(ns->pad1, sche_kwds_nw[2], 8)) {
		syslog(LOG_DEBUG, "malformed nsche_nw");
		return -1;
	}
	nw->u.w.total_elem_num = strtoul(ns->total_elem_num, NULL, 10);	/*XXX*/
	nw->u.w.total_freq_sum = strtoul(ns->total_freq_sum, NULL, 10);	/*XXX*/

	if (dist) {
		if (strncmp(ns->dist.pad2, sche_kwds_nw[3], 8)) {
			syslog(LOG_DEBUG, "malformed nsche_nw");
			return -1;
		}
		nw->u.w.dist.role = strtoul(ns->dist.role, NULL, 10);
		if (nw->u.w.dist.role != NDWAM_ROLE_PARENT && nw->u.w.dist.role != NDWAM_ROLE_CHILD) {
			syslog(LOG_DEBUG, "malformed nsche_nw");
			return -1;
		}

		nw->u.w.dist.dir = strtoul(ns->dist.dir, NULL, 10);
		nw->u.w.dist.node_id = strtoul(ns->dist.node_id, NULL, 10);
		nw->u.w.dist.nn[0] = strtoul(ns->dist.nn[0], NULL, 10);
		nw->u.w.dist.nn[1] = strtoul(ns->dist.nn[1], NULL, 10);
	}
	else {
		nw->u.w.dist.role = NDWAM_MONOLITHIC;
		nw->u.w.dist.dir = 0;
		nw->u.w.dist.node_id = 0;
		nw->u.w.dist.nn[0] = 0;
		nw->u.w.dist.nn[1] = 0;
	}

	return 0;
}

#define	SCHE_MAGIC_PR	"#NPR  \r\n"

static int
sync_ns_pr(pr)
WAM *pr;
{
	FILE *f;
	struct nsche_pr ns0, *ns = &ns0;

	if (pr->type != NWAM_D_TYPE_PROP_FILE) {
		syslog(LOG_DEBUG, "internal error: !PR");
		return -1;
	}

	memmove(ns->magic, SCHE_MAGIC_PR, 8);	/* safe */

	setdf_t(pr->u.p.nentries, ns->nentries);

	if (!(f = fopen(pr->sche, "wb"))) {
		syslog(LOG_DEBUG, "%s: %s", pr->sche, strerror(errno));
		return -1;
	}
	if (fwrite(ns, sizeof *ns, 1, f) != 1) {
		syslog(LOG_DEBUG, "%s: %s", pr->sche, strerror(errno));
		fclose(f);
		return -1;
	}
	fclose(f);
	return 0;
}

static int
load_ns_pr(pr)
WAM *pr;
{
	FILE *f;
	struct nsche_pr ns0, *ns = &ns0;

	if (pr->type != NWAM_D_TYPE_PROP_FILE) {
		syslog(LOG_DEBUG, "internal error: !PR");
		return -1;
	}

	if (!(f = fopen(pr->sche, "rb"))) {
		syslog(LOG_DEBUG, "%s: %s", pr->sche, strerror(errno));
		return -1;
	}
	if (fread(ns, sizeof *ns, 1, f) != 1) {
		syslog(LOG_DEBUG, "%s: %s", pr->sche, strerror(errno));
		fclose(f);
		return -1;
	}
	fclose(f);

	if (strncmp(ns->magic, SCHE_MAGIC_PR, 8)) {
		syslog(LOG_DEBUG, "!magic");
		return -1;
	}

	pr->u.p.nentries = strtoul(ns->nentries, NULL, 10);

	return 0;
}


#define	SCHE_MAGIC_FS	"#NFSS \r\n"

static int
sync_ns_fs(fs)
WAM *fs;
{
	FILE *f;
	struct nsche_fs ns0, *ns = &ns0;

	if (fs->type != NWAM_D_TYPE_FSS) {
		syslog(LOG_DEBUG, "internal error: !FSS");
		return -1;
	}

	memmove(ns->magic, SCHE_MAGIC_FS, 8);	/* safe */

	setint32_t(fs->u.fss.j, ns->fss_j);
	setidx_t(fs->u.fss.nsegms, ns->fss_nsegms);

	if (!(f = fopen(fs->sche, "wb"))) {
		syslog(LOG_DEBUG, "%s: %s", fs->sche, strerror(errno));
		return -1;
	}
	if (fwrite(ns, sizeof *ns, 1, f) != 1) {
		syslog(LOG_DEBUG, "%s: %s", fs->sche, strerror(errno));
		fclose(f);
		return -1;
	}
	fclose(f);
	return 0;
}

static int
load_ns_fs(fs)
WAM *fs;
{
	FILE *f;
	struct nsche_fs ns0, *ns = &ns0;

	if (fs->type != NWAM_D_TYPE_FSS) {
		syslog(LOG_DEBUG, "internal error: !FSS");
		return -1;
	}

	if (!(f = fopen(fs->sche, "rb"))) {
		syslog(LOG_DEBUG, "%s: %s", fs->sche, strerror(errno));
		return -1;
	}
	if (fread(ns, sizeof *ns, 1, f) != 1) {
		syslog(LOG_DEBUG, "%s: %s", fs->sche, strerror(errno));
		fclose(f);
		return -1;
	}
	fclose(f);

	if (strncmp(ns->magic, SCHE_MAGIC_FS, 8)) {
		syslog(LOG_DEBUG, "!magic");
		return -1;
	}

	fs->u.fss.j = strtoul(ns->fss_j, NULL, 10);
	fs->u.fss.nsegms = strtoul(ns->fss_nsegms, NULL, 10);

	return 0;
}

void
wam_dumpall(handle, keys, stream)
char const *handle, **keys;
FILE *stream;
{
	WAM *nw;
	int d;
	size_t j;

	if (!(nw = wam_open(handle, NULL))) {
		return;
	}
	for (d = 0; d < 2; d++) {
		fprintf(stream, "%s:\n", nw->u.w.d[d].cwp);
		dump_cw(nw->u.w.d[d].cw, stream);
	}

	for (d = 0; d < 2; d++) {
		fprintf(stream, "%s:\n", nw->u.w.d[d].crp);
		dump_cr(nw->u.w.d[d].cr, stream);
	}

	for (d = 0; d < 2; d++) {
		int rc[2] = { WAM_ROW, WAM_COL };
		fprintf(stream, "%s:\n", nw->u.w.d[d].xrp);
		dump_xr(nw->u.w.d[d].xr, rc[d], nw, stream);
	}

	for (j = 0; keys[j]; j++) {
		WAM *pr, *fs;
		if (!strcmp(keys[j], FSSKEY)) {
			if (fs = wam_fss_open(nw)) {
				fprintf(stream, "FSS:\n");
				wam_fss_dump(fs, stream);
				wam_close(fs);
			}
		}
		else if (pr = wam_prop_open(nw, WAM_ROW, keys[j])) {
			fprintf(stream, "%s:\n", pr->u.p.prp);
			dump_pr(pr->u.p.pr, WAM_ROW, nw, stream);
			if (wam_close(pr) == -1) {
				fprintf(stderr, "wam_close failed\n");
			}
		}
	}
	if (wam_close(nw) == -1) {
		fprintf(stderr, "wam_close failed\n");
	}
}

static int
dump_cw(cw, stream)
NWDB *cw;
FILE *stream;
{
	nwdb_rewind(cw);
	for (;;) {
		char *name;
		struct cw_entry *cwe;
		size_t size;

		switch (nwdb_seq(cw, &name, &cwe, &size)) {
		case -1:
			return -1;
		case 0:
			fprintf(stream, "cw: %s: %"PRIuIDX_T" %"PRIdFREQ_T" %"PRIdDF_T"\n", name, cwe->id, cwe->tf, cwe->df);
			break;
		case 1:
			return 0;
		default:
			return -1;
		}
	}
}

static int
dump_cr(cr, stream)
NWDB *cr;
FILE *stream;
{
	nwdb_rewind(cr);
	for (;;) {
		idx_t id;
		struct cr_entry *cre;
		size_t size;

		switch (nwdb_seq(cr, &id, &cre, &size)) {
		case -1:
			return -1;
		case 0:
			fprintf(stream, "cr: %"PRIuIDX_T": %s %"PRIdFREQ_T" %"PRIdDF_T"\n", id, cre->name, cre->tf, cre->df);
			break;
		case 1:
			return 0;
		default:
			return -1;
		}
	}
}

static int
dump_xr(xr, dir, nw, stream)
NWDB *xr;
WAM *nw;
FILE *stream;
{
	df_t i, max;
	int rdir;
	max = 8;
	rdir = WAM_REVERT_DIRECTION(dir);
	nwdb_rewind(xr);
	for (;;) {
		idx_t id;
		struct xr_vec *v;
		size_t size;

		switch (nwdb_seq(xr, &id, &v, &size)) {
		case -1:
			return -1;

		case 0:

			fprintf(stream, "xr: {%"PRIuIDX_T", \"%s\"}", id, wam_id2name(nw, dir, id));
			fprintf(stream, ": {%"PRIdDF_T", %"PRIdFREQ_T"}", v->elem_num, v->freq_sum);

			for (i = 0; i < v->elem_num && (max == 0 || i < max); i++) {
				struct xr_elem const *e = &v->elems[i];
				fprintf(stream, ", [%"PRIuIDX_T", \"%s\", %"PRIdFREQ_T"]", e->id, wam_id2name(nw, rdir, e->id), e->freq);
			}
			if (i < v->elem_num) {
				fprintf(stream, ", ...");
			}

			fprintf(stream, "\n");
			break;
		case 1:
			return 0;
		default:
			return -1;
		}
	}
}

static int
dump_pr(pr, dir, nw, stream)
NWDB *pr;
WAM *nw;
FILE *stream;
{
	size_t i;
	nwdb_rewind(pr);
	for (;;) {
		idx_t id;
		char *value;
		size_t size;

		switch (nwdb_seq(pr, &id, &value, &size)) {
		case -1:
			return -1;

		case 0:
			fprintf(stream, "pr: {%"PRIuIDX_T", \"%s\"}", id, wam_id2name(nw, dir, id));

			for (i = 0; i < size; i++) {
				putc(value[i], stream);
			}

			fprintf(stream, "\n");
			break;
		case 1:
			return 0;
		default:
			return -1;
		}
	}
}

static int
p1_eject_vec(r, ci, rs)
struct raw_vec *r;
struct corpus_i *ci;
struct rvst *rs;
{
	df_t i;
	size_t l = 0;
	freq_t freq_sum;
	idx_t id;

	struct cw_entry *cwe;
	size_t size;

	if (r->type == RAWVEC_PROP) {
		if (reserved_prop(r->p)) {
			syslog(LOG_DEBUG, "warning: reserved attribute: %s", r->p);
			return 0;
		}
		if (add_ci(ci, r->p, 0) != 0) {
			return -1;
		}
		return 0;
	}
	if (r->type == RAWVEC_IOPT) {
		free(rs->fss.ioptions);
		if (!(rs->fss.ioptions = strdup(r->p))) {
			return -1;
		}
		return 0;
	}

	assert(r->type == RAWVEC_VEC);

	switch (nwdb_get(rs->nw->u.w.d[0].cw, r->name, &cwe, &size)) {
	case -1:
		syslog(LOG_DEBUG, "nwdb_get: %s", strerror(errno));
		return -1;
	case 0:
		fprintf(stderr, "WARNING: duplicated article \"%s\" ignored.\n", r->name);
		return 0;
	case 1:	
		break;
	}

	freq_sum = 0;
	for (i = 0; i < r->elem_num; i++) {
		struct raw_elem *re = &r->elems[i];

		if ((id = insert_cw(re->name, 1, re->tf, rs->nw->u.w.d[1].cw, &rs->nw->u.w.d[1].maxid, rs->nw->u.w.d[1].cr, rs)) == 0) {
			return -1;
		}
		freq_sum += re->tf;
#if defined USEIDFORIF
		re->id = id;
#endif
	}
	if ((id = insert_cw(r->name, r->elem_num, freq_sum, rs->nw->u.w.d[0].cw, &rs->nw->u.w.d[0].maxid, rs->nw->u.w.d[0].cr, rs)) == 0) {
		return -1;
	}
#if defined USEIDFORIF
	r->id = id;
#endif

	if (rs->ifrg.stream && rs->ifrg.ej_size > IFRGSZ) {
		fclose(rs->ifrg.stream);
		fprintf(stderr, "$>");
		rs->ifrg.stream = NULL;
		rs->ifrg.ej_size = 0;
	}
	if (!rs->ifrg.stream) {
		char template[MAXPATHLEN];
		int fd;
		if (rs->ifrg.n >= nelems(rs->ifrg.paths)) {
			syslog(LOG_DEBUG, "p1_eject_vec: too many fragments: %s", strerror(errno));
			return -1;
		}
		snprintf(template, sizeof template, "%s" DIRECTORY_DELIMITER "nwamXXXXXX", rs->tmpdir);
		if ((fd = mkstemp(template)) == -1) {
			syslog(LOG_DEBUG, "p1_eject_vec: mkstemp: %s", strerror(errno));
			syslog(LOG_DEBUG, "%s: %s", template, strerror(errno));
			return -1;
		}
		nwam_add_tmp(template, NWAM_F);
		if (!(rs->ifrg.stream = fdopen(fd, "wb"))) {
			syslog(LOG_DEBUG, "p1_eject_vec: fdopen: %s", strerror(errno));
			return -1;
		}
		if (!(rs->ifrg.paths[rs->ifrg.n] = strdup(template))) {
			syslog(LOG_DEBUG, "p1_eject_vec: nomem: %s", strerror(errno));
			return -1;
		}
		fprintf(stderr, "$<%s", basnam(rs->ifrg.paths[rs->ifrg.n]));
		rs->ifrg.n++;
	}

#if defined USEIDFORIF
	l += fprintf(rs->ifrg.stream, "i%"PRIuIDX_T"\n", r->id);
	for (i = 0; i < r->elem_num; i++) {
		struct raw_elem *re = &r->elems[i];
		l += fprintf(rs->ifrg.stream, "%"PRIdFREQ_T" %"PRIuIDX_T"\n", re->tf, re->id);
	}
#else
	l += fprintf(rs->ifrg.stream, "i%s\n", r->name);
	for (i = 0; i < r->elem_num; i++) {
		struct raw_elem *re = &r->elems[i];
		l += fprintf(rs->ifrg.stream, "%"PRIdFREQ_T" %s\n", re->tf, re->name);
	}
#endif
	for (i = 0; i < r->nprops; i++) {
		struct raw_prop *rp = &r->props[i];
		if (rp->value_size) {
			l += fprintf(rs->ifrg.stream, "#%s=%s\n", rp->key, rp->value);
		}
	}
	for (i = 0; i < r->nsegms; i++) {
		struct raw_prop *rp = &r->segms[i];
		l += fprintf(rs->ifrg.stream, "+%s\n", rp->value);
	}
	rs->ifrg.ej_size += l;

	return 0;
}

static idx_t
insert_cw(name, df, tf, cw, maxid, cr, rs)
char *name;
df_t df;
freq_t tf;
NWDB *cw, *cr;
idx_t *maxid;
struct rvst *rs;
{
	struct cw_entry *cwe, ncwe;
	struct cr_entry *cre, *ncre;
	size_t l, size;
	idx_t id;

	switch (nwdb_get(cw, name, &cwe, &size)) {
	case -1:
		syslog(LOG_DEBUG, "%s: %s", name, strerror(errno));
		return 0;
	case 0:	/* update */
		ncwe = *cwe;
		ncwe.df += df;
		ncwe.tf += tf;
		break;
	case 1:	/* create new */
		ncwe.id = ++*maxid;
		ncwe.df = df;
		ncwe.tf = tf;
		break;
	}

	if (nwdb_put(cw, name, &ncwe, sizeof ncwe) == -1) {
		syslog(LOG_DEBUG, "%s: %s", name, strerror(errno));
		return 0;
	}

	id = ncwe.id;

	l = strlen(name) + 1;
	size = offsetof(struct cr_entry, name[l]);
	if (rs->s.ncre_size < size) {
		void *new;
		size_t newsize = size;
		newsize = NA(newsize, 128);
		if (!(new = realloc(rs->s.ncre, newsize))) {
			syslog(LOG_DEBUG, "realloc: %s", strerror(errno));
			return 0;
		}
		rs->s.ncre_size = newsize;
		rs->s.ncre = new;
	}
	ncre = rs->s.ncre;
	memmove(ncre->name, name, l);

	switch (nwdb_get(cr, &id, &cre, &size)) {
	case -1:
		syslog(LOG_DEBUG, "%s: %s", name, strerror(errno));
		return 0;
	case 0:	/* update */
		ncre->df = cre->df + df;
		ncre->tf = cre->tf + tf;
		break;
	case 1:	/* create new */
		ncre->df = df;
		ncre->tf = tf;
		break;
	}
	if (nwdb_put(cr, &id, ncre, size) == -1) {
		syslog(LOG_DEBUG, "%s: %s", name, strerror(errno));
		return 0;
	}
	return id;
}

static int
dec_cw(id, df, tf, d, rs)
idx_t id;
df_t df;
freq_t tf;
struct rvst *rs;
{
	NWDB *cw, *cr;
	struct cw_entry *cwe, ncwe;
	struct cr_entry *cre, *ncre;
	size_t l, size;

	cw = rs->nw->u.w.d[d].cw;
	cr = rs->nw->u.w.d[d].cr;

	switch (nwdb_get(cr, &id, &cre, &size)) {
	default:
	case -1:
		syslog(LOG_DEBUG, "%"PRIuIDX_T": %s", id, strerror(errno));
		return -1;
	case 1:	/* create new */
		syslog(LOG_DEBUG, "dec_cw: internal error: nwdb_get(%"PRIuIDX_T") failed", id);
		return -1;
	case 0:	/* update */
		l = strlen(cre->name) + 1;
		size = offsetof(struct cr_entry, name[l]);
		if (rs->s.ncre_size < size) {
			void *new;
			size_t newsize = size;
			newsize = NA(newsize, 128);
			if (!(new = realloc(rs->s.ncre, newsize))) {
				syslog(LOG_DEBUG, "realloc: %s", strerror(errno));
				return -1;
			}
			rs->s.ncre_size = newsize;
			rs->s.ncre = new;
		}
		ncre = rs->s.ncre;
		memmove(ncre, cre, size);

		if (ncre->df == rs->nw->u.w.d[d].max_elem_num) {
			rs->flags |= RVST_LOST_MAX;
		}
		if (ncre->tf == rs->nw->u.w.d[d].max_freq_sum) {
			rs->flags |= RVST_LOST_MAX;
		}
		ncre->df -= df;
		ncre->tf -= tf;
		if (ncre->df == 0) {
			assert(ncre->tf == 0);
			if (delete_cw(id, cre->name, cw, cr) == -1) {
				syslog(LOG_DEBUG, "delete_cw failed");
				return -1;
			}
			return 0;
		}
		break;
	}
	if (nwdb_put(cr, &id, ncre, size) == -1) {
		syslog(LOG_DEBUG, "%"PRIuIDX_T": %s", id, strerror(errno));
		return -1;
	}

	switch (nwdb_get(cw, ncre->name, &cwe, &size)) {
	default:
	case -1:
		syslog(LOG_DEBUG, "%s: %s", ncre->name, strerror(errno));
		return -1;
	case 1:	/* create new */
		syslog(LOG_DEBUG, "dec_cw: internal error: nwdb_get(\"%s\") failed", ncre->name);
		return -1;
	case 0:	/* update */
		ncwe = *cwe;
		ncwe.df -= df;
		ncwe.tf -= tf;
		assert(ncre->df != 0);
		break;
	}
	if (nwdb_put(cw, ncre->name, &ncwe, sizeof ncwe) == -1) {
		syslog(LOG_DEBUG, "%s: %s", ncre->name, strerror(errno));
		return -1;
	}
	return 0;
}

static int
delete_cw(id, name, cw, cr)
idx_t id;
char *name;
NWDB *cw, *cr;
{
	if (nwdb_del(cw, name) != 0) {
		return -1;
	}
	if (nwdb_del(cr, &id) != 0) {
		return -1;
	}
	return 0;
}

static int
insert_xr(id, v, xr)
idx_t id;
struct xr_vec *v;
NWDB *xr;
{
	if (nwdb_put(xr, &id, v, sizeof_dxr_vec(v->elem_num)) == -1) {
		syslog(LOG_DEBUG, "insert_xr: update db failed: id = %"PRIuIDX_T, id);
		return -1;
	}
	return 0;
}

static int
retrieve_xr(id, v, xr)
idx_t id;
struct xr_vec **v;
NWDB *xr;
{
	size_t size;

	switch (nwdb_get(xr, &id, v, &size)) {
	case -1:
		return -1;
	case 1:
		return 1;
	case 0:
		if (size != sizeof_dxr_vec((*v)->elem_num)) {
			return -1;
		}
		return 0;
	}
	return -1;
}

df_t
wam_get_vec(nw, d, id, v)
WAM *nw;
idx_t id;
struct xr_vec const **v;
{
	NWDB *xr;
	size_t size;

	if (nw->u.w.dist.role == NDWAM_ROLE_PARENT) {
		assert(nw->u.w.xs);
		if (!(*v = cb_wam_get_vec(nw->u.w.xs, d, id))) {
			return -1;
		}
		return (*v)->elem_num;
	}

	xr = nw->u.w.d[d].xr;

	switch (nwdb_get(xr, &id, v, &size)) {
	case -1:
	case 1:
	default:
		return -1;
	case 0:
		if (size != sizeof_dxr_vec((*v)->elem_num)) {
			syslog(LOG_DEBUG, "internal error: size(%"PRIuSIZE_T") != x%"PRIuSIZE_T, (pri_size_t)size, (pri_size_t)sizeof_dxr_vec((*v)->elem_num));
			return -1;
		}
		if (nw->u.w.d[d].mask_set) {
			if (nw->u.w.tv.size < size) {
				void *new;
				size_t newsize = size;
				newsize = NA(newsize, 128);
				if (!(new = realloc(nw->u.w.tv.ptr, newsize))) {
					return -1;
				}
				nw->u.w.tv.size = newsize;
				nw->u.w.tv.ptr = new;
			}
			memmove(nw->u.w.tv.ptr, *v, size);
			mask_vec(nw->u.w.tv.ptr, nw->u.w.d[d].pmask.mask, nw->u.w.d[d].pmask.len, nw->u.w.d[d].nmask.mask, nw->u.w.d[d].nmask.len); 
			*v = nw->u.w.tv.ptr;
		}
		return (*v)->elem_num;
	}
	/* NOTREACHED */
}

static int
delete_xr(id, xr)
idx_t id;
NWDB *xr;
{
	struct xr_vec *v0;
	size_t size;

	switch (nwdb_get(xr, &id, &v0, &size)) {
	default:
		syslog(LOG_DEBUG, "delete_xr: error: id=%"PRIuIDX_T, id);
		return -1;
	case 0:
		if (nwdb_del(xr, &id) == -1) {
			return -1;
		}
		return 0;
	}
}

static int
delete_pr(id, pr)
idx_t id;
NWDB *pr;
{
	char *v0;
	size_t size;

	switch (nwdb_get(pr, &id, &v0, &size)) {
	default:
		syslog(LOG_DEBUG, "delete_pr: error: id=%"PRIuIDX_T, id);
		return -1;
	case 1:
		return 0;	/* no error. */
	case 0:
		if (nwdb_del(pr, &id) == -1) {
			return -1;
		}
		return 0;
	}
}

static int
regenerate_max(rs)
struct rvst *rs;
{
	int d;
	for (d = 0; d < 2; d++) {
		freq_t max_freq_sum;
		df_t max_elem_num;
		max_elem_num = 0;
		max_freq_sum = 0;
		nwdb_rewind(rs->nw->u.w.d[d].cr);
		for (;;) {
			idx_t id;
			struct cr_entry *cre;
			size_t size;

			switch (nwdb_seq(rs->nw->u.w.d[d].cr, &id, &cre, &size)) {
			case -1:
				return -1;
			case 0:
				if (max_elem_num < cre->df) {
					max_elem_num = cre->df;
				}
				if (max_freq_sum < cre->tf) {
					max_freq_sum = cre->tf;
				}
				break;
			case 1:
				goto cont;
			default:
				return -1;
			}
		}
	cont: ;
		rs->nw->u.w.d[d].max_elem_num = max_elem_num;
		rs->nw->u.w.d[d].max_freq_sum = max_freq_sum;
	}
	return 0;
}

static int
p2_eject_vec(r, rs, dc)
struct raw_vec *r;
struct rvst *rs;
struct distconf_t *dc;
{
	size_t j;
	df_t i;
	df_t elem_num;
	freq_t freq_sum;
	idx_t id;
	int myxr, mypi;

#if defined USEIDFORIF
	char *end;
	id = strtoul(r->name, &end, 10);
	if (!(*r->name && !*end)) {
		syslog(LOG_DEBUG, "internal error: strtoul failed for [%s]", r->name);
		return -1;
	}
#else
	if ((id = wam_name2id(rs->nw, WAM_ROW, r->name)) == 0) {
		syslog(LOG_DEBUG, "internal error: wam_name2id(row) failed for [%s]", r->name);
		return -1;
	}
#endif
	if (rs->v_size < r->elem_num) {
		void *new;
		size_t newsize = r->elem_num;
		newsize = NA(newsize, 32);
		if (!(new = realloc(rs->v, sizeof_dxr_vec(newsize)))) {
			free(rs->v);
			rs->v = NULL;
			rs->v_size = 0;
			return -1;
		}
		rs->v_size = newsize;
		rs->v = new;
	}
	rs->v->elem_num = r->elem_num;
	for (i = 0, freq_sum = 0; i < r->elem_num; i++) {
		struct rc_pair rcp;
		struct raw_elem *re = &r->elems[i];
		struct xr_elem *e = &rs->v->elems[i];
#if defined USEIDFORIF
		char *end;
		e->id = strtoul(re->name, &end, 10);
		if (!(*re->name && !*end)) {
			syslog(LOG_DEBUG, "internal error: strtoul failed for [%s]", r->name);
			return -1;
		}
#else
		if ((e->id = wam_name2id(rs->nw, WAM_COL, re->name)) == 0) {
			syslog(LOG_DEBUG, "internal error: wam_name2id(col) failed for [%s]", r->name);
			return -1;
		}
#endif
		e->freq = re->tf;
		freq_sum += re->tf;
		rcp.r_id = id;
		rcp.c_id = e->id;
		rcp.tf = re->tf;
		if (p2_eject_pair(&rcp, rs) == -1) {
			return -1;
		}
	}
	rs->v->freq_sum = freq_sum;

	qsort(rs->v->elems, rs->v->elem_num, sizeof rs->v->elems[0], ecompar);
	uniq_vec(rs);

	elem_num = r->elem_num;

	switch (dc->role) {
	case NDWAM_MONOLITHIC:
		myxr = mypi = 1;
		break;
	case NDWAM_ROLE_PARENT:
		myxr = mypi = 0;
		break;
	case NDWAM_ROLE_CHILD:
		switch (dc->dir) {
		case WAM_COL:
			reduce_vec(rs->v, dc);
			myxr = 1, mypi = 0;
			break;
		case WAM_ROW:
			myxr = mypi = minep(id, dc);
			break;
		default:
			return -1;
		}
		break;
	default:
		return -1;
	}

	if (myxr) {
		if (insert_xr(id, rs->v, rs->nw->u.w.d[0].xr) == -1) {
			return -1;
		}
	}
	if (rs->nw->u.w.d[0].max_elem_num < elem_num) {
		rs->nw->u.w.d[0].max_elem_num = elem_num;
	}
	if (rs->nw->u.w.d[0].max_freq_sum < freq_sum) {
		rs->nw->u.w.d[0].max_freq_sum = freq_sum;
	}
	rs->nw->u.w.d[0].nentries++;
	rs->nw->u.w.total_elem_num += elem_num;
	rs->nw->u.w.total_freq_sum += freq_sum;

	if (!mypi) {
		return 0;
	}

	for (j = 0; j < rs->nprs; j++) {
		WAM *pr = rs->prs[j];
		if (r->props[j].value && *r->props[j].value) {
			if (insert_pr(id, &r->props[j], pr->u.p.pr) == -1) {
				return -1;
			}
			pr->u.p.nentries++;
		}
	}

	if (rs->make_fssi) {
		struct flwd_idx idx;
		ssize_t l;
		size_t size;
		struct fss_rmap_ent_t rmap_e0, *rmap_e;
		idx.id = id;

		switch (nwdb_get(rs->fss.rmap->u.p.pr, &id, &rmap_e, &size)) {
		case -1:
		case 0:	/* overwriting fss is prohibited */
			syslog(LOG_DEBUG, "nwdb_get fss.rmap failed");
			return -1;
		case 1:
			/* do nothing */
			break;
		}
		rmap_e = &rmap_e0;
		rmap_e->id = id;
		rmap_e->j = rs->fss.j - 1;
		rmap_e->offset = ftell(rs->fss.flwd);	/* XXX */
		if (nwdb_put(rs->fss.rmap->u.p.pr, &id, rmap_e, sizeof *rmap_e) == -1) {
			syslog(LOG_DEBUG, "nwdb_put fss.rmap failed");
			return -1;
		}

		for (j = 0; j < r->nsegms; j++) {
#if ! defined FLWD_UTF32
			size_t pad;
#endif
			struct raw_prop *rp = &r->segms[j];

			idx.segid = j;
			if (fwrite(&idx, sizeof idx, 1, rs->fss.flwd) != 1) {
				return -1;
			}
#if defined FLWD_UTF32
			if (fwrite("\0\0\0\0", 4, 1, rs->fss.flwd) != 1) {
				return -1;
			}
			if ((l = fwputs(rp->value, rs->fss.flwd)) == -1) {
				return -1;
			}
			rs->fss.ej_size += sizeof idx + l + 4;
#else
			if (fwrite("", 1, 1, rs->fss.flwd) != 1) {
				return -1;
			}
			l = strlen(rp->value) + 1;
			if (fwrite(rp->value, 1, l, rs->fss.flwd) != l) {
				return -1;
			}
			rs->fss.ej_size += sizeof idx + 1 + l;
			pad = FWIDXALIGN - (sizeof idx + 1 + l) % FWIDXALIGN;
			if (pad != FWIDXALIGN) {
				if (fwrite(FWPAD, 1, pad, rs->fss.flwd) != pad) {
					return -1;
				}
				rs->fss.ej_size += pad;
			}
#endif
		}
		if (rs->fss.ej_size > FSSFRGSZ) {
			struct fss_paths *p;
			if (rs->fss.j >= MAXNFSSFRG) {
				syslog(LOG_DEBUG, "too many fss-fragments");
				return -1;
			}
			fclose(rs->fss.flwd);
			p = &rs->fss.f->u.fss.paths[rs->fss.j];
			if (!(rs->fss.flwd = fopen(p->flwd, "wb"))) {
				syslog(LOG_DEBUG, "%s: %s", p->flwd, strerror(errno));
				return -1;
			}
			rs->fss.j++;
			rs->fss.ej_size = 0;
		}
	}

	return 0;
}

static int
p2_eject_pair(rcp, rs)
struct rc_pair *rcp;
struct rvst *rs;
{
	if (rs->pfrg.stream && rs->pfrg.ej_size > PFRGSZ) {
		fclose(rs->pfrg.stream);
		fprintf(stderr, "$)");
		rs->pfrg.stream = NULL;
		rs->pfrg.ej_size = 0;
	}
	if (!rs->pfrg.stream) {
		char template[MAXPATHLEN];
		int fd;
		if (rs->pfrg.n >= nelems(rs->pfrg.paths)) {
			syslog(LOG_DEBUG, "p2_eject_pair: too many fragments: %s", strerror(errno));
			return -1;
		}
		snprintf(template, sizeof template, "%s" DIRECTORY_DELIMITER "nwamXXXXXX", rs->tmpdir);
		if ((fd = mkstemp(template)) == -1) {
			syslog(LOG_DEBUG, "p2_eject_pair: mkstemp: %s", strerror(errno));
			syslog(LOG_DEBUG, "%s: %s", template, strerror(errno));
			return -1;
		}
		nwam_add_tmp(template, NWAM_F);
		if (!(rs->pfrg.stream = fdopen(fd, "wb"))) {
			syslog(LOG_DEBUG, "p2_eject_pair: fdopen: %s", strerror(errno));
			return -1;
		}
		if (!(rs->pfrg.paths[rs->pfrg.n] = strdup(template))) {
			syslog(LOG_DEBUG, "p2_eject_pair: nomem: %s", strerror(errno));
			return -1;
		}
		fprintf(stderr, "$(%s", basnam(rs->pfrg.paths[rs->pfrg.n]));
		rs->pfrg.n++;
	}
	if (fwrite(rcp, sizeof *rcp, 1, rs->pfrg.stream) != 1) {
		syslog(LOG_DEBUG, "%s: %s", rs->pfrg.paths[rs->pfrg.n], strerror(errno));
		return -1;
	}
	rs->pfrg.ej_size += sizeof *rcp;
	return 0;
}

static struct raw_vec *
new_raw_vec(keys)
char const **keys;
{
	struct raw_vec *p;
	size_t i, n;

	if (!(p = calloc(1, sizeof *p))) {
		return NULL;
	}
	p->type = 0;
	p->p = NULL;

	p->st.elems_size = 0;
	p->st.name_size = 0;

	p->name = NULL;
	p->elem_num = 0;
	p->elems = NULL;

	for (i = 0, n = 0; keys[i]; i++) {
		if (!strcmp(keys[i], FSSKEY)) continue;
		n++;
	}
	if (!(p->props = calloc(n, sizeof *p->props))) {
		free(p);
		return NULL;
	}
	for (i = 0, p->nprops = 0; keys[i]; i++) {
		struct raw_prop *pr;
		if (!strcmp(keys[i], FSSKEY)) continue;
		pr = &p->props[p->nprops++];
		pr->key = keys[i];
		pr->value = NULL;
		pr->value_size = 0;
	}
	assert(n == p->nprops);
	qsort(p->props, p->nprops, sizeof *p->props, pcompar);

	for (i = 0; i < nelems(p->segms); i++) {
		struct raw_prop *pr = &p->segms[i];
		pr->key = NULL;
		pr->value = NULL;
		pr->value_size = 0;
	}

	return p;
}

static int
read_raw_vec(r, rs
#if defined ENABLE_RAWVECLEN_LIMIT
	, rawveclen_limit
#endif
)
struct raw_vec *r;
struct rvst *rs;
#if defined ENABLE_RAWVECLEN_LIMIT
df_t rawveclen_limit;
#endif
{
	size_t i;
	char *p;

++rs->pi.read;
PI_CHECK(&rs->pi.pi, rs->pi.read);

	for (i = 0; i < r->nprops; i++) {
		if (r->props[i].value) {
			*r->props[i].value = '\0';
		}
	}
	r->nsegms = 0;
	for (; p = rs_readln(rs); ) {
		if (!*p) {
			p = NULL;
		}
		else {
			chop(p);
			if (rs->lineno == 1 && memcmp(p, BOM, 3) == 0) {
				p += 3;
			}
		}
#define	RETURN_EOF	-2
#define	GOTO_EOV	1
#define	GOTO_MALF	2
#define	GOTO_NOMEM	3
#define	GOTO_BRK	4
#define	RETURN_ZERO	5
		switch (itb_prochdr(r, rs, p)) {
		case RETURN_EOF:
			return -2;
		case -1:
			return -1;
		case 0:
			break;
		case GOTO_MALF:
			goto malf;
		case GOTO_NOMEM:
			goto nomem;
		case GOTO_BRK:
			goto brk;
		case RETURN_ZERO:
			return 0;
		default:
			fprintf(stderr, "internal error: !itb_prochdr\n");
			abort();
		}
	}
	assert(!p);
	return -1;	/* ERROR */
brk:
	if (xstrdup(&r->name, &r->st.name_size, p + 1) == -1) {
		goto nomem;
	}
	r->elem_num = 0;
	for (; p = rs_readln(rs); ) {
		if (!*p) {
			p = NULL;
		}
		else {
			chop(p);
			if (rs->lineno == 1 && memcmp(p, BOM, 3) == 0) {
				p += 3;
			}
		}
		switch (itb_procline(r, rs, p)) {
		case -1:
			return -1;
		case 0:
			break;
		case GOTO_EOV:
			goto eov;
		case GOTO_MALF:
			goto malf;
		case GOTO_NOMEM:
			goto nomem;
		default:
			fprintf(stderr, "internal error: !itb_procline\n");
			abort();
		}
	}
	assert(!p);
	return -1;	/* ERROR */
eov:
	if (rs->pass == 1 && rs->ps.nn.n) {	/* fullwd && pass 1 */
		assert(rs->make_fssi);
		if (rs->fss.no_last_segid) {
			rs->fss.last_segid = r->nsegms;
			rs->fss.no_last_segid = 0;
		}
		else if (r->nsegms != rs->fss.last_segid) {
			fprintf(stderr, "warning: seg # changes from %"PRIuIDX_T" to %"PRIuIDX_T", at file %s line %"PRIuSIZE_T"\n", rs->fss.last_segid, r->nsegms, rs->path, (pri_size_t)rs->lineno);
		}
	}
#if defined ENABLE_RAWVECLEN_LIMIT
	if (rawveclen_limit && r->elem_num > rawveclen_limit) {
		r->elem_num = rawveclen_limit;
	}
#endif
	sort_uniq_rvec(r);
	r->type = RAWVEC_VEC;
	return 1;
malf:
	fprintf(stderr, "malformed line: file = %s, lineo = %"PRIuSIZE_T", byte = %"PRIuSIZE_T", [%s]\n", rs->path, (pri_size_t)rs->lineno, (pri_size_t)rs->pos, p ? p : "(null)");
	return -1;
nomem:
	fprintf(stderr, "memory exhausted\n");
	return -1;
}

static int
itb_prochdr(r, rs, p)
struct raw_vec *r;
struct rvst *rs;
char *p;
{
	if (!p) {
		return RETURN_EOF;
	}

	switch (*p) {
	case '\0':
		return 0;	/* ignore this line */
	case 'i':
		return GOTO_BRK;
	case '@':
		if (rs->pass == 1) {
			r->type = RAWVEC_PROP;
			free(r->p);
			if (!(r->p = strdup(p))) {
				return GOTO_NOMEM;
			}
			return RETURN_ZERO;
		}
		break;
	case '$':
		if (!strncmp(p, "$ioptions=", 10)) {
			r->type = RAWVEC_IOPT;
			free(r->p);
			if (!(r->p = strdup(p + 10))) {
				return GOTO_NOMEM;
			}
			return RETURN_ZERO;
		}
		break;
	default:
		return GOTO_MALF;
	}
	return 0;
}

static int
itb_procline(r, rs, p)
struct raw_vec *r;
struct rvst *rs;
char *p;
{
	if (!p) {
		if (rs->path) {
			fprintf(stderr, "]");
			free(rs->path);
			rs->path = NULL;
		}
		return GOTO_EOV;
	}

	switch (*p) {
		freq_t tf;
		char *end;
		struct raw_elem *re;
		struct raw_prop *rp, rpk;
		char *key, *value, *q;
	case '\0':
		return 0;	/* ignore this line */
	case '@':
	case '$':
	case 'i':
		if (rs_unread(rs) == -1) {
			fprintf(stderr, "internal error: double pushback error\n");
			return -1;
		}
		return GOTO_EOV;
	case '#':
		key = p + 1;
		if (!(q = index(key, '='))) {
			return GOTO_MALF;
		}
		*q++ = '\0';
		value = q;
		rpk.key = key;
		if (rp = bsearch(&rpk, r->props, r->nprops, sizeof *r->props, pcompar)) {
			if (xappend(&rp->value, &rp->value_size, value) == -1) {
				return GOTO_NOMEM;
			}
		}
		break;
	case ' ':
		tf = 1;
		end = p + 1;
		goto num;
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		tf = strtoul(p, &end, 10);	/*XXX*/
		if (*end != ' ') {
			return GOTO_MALF;
		}
		end++;
	num:
		if (r->st.elems_size <= r->elem_num) {
			if (extend_elems(r) == -1) {
				return GOTO_NOMEM;
			}
		}
		re = &r->elems[r->elem_num++];	/* safe */
		re->tf = tf;
		if (xstrdup(&re->name, &re->name_size, end) == -1) {
			return GOTO_NOMEM;
		}
		break;
	case 'b':
		if (rs->pass != 1) break;
		if (proc_body(r, rs, p) == -1) {
			fprintf(stderr, "proc_body failed\n");
			return -1;
		}
		break;
	case '+':
	case '!':
		if (!rs->make_fssi) break;	/* no fullwd */
		if (proc_flwd(r, rs, p) == -1) {
			fprintf(stderr, "proc_flwd failed\n");
			return -1;
		}
		break;
	default:
		return GOTO_MALF;
	}
	return 0;
}
#undef	GOTO_BRK
#undef	GOTO_NOMEM
#undef	GOTO_MALF
#undef	GOTO_EOV

static char *
rs_readln(rs)
struct rvst *rs;
{
	char *p;
	if (rs->relay_out) {
rs_readln_relay_out:
		p = readln(rs->relay_out);
		if (p && p[0] == '.') {
			if (p[1] != '.') {
				if (rs->path) {
					fprintf(stderr, "]");
					free(rs->path);
				}
				rs->pos = 0;
				rs->lineno = 0;
				chop(p);
				if (!(rs->path = strdup(p + 2))) {
					fprintf(stderr, "memory exhausted\n");
					return NULL;
				}
				fprintf(stderr, "[%s ", basnam(rs->path));
				goto rs_readln_relay_out;
			}
			else {
				p++;
			}
		}
		goto ret_p;
	}

rs_readln_stream:
	if (!rs->stream) {
		if (rs->ifrg.j >= rs->ifrg.n) {	/* EOF */
			return "";
		}
		if (!(rs->stream = nopen(rs->ifrg.paths[rs->ifrg.j], "rb"))) {	/* ERROR */
			return NULL;
		}
		fprintf(stderr, "<%s", basnam(rs->ifrg.paths[rs->ifrg.j]));
		rs->pos = 0;
		rs->lineno = 0;
	}
	if (!(p = readln(rs->stream))) {	/* ERROR */
		nfclose(rs->stream);
		fprintf(stderr, ">");
		rs->stream = NULL;
		return NULL;
	}
	if (*p == '\0') {	/* EOF. try next file. */
		nfclose(rs->stream);
		fprintf(stderr, ">");
		rs->stream = NULL;
		rs->ifrg.j++;
		goto rs_readln_stream;
	}

ret_p:
	rs->last_l = strlen(p);
	rs->pos += rs->last_l;
	rs->lineno++;
	return p;
}

static int
rs_unread(rs)
struct rvst *rs;
{
	if (rs->last_l == 0) {
		return -1;
	}
	rs->pos -= rs->last_l;
	rs->lineno--;
	rs->last_l = 0;
	if (rs->relay_out) {
		return nfunread(rs->relay_out);
	}
	else if (rs->stream) {
		return nfunread(rs->stream);
	}
	else {
		return -1;
	}
}

static void
sort_uniq_rvec(r)
struct raw_vec *r;
{
	struct raw_elem *p, *q, *x, *e;
	qsort(r->elems, r->elem_num, sizeof *r->elems, rcompar);
	for (x = p = r->elems, e = x + r->elem_num; p < e; p = q, x++) {
		for (q = p + 1; q < e && !strcmp(p->name, q->name); q++) {
			p->tf += q->tf;
			r->elem_num--;
		}
		if (p != x) {
			struct raw_elem t;
			t = *p;
			*p = *x;
			*x = t;
		}
	}
}

static void
chop(s)
char *s;
{
	size_t l = strlen(s);
	if (l > 0 && s[l - 1] == '\n') {
		s[--l] = '\0';
	}
	if (l > 0 && s[l - 1] == '\r') {
		s[--l] = '\0';
	}
}

static void
free_raw_vec(r)
struct raw_vec *r;
{
	size_t i;

	free(r->p);
	for (i = 0; i < r->st.elems_size; i++) {
		free(r->elems[i].name);
	}
	for (i = 0; i < r->nprops; i++) {
		free(r->props[i].value);
	}
	for (i = 0; i < nelems(r->segms); i++) {
		free(r->segms[i].value);
	}

	free(r->name);
	free(r->elems);
	free(r->props);

	free(r);
}

static void
setint32_t(n, s)
int32_t n;
char s[INT32_TWID];
{
	snprintf(s, INT32_TWID - 1, "%-"PRIdINT32_TWID"d", (int)n);	/*XXX*/
	s[INT32_TWID - 2] = '\r';
	s[INT32_TWID - 1] = '\n';
}

static void
setdf_t(n, s)
df_t n;
char s[DF_TWID];
{
	snprintf(s, DF_TWID - 1, "%-"PRIdDF_TWID PRIdDF_T, n);
	s[DF_TWID - 2] = '\r';
	s[DF_TWID - 1] = '\n';
}

static void
setidx_t(n, s)
idx_t n;
char s[IDX_TWID];
{
	snprintf(s, IDX_TWID - 1, "%-"PRIuIDX_TWID PRIuIDX_T, n);
	s[IDX_TWID - 2] = '\r';
	s[IDX_TWID - 1] = '\n';
}

static void
setfreq_t(n, s)
freq_t n;
char s[FREQ_TWID];
{
	snprintf(s, FREQ_TWID - 1, "%-"PRIdFREQ_TWID PRIdFREQ_T, n);
	s[FREQ_TWID - 2] = '\r';
	s[FREQ_TWID - 1] = '\n';
}

static int
icompar(va, vb)
const void *va, *vb;
{
	idx_t const *a, *b;
	a = va;
	b = vb;
	if (*a < *b) {
		return -1;
	}
	else if (*a > *b) {
		return 1;
	}
	return 0;
}

static int
pcompar(va, vb)
const void *va, *vb;
{
	struct raw_prop const *a, *b;
	a = va;
	b = vb;
	return strcmp(a->key, b->key);
}

static int
rcompar(va, vb)
void const *va, *vb;
{
	struct raw_elem const *a, *b;
	a = va;
	b = vb;
	return strcmp(a->name, b->name);
}

static int
ecompar(va, vb)
void const *va, *vb;
{
	struct xr_elem const *a, *b;
	a = va;
	b = vb;
	if (a->id < b->id) {
		return -1;
	}
	else if (a->id > b->id) {
		return 1;
	}
	return 0;
}

static int
xcompar(va, vb)
const void *va, *vb;
{
	struct rc_pair const *a, *b;
	a = va;
	b = vb;
	if (a->c_id < b->c_id) {
		return -1;
	}
	else if (a->c_id > b->c_id) {
		return 1;
	}
	else if (a->r_id < b->r_id) {
		return -1;
	}
	else if (a->r_id > b->r_id) {
		return 1;
	}
	return 0;
}

static int
extend_elems(r)
struct raw_vec *r;
{
	size_t i;
	void *new;
	size_t newsize = r->st.elems_size;
	newsize = NA(newsize + 1, 128);
	if (!(new = realloc(r->elems, newsize * sizeof *r->elems))) {
		return -1;
	}
	r->elems = new;
	for (i = r->st.elems_size; i < newsize; i++) {
		r->elems[i].name = NULL;
		r->elems[i].name_size = 0;
	}
	r->st.elems_size = newsize;
	return 0;
}

static int
xstrdup(p, size, t)
char **p;
size_t *size;
char const *t;
{
	size_t l = strlen(t) + 1;
	if (*size < l) {
		void *new;
		size_t newsize = l;
		newsize = NA(newsize, 128);
		if (!(new = realloc(*p, newsize))) {
			return -1;
		}
		*size = newsize;
		*p = new;
	}
	memmove(*p, t, l);
	return 0;
}

static int
xappend(p, size, t)
char **p;
size_t *size;
char const *t;
{
	size_t end = *p ? strlen(*p) : 0;
	size_t l = strlen(t) + 1;

	if (*size < end + l) {
		void *new;
		size_t newsize = end + l;
		newsize = NA(newsize, 128);
		if (!(new = realloc(*p, newsize))) {
			return -1;
		}
		*size = newsize;
		*p = new;
	}
	memmove(*p + end, t, l);
	return 0;
}

static int
setup_pstem(ps, g, make_fssi)
struct stm_d *ps;
struct wam_update_args *g;
{
	char const **paths, *stemmer;
	int stem_a_no, stem_n_no;
	size_t p;

	paths = g->i.itbs;
	stemmer = g->i.s.stemmer;
	stem_a_no = g->i.s.stem_a_no;
	stem_n_no = make_fssi ? g->i.s.stem_n_no : 0;

	assert(stem_a_no);

	ps->host = g->i.s.host;
	ps->serv = g->i.s.serv;
#if defined NO_LOCALSOCKET
	ps->sock = NULL;
#else
	ps->sock = g->i.s.sock;
#endif
#if ! defined NO_FORK
	ps->path = g->i.s.path;
#endif
	ps->reader0_arg.ps = NULL;
	ps->reader0_arg.stemmer = NULL;
	ps->reader0_arg.paths = NULL;

	ps->aa.n = stem_a_no;
	ps->aa.p = 0;
	ps->nn.n = stem_n_no;
	ps->nn.p = 0;

	if (ps->aa.n > nelems(ps->aa.x)) {
		return -1;
	}
	if (ps->nn.n > nelems(ps->nn.x)) {
		return -1;
	}

	for (p = 0; p < ps->aa.n; p++) {
		struct stm_x *e = &ps->aa.x[p];
#if ! defined NO_FORK
		e->pid = -1;
#endif
		e->ps = NULL;
		if (start_stmd(ps, e, 'A', g->environ) == -1) {
			return -1;
		}
	}
	for (p = 0; p < ps->nn.n; p++) {
		struct stm_x *e = &ps->nn.x[p];
#if ! defined NO_FORK
		e->pid = -1;
#endif
		e->ps = NULL;
		if (start_stmd(ps, e, 'N', g->environ) == -1) {
			return -1;
		}
	}

	if (start_reader(ps, stemmer, paths) == -1) {
		return -1;
	}

fprintf(stderr, ".P: %d\n", getpid());
	fcntl(ps->r.fds[0], F_SETFD, 0);
	if (!(ps->relay_out = fdopen(ps->r.fds[0], "rb"))) {
		syslog(LOG_DEBUG, "fdopen: %s", strerror(errno));
		return -1;
	}
	for (p = 0; p < ps->aa.n; p++) {
		struct stm_x *e = &ps->aa.x[p];
		fcntl(e->fds[0], F_SETFD, 0);
		e->fds[1] = -1;
		if (!(e->ps = xstem_create_pipe(e->fds, stemmer, e->flags))) {
			fprintf(stderr, "xstem_create_pipe A: %d,%d: %s\n", e->fds[0], e->fds[1], strerror(errno));
			return -1;
		}
	}
	for (p = 0; p < ps->nn.n; p++) {
		struct stm_x *e = &ps->nn.x[p];
		fcntl(e->fds[0], F_SETFD, 0);
		e->fds[1] = -1;
		if (!(e->ps = xstem_create_pipe(e->fds, NORMALIZER, e->flags))) {
			fprintf(stderr, "xstem_create_pipe N: %d,%d: %s\n", e->fds[0], e->fds[1], strerror(errno));
			return -1;
		}
	}
#if defined NO_FORK
	fcntl(ps->r.fds[1], F_SETFD, 0);
#endif
	fdcloseexec();
	return 0;
}

#if ! defined F_MAXFD
static int maxfd = 3;
#endif

static int
start_stmd(ps, x, nam, environ)
struct stm_d *ps;
struct stm_x *x;
char **environ;
{
#if ! defined NO_FORK
	int fds0[2], fds1[2];
#endif

	if (ps->host && ps->serv || ps->sock) {
		int s;
		if ((s = ga_nnet_cli(ps->host, ps->serv, ps->sock)) != -1) {
#if ! defined NO_FORK
			x->pid = -1;
#endif
			x->fds[0] = x->fds[1] = s;
			if (fcntl(s, F_SETFD, FD_CLOEXEC) == -1) {
				syslog(LOG_DEBUG, "fcntl: %s", strerror(errno));
			}
#if ! defined F_MAXFD
			if (maxfd < s) maxfd = s;
#endif
fprintf(stderr, "PSTEM@socket: %d\n", s);
			x->flags = XSTEM_SOCKET;
			return 0;
		}
		else {
			fprintf(stderr, "nnet_cli: %s\n", strerror(errno));
		}
	}
#if ! defined NO_FORK
	if (pipe(fds1) == -1) {
		syslog(LOG_DEBUG, "pipe: %s", strerror(errno));
		return -1;
	}
	if (fcntl(fds1[0], F_SETFD, FD_CLOEXEC) == -1) {
		syslog(LOG_DEBUG, "fcntl: %s", strerror(errno));
	}
	if (fcntl(fds1[1], F_SETFD, FD_CLOEXEC) == -1) {
		syslog(LOG_DEBUG, "fcntl: %s", strerror(errno));
	}
#if ! defined F_MAXFD
	if (maxfd < fds1[0]) maxfd = fds1[0];
	if (maxfd < fds1[1]) maxfd = fds1[1];
#endif
	if (pipe(fds0) == -1) {
		syslog(LOG_DEBUG, "pipe: %s", strerror(errno));
		return -1;
	}
	if (fcntl(fds0[0], F_SETFD, FD_CLOEXEC) == -1) {
		syslog(LOG_DEBUG, "fcntl: %s", strerror(errno));
	}
	if (fcntl(fds0[1], F_SETFD, FD_CLOEXEC) == -1) {
		syslog(LOG_DEBUG, "fcntl: %s", strerror(errno));
	}
#if ! defined F_MAXFD
	if (maxfd < fds0[0]) maxfd = fds0[0];
	if (maxfd < fds0[1]) maxfd = fds0[1];
#endif

	switch (x->pid = fork()) {
		char *argv[3];
	case -1:
		syslog(LOG_DEBUG, "fork: %s", strerror(errno));
		return -1;
	case 0:
		dup2(fds1[0], 0);
		dup2(fds0[1], 1);
		argv[0] = ps->path;
		argv[1] = "-q";
		argv[2] = NULL;
fprintf(stderr, ".pstem %c: %d\n", nam, getpid());
		if (execve(ps->path, argv, environ) == -1) {
			syslog(LOG_DEBUG, "%s: %s", ps->path, strerror(errno));
			_exit(1);
		}
		/* NOTREACHED */
		_exit(0);
	default:
		x->fds[0] = fds0[0];
		x->fds[1] = fds1[1];
		break;
	}
fprintf(stderr, "PSTEM@pipe\n");
	x->flags = 0;
	return 0;
#else	/* NO_FORK */
	return -1;
#endif	/* NO_FORK */
}

static int
start_reader(ps, stemmer, paths)
struct stm_d *ps;
char const *stemmer, **paths;
{
#if defined NO_FORK
	struct stm_d *a;
#endif
	if (pipe(ps->r.fds) == -1) {
		syslog(LOG_DEBUG, "pipe: %s", strerror(errno));
		return -1;
	}
	if (fcntl(ps->r.fds[0], F_SETFD, FD_CLOEXEC) == -1) {
		syslog(LOG_DEBUG, "fcntl: %s", strerror(errno));
	}
	if (fcntl(ps->r.fds[1], F_SETFD, FD_CLOEXEC) == -1) {
		syslog(LOG_DEBUG, "fcntl: %s", strerror(errno));
	}
#if ! defined F_MAXFD
	if (maxfd < ps->r.fds[0]) maxfd = ps->r.fds[0];
	if (maxfd < ps->r.fds[1]) maxfd = ps->r.fds[1];
#endif
#if ! defined NO_FORK
	switch (ps->r.pid = fork()) {
		int e;
	case -1:
		syslog(LOG_DEBUG, "fork: %s", strerror(errno));
		return -1;
	case 0:
		need_cleanup = 0;
		ps->reader0_arg.ps = ps;	/* NOTE: self */
		ps->reader0_arg.stemmer = stemmer;
		ps->reader0_arg.paths = paths;
		e = start_reader0(&ps->reader0_arg) != NULL;
		fprintf(stderr, "reader0: end.\n");
		_exit(e);
		/* NOTREACHED */
	default:
		break;
	}
#else	/* NO_FORK */
	ps->reader0_arg.ps = a = bdup(ps, sizeof *ps);
	ps->reader0_arg.stemmer = stemmer;
	ps->reader0_arg.paths = paths;
	bzero(&ps->reader0_arg.ps->reader0_arg, sizeof ps->reader0_arg.ps->reader0_arg);

	a->host = a->serv = a->sock = NULL;
	a->relay_out = NULL;

	if (pthread_create(&ps->thread, NULL, start_reader0, &ps->reader0_arg)) {
		syslog(LOG_DEBUG, "pthread_create: %s", strerror(errno));
		return -1;
	}
#endif	/* NO_FORK */
	return 0;
}

static void *
start_reader0(cookie)
void *cookie;
{
	struct reader0_arg *c = cookie;
	struct stm_d *ps;
	int p;
	struct stm_d_s ss0, *ss = &ss0;
	FILE *relay_in;
	void *r = NULL;
#if ! defined NO_FORK
	int flags = 0;
#else
	int flags = XSTEM_NOCLOSE;
#endif

	ps = c->ps;

	ss->path = ss->paths = c->paths;
	ss->stream = NULL;
	ss->lineno = 0;
	fcntl(ps->r.fds[1], F_SETFD, 0);	/* if NO_FORK, this fd is retained by main-thread, too. */
	if (!(relay_in = fdopen(ps->r.fds[1], "wb"))) {
		syslog(LOG_DEBUG, "fdopen: %s", strerror(errno));
		return "fdopen";
	}
fprintf(stderr, ".relay: %d\n", getpid());
	for (p = 0; p < ps->aa.n; p++) {
		struct stm_x *e = &ps->aa.x[p];
		e->fds[0] = -1;
		fcntl(e->fds[1], F_SETFD, 0);
assert(!(flags & XSTEM_NOCLOSE) || (e->flags & XSTEM_SOCKET));
		if (!(e->ps = xstem_create_pipe(e->fds, c->stemmer, e->flags | flags))) {
			fprintf(stderr, "xstem_create_pipe a: %d,%d: %s\n", e->fds[0], e->fds[1], strerror(errno));
			return "xstem_create_pipe";
		}
	}
	for (p = 0; p < ps->nn.n; p++) {
		struct stm_x *e = &ps->nn.x[p];
		e->fds[0] = -1;
		fcntl(e->fds[1], F_SETFD, 0);
assert(!(flags & XSTEM_NOCLOSE) || (e->flags & XSTEM_SOCKET));
		if (!(e->ps = xstem_create_pipe(e->fds, NORMALIZER, e->flags | flags))) {
			fprintf(stderr, "xstem_create_pipe n: %d,%d: %s\n", e->fds[0], e->fds[1], strerror(errno));
			return "xstem_create_pipe";
		}
	}
#if ! defined NO_FORK
	fdcloseexec();
#endif
	for (;;) {
		char *buf;
		size_t err;
		if (!(buf = ss_readln(ss))) {
			syslog(LOG_DEBUG, "ss_readln: %s", strerror(errno));
			fprintf(stderr, "readln failed at %s:%"PRIuSIZE_T"\n", *ss->path, (pri_size_t)ss->lineno);
			return "ss_readln";
		}

		if (!verify_utf8_string(buf, &err)) {
			fprintf(stderr, "error: invalid utf-8 encoding at %s:%"PRIuSIZE_T" offset %"PRIuSIZE_T"\n", *ss->path, (pri_size_t)ss->lineno, (pri_size_t)err);
			r = "utf8";
		}
		if (!*buf) {
			break;	/* EOF */
		}
		if (ss->lineno == 1) {
			fputs(". ", relay_in);
			fputs(*ss->path, relay_in);
			fputs("\n", relay_in);
		}
		/* note: thru CRLF, BOM */
		if (!strncmp(buf, "#body=", 6)) {
			fputs("b1", relay_in);
			fputs(buf + 6, relay_in);
			fflush(relay_in);
			if (relay_aa(ps, buf + 6) == -1) {
				return "xstem_send";
			}
			if (ps->nn.n) {
				fputs("!", relay_in);
				fputs(buf + 6, relay_in);
				fflush(relay_in);
				if (relay_nn(ps, buf + 6) == -1) {
					return "xstem_send";
				}
			}
		}
		else if (*buf == '!') {
			if (ps->nn.n) {
				fputs(buf, relay_in);
				fflush(relay_in);
				if (relay_nn(ps, buf + 1) == -1) {
					return "xstem_send";
				}
			}
		}
		else if (*buf == 'b') {
			fputs(buf, relay_in);
			fflush(relay_in);
			if (relay_aa(ps, buf + 2) == -1) {
				return "xstem_send";
			}
		}
		else if (*buf == '.') {
			fputs(".", relay_in);
			fputs(buf, relay_in);
		}
		else {
			fputs(buf, relay_in);
		}
	}
	fclose(relay_in);
	for (p = 0; p < ps->aa.n; p++) {
		struct stm_x *e = &ps->aa.x[p];
		xstem_destroy(e->ps);
		e->ps = NULL;
	}
	for (p = 0; p < ps->nn.n; p++) {
		struct stm_x *e = &ps->nn.x[p];
		xstem_destroy(e->ps);
		e->ps = NULL;
	}
	fprintf(stderr, "reader0 exit: %s\n", r ? (char *)r : "(null)");
	return r;
}

static int
relay_aa(ps, buf)
struct stm_d *ps;
char *buf;
{
	struct stm_x *e;
	assert(ps->aa.n);
	e = &ps->aa.x[ps->aa.p++];
	if (ps->aa.p >= ps->aa.n) ps->aa.p = 0;
	if (xstem_send(e->ps, buf) == -1) {
		fprintf(stderr, "xstem_send failed\n");
		return -1;
	}
	return 0;
}

static int
relay_nn(ps, buf)
struct stm_d *ps;
char *buf;
{
	struct stm_x *e;
	assert(ps->nn.n);
	e = &ps->nn.x[ps->nn.p++];
	if (ps->nn.p >= ps->nn.n) ps->nn.p = 0;
	if (xstem_send(e->ps, buf) == -1) {
		fprintf(stderr, "xstem_send failed\n");
		return -1;
	}
	return 0;
}

static char *
ss_readln(ss)
struct stm_d_s *ss;
{
	char *p;

ss_readln:
	if (!ss->stream) {
		if (*ss->path == NULL) {	/* EOF */
			return "";
		}
		else if (!strcmp(*ss->path, "-")) {
			if (!(ss->stream = nfopen(stdin))) {
fprintf(stderr, "OPEN STDIN\n");
				/* ERROR */
				return NULL;
			}
		}
		else if (!(ss->stream = nopen(*ss->path, "rb"))) {
			/* ERROR */
			return NULL;
		}
		ss->lineno = 0;
	}
	ss->lineno++;
	if (!(p = readln(ss->stream))) {
		/* ERROR */
		nfclose(ss->stream);
		ss->stream = NULL;
		return NULL;
	}
	if (*p == '\0') {	/* EOF. try next file. */
		nfclose(ss->stream);
		ss->stream = NULL;
		ss->path++;
		goto ss_readln;
	}
	return p;
}

static int
cleanup_pstem(ps)
struct stm_d *ps;
{
#if ! defined NO_FORK
	int status;
	char cmd[MAXPATHLEN];	/* XXX */
#else
	void *value;
#endif
	int p;
#if ! defined NO_FORK
	fprintf(stderr, "waitpid R %"PRIuSIZE_T"\n", (pri_size_t)ps->r.pid);
	if (waitpid(ps->r.pid, &status, 0) == -1) {
		syslog(LOG_DEBUG, "waitpid: %s", strerror(errno));
		return -1;
	}
	snprintf(cmd, sizeof cmd, "%d.relay", ps->r.pid);
	if (diag_stat(cmd, status) == -1) {
		return -1;
	}
#else	/* NO_FORK */
fprintf(stderr, "J\n");
	if (pthread_join(ps->thread, &value)) {
		syslog(LOG_DEBUG, "pthread_join: %s", strerror(errno));
		return -1;
	}
	if (value) {
		syslog(LOG_DEBUG, "error: %s", (char *)value);
		return -1;
	}
#endif	/* NO_FORK */
	for (p = 0; p < ps->aa.n; p++) {
		struct stm_x *e = &ps->aa.x[p];
		xstem_destroy(e->ps);
		e->ps = NULL;
	}
	for (p = 0; p < ps->nn.n; p++) {
		struct stm_x *e = &ps->nn.x[p];
		xstem_destroy(e->ps);
		e->ps = NULL;
	}
#if ! defined NO_FORK
	for (p = 0; p < ps->aa.n; p++) {
		struct stm_x *e = &ps->aa.x[p];
		if (e->pid == -1) continue;
		fprintf(stderr, "waitpid A %"PRIuSIZE_T"\n", (pri_size_t)e->pid);
		if (waitpid(e->pid, &status, 0) == -1) {
			syslog(LOG_DEBUG, "waitpid: %s", strerror(errno));
			return -1;
		}
		snprintf(cmd, sizeof cmd, "%d.pstem A", e->pid);
		if (diag_stat(cmd, status) == -1) {
			return -1;
		}
		e->pid = -1;
	}
	for (p = 0; p < ps->nn.n; p++) {
		struct stm_x *e = &ps->nn.x[p];
		if (e->pid == -1) continue;
		fprintf(stderr, "waitpid N %"PRIuSIZE_T"\n", (pri_size_t)e->pid);
		if (waitpid(e->pid, &status, 0) == -1) {
			syslog(LOG_DEBUG, "waitpid: %s", strerror(errno));
			return -1;
		}
		snprintf(cmd, sizeof cmd, "%d.pstem N", e->pid);
		if (diag_stat(cmd, status) == -1) {
			return -1;
		}
		e->pid = -1;
	}
#else	/* NO_FORK */
	free(ps->reader0_arg.ps);
	ps->reader0_arg.ps = NULL;
	ps->reader0_arg.stemmer = NULL;
	ps->reader0_arg.paths = NULL;
#endif	/* NO_FORK */
	ps->aa.n = ps->aa.p = 0;
	ps->nn.n = ps->nn.p = 0;
	if (ps->relay_out) {
		fclose(ps->relay_out);
	}
	return 0;
}

static int
proc_body(r, rs, b)
struct raw_vec *r;
struct rvst *rs;
char *b;
{
	int btype;
	char *p, *q;
	struct stm_x *e;

	assert(rs->ps.aa.n);
	e = &rs->ps.aa.x[rs->ps.aa.p++];
	if (rs->ps.aa.p >= rs->ps.aa.n) rs->ps.aa.p = 0;
	if (!(p = xstem_recv(e->ps))) {
		syslog(LOG_DEBUG, "xstem_recv failed");
		return -1;
	}
	btype = b[1] & 0xff;
	if (btype != '1') {
		syslog(LOG_DEBUG, "invalid btype: '%c'", btype);
		return -1;
	}
	if (b[2] == '\0') {
		return 0;	/* ok. do nothing */
	}
	for (q = p; *p; p++) {
		if (*p == '\n') {
			*p = '\0';
			if (proc_body_eject(r, q) == -1) {
				return -1;
			}
			q = p + 1;
		}
	}
	if (p != q) {
		if (proc_body_eject(r, q) == -1) {
			return -1;
		}
	}
	return 0;
}

static int
proc_body_eject(r, q)
struct raw_vec *r;
char *q;
{
	struct raw_elem *re;
	if (r->st.elems_size <= r->elem_num) {
		if (extend_elems(r) == -1) {
			goto nomem;
		}
	}
	re = &r->elems[r->elem_num++];	/* safe */
	re->tf = 1;
	if (xstrdup(&re->name, &re->name_size, q) == -1) {
		goto nomem;
	}
	return 0;
nomem:
	syslog(LOG_DEBUG, "memory exhausted");
	return -1;
}

static int
proc_flwd(r, rs, b)
struct raw_vec *r;
struct rvst *rs;
char *b;
{
	char *p;
	struct raw_prop *rp;
	size_t l;

	if (r->nsegms >= nelems(r->segms)) {
		syslog(LOG_DEBUG, "too many segments (!-lines) in a article");
		return -1;
	}

	switch (b[0]) {
		struct stm_x *e;
	case '!':
		assert(rs->pass == 1);
		e = &rs->ps.nn.x[rs->ps.nn.p++];
		if (rs->ps.nn.p >= rs->ps.nn.n) rs->ps.nn.p = 0;
		if (!(p = xstem_recv(e->ps))) {
			syslog(LOG_DEBUG, "xstem_recv failed");
			return -1;
		}
		break;
	case '+':
		p = b + 1;
		break;
	default:
		return -1;
	}

	l = strlen(p);
	if (l > 0 && p[l - 1] == '\n') {
		p[l - 1] = '\0';
	}

	rp = &r->segms[r->nsegms++];
	if (xstrdup(&rp->value, &rp->value_size, p) == -1) {
		return -1;
	}
	return 0;
}

idx_t
wam_name2id(nw, d, name)
WAM *nw;
char const *name;
{
	NWDB *cw;
	struct cw_entry *cwe;
	size_t size;

	cw = nw->u.w.d[d].cw;

	switch (nwdb_get(cw, name, &cwe, &size)) {
	case -1:
	case 1:
	default:
		return 0;
	case 0:
		return cwe->id;
	}
	/* NOTREACHED */
}

char const *
wam_id2name(nw, d, id)
WAM *nw;
idx_t id;
{
	NWDB *cr;
	struct cr_entry *cre;
	size_t size;

	cr = nw->u.w.d[d].cr;

	switch (nwdb_get(cr, &id, &cre, &size)) {
	case -1:
	case 1:
	default:
		return NULL;
	case 0:
		return cre->name;
	}
	/* NOTREACHED */
}

static int
sort_pair_file(path)
char const *path;
{
	struct rc_pair *p;
	struct stat sb;
	size_t n;
	FILE *f;

	fprintf(stderr, "(%s", basnam(path));
	if (stat(path, &sb) == -1) {
		syslog(LOG_DEBUG, "%s: %s", path, strerror(errno));
		return -1;
	}
	if (sb.st_size % sizeof *p) {
		syslog(LOG_DEBUG, "invalid size: %"PRIuSIZE_T" (%s)", (pri_size_t)sb.st_size, path);
		return -1;
	}
	if (!(p = malloc(sb.st_size))) {
		syslog(LOG_DEBUG, "malloc(%"PRIuSIZE_T"): %s", (pri_size_t)sb.st_size, strerror(errno));
		return -1;
	}
	n = sb.st_size / sizeof *p;
	if (!(f = fopen(path, "rb"))) {
		syslog(LOG_DEBUG, "%s: %s", path, strerror(errno));
		return -1;
	}
	if (fread(p, sizeof *p, n, f) != n) {
		syslog(LOG_DEBUG, "%s: %s", path, strerror(errno));
		return -1;
	}
	fclose(f);

	if (!(f = fopen(path, "wb"))) {
		syslog(LOG_DEBUG, "%s: %s", path, strerror(errno));
		return -1;
	}
	qsort(p, n, sizeof *p, xcompar);
	if (fwrite(p, sizeof *p, n, f) != n) {
		syslog(LOG_DEBUG, "%s: %s", path, strerror(errno));
		return -1;
	}
	fclose(f);

	free(p);
	fprintf(stderr, ")");
	return 0;
}

static int
next_pair(b)
struct pair_stream *b;
{
	if (fread(&b->p, sizeof b->p, 1, b->stream) != 1) {
		return EOF;
	}
	return 0;
}

static int
insert_pr(id, rp, pr)
idx_t id;
struct raw_prop *rp;
NWDB *pr;
{
	char *v0;
	size_t size;

	switch (nwdb_get(pr, &id, &v0, &size)) {
	default:
	case -1:
	case 0:	/* updating is prohibited */
		syslog(LOG_DEBUG, "insert_pr: error: id=%"PRIuIDX_T, id);
		return -1;
	case 1:	/* create new */
		break;
	}
	if (nwdb_put(pr, &id, rp->value, strlen(rp->value) + 1) == -1) {
		fprintf(stderr, "insert_pr: %s: update db failed (%p %"PRIuIDX_T" %p %"PRIuSIZE_T")\n", strerror(errno), pr, id, rp->value, (pri_size_t)strlen(rp->value) + 1);
		return -1;
	}
	return 0;
}

ssize_t
wam_prop_gets(nw, id, s)
WAM *nw;
idx_t id;
char const **s;
{
	size_t size;
	NWDB *pr;
	WAM *master;

	assert(nw->type == NWAM_D_TYPE_PROP_FILE);
	master = nw->u.p.master;
	if (master && master->u.w.dist.role == NDWAM_ROLE_PARENT) {
		if (!(*s = cb_wam_prop_gets(master->u.w.xs, nw->u.p.key, id))) {
			return -1;
		}
		return strlen(*s);
	}

	pr = nw->u.p.pr;
	switch (nwdb_get(pr, &id, s, &size)) {
	default:
	case -1:
	case 1:
		syslog(LOG_DEBUG, "wam_prop_gets: error: id=%"PRIuIDX_T, id);
		return -1;
	case 0:
		return size;
		break;
	}
	/* NOTREACHED */
}

df_t
wam_prop_nentries(nw)
WAM *nw;
{
	return nw->u.p.nentries;
}

static int
merge_vec(rs, v)
struct rvst *rs;
struct xr_vec const *v;
{
	df_t elem_num;

	elem_num = rs->v->elem_num + v->elem_num;
	if (rs->v_size < elem_num) {
		void *new;
		size_t newsize = elem_num;
		newsize = NA(newsize, 128);
		if (!(new = realloc(rs->v, sizeof_dxr_vec(newsize)))) {
			free(rs->v);
			rs->v = NULL;
			rs->v_size = 0;
			return -1;
		}
		rs->v_size = newsize;
		rs->v = new;
	}

	if (v->elem_num == 0) {
		/* do nothing */
	}
	else if (rs->v->elem_num == 0) {
		memcpy(rs->v->elems, v->elems, v->elem_num * sizeof *v->elems);
	}
	else {
		struct xr_elem *ep = &rs->v->elems[elem_num - 1];
		struct xr_elem *pp = &rs->v->elems[rs->v->elem_num - 1];
		struct xr_elem const *np = &v->elems[v->elem_num - 1];
		for (;;) {
			if (ecompar(pp, np) > 0) {
				*ep-- = *pp;
				if (pp == rs->v->elems) {
					for (;;) {
						*ep-- = *np;
						if (np == v->elems) break;
						np--;
					}
					break;
				}
				pp--;
			}
			else {
				*ep-- = *np;
				if (np == v->elems) {
					assert(ep == pp);
					break;
				}
				np--;
			}
		}
	}

	rs->v->elem_num = elem_num;
	rs->v->freq_sum += v->freq_sum;

	return 0;
}

static void
uniq_vec(rs)
struct rvst *rs;
{
	freq_t freq_sum;
	df_t i, j, k;
	freq_sum = 0;
	for (i = k = 0; i < rs->v->elem_num; i = j) {
		struct xr_elem *e = &rs->v->elems[k];
		*e = rs->v->elems[i];
		for (j = i + 1; j < rs->v->elem_num && e->id == rs->v->elems[j].id; j++) {
			e->freq += rs->v->elems[j].freq;
		}
assert(j == rs->v->elem_num || e->id < rs->v->elems[j].id);
		if (e->freq) {
			freq_sum += e->freq;
			k++;
		}
	}

	rs->v->elem_num = k;
	assert(rs->v->freq_sum == freq_sum);
}

char const *
wam_handle(nw)
WAM *nw;
{
	return nw->handle;
}

df_t
wam_size(nw, d)
WAM *nw;
{
	return nw->u.w.d[d].nentries;
}

df_t
wam_elem_num(nw, d, id)
WAM *nw;
idx_t id;
{
	NWDB *cr;
	struct cr_entry *cre;
	size_t size;

	cr = nw->u.w.d[d].cr;

	switch (nwdb_get(cr, &id, &cre, &size)) {
	case -1:
	case 1:
	default:
		return -1;
	case 0:
		return cre->df;
	}
	/* NOTREACHED */
}

freq_t
wam_freq_sum(nw, d, id)
WAM *nw;
idx_t id;
{
	NWDB *cr;
	struct cr_entry *cre;
	size_t size;

	cr = nw->u.w.d[d].cr;

	switch (nwdb_get(cr, &id, &cre, &size)) {
	case -1:
	case 1:
	default:
		return -1;
	case 0:
		return cre->tf;
	}
	/* NOTREACHED */
}

df_t
wam_max_elem_num(nw, d)
WAM *nw;
{
	return nw->u.w.d[d].max_elem_num;
}

freq_t
wam_max_freq_sum(nw, d)
WAM *nw;
{
	return nw->u.w.d[d].max_freq_sum;
}

freq_t
wam_total_elem_num(WAM *nw, ...)
{
	return nw->u.w.total_elem_num;
}

freq_t
wam_total_freq_sum(WAM *nw, ...)
{
	return nw->u.w.total_freq_sum;
}

int
wam_setmask(nw, d, pmask, plen, nmask, nlen)
WAM *nw;
idx_t *pmask, *nmask;
df_t plen, nlen;
{ 
	if (setmask0(&nw->u.w.d[d].pmask, pmask, plen) == -1) {
		return -1;
	}
	if (setmask0(&nw->u.w.d[d].nmask, nmask, nlen) == -1) {
		return -1;
	}
	nw->u.w.d[d].mask_set = nw->u.w.d[d].pmask.mask || nw->u.w.d[d].nmask.mask;
	return 0;
}

static int
setmask0(m, mask, len)
struct mask_t *m;
idx_t *mask;
df_t len;
{
	size_t size;
	df_t i;

	if (!mask) {
		m->mask = NULL;
		m->len = 0;
		return 0;
	}
	size = (len + 1) * sizeof *mask;
	if (m->size < size) {
		void *new;
		size_t newsize = size;
		newsize = NA(newsize, 128);
		if (!(new = realloc(m->buf, newsize))) {
			free(m->buf);
			m->mask = m->buf = NULL;
			m->size = 0;
			m->len = 0;
			return -1;
		}
		m->size = newsize;
		m->buf = new;
	}
	m->mask = m->buf;
	m->len = len;
	memmove(m->mask, mask, len * sizeof *mask);	/* NOTE: do not use size here */
	for (i = 0; i + 1 < len; i++) {
		if (!(m->mask[i] <= m->mask[i + 1])) {
			qsort(m->mask, len, sizeof *m->mask, icompar);
			break;
		}
	}
	return 0;
}

static void
mask_vec(v, pmask, plen, nmask, nlen)
struct xr_vec *v;
idx_t const *pmask, *nmask;
df_t plen, nlen;
{
	idx_t const *pme = NULL, *nme = NULL;
	struct xr_elem *p, *q, *pe;
	freq_t freq_sum;

	if (pmask) {
		pme = pmask + plen;
	}
	if (nmask) {
		nme = nmask + nlen;
	}
	freq_sum = 0;
	for (p = q = v->elems, pe = p + v->elem_num; p < pe; p++) {
		idx_t id = p->id;
		if (pmask) for (; pmask < pme && (*pmask) < id; pmask++) ;
		if (nmask) for (; nmask < nme && (*nmask) < id; nmask++) ;
		if ((pmask && pmask < pme && *pmask == id) ||
		    (nmask && (nmask >= nme || *nmask != id))) {
			*q = *p;
			freq_sum += q->freq;
			q++;
		}
	}
	v->elem_num = q - v->elems;
	v->freq_sum = freq_sum;
}

static struct syminfo *
wsh_assv(q, qlen, w, dir, w_type, nd, total, sc)
struct syminfo const *q;
df_t qlen;
WAM *w;
df_t *nd, *total;
struct assv_scookie *sc;
{
	struct syminfo *r0;
	df_t nr0;
	df_t r0total;

	nr0 = *nd;

	if (r0 = assv(q, qlen, w, dir, w_type, &nr0, &r0total, sc)) {
		*nd = nr0;
		if (total) *total = r0total;
	}

	return r0;
}

struct syminfo *
wsh(q, qlen, w, dir, w_type, nd, total, scookie)
struct syminfo const *q;
df_t qlen;
WAM *w;
df_t *nd, *total;
struct assv_scookie *scookie;
{
	return wsh_assv(q, qlen, w, dir, w_type, nd, total, scookie);
}

struct syminfo *
bex_wsh(q, qlen, w, dir, w_type, nd, total, b, blen, scookie)
struct syminfo const *q;
df_t qlen;
WAM *w;
df_t *nd, *total;
struct bxue_t *b;
df_t blen;
struct assv_scookie *scookie;
{
	struct assv_scookie sc;

	if (scookie) {
		sc = *scookie;
	}
	else {
		bzero(&sc, sizeof sc);
	}

	sc.flag |= ASSV_SC_BX;
	sc.bx.b = b;
	sc.bx.blen = blen;

	return wsh_assv(q, qlen, w, dir, w_type, nd, total, &sc);
}

struct syminfo *
fss_wsh(q, qlen, w, dir, w_type, nd, total, scookie, fssq)
struct syminfo const *q;
df_t qlen;
WAM *w;
df_t *nd, *total;
struct assv_scookie *scookie;
struct fss_query *fssq;
{
	struct assv_scookie sc;

	if (scookie) {
		sc = *scookie;
	}
	else {
		bzero(&sc, sizeof sc);
	}

	sc.flag = ASSV_SC_FSS;
	sc.fssq = fssq;
	return wsh_assv(q, qlen, w, dir, w_type, nd, total, &sc);
}

void *
bdup(b, len)
void const *b;
size_t len;
{
	void *r;

	if (!(r = malloc(len))) {
		return NULL;
	}
	memcpy(r, b, len);
	return r;
}

struct xr_vec *
dxr_dup(p)
struct xr_vec const *p;
{
	return bdup(p, sizeof_dxr_vec(p->elem_num));
}

static char const *
basnam(s)
char const *s;
{
	char const *r;
	if (r = rindex(s, '/')) {
		return r + 1;
	}
	return s;
}

#if ! defined NO_F_SETLKW
static int
setlkw(type, path)
char const *path;
{
	int d;
	struct flock flk;

	flk.l_start = 0;
	flk.l_len = 0;
	flk.l_pid = getpid();
	flk.l_type = type;
	flk.l_whence = SEEK_SET;

	switch (type) {
	case F_WRLCK:
		if ((d = open(path, O_RDWR | O_CREAT, 0666)) == -1) {
			syslog(LOG_DEBUG, "%s: %s", path, strerror(errno));
			return -1;
		}
		break;
	case F_RDLCK:
		if ((d = open(path, O_RDONLY)) == -1) {
			syslog(LOG_DEBUG, "%s: %s", path, strerror(errno));
			return -1;
		}
		break;
	default:
		return -1;
	}
	if (fcntl(d, F_SETLKW, &flk) == -1) {
		syslog(LOG_DEBUG, "flock: %s", strerror(errno));
		return -1;
	}
	return d;
}

static int
unsetlk(d)
{
	struct flock flk;

	flk.l_start = 0;
	flk.l_len = 0;
	flk.l_pid = getpid();
	flk.l_type = F_UNLCK;
	flk.l_whence = SEEK_SET;

	if (fcntl(d, F_SETLK, &flk) == -1) {
		syslog(LOG_DEBUG, "flock: %s", strerror(errno));
		return -1;
	}
	return 0;
}
#endif

static void
fdcloseexec()
{
#if ! defined NO_FD_CLOEXEC
	int i;
#if defined F_MAXFD
	int maxfd = fcntl(0, F_MAXFD, 0);
#endif
	printf("maxfd = %d\n", maxfd);
	for (i = 0; i <= maxfd; i++) {
		int arg;
		if ((arg = fcntl(i, F_GETFD, 0)) != -1 && (arg & FD_CLOEXEC)) {
			syslog(LOG_DEBUG, "{%d}", i);
			close(i);
		}
	}
#endif
}

static int
verify_utf8_string(s, erroffs)
char const *s;
size_t *erroffs;
{
	size_t length = strlen(s);
	int32_t i;
	for (i = 0; i < length; ) {
		UChar32 u;
		U8_NEXT(s, i, length, u);
		if (u < 0) {
			*erroffs = i;
			return 0;
		}
	}

	return 1;
}

#if defined FLWD_UTF32
static ssize_t
fwputs(s, stream)
char const *s;
FILE *stream;
{
	size_t length = strlen(s);
	int32_t i, j;
	unsigned int k;
	for (i = j = k = 0; i < length; j++) {
		UChar32 u;
		U8_NEXT(s, i, length, u);
		if (u < 0) {
			return -1;
		}
		if (k < 255) k++;
		u |= k << 24;
		if (fwrite(&u, 4, 1, stream) != 1) {
			return -1;
		}
	}
	if (fwrite("\0\0\0\0", 4, 1, stream) != 1) {
		return -1;
	}
	j++;

	return j * 4;
}
#endif

static int
register_handle(w, h, key)
WAM *w, *h;
char const *key;
{
	size_t i, j;

	assert(w->type == NWAM_D_TYPE_WAM);
	assert(h->type == NWAM_D_TYPE_PROP_FILE || h->type == NWAM_D_TYPE_FSS);
	for (i = 0; i < w->u.w.slave_size; i++) {
		if (!w->u.w.slave[i].handle) {
			break;
		}
	}
	if (w->u.w.slave_size <= i) {
		void *new;
		size_t newsize = i;
		newsize = NA(newsize + 1, 8);
		if (!(new = realloc(w->u.w.slave, newsize * sizeof *w->u.w.slave))) {
			return -1;
		}
		w->u.w.slave_size = newsize;
		w->u.w.slave = new;
		for (j = i; j < newsize; j++) {
			w->u.w.slave[j].handle = NULL;
			w->u.w.slave[j].key = NULL;
		}
	}
	if (!(w->u.w.slave[i].key = strdup(key))) {
		return -1;
	}
	w->u.w.slave[i].handle = h;
	return 0;
}

static int
unregister_handle(w, h)
WAM *w, *h;
{
	size_t i;

	if (!w) {
		return 0;
	}
	assert(w->type == NWAM_D_TYPE_WAM);
	assert(h->type == NWAM_D_TYPE_PROP_FILE || h->type == NWAM_D_TYPE_FSS);
	for (i = 0; i < w->u.w.slave_size; i++) {
		if (w->u.w.slave[i].handle == h) {
			free(w->u.w.slave[i].key);
			w->u.w.slave[i].key = NULL;
			w->u.w.slave[i].handle = NULL;
			return 0;
		}
	}
	return -1;
}

WAM *
get_registered_handle(w, key)
WAM *w;
char const *key;
{
	size_t i;

	assert(w->type == NWAM_D_TYPE_WAM);
	for (i = 0; i < w->u.w.slave_size; i++) {
		if (w->u.w.slave[i].key && !strcmp(w->u.w.slave[i].key, key)) {
			return w->u.w.slave[i].handle;
		}
	}
	return NULL;
}

static void
close_registered_handles(w)
WAM *w;
{
	size_t i;

	assert(w->type == NWAM_D_TYPE_WAM);
	for (i = 0; i < w->u.w.slave_size; i++) {
		WAM *h;
		if (h = w->u.w.slave[i].handle) {
			switch (h->type) {
			case NWAM_D_TYPE_PROP_FILE:
				h->u.p.open_count = 0;
				break;
			case NWAM_D_TYPE_FSS:
				h->u.fss.open_count = 0;
				break;
			default:
				syslog(LOG_DEBUG, "close_registered_handles: internal error");
				continue;
			}
			wam_close(h);
			assert(w->u.w.slave[i].handle == NULL);
		}
	}
	bzero(w->u.w.slave, w->u.w.slave_size * sizeof *w->u.w.slave);
	free(w->u.w.slave);
	w->u.w.slave = NULL;
	w->u.w.slave_size = 0;
}

static int
reserved_prop(p)
char const *p;
{
	char const *a;
	if (*p != '@') return 0;

	a = p + 1;
/* NOTE: see stp.c reserved_attr */
	return !strncmp(a, "name=", 5) ||		/* stp.c */
	       !strncmp(a, "created=", 8) ||		/* inf2xml.c */
	       !strncmp(a, "stemmer=", 8) ||
	       !strncmp(a, "servers=", 8) ||
	       !strncmp(a, "stat.", 5) ||
	       !strncmp(a, "properties=", 11) ||	/* nwam.c -> stp.c */
	       !strncmp(a, "number-of-articles=", 19);	/* inf2xml.c */
}

static struct servlist_t *
get_server_list(nw, server_conf_paths)
WAM *nw;
char const **server_conf_paths;
{
	struct servlist_t *ss;
	char *sp;
	if (sp = lookup_ci(&nw->u.w.ci, "servers")) {
		ss = decode_servers(sp);
		free(sp);
		return ss;
	}

	if (!server_conf_paths) {
		syslog(LOG_DEBUG, "get_server_list: no previous settings");
		return NULL;
	}

	if (!(ss = xgwam_servlist(&nw->u.w.dist, server_conf_paths))) {
		syslog(LOG_DEBUG, "get_server_list: xgwam_servlist failed");
		return NULL;
	}

	if (!(sp = encode_servers(ss))) {
		syslog(LOG_DEBUG, "get_server_list: encode_servers failed");
		return NULL;
	}
	if (add_ci_pair(&nw->u.w.ci, "servers", sp, 0) != 0) {
		syslog(LOG_DEBUG, "get_server_list: add_ci_pair failed");
		return NULL;
	}
	if (sync_ci(nw, &nw->u.w.ci) == -1) {
		syslog(LOG_DEBUG, "get_server_list: sync_ci failed");
		return NULL;
	}
	free(sp);

	return ss;
}

static char *
encode_servers(ss)
struct servlist_t *ss;
{
	char *sp;
	size_t i, l, sl;
	for (i = l = 0; ss[i].host || ss[i].localsocket; i++) {
		if (ss[i].host) sl = strlen(ss[i].host), l += sl;
		if (ss[i].port) sl = strlen(ss[i].port), l += sl;
		if (ss[i].localsocket) sl = strlen(ss[i].localsocket), l += sl;
		l += 3;
	}
	l++;
	if (!(sp = malloc(l))) {
		syslog(LOG_DEBUG, "encode_servers: malloc: %s", strerror(errno));
		return NULL;
	}
	for (i = l = 0; ss[i].host || ss[i].localsocket; i++) {
		if (ss[i].host) sl = strlen(ss[i].host), memmove(sp + l, ss[i].host, sl), l += sl;
		*(sp + l) = SSEP, l++;
		if (ss[i].port) sl = strlen(ss[i].port), memmove(sp + l, ss[i].port, sl), l += sl;
		*(sp + l) = SSEP, l++;
		if (ss[i].localsocket) sl = strlen(ss[i].localsocket), memmove(sp + l, ss[i].localsocket, sl), l += sl;
		*(sp + l) = SSEP, l++;
	}
	*(sp + l) = '\0';
	
	return sp;
}

static struct servlist_t *
decode_servers(sp)
char const *sp;
{
	struct servlist_t *ss;
	size_t xx, j;
	char const *p;
	xx = count_char(sp, SSEP);
	if (xx % 3) {
		syslog(LOG_DEBUG, "malformed server_list");
		return NULL;
	}
	xx /= 3;
	if (!(ss = calloc(xx + 1, sizeof *ss))) {
		syslog(LOG_DEBUG, "decode_servers: calloc: %s", strerror(errno));
		return NULL;
	}
	for (j = 0, p = sp; j < xx; j++) {
		char *q;
		ss[j].host = ss[j].port = ss[j].localsocket = NULL;
		if (!(q = index(p, SSEP))) {
			syslog(LOG_DEBUG, "decode_servers: assertion failed A");
			return NULL;
		}
		if (p != q) if (!(ss[j].host = strldup(p, q - p))) return NULL;
		p = q + 1;
		if (!(q = index(p, SSEP))) {
			syslog(LOG_DEBUG, "decode_servers: assertion failed B");
			return NULL;
		}
		if (p != q) if (!(ss[j].port = strldup(p, q - p))) return NULL;
		p = q + 1;
		if (!(q = index(p, SSEP))) {
			syslog(LOG_DEBUG, "decode_servers: assertion failed C");
			return NULL;
		}
		if (p != q) if (!(ss[j].localsocket = strldup(p, q - p))) return NULL;
		p = q + 1;
	}
	ss[j].host = ss[j].port = ss[j].localsocket = NULL;
	return ss;
}

static char *
strldup(p, l)
char const *p;
size_t l;
{
	char *r;
	if (!(r = malloc(l + 1))) {
		syslog(LOG_DEBUG, "strldup: malloc: %s", strerror(errno));
		return NULL;
	}
	memcpy(r, p, l);
	r[l] = '\0';
	return r;
}

int
wam_setopt(WAM *w, int cmd, ...)
{
	va_list ap;

	switch (cmd) {
	case WAM_SETOPT_ALLOWERROR:
		va_start(ap, cmd);
		w->u.w.allowerror = *va_arg(ap, unsigned int *);
		va_end(ap);
		break;
	case WAM_SETOPT_NEED_DISTSTAT:
		va_start(ap, cmd);
		w->u.w.need_diststat = *va_arg(ap, int *);
		va_end(ap);
		break;
	default:
		return -1;
	}
	return 0;
}

int
wam_getopt(WAM *w, int cmd, ...)
{
	va_list ap;

	switch (cmd) {
	case WAM_GETOPT_ALLOWERROR:
		va_start(ap, cmd);
		*va_arg(ap, unsigned int *) = w->u.w.allowerror;
		va_end(ap);
		break;
	case WAM_GETOPT_MAY_INCOMPLETE:
		va_start(ap, cmd);
		*va_arg(ap, int *) = w->u.w.may_incomplete;
		va_end(ap);
		break;
	case WAM_GETOPT_DISTSTAT:
		va_start(ap, cmd);
		*va_arg(ap, struct diststat **) = w->u.w.diststat;
		va_end(ap);
		break;
	case WAM_GETOPT_DISTSTAT_NN_ROW:
		va_start(ap, cmd);
		*va_arg(ap, size_t *) = w->u.w.dist.nn[0];
		va_end(ap);
		break;
	case WAM_GETOPT_DISTSTAT_NN_COL:
		va_start(ap, cmd);
		*va_arg(ap, size_t *) = w->u.w.dist.nn[1];
		va_end(ap);
		break;
	default:
		return -1;
	}
	return 0;
}
