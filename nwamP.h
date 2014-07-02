/*	$Id: nwamP.h,v 1.66 2010/12/14 08:34:09 nis Exp $	*/

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

#if defined NO_FORK
#include <pthread.h>
#include <semaphore.h>
#endif

#include "nwdb.h"

#define	LOCKKEY	"!.lock"
#define	INFKEY	"!.inf"
#define	SCHEKEY_NW	"!.sche"
#define	SCHE_EXT_PR	".sche"
#define	SCHEKEY_FS	"nfss.sche"

#define	NAMESKEY	"nfss.names"
#define	FLWDKEY	"nfss.flwd"
#define	IDXKEY	"nfss.idx"
#define	MAPKEY	"nfss.map"
#define	RMAPKEY	".rmap"

#define	FSSKEY	"@fss"

#define	WAM_UPDATE_FROM_ITB	0
#define	WAM_UPDATE_DELIDL 	1

struct distconf_t {
	int role;		/* common */
#define	NDWAM_INHERIT	1
#define	NDWAM_MONOLITHIC	2
#define	NDWAM_ROLE_PARENT	4
#define	NDWAM_ROLE_CHILD	8
	int dir;		/* child node */
	df_t node_id;		/* child node */
	df_t nn[2];		/* common */
};

struct wam_update_args {
	int command;
	char const *handle;
	char const *tmpdir;
	struct {
		char const **itbs;
		char const **keys;
		struct {
			char const *stemmer;
			int stem_a_no, stem_n_no;
			
			char const *host, *serv
#if ! defined NO_LOCALSOCKET
				, *sock
#endif
#if ! defined NO_FORK
				, *path
#endif
				;
		} s;
		int mkri_ncpu;
#if defined ENABLE_RAWVECLEN_LIMIT
		df_t rawveclen_limit;
#endif
	} i;
	struct {
		idx_t *id;
		size_t nid;
		struct xr_vec **vecs;
	} d;

	struct {
		struct distconf_t c;
		struct nrpc_t *n, *nn;	/* child node */
		char const **server_conf_paths;
	} dist;
	char **environ;
};

#define	INT32_TWID	16
#define	PRIdINT32_TWID	"14"
#define	DF_TWID		16
#define	PRIdDF_TWID	"14"
#define IDX_TWID	16
#define	PRIuIDX_TWID	"14"
#if defined ENABLE_TF64
#define FREQ_TWID	32
#define	PRIdFREQ_TWID	"30"
#else
#define FREQ_TWID	16
#define	PRIdFREQ_TWID	"14"
#endif

struct nsche_nw {
	char magic[8];
	struct {
		char pad0[8];
		char nentries[DF_TWID];
		char max_elem_num[DF_TWID];
		char max_freq_sum[FREQ_TWID];
		char maxid[IDX_TWID];
		char compress_mode[INT32_TWID];
	} d[2];

	char pad1[8];
	char total_elem_num[FREQ_TWID];
	char total_freq_sum[FREQ_TWID];

	struct {
		char pad2[8];
		char role[INT32_TWID];
		char dir[INT32_TWID];
		char node_id[DF_TWID];
		char nn[2][DF_TWID];
	} dist;
};

struct nsche_pr {
	char magic[8];
	char nentries[DF_TWID];
};

struct nsche_fs {
	char magic[8];
	char fss_j[INT32_TWID];
	char fss_nsegms[IDX_TWID];
};

struct mask_t {
	void *buf;
	idx_t *mask;
	size_t size;
	df_t len;
};

struct corpus_prop {
	char *key, *value;
};

struct corpus_i {
	size_t ncprop, cprop_size;
	struct corpus_prop *cprop;
};

