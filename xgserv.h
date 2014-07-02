/*	$Id: xgserv.h,v 1.26 2010/01/16 00:31:36 nis Exp $	*/

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

#define	NRPC_EXIT	0
#define	NRPC_DISTCONF	1
#define	NRPC_WAM_UPDATE	2
#define NRPC_RAWVEC	3
#define NRPC_RAWVEC_END	4
#define NRPC_WAM_UPDATE_END	5
#define NRPC_WAM_OPEN	6
#define NRPC_WAM_PROP_OPEN	7
#define NRPC_WAM_FSS_OPEN	8
#define NRPC_WAM_CLOSE	9
#define NRPC_WAM_PROP_CLOSE	10
#define NRPC_WAM_FSS_CLOSE	11
#define NRPC_WAM_DROP	12
#define NRPC_WAM_RENAME	13
#define NRPC_WAM_GET_VEC	14
#define NRPC_WAM_PROP_GETS	15
#define NRPC_WAM_FSS_GETS	16
#define NRPC_ASSV	17
#define NRPC_XFSS	18
#define NRPC_SETMASK	19

struct diststat {
	df_t j, nd, total;
};

struct nrpc_t;

int cb_wam_update_send_ack(struct nrpc_t *, int);
int cb_wam_update_read_raw_vec(struct nrpc_t *, struct raw_vec **);
int cb_wam_openall(struct xs_t *, char const *, char const *);
int cb_wam_dropall(struct xs_t *, char const *, char const *);
int cb_wam_renameall(struct xs_t *, char const *, char const *, char const *);
int cb_wam_prop_openall(struct xs_t *, int, char const *);
int cb_wam_fss_openall(struct xs_t *);
int cb_wam_closeall(struct xs_t *);
int cb_wam_prop_closeall(struct xs_t *, int, char const *);
int cb_wam_fss_closeall(struct xs_t *);
void *cb_wam_get_vec(struct xs_t *, int, idx_t);
void *cb_wam_prop_gets(struct xs_t *, char const *, idx_t);
void *cb_assv(struct xs_t *, struct syminfo *, df_t, int, int, df_t *, df_t *, struct assv_scookie *, unsigned int, int *, struct diststat **);
int cb_xfss(struct xs_t *, struct fss_query *, struct fss_simple_query *, struct fss_simple_query *, unsigned int, int *);
void *cb_wam_fss_gets(struct xs_t *, idx_t, unsigned int);

struct xs_t;
struct servlist_t *xgwam_servlist(struct distconf_t *, char const **);
struct xs_t *xgwam_connect(struct distconf_t *, struct servlist_t *);
int xgwam_disconnect(struct xs_t *);

/* for wam_update */
int xwamupdate_init(struct xs_t *, struct wam_update_args *);
int xwamupdate_push_rawvec(struct xs_t *, struct raw_vec *);
int xwamupdate_rawvec_end(struct xs_t *);
int xwamupdate_end(struct xs_t *);
