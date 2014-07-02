/*	$Id: nvmt.c,v 1.35 2011/09/14 02:36:15 nis Exp $	*/

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
static char rcsid[] = "$Id: nvmt.c,v 1.35 2011/09/14 02:36:15 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <unistd.h>

#include "util.h"

#include "nvmt.h"

#include <gifnoc.h>

/* #define DBG	1 /* */
/* #define DBGxx	1 /* */

#if defined ENABLE_LP64
#define	MAPFRGSZ	(256 * 1024 * 1024)	/* 256MB * 24 => 6GB */
#define	FULLWDFRGSZ	(128 * 1024 * 1024)	/* 128MB * 24 => 3GB */
#else
#define	MAPFRGSZ	(64 * 1024 * 1024)	/* 64MB * 24 => 1.5GB */
#define	FULLWDFRGSZ	(8 * 1024 * 1024)	/* 8MB * 24 => 192MB */
#endif

#if ! defined MIN
#define	MIN(a, b)	((a) < (b) ? (a) : (b))
#endif

static int nvmt_writev(char const *, struct nvmt *, size_t, size_t);

int
nvmt_drop(path)
char const *path;
{
	size_t nmap;
	char fpath[MAXPATHLEN];
	size_t pathlen;
	size_t bp = 0;
	FILE *f;
	int r = 0;

	if ((pathlen = strlen(path)) + 16 >= MAXPATHLEN) {
		syslog(LOG_DEBUG, "nvmt_drop: %s: too long", path);
		return -1;
	}
	strcpy(fpath, path);	/* OK */

	if (f = fopen(path, "r")) {
		char buf[32];
		*buf = '\0';
		fgets(buf, sizeof buf, f);
		fclose(f);
		nmap = strtoul(buf, NULL, 10);
	}
	else {
/*
		syslog(LOG_DEBUG, "nvmt_drop: %s: %s", path, strerror(errno));
		return -1;
*/
		return 0;
	}

	for (bp = 0; bp < nmap; bp++) {
		sprintf(fpath + pathlen, ".%"PRIuSIZE_T, (pri_size_t)bp);	/* OK */
		if (unlink(fpath) == -1) {
			syslog(LOG_DEBUG, "nvmt_drop: %s: %s", fpath, strerror(errno));
			r = -1;
		}
	}

	if (unlink(path) == -1) {
		syslog(LOG_DEBUG, "nvmt_drop: %s: %s", path, strerror(errno));
		r = -1;
	}
	return r;
}