struct nwam_d {
	int type;
#define	NWAM_D_TYPE_WAM		0
#define	NWAM_D_TYPE_PROP_FILE	1
#define	NWAM_D_TYPE_PROP_CMD	2
#define	NWAM_D_TYPE_PROP_DL	3
#define	NWAM_D_TYPE_FSS		4
	char *rootdir;
	char handle[MAXPATHLEN];
	char base[MAXPATHLEN];
	char sche[MAXPATHLEN];
	struct {
		char path[MAXPATHLEN];
		int d;
	} lock;
	union {
		struct {
			struct {
				df_t nentries, max_elem_num;
				freq_t max_freq_sum;
				idx_t maxid;
				int compress_mode;
				NWDB *cw, *cr, *xr;
				char cwp[MAXPATHLEN], crp[MAXPATHLEN], xrp[MAXPATHLEN];
				struct mask_t pmask, nmask;
				int mask_set;
			} d[2];
			freq_t total_elem_num, total_freq_sum;
			char inf[MAXPATHLEN];
			struct {
				void *ptr;
				size_t size;
			} tv;
			struct {
				char *key;
				WAM *handle;
			} *slave;
			size_t slave_size;
			struct distconf_t dist;
			struct xs_t *xs;
			struct corpus_i ci;
			unsigned int allowerror;
			int may_incomplete;
			int need_diststat;
			struct diststat *diststat;
		} w;
		struct {
			int dir;
			df_t nentries;
			NWDB *pr;
			char prp[MAXPATHLEN];
			WAM *master;
			char *key;
			int open_count;
		} p;
		struct {
			FSS *f;
			size_t j;
			idx_t nsegms;
			struct fss_paths *paths;
			WAM *master;
			int open_count;
		} fss;
	} u;
};

#define	NIPFRG	256	/* max # of itb/pair cache file */
#if defined ENABLE_LP64
#define	IFRGSZ	(128 * 1024 * 1024)		/* 128MB * 256 => 32GB */
#define	PFRGSZ	(128 * 1024 * 1024)		/* 128MB * 256 => 32GB */
#define	FSSFRGSZ	(1024 * 1024 * 1024)	/* 1GB * 24 => 24GB */
#else
#define	IFRGSZ	(32 * 1024 * 1024)		/* 32MB * 256 => 8GB */
#define	PFRGSZ	(32 * 1024 * 1024)		/* 32MB * 256 => 8GB */
#define	FSSFRGSZ	(64 * 1024 * 1024)	/* 64MB * 24 => 1.5GB */
#endif
/* fssP.h: MAXNFSSFRG	24	max # of fss file */

#define	SSEP	'\t'

struct stm_d_s {
	char const **paths, **path;
	NFILE *stream;
	size_t lineno;
};

struct stm_x {
	int fds[2];		/* XXX pthread no toki ha, oya ga subete retain suru. */
	struct xstems *ps;	/* XXX pthread no toki ha, hukusei wo tukuru. */
#if ! defined NO_FORK
	pid_t pid;
#endif
	int flags;
};

struct stm_xl {
	size_t n, p;
	struct stm_x x[32];
};

struct reader0_arg {
	struct stm_d *ps;
	char const *stemmer, **paths;
};

struct stm_d {
	struct stm_xl aa, nn;
	struct {
		int fds[2];
#if ! defined NO_FORK
		pid_t pid;
#endif
	} r;

	FILE *relay_out;

	char *host, *serv, *sock;
#if ! defined NO_FORK
	char *path;
#else
	pthread_t thread;
#endif
	struct reader0_arg reader0_arg;
};

struct rvst {
	int flags;
#define RVST_LOST_MAX	1
	int pass;

	char const *tmpdir;

	char *path;
	off_t pos;
	size_t lineno;
	size_t last_l;

	NFILE *relay_out;
	struct stm_d ps;

	struct {
		size_t read;
		struct pi pi;
	} pi;	/* progress indicator */

	struct {
		size_t n, j;
		char *paths[NIPFRG];
		FILE *stream;
		size_t ej_size;
	} ifrg, pfrg;

	NFILE *stream;

	df_t v_size;
	struct xr_vec *v;

	struct nwam_d *nw, **prs;
	size_t nprs;

