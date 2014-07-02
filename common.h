/*	$Id: common.h,v 1.14 2010/12/15 06:03:33 nis Exp $	*/

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

struct escapedq_t {
	char *b;
	size_t l;
};

char *vec2jsvec(WAM *, int, struct xr_vec const *);
char *escapedq(char const *, struct escapedq_t *);
df_t wam_get_vec_byname(WAM *, int, char const *, struct xr_vec const **);
struct xr_vec *dxr_dup2(WAM *, WAM *, int, struct xr_vec const *);
struct syminfo *syminfo_dup2(WAM *, WAM *, int, struct syminfo const *, df_t *);
struct element *read_catalogue(void);
df_t csalen_sum(struct cs_elem *, df_t);
int mapcsa(struct cs_elem *, df_t, int (*)(struct cs_elem *, df_t, struct syminfo *, df_t, void *, df_t), void *);
int send_file_datadir_relative(char const *, int, char const *);

/* relative to GETAROOT */
#define	ETCDIR		"etc"
#define	NWAMDIR		"nwam"
#define	WORKDIR 	"tmp"
/* #define	PWAMDIR	"pwam" */

/* relative to GETAROOT / ETCDIR */
#define	DNWAM_CONF	"dnwam.conf"
#define CATALOGUE_XML	"catalogue.xml"
#define INDEX_KEY	"index.key"
#define	RELAY_CONF	"relay.conf"