struct nvmt *
nvmt_open(path, membsize, alignshift, flags)
char const *path;
size_t membsize;
mode_t flags;
{
	size_t nmap;
	size_t nm;
	char fpath[MAXPATHLEN];
	size_t pathlen;
	struct stat sb;
	struct nvmt *h = NULL;
	size_t bp = 0;
	FILE *f;

#if defined DBG
syslog(LOG_DEBUG, "nvmt_open: %s", path);
#endif

	if ((pathlen = strlen(path)) + 16 >= MAXPATHLEN) {
		syslog(LOG_DEBUG, "nvmt_open: %s: too long", path);
		return NULL;
	}
	strcpy(fpath, path);	/* OK */

	if (membsize == 0) {
		syslog(LOG_DEBUG, "nvmt_open: membsize == 0");
		return NULL;
	}

	if (!(h = calloc(1, sizeof *h))) {
		syslog(LOG_DEBUG, "nvmt_open: calloc: %s", strerror(errno));
		goto err;
	}

	h->membsize = membsize;
	h->alignment = 1 << alignshift;

	h->map.maps = NULL;
	h->map.bases = NULL;
	h->mem.maps = NULL;
	h->mem.bases = NULL;
	h->path = NULL;

	if (!(h->path = strdup(path))) {
		syslog(LOG_DEBUG, "nvmt_open: strdup: %s", strerror(errno));
		goto err;
	}

	if (f = fopen(h->path, "r")) {
		char buf[32];
		*buf = '\0';
		fgets(buf, sizeof buf, f);
		fclose(f);
		nmap = strtoul(buf, NULL, 10);
	}
	else {
		nmap = 0;
	}

#if defined DBG
syslog(LOG_DEBUG, "nvmt_open: %s: nmap = %d", h->path, (int)nmap);
#endif

#if defined DBG
if (nmap == 0 && !(flags & O_CREAT)) {
	syslog(LOG_DEBUG, "nvmt_open: nmap == 0: %s", path);
}
#endif

	h->rdonly = (flags & O_ACCMODE) == O_RDONLY;

	h->map.size = h->map.bpend = nmap;
	if (!(h->map.maps = calloc(h->map.size, sizeof *h->map.maps))) {
		syslog(LOG_DEBUG, "nvmt_open: calloc: %s", strerror(errno));
		goto err;
	}
	if (!(h->map.bases = calloc(h->map.size, sizeof *h->map.bases))) {
		syslog(LOG_DEBUG, "nvmt_open: calloc: %s", strerror(errno));
		goto err;
	}
	h->map.xend = 0;
	h->map.nmemb = 0;

	h->map.allcsz = h->map.r_allcsz = 0;
	for (bp = 0; bp < h->map.bpend; bp++) {
		sprintf(fpath + pathlen, ".%"PRIuSIZE_T, (pri_size_t)bp);	/* OK */
#if defined DBG
syslog(LOG_DEBUG, "nvmt_open: fpath = %s", fpath);
#endif
		if (stat(fpath, &sb) == -1) {
			syslog(LOG_DEBUG, "nvmt_open: stat: %s: %s", fpath, strerror(errno));
			goto err;
		}
		if (sb.st_size == 0 || sb.st_size % h->membsize != 0) {
			syslog(LOG_DEBUG, "nvmt_open: %s: empty", fpath);
			goto err;
		}
		if (bp == 0) {
			h->map.allcsz = sb.st_size;
		}
		else if (bp + 1 == h->map.bpend) {
			if (h->map.allcsz < sb.st_size) {
				syslog(LOG_DEBUG, "nvmt_open: %s: invalid size: %"PRIuSIZE_T, fpath, (pri_size_t)sb.st_size);
				goto err;
			}
		}
		else {
			if (h->map.allcsz != sb.st_size) {
				syslog(LOG_DEBUG, "nvmt_open: %s: invalid size: %"PRIuSIZE_T, fpath, (pri_size_t)sb.st_size);
				goto err;
			}
		}
		h->map.r_allcsz = sb.st_size;
		if (!(h->map.maps[bp] = mapfile_prot(fpath, h->rdonly ? PROT_READ : PROT_READ|PROT_WRITE, MAP_SHARED))) {
			syslog(LOG_DEBUG, "nvmt_open: mapfile_prot failed: %s: %s", fpath, strerror(errno));
			goto err;
		}
#if defined DBG
syslog(LOG_DEBUG, "nvmt_open: %s => %p", fpath, h->map.maps[bp]->ptr);
#if defined DBGxx
 { int i;
  char *pp = h->map.maps[bp]->ptr;
 for (i = 0; i < 8; i++) {
   fprintf(stderr, "%02x ", pp[i] & 0xff);
 }
 fprintf(stderr, "\n");
 }

#endif
#endif
		h->map.bases[bp] = h->map.maps[bp]->ptr;
		if (h->map.maps[bp]->size != sb.st_size) {
			syslog(LOG_DEBUG, "nvmt_open: internal error");
			goto err;
		}
		h->map.xend += sb.st_size / h->membsize;
	}

	h->map.nmemb = h->map.allcsz / h->membsize;
	h->map.r_nmemb = h->map.r_allcsz / h->membsize;

	if ((nm = MAPFRGSZ / h->membsize) == 0) {
		syslog(LOG_DEBUG, "nvmt_open: %"PRIuSIZE_T": membsize too large", (pri_size_t)h->membsize);
		goto err;
	}
	if (h->map.bpend == 0 || h->map.bpend == 1 && h->map.nmemb < nm) {
		h->map.nmemb = nm;
		h->map.allcsz = h->map.nmemb * h->membsize;
	}

	h->mem.size = h->mem.bpend = 0;
	if ((h->mem.nmemb = FULLWDFRGSZ / h->membsize) == 0) {
		syslog(LOG_DEBUG, "nvmt_open: %"PRIuSIZE_T": membsize too large", (pri_size_t)h->membsize);
		goto err;
	}
	h->mem.allcsz = h->mem.nmemb * h->membsize;
	h->mem.xend = h->map.xend;

	return h;

err:
	if (h->map.maps) {
		size_t i;
		for (i = 0; i < bp; i++) {
			mapfile_destroy(h->map.maps[i]);
		}
		free(h->map.bases);
		free(h->map.maps);
	}
	free(h->path);
	free(h);
	return NULL;
}

