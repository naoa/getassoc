/*	$Id: mkri.c,v 1.98 2011/09/14 03:06:09 nis Exp $	*/

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
static char rcsid[] = "$Id: mkri.c,v 1.98 2011/09/14 03:06:09 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <unicode/utf.h>
#include <unicode/utf8.h>

#include "nwam.h"
#include "util.h"
#include "pi.h"

#include "xstrcmpP.h"
#include "fssP.h"

#include "sort_pcompar.h"

#include <gifnoc.h>

/* #define DBG	1 /* */

/* XSORT_PCOPAR, TPSORT_PCOMPAR, QSORT_PCOMPAR are exclusive. choose just one.
 * the default is to choose
 *  RXSORT_PCOMPAR -- if --enable-nwam=yes
 *  XSORT_PCOMPAR  -- otherwise
 */

#if ! defined ENABLE_GETA

#if 0
#define XSORT_PCOMPAR	1
#endif
#if 1
#define	TPSORT_PCOMPAR	1
#endif

#else	/* !ENABLE_GETA */

#if 0
#define XSORT_PCOMPAR	1
#endif
#if 1
#define	TPSORT_PCOMPAR	1
#endif

#endif	/* !ENABLE_GETA */

#if ! defined XSORT_PCOMPAR && ! defined TPSORT_PCOMPAR
#define	QSORT_PCOMPAR	1
#endif

#if defined ENABLE_GETA
static int gen_nameitbl(char const *, char const *, char const *, idx_t **, size_t *);
#endif

#if defined DBG_DMP
static void dump_ptr_u_table(struct ptr_u *, size_t, FILE *, size_t);
#endif
#if defined DBG_DMP || defined DBG_CHECK
static void prndat(char const *, FILE *);
#endif

#if defined DBG_CHECK
static int check(struct ptr_u *, size_t);
static void astream_gets(uint8_t *, size_t, char const *, int);
#endif

#if defined ENABLE_INDEXP
static int indexp(char const *);
static int indexp2(UChar32, UChar32);
static int cclass(UChar32);
#endif

static xsc_opt_t xsc_options;
static char const *g_base;

