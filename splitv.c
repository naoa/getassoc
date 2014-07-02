/*	$Id: splitv.c,v 1.8 2009/08/17 01:51:15 nis Exp $	*/

/*-
 * Copyright (c) 2007 Shingo Nishioka.
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
static char rcsid[] = "$Id: splitv.c,v 1.8 2009/08/17 01:51:15 nis Exp $";
#endif

#include <config.h>

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "nwam.h"
#include "util.h"

#include <gifnoc.h>

size_t
splitv(s, c, v, n, collapse_adjoining_separators)
char *s;
char *v[];
size_t n;
{
	size_t i;

	if (n == 0) {
		return 0;
	}
	for (i = 0; *s; ) {
		v[i++] = s;
		if (i >= n) {
			break;
		}
		for (; *s && (*s & 0xff) != c; s++) ;
		if (*s) {
			*s++ = '\0';
		}
		if (collapse_adjoining_separators) {
			for (; *s && (*s & 0xff) == c; s++) ;
		}
	}
	return i;
}

size_t
count_char(s, c)
char const *s;
{
	int n;
	for (n = 0; s && *s; n++) {
		if (s = index(s, c)) {
			s++;
		}
	}
	return n;
}
