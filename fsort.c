/*	$Id: fsort.c,v 1.19 2009/08/17 02:39:56 nis Exp $	*/

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
static char rcsid[] = "$Id: fsort.c,v 1.19 2009/08/17 02:39:56 nis Exp $";
#endif

#include <config.h>

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

#include <gifnoc.h>

#define USE_HSORT	0

#if USE_HSORT
static int hsort(void *, size_t, size_t, int (*)(const void *, const void *));
#endif

int
fsort(stream, nmemb, size, compar)
FILE *stream;
size_t nmemb, size;
int (*compar)(const void *, const void *);
{
	void *base;
	if (!(base = malloc(nmemb * size))) {
		perror("malloc");
		return -1;
	}
	rewind(stream);
	if (fread(base, size, nmemb, stream) != nmemb) {
		perror("fread");
		free(base);
		return -1;
	}
#if USE_HSORT
	if (hsort(base, nmemb, size, compar) == -1) {
		perror("hsort");
		free(base);
		return -1;
	}
#else
	qsort(base, nmemb, size, compar);
#endif
	rewind(stream);
	if (fwrite(base, size, nmemb, stream) != nmemb) {
		perror("fwrite");
		free(base);
		return -1;
	}
	free(base);
	return 0;
}

#if USE_HSORT
/*
 *	base[0..nmemb-1]	2*i+1 2*i+2 (nmemb+1)/2 .. nmemb-1
 */

static int
hsort(base, nmemb, size, compar)
void *base;
size_t nmemb, size;
int (*compar)(const void *, const void *);
{
	size_t i, j;
	size_t l, r;
	void *x;
#define	BASE(i)	((void *)((char *)base + (i) * size))
#define	COPY(dst, src)	(memmove((dst), (src), size))

	if (nmemb == 0) {
		return 0;
	}

	if (!(x = calloc(1, size))) {
		return -1;
	}

	l = (nmemb + 1)/2;
	r = nmemb - 1;

	while (0 < l) {
		l--;
		for (i = l, COPY(x, BASE(i)); (j = 2 * i + 1) <= r; i = j) {
			if (j + 1 <= r && (*compar)(BASE(j), BASE(j + 1)) < 0) {
				j++;
			}
			if ((*compar)(BASE(j), x) < 0) {
				break;
			}
			COPY(BASE(i), BASE(j));
		}
		if (i != l) {
			COPY(BASE(i), x);
		}
	}

	/* now, 0 .. r is heap. */

	while (0 < r) {
		COPY(x, BASE(r));
		COPY(BASE(r), BASE(0));
		r--;

		for (i = 0; (j = 2 * i + 1) <= r; i = j) {
			if (j + 1 <= r && (*compar)(BASE(j), BASE(j + 1)) < 0) {
				j++;
			}
			if ((*compar)(BASE(j), x) < 0) {
				break;
			}
			COPY(BASE(i), BASE(j));
		}
		COPY(BASE(i), x);
	}
	free(x);
	return 0;
}
#endif