int
#if ! defined ENABLE_GETA
mkri(w, flwd, out, bmp, opts, tmpdir, ncpu)
WAM *w;
#else
mkri(getaroot, handle, flwd, names, out, bmp, opts, ncpu)
char const *getaroot;
char const *handle;
#endif
char const *flwd;
#if defined ENABLE_GETA
char const *names;
#endif
char const *out;
char const *bmp;
char const *opts;
#if ! defined ENABLE_GETA
char const *tmpdir;
#endif
{
#if ! defined FLWD_UTF32
	int c;
#endif
	struct ptr_u *ptrs = NULL;
#if USE_MMAP
	struct mapfile_t *ptrs_m = NULL;
#endif
	unsigned char *bitmap;
	size_t nframes, bitmap_size;
	struct mapfile_t *m;
	char *data;
	size_t size;
#if defined CACHE_PREFETCH
	FILE *pcache;
	int d;
	char pcache_path[MAXPATHLEN];
#endif

	char *p;
	char *e;
	struct flwd_idx *i;

	size_t k, n;
	int pass;
size_t nsegs;
#if defined ENABLE_GETA
	idx_t *nameitbl;
	size_t nnameitbl;
#endif

	FILE *o;
	size_t idx;

	struct idx_hdr header;

#if defined ENABLE_GETA
	BEGIN();
#endif

#if defined ENABLE_GETA
	fprintf(stderr, "gen_nameitbl\n");

	if (gen_nameitbl(getaroot, handle, names, &nameitbl, &nnameitbl) == -1) {
		return -1;
	}
#endif

	PRINT_ELAPSED("pass1", stderr);
	fprintf(stderr, "fill ptr_u table\n");

	if (parse_opts(opts, &xsc_options) == -1) {
		fprintf(stderr, "parse options failed\n");
		return -1;
	}
	xsc_options &= ~POPTIONS_MASK;

	if (!(m = mapfile_rw(flwd))) {
		return -1;
	}
	data = m->ptr;
	size = m->size;

	nframes = (size + bytes_per_frame - 1) / bytes_per_frame;
	bitmap_size = (nframes + 7) / 8;
/* XXX right? */
#if defined DBG
fprintf(stderr, "malloc: %"PRIuSIZE_T"\n", (pri_size_t)bitmap_size);
#endif
if (bitmap_size) {
	if (!(bitmap = malloc(bitmap_size))) {
		perror("malloc");
		return -1;
	}
	bzero(bitmap, bitmap_size);
}
else {
	bitmap = NULL;
}

if (size) {
#if defined FLWD_UTF32
	if (size % 4 != 0 || *(UChar32 *)&data[size - 4] != 0) {
		fprintf(stderr, "\\0 is expected just before EOF\n");
		return -1;
	}
#else
	if (data[size - 1] != '\0' && (data[size - 1]&0xff) != FWPADCHR) {
		fprintf(stderr, "\\0 or \\%03o is expected just before EOF\n", FWPADCHR);
		return -1;
	}
#endif
}
#if defined CACHE_PREFETCH
	snprintf(pcache_path, sizeof pcache_path, "%s/pcacheXXXXXX", tmpdir);
	if ((d = mkstemp(pcache_path)) == -1) {
		perror(pcache_path);
		return -1;
	}
	nwam_add_tmp(pcache_path, S_IFREG);
	if (!(pcache = fdopen(d, "wb"))) {
		perror("fdopen");
		return -1;
	}
#endif

	k = n = 0;
	for (pass = 0; pass < 3; pass++) {
		struct pi pi;
#if defined ENABLE_GETA
		idx_t id;
#endif
pi_init(&pi, size, '0' + pass, 0, 60, stderr);

		switch (pass) {
		case 2:
#if USE_MMAP
#if defined DBG
fprintf(stderr, "mmap ptrs: %"PRIuSIZE_T"\n", (pri_size_t)(k * sizeof *ptrs));
#endif
		if (k) {
			if (!(ptrs_m = mmap_alloc((k * sizeof *ptrs)))) {
				perror("mmap_alloc ptrs");
				return -1;
			}
			ptrs = ptrs_m->ptr;
		}
		else {
			ptrs = NULL;
			ptrs_m = NULL;
		}
#else
		if (k) {
			if (!(ptrs = calloc(k, sizeof *ptrs))) {
				perror("calloc");
				return -1;
			}
		}
		else {
			ptrs = NULL;
		}
#endif
			break;
		}

	if (size) {
		assert(sizeof *i + 2 < size);
		assert(data[sizeof *i] == '\0');
	}

		idx = 0;
#if defined ENABLE_GETA
		id = 0;
#endif

nsegs = 0;
		for (p = data, e = data + size; p < e; ) {
#if defined CACHE_PREFETCH
			struct astream as;
			char *p0;
			ssize_t u1ptr;
			int ignore;
#endif
			i = (struct flwd_idx *)p;

nsegs++;
#if ! defined ENABLE_GETA
			if (i->segid == 0) {
				idx++;
			}
#else
			if (i->segid == 0) {
				id = nameitbl[idx++];
			}
			switch (pass) {
			case 0:
				i->id = id;
				break;
			case 1:
			case 2:
				assert(i->id == id);
				break;
			}
#endif
			if (p + sizeof *i + 2 > e) {
				fprintf(stderr, "short header at EOF\n");
				return -1;
			}

#if defined ENABLE_GETA
			if (idx > nnameitbl) {
				fprintf(stderr, "too few names\n");
				return -1;
			}
#endif

			p += sizeof *i;
#if defined CACHE_PREFETCH
			switch (pass) {
			case 1:
				if (fwrite(i, sizeof *i, 1, pcache) != 1) {
					perror("fwrite");
					return -1;
				}
				break;
			}
#endif

#if defined FLWD_UTF32
			if (*(UChar32 *)p != 0) {
				fprintf(stderr, "\\0 expected\n");
				return -1;
			}
#else
			if (*p != '\0') {
				fprintf(stderr, "\\0 expected\n");
				return -1;
			}
#endif

			switch (pass) {
				size_t frame, bitmap_idx;
				int bitmap_sft;
			case 0:
				frame = (p - data) / bytes_per_frame;
				bitmap_idx = frame / 8;
				bitmap_sft = frame % 8;
				assert(bitmap_idx < bitmap_size);
				bitmap[bitmap_idx] |= 1 << bitmap_sft;
				break;
			}

#if defined FLWD_UTF32
#if defined CACHE_PREFETCH
			switch (pass) {
			case 1:
				if (fwrite((UChar32 *)p, 4, 1, pcache) != 1) {
					perror("fwrite");
					return -1;
				}
				break;
			}
#endif
			p += 4;

#if defined CACHE_PREFETCH
			astream_init_r(p, -1, xsc_options, &as, -1);
			p0 = p;
#endif
			for (; p < e && *(UChar32 *)p; p += 4) {
				switch (pass) {
					struct ptr_u *r;
#if defined CACHE_PREFETCH
					UChar32 u, u0;
#endif
				case 1:
#if defined CACHE_PREFETCH
					u0 = astream_getc_noskip(&as, &u1ptr, &ignore);
					u = u0 | *(UChar32 *)p & ~0x1fffff;
					assert(p0 + u1ptr == p);
					if (fwrite(&u, 4, 1, pcache) != 1) {
						perror("fwrite");
						return -1;
					}
#endif
#if defined ENABLE_INDEXP
					if (indexp(p)) {
#endif
						k++;
#if defined ENABLE_INDEXP
					}
#endif
					break;
				case 2:
#if defined ENABLE_INDEXP
					if (indexp(p)) {
#endif
						r = &ptrs[n++];
						r->p = p - data;
#if defined ENABLE_INDEXP
					}
#endif
					break;
				}
			}
#else
			p++;

			for (; p < e && (c = *p & 0xff); p++) {
				if ((c & 0xc0) != 0x80) {
					switch (pass) {
						struct ptr_u *r;
					case 1:
#if defined ENABLE_INDEXP
						if (indexp(p)) {
#endif
							k++;
#if defined ENABLE_INDEXP
						}
#endif
						break;
					case 2:
#if defined ENABLE_INDEXP
						if (indexp(p)) {
#endif
							r = &ptrs[n++];
							r->p = p - data;
#if defined ENABLE_INDEXP
						}
#endif
						break;
					}
				}
			}
#endif
PI_CHECK(&pi, p - data);
			if (p == e) {
				fprintf(stderr, "\\0 expected just before EOF");
				return -1;
			}
#if defined FLWD_UTF32
#if defined CACHE_PREFETCH
			switch (pass) {
			case 1:
				if (fwrite((UChar32 *)p, 4, 1, pcache) != 1) {
					perror("fwrite");
					return -1;
				}
				break;
			}
#endif
			p += 4;
#else
			p++;
			if (p < e) {	/* align here */
				size_t pad;
				pad = FWIDXALIGN - (p - (char *)i) % FWIDXALIGN;
				if (pad != FWIDXALIGN) {
					char *pe = p + pad;
					for (; p < pe; p++) {
						c = *p & 0xff;
						if (c != FWPADCHR) {
							fprintf(stderr, "\\%03o expected (encountered w/ %d)\n", FWPADCHR, c);
							return -1;
						}
					}
				}
			}
#endif
		}
#if defined ENABLE_GETA
		if (idx != nnameitbl) {
			fprintf(stderr, "# of articles mismatch %"PRIuSIZE_T" %"PRIuSIZE_T"\n", (pri_size_t)idx, (pri_size_t)nnameitbl + 1);
			return -1;
		}
#endif
fprintf(stderr, "\n");
	}
	if (k != n) {
		fprintf(stderr, "internal error (1)\n");
		return -1;
	}

	fprintf(stderr, "total: %"PRIuSIZE_T" entries, %"PRIuSIZE_T" segments, %"PRIuSIZE_T" articles\n", (pri_size_t)k, (pri_size_t)nsegs, (pri_size_t)idx);

	PRINT_ELAPSED("pass2", stderr);
	fprintf(stderr, "sort table\n");
#if defined CACHE_PREFETCH
	fclose(pcache);
	mapfile_destroy(m);
	if (!(m = mapfile_rw(pcache_path))) {
		return -1;
	}
	data = m->ptr;
	assert(size == m->size);
	size = m->size;
#endif

	g_base = data;

#undef SDEF
#if defined XSORT_PCOMPAR
#define	SDEF 1
#endif
#if defined TPSORT_PCOMPAR
#if defined SDEF
#define ERR 1
#endif
#define	SDEF 1
#endif
#if defined QSORT_PCOMPAR
#if defined SDEF
#define ERR 1
#endif
#define	SDEF 1
#endif
#if defined ERR || ! defined SDEF
CONFIGURATION_ERROR
#endif
#undef SDEF

#if ! defined QSORT_PCOMPAR
	/*if (n > 100000) ??<*/
	if (n > 100) {
#endif
#if defined XSORT_PCOMPAR
#if ! defined ENABLE_GETA
		if (xsort_pcompar(tmpdir, ptrs, n, ncpu, g_base, xsc_options) == -1) return -1;
#else
		if (xsort_pcompar(getaroot, ptrs, n, ncpu, g_base, xsc_options) == -1) return -1;
#endif
#endif
#if defined TPSORT_PCOMPAR
		if (tpsort_pcompar(ptrs, n, g_base, xsc_options) == -1) {
			fprintf(stderr, "tpsort_pcompar failed\n");
			return -1;
		}
#endif
#if ! defined QSORT_PCOMPAR
	}
	else {
#endif
		if (n) {
			qsort(ptrs, n, sizeof *ptrs, pcompar);
		}
#if ! defined QSORT_PCOMPAR
	}
#endif

#if defined DBG_CHECK
#if defined CACHE_PREFETCH
	mapfile_destroy(m);
	trucate(pcache_path, 0);
	if (!(m = mapfile_rw(flwd))) {
		return -1;
	}
	data = m->ptr;
	size = m->size;
#endif
	if (check(ptrs, n) == -1) {
		return -1;
	}
#endif

	mapfile_destroy(m);

	PRINT_ELAPSED("pass3", stderr);
	fprintf(stderr, "dump table\n");

	if (!(o = fopen(out, "wb"))) {
		perror(out);
		return -1;
	}
	header.nentries = n;
	header.xsc_options = xsc_options | POPTIONS_MASK;

	if (fwrite(&header, sizeof header, 1, o) != 1) {
		perror("fwrite");
		return -1;
	}
	if (n) {
		if (fwrite(ptrs, sizeof *ptrs, n, o) != n) {
			perror("fwrite");
			return -1;
		}
	}
	fclose(o);

#if USE_MMAP
	if (ptrs_m) {
		mapfile_destroy(ptrs_m);
	}
#else
	free(ptrs);
#endif

	PRINT_ELAPSED("pass4", stderr);

	if (!(o = fopen(bmp, "wb"))) {
		perror(bmp);
		return -1;
	}
	if (bitmap_size) {
		if (fwrite(bitmap, 1, bitmap_size, o) != bitmap_size) {
			perror(bmp);
			return -1;
		}
	}
	fclose(o);

	free(bitmap);

	return 0;
}