int
nvmt_close(h)
struct nvmt *h;
{
	int e = 0;
	size_t bp, r;
	size_t offs, c, rest;
	char fpath[MAXPATHLEN];
	size_t pathlen;
	size_t nmap;
	FILE *f;
#if defined DBG
syslog(LOG_DEBUG, "nvmt_close: %s: size = %"PRIuSIZE_T, h->path, (pri_size_t)h->mem.xend);
#endif

	for (bp = 0; bp < h->map.bpend; bp++) {
		mapfile_destroy(h->map.maps[bp]);
	}
	free(h->map.maps);
	free(h->map.bases);

	if (h->rdonly) {
		assert(!h->mem.bases);
	}
	else {
		if (h->map.bpend == 0) {
			bp = 0;
			r = h->map.nmemb;
		}
		else {
			bp = h->map.bpend - 1;
			r = h->map.nmemb - h->map.r_nmemb;
		}
		if (rest = h->mem.xend - h->map.xend) {
			if ((pathlen = strlen(h->path)) + 16 >= MAXPATHLEN) {
				syslog(LOG_DEBUG, "nvmt_close: internal error");
				return -1;
			}
			strcpy(fpath, h->path);	/* OK */
			for (offs = h->map.xend; rest > 0; rest -= c, offs += c, bp++) {
				c = MIN(rest, r);
				sprintf(fpath + pathlen, ".%"PRIuSIZE_T, (pri_size_t)bp);	/* OK */
#if defined DBGxx
syslog(LOG_DEBUG, "nvmt_close: %s.bp+%d", h->path, (int)bp);
#endif
				if (e = nvmt_writev(fpath, h, offs, offs + c)) {
					break;
				}
				r = h->map.nmemb;
			}
			nmap = bp;
#if defined DBGxx
syslog(LOG_DEBUG, "nvmt_close: %s.nmap+%d", h->path, (int)nmap);
#endif
			if (!(f = fopen(h->path, "w"))) {
				e = -1;
			}
			fprintf(f, "%"PRIuSIZE_T"\n", (pri_size_t)nmap);
			fclose(f);
		}

		for (bp = 0; bp < h->mem.bpend; bp++) {
			free(h->mem.bases[bp]);
		}
		free(h->mem.bases);
	}

	free(h->path);
	free(h);

	return e;
}

/*
 * write h->mem [offs .. e) to path
 */
static int
nvmt_writev(path, h, offs, e)
char const *path;
struct nvmt *h;
size_t offs, e;
{
	FILE *f;
	size_t l;
	int d;

#if defined DBG
syslog(LOG_DEBUG, "nvmt_writev: %s: size = %"PRIuSIZE_T, path, (pri_size_t)(e - offs));
#endif
	if ((d = open(path, O_RDWR | O_CREAT, 0666)) == -1) {
		syslog(LOG_DEBUG, "nvmt_writev: %s: %s", path, strerror(errno));
		return -1;
	}

	if (!(f = fdopen(d, "r+b"))) {
		syslog(LOG_DEBUG, "nvmt_writev: %s: %s", path, strerror(errno));
		return -1;
	}
	if (fseek(f, 0L, SEEK_END) == -1) {
		syslog(LOG_DEBUG, "nvmt_writev: %s: %s", path, strerror(errno));
		return -1;
	}

	for (; offs < e; offs += l) {
		void *p;
		size_t bp, bi;

		bp = nVMTmem_bp(h, offs);
		bi = nVMTmem_bi(h, offs);
		p = h->mem.bases[bp] + bi * h->membsize;

		l = MIN(h->mem.nmemb - bi, e - offs);

		if (fwrite(p, h->membsize, l, f) != l) {
			fclose(f);
			syslog(LOG_DEBUG, "nvmt_writev: %s: %s", path, strerror(errno));
			return -1;
		}
	}

	fclose(f);
	return 0;
}

