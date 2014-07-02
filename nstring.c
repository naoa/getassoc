/*	$Id: nstring.c,v 1.10 2009/08/18 08:46:36 nis Exp $	*/

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
static char rcsid[] = "$Id: nstring.c,v 1.10 2009/08/18 08:46:36 nis Exp $";
#endif

#include <config.h>

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "util.h"

#include "nstring.h"

#include <gifnoc.h>

int
ns_append(a, s, len)
struct nstring *a;
char const *s;
ssize_t len;
{
	if (len == -1) {
		len = strlen(s);
	}
	if (a->size <= a->len + len) {
		void *new;
		size_t newsize = a->len + len;
		newsize = NA(newsize + 1, 1024);
		if (!(new = realloc(a->ptr, newsize))) {
			return -1;
		}
		a->size = newsize;
		a->ptr = new;
	}
	memmove(a->ptr + a->len, s, len);
	(a->ptr + a->len)[len] = '\0';
	a->len += len;
	return 0;
}

int
writefn_ns_append(cookie, buf, size)
void *cookie;
char *buf;
{
	if (ns_append(cookie, buf, size) == -1) {
		return -1;
	}
	return size;
}