	struct {
		struct cr_entry *ncre;
		size_t ncre_size;
	} s;

	int make_fssi;
	struct {
		FILE *flwd;
		size_t j;
		size_t ej_size;
		idx_t last_segid;
		int no_last_segid;
		char *ioptions;
		struct nwam_d *f;
		struct nwam_d *rmap;
	} fss;
};

#define USEIDFORIF	1	/* use idx for intermediate file */

struct raw_elem {
	char *name;
#if defined USEIDFORIF
	idx_t id;
#endif
	size_t name_size;
	freq_t tf;
};

struct raw_prop {
	char const *key;
	size_t value_size;
	char *value;
};

struct raw_vec {
	int type;
#define	RAWVEC_VEC	0	/* i */
#define	RAWVEC_PROP	1	/* @ */
#define	RAWVEC_IOPT	2	/* $ */
	char *p;
	struct {
		size_t elems_size, name_size;
	} st;

	char *name;
#if defined USEIDFORIF
	idx_t id;
#endif
	df_t elem_num;
	struct raw_elem *elems;
	size_t nprops;
	struct raw_prop *props;
	idx_t nsegms;
	struct raw_prop segms[MAXSEGID + 1];
};

#define	sizeof_dxr_vec(n)	(offsetof(struct xr_vec, elems[(n)]))

/*

 * tables:
	lo: lock file (empty)
	in: meta_info @-line
	mi: (struct nsche) meta_info
	cw: name,strlen(name)+1 -> id,sizeof(df_t)
	cr: id,sizeof(df_t) -> name,strlen(name)+1
	xr: id,sizeof(df_t) -> xr_vec,sizeof_dxr_vec(xr_vec.elem_num)
	pr: id,sizeof(df_t) -> property,size

 * file name interpretation:
	lo: $GETAROOT/dwam/$handle/!.lock
	in: $GETAROOT/dwam/$handle/!.inf   => @.inf
	mi: $GETAROOT/dwam/$handle/!.sche
	cw: $GETAROOT/dwam/$handle/ncw[rc].db
	cr: $GETAROOT/dwam/$handle/ncr[rc].db
	xr: $GETAROOT/dwam/$handle/nxr[rc].db
	pr: $GETAROOT/dwam/$handle/npr[rc].$prop.sche
	  | $GETAROOT/dwam/$handle/npr[rc].$prop.db

	db: $f.i  -- index part
	    $f.d  -- fullword part

	part: $p    -- # of parts
	      $p.%d -- part

	nfss.sche
	nfss.flwd.%d
	nfss.idx.%d
	nfss.map.%d		-- struct member name == bitmap
	nfss.rmap.db -- rmap

 * progress message:
	[ itb(source)
	$< itb-fragment(eject)
	< itb-fragment(read)
	$( rcp(eject)
	( rcp(sort)

*/

struct rc_pair {
	idx_t r_id, c_id;
	freq_t tf;
};

struct pair_stream {
	struct rc_pair p;
	FILE *stream;
};

struct fss_rmap_ent_t {
	idx_t id;
	int j;
	uint32_t offset;
};

struct servlist_t {
	char const *host;
	char const *port;
	char const *localsocket;
};

WAM *wam_open_dc(char const *, char const *, struct distconf_t *);
void reduce_vec(struct xr_vec *, struct distconf_t *);
int wam_drop_dc(char const *, char const *, struct distconf_t *);
int wam_rename_dc(char const *, char const *, char const *, struct distconf_t *);

#if ! defined DBG_XPART

#define	XPART(id, nn)	(id % (nn))
#define	XPART_TYPE	"SEQUENTIAL"

#else

#define	XPART(id, nn)	(xparts[(nn) - 1].s[id % xparts[(nn) - 1].m])
#define	XPART_TYPE	"SCRAMBLE"

struct xpart_t {
	unsigned int m, s[32 * 32];
};

static struct xpart_t xparts[] = {
	configuration error
};

#endif