#if defined ENABLE_GETA
static int
gen_nameitbl(getaroot, handle, names, idsp, np)
char const *getaroot;
char const *handle;
char const *names;
idx_t **idsp;
size_t *np;
{
	char const *p;
	char const *e;
	char const *nmp;
	struct mapfile_t *m;
	size_t size, n, j;
#if defined ENABLE_GETA
	WAM *w = NULL;
#endif
	idx_t *ids;
	struct mapfile_t *ids_m;
	char const *q;

	if (!(m = mapfile(names))) {
		goto error;
	}
	nmp = m->ptr;
	size = m->size;

#if defined ENABLE_GETA
	wam_init(getaroot);
	if (!(w = wam_open(handle))) {
		perror(handle);
		goto error;
	}
#endif

	n = 0;
	for (p = nmp, e = nmp + size; p < e; p = q + 1) {
		for (q = p; q < e && *q; q++) ;
		if (q == e) {
			fprintf(stderr, "no \\0 at EOF (%s)\n", names);
			goto error;
		}
		n++;
	}

#if USE_MMAP
#if defined DBG
fprintf(stderr, "mmap ids: %"PRIuSIZE_T"\n", (pri_size_t)(n * sizeof *ids));
#endif
#warning "XXX there are no way to unmap ids_m->ptr"
	if (!(ids_m = mmap_alloc(n * sizeof *ids))) {
		perror("mmap_alloc ids");
		return -1;
	}
	ids = ids_m->ptr;
#else
#if defined DBG
fprintf(stderr, "calloc ids: %"PRIuSIZE_T"\n", (pri_size_t)(n * sizeof *ids));
#endif
	if (!(ids = calloc(n, sizeof *ids))) {
		perror("calloc");
		goto error;
	}
#endif

	j = 0;
	for (p = nmp, e = nmp + size; p < e; p = q + 1) {
		idx_t id;
		for (q = p; q < e && *q; q++) ;
		if (q == e) {
			fprintf(stderr, "no \\0 at EOF (%s)\n", names);
			goto error;
		}
		id = wam_name2id(w, WAM_ROW, p);
		assert(j <= n);
		ids[j++] = id;
	}
	assert(j == n);

#if defined ENABLE_GETA
	wam_close(w);
#endif

	*idsp = ids;
	*np = n;

	mapfile_destroy(m);
	return 0;

error:
#if defined ENABLE_GETA
	if (w) {
		wam_close(w);
	}
#endif
	return -1;
}
#endif