int
nvmt_extend(h, o, clear)
struct nvmt *h;
size_t o;
{
	size_t bpend;

	if (h->rdonly) {
		syslog(LOG_DEBUG, "nvmt_extend: readonly");
		return -1;
	}

	if (nVMTMMAPEDP(h, o - 1)) {	/* do nothing */
		return 0;
	}
	bpend = nVMTmem_bp(h, o - 1) + 1;
	if (h->mem.size < bpend) {
		void *new;
		size_t newsize = bpend;
		newsize = NA(newsize, 1024);
#if defined DBG
syslog(LOG_DEBUG, "nvmt_extend: realloc: h->mem.size = %"PRIuSIZE_T, (pri_size_t)h->mem.size);
syslog(LOG_DEBUG, "nvmt_extend: realloc: bpend = %"PRIuSIZE_T, (pri_size_t)bpend);
syslog(LOG_DEBUG, "nvmt_extend: realloc: newsize = %"PRIuSIZE_T, (pri_size_t)newsize);
#endif
		if (!(new = realloc(h->mem.bases, newsize * sizeof *h->mem.bases))) {
			syslog(LOG_DEBUG, "nvmt_extend: realloc: %s", strerror(errno));
			return -1;
		}
		h->mem.size = newsize;
		h->mem.bases = new;
#if defined DBG
syslog(LOG_DEBUG, "nvmt_extend: realloc: bases = %p", h->mem.bases);
#endif
	}

	assert(bpend <= h->mem.size);

	for (; h->mem.bpend < bpend; h->mem.bpend++) {
		if (!(h->mem.bases[h->mem.bpend] = malloc(h->mem.allcsz))) {
			syslog(LOG_DEBUG, "nvmt_extend: malloc(%"PRIuSIZE_T"): %s", (pri_size_t)h->mem.allcsz, strerror(errno));
			return -1;
		}
#if defined DBG
syslog(LOG_DEBUG, "nvmt_extend: new base = %p", h->mem.bases[h->mem.bpend]);
#endif
		if (clear) {
			bzero(h->mem.bases[h->mem.bpend], h->mem.allcsz);
		}
	}
	h->mem.xend = o;
	return 0;
}

/* 
 * allocate sequential `size' objects from `offs':
 * `end' points just one after allocated object ... 
 * it may be passed as `offs' on next call to this function
 * (`end' may not be aligned)
 * if `offs' is not aligned, offs will advanced to next bin.
 * if `offs' + 'size' across the fragment boundary,
 *   it is advanced to next fragment.
 */
size_t
nvmt_alloc(h, offs, size, end, clear)
struct nvmt *h;
size_t offs, size, *end;
{
	size_t bp, bi;

	if (h->rdonly) {
		syslog(LOG_DEBUG, "nvmt_alloc: readonly");
		return -1;
	}

	if (offs % h->alignment != 0) {
		offs += h->alignment - offs % h->alignment;
	}

	if (size == 0 || nVMTMMAPEDP(h, offs)) {
		syslog(LOG_DEBUG, "nvmt_extend: requested size is 0");
		return 0;
	}

	if (h->mem.nmemb < size) {
		syslog(LOG_DEBUG, "nvmt_extend: too long word (len = %"PRIuSIZE_T" > %"PRIuSIZE_T")", (pri_size_t)size, (pri_size_t)h->mem.nmemb);
		return 0;	/* too long word */
	}

	bp = nVMTmem_bp(h, offs);
	bi = nVMTmem_bi(h, offs);

	if (h->mem.nmemb - bi < size) {
		/* cannot use current block */
		offs += h->mem.nmemb - bi;

		bp = nVMTmem_bp(h, offs);
		bi = nVMTmem_bi(h, offs);

		assert(bi == 0);
	}

	if (h->mem.xend < offs + size) {
		if (nvmt_extend(h, offs + size, clear) == -1) {
			syslog(LOG_DEBUG, "nvmt_extend failed");
			return 0;
		}
	}

	*end = offs + size;
	return offs;
}
