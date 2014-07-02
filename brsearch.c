/*	$Id: brsearch.c,v 1.8 2009/08/09 23:47:53 nis Exp $	*/

/*-
 * Copyright (c) 2001 Shingo Nishioka.
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
static char rcsid[] = "$Id: brsearch.c,v 1.8 2009/08/09 23:47:53 nis Exp $";
#endif

#include <config.h>

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>

#include "util.h"

#include <gifnoc.h>

int
brsearch(key, base, nmemb, size, compar, left, right)
const void *key, *base;
size_t nmemb, size;
int (*compar)(const void *, const void *);
size_t *left, *right;
{
	size_t l, m, r;
	int e;
#define	BASE(m)	((char *)base + (m) * size)

	if (nmemb == 0) {
		*left = *right = 0;
		return -1;
	}

	for (l = 0, r = nmemb; l < r-1;) {
		m = (l + r - 1) / 2;
		if ((*compar)(key, BASE(m)) > 0) {
			l = m + 1;
		}
		else {
			r = m + 1;
		}
	}
	e = (*compar)(key, BASE(l));
	if (e < 0) {
		*left = *right = l;
		return -1;
	}
	else if (e > 0) {
		*left = *right = r;
		return -1;
	}
	*left = l;
	for (r = nmemb; l < r - 1; ) {
		m = (l + r) / 2;
		if ((*compar)(key, BASE(m)) >= 0) {
			l = m;
		}
		else {
			r = m;
		}
	}
	*right = r;
	return 0;
}