static int
pcompar(va, vb)
void const *va;
void const *vb;
{
	struct ptr_u *a, *b;
	int e;

	a = (struct ptr_u *)va;
	b = (struct ptr_u *)vb;
	if (e = xstrcmp(a->p + g_base, b->p + g_base, xsc_options)) {
		return e;
	}
	else if (a->p < b->p) {
		return -1;
	}
	else if (a->p > b->p) {
		return 1;
	}
	return 0;
}

#if defined DBG_DMP
static void
dump_ptr_u_table(ptrs, n, stream, offs)
struct ptr_u *ptrs;
size_t n, offs;
FILE *stream;
{
	size_t i;
	for (i = 0; i < n; i++) {
		struct ptr_u *p = &ptrs[i + offs];
		prndat(&g_base[p->p], stream);
		putc('\n', stream);
	}
}
#endif

#if defined DBG_DMP || defined DBG_CHECK
static void
prndat(s, stream)
char const *s;
FILE *stream;
{
	int j, k;
	k = 0;
#if defined FLWD_UTF32
	for (j = 0; j < 48 && *(UChar32 *)&s[k]; j++) {
		char t[8];
		UChar32 u;
		UBool isError;
		int o;
		isError = 0, o = 0;
		u = *(UChar32 *)&s[k] & 0x1fffff;
		U8_APPEND(t, o, sizeof t, u, isError);
		t[o] = '\0';
		fputs(t, stream);
		k += 4;
	}
#else
	for (j = 0; j < 48 && s[k]; j++) {
		putc(s[k++], stream);
		while ((s[k] & 0xc0) == 0x80) {
			putc(s[k++], stream);
		}
	}
#endif
}
#endif

