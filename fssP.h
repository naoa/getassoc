/*	$Id: fssP.h,v 1.23 2010/01/16 00:31:35 nis Exp $	*/

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

struct flwd_idx {
	idx_t id;
	unsigned int segid;
};

#define	FWIDXALIGN	4
#define	FWPADCHR	0377
#define	FWPAD		"\377\377\377"

struct idx_hdr {
	size_t nentries;
	xsc_opt_t xsc_options;
};

struct ptr_u {
	uint32_t p;	/* offset in segment */
};

#define bytes_per_frame 64

#if ! defined ENABLE_GETA
#define	MAXNFSSFRG	24	/* max # of fss file */
#else
#define	MAXNFSSFRG	1
#endif

struct fss_paths {
	char flwd[MAXPATHLEN];
	char idx[MAXPATHLEN];
	char bitmap[MAXPATHLEN];
};

#if ! defined ENABLE_GETA
struct _fss;
typedef struct _fss FSS;
#endif

#if ! defined ENABLE_GETA
FSS *fss_open(struct fss_paths *, size_t);
int fss_close(FSS *);
int mkri(WAM *, char const *, char const *, char const *, char const * , char const *, int);
#else
int fss_dump(FSS *, FILE *);
int mkri(char const *, char const *, char const *, char const *, char const *, char const *, char const *, int);
#endif
