/*	$Id: nwdb.h,v 1.12 2009/07/30 00:08:15 nis Exp $	*/

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

struct _nwam_db;
typedef struct _nwam_db NWDB;

#if defined ENABLE_DB
#define	NWDB_CW	0
#define	NWDB_CR	1
#define	NWDB_XR	2
#define	NWDB_PR	3
#endif

#define	NWDB_NCW	4
#define	NWDB_NCR	5
#define	NWDB_NXR	6
#define	NWDB_NPR	7

int nwdb_drop(char const *, int);
NWDB *nwdb_open(char const *, int, mode_t);
int nwdb_close(NWDB *);
int nwdb_put(NWDB *, void *, void *, size_t);
int nwdb_get(NWDB *, void *, void *, size_t *);
int nwdb_seq(NWDB *, void *, void *, size_t *);
int nwdb_del(NWDB *, void *);
void nwdb_rewind(NWDB *);

struct cw_entry {
	idx_t id;
	df_t df;
	freq_t tf;
};

struct cr_entry {
	df_t df;
	freq_t tf;
	char name[1];
};