#if defined DBG_CHECK
static int
check(ptrs, n)
struct ptr_u *ptrs;
size_t n;
{
	size_t i;
struct pi pi;
pi_init(&pi, n, '.', 0, 60, stderr);
	for (i = 0; i + 1 < n; i++) {
		char const *a, *b;
		int e;
PI_CHECK(&pi, i);
		a = ptrs[i].p + g_base;
		b = ptrs[i + 1].p + g_base;
		e = xstrcmp(a, b, xsc_options);
		if (e > 0) {
			uint8_t s[4 * 12 + 1];
			fprintf(stderr, "!!! check failed: e = %d\n", e);

fprintf(stderr, "a_raw:   ["); prndat(a, stderr); fprintf(stderr, "]\n");
fprintf(stderr, "a_stream:["); astream_gets(s, sizeof s, a, 12); fprintf(stderr, "%s]\n", s);
fprintf(stderr, "b_raw:   ["); prndat(b, stderr); fprintf(stderr, "]\n");
fprintf(stderr, "b_stream:["); astream_gets(s, sizeof s, b, 12); fprintf(stderr, "%s]\n", s);

			return -1;
		}
	}
fprintf(stderr, "ok\n");

#if defined DBG_DMP
dump_ptr_u_table(ptrs, MIN(n / 2, 32), stderr, n / 2);
#endif
	return 0;
}

static void
astream_gets(s, size, p, n)
uint8_t *s;
size_t size;
char const *p;
{
	struct astream bs;
	int o, j;
	UBool isError;
	isError = 0, o = 0;
	astream_init_r(p, -1, xsc_options, &bs, -1);
	s[0] = '\0';
	for (j = 0; j < n; j++) {
		unsigned int c;
		c = astream_getc(&bs, NULL);
		U8_APPEND(s, o, size, c, isError);
		s[o] = '\0';
		if (isError) {
			memmove(&s[o], "?", 2);
			break;
		}
	}
}
#endif

#if defined ENABLE_INDEXP
static int
indexp(p)
char const *p;
{
	UChar32 u, v;
#if defined FLWD_UTF32
	u = *(UChar32 *)p & ~0x1fffff;
	assert(u);
	v = *((UChar32 *)p - 1) & ~0x1fffff;
#else
	size_t length;
	int32_t i, j;
	assert(p[0]);
	for (i = 0, j = 1; (p[j] & 0xc0) == 0x80; j++) ;
	length = j - i;
	U8_NEXT(p, i, length, u);
	if (u < 0) {
		return 0;
	}
	for (i = 0, j = -1; (p[j] & 0xc0) == 0x80; j--) ;
	if (!p[j]) {
		v = 0;
	}
	else {
		length = i - j;
		U8_NEXT(p, j, length, v);
		if (v < 0) {
			return 0;
		}
	}
#endif
	return indexp2(v, u);
}

#define	CCLASS_ALFA	1
#define	CCLASS_DIGIT	2
#define	CCLASS_SYMBOL	3
#define CCLASS_OHTER	99

static int
indexp2(prev, cur)
UChar32 prev, cur;
{
	int pcls, ccls;
	pcls = cclass(prev);
	ccls = cclass(cur);
	if (ccls == CCLASS_SYMBOL) return 0;
	if (pcls == CCLASS_ALFA && ccls == CCLASS_ALFA) return 0;
	if (pcls == CCLASS_DIGIT && ccls == CCLASS_DIGIT) return 0;
	return 1;
}

static int
cclass(u)
UChar32 u;
{
	if ('a' <= u && u <= 'z' || 'A' <= u && u <= 'Z') return CCLASS_ALFA;
	if ('0' <= u && u <= '9') return CCLASS_DIGIT;
	if (u == '#' || u== '$' || u == '%' || u == '&') return CCLASS_OHTER;
	if (u < 0x80) return CCLASS_SYMBOL;	/* (ascii) - (alfa | digit | '#' | '$' | '%' | '&') */
	return CCLASS_OHTER;
}
#endif
