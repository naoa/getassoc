/*	$Id: strip2.c,v 1.5 2009/07/30 00:08:16 nis Exp $	*/

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
static char rcsid[] = "$Id: strip2.c,v 1.5 2009/07/30 00:08:16 nis Exp $";
#endif

#include <config.h>

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "util.h"

#include <gifnoc.h>

/*
 "/var/users/nis/etc/htdocs/ex/env" => "/var/users/nis/etc/htdocs"
 "/var/users/nis/etc/htdocs/ex/env/" => "/var/users/nis/etc/htdocs"
 "/var/users/nis/etc/htdocs/ex/env//" => "/var/users/nis/etc/htdocs/ex"
 */

char *
strip2(s)
char *s;
{
	char *p;
	int cnt;
	if (!s) {
		return NULL;
	}
	p = s + strlen(s);

	if (p > s && *(p - 1) == '/') {	/* strip a last `/' */
		*--p = '\0';
	}
	for (cnt = 0; p > s; ) {
		p--;
		if (*p == '/') {
			*p = '\0';
			if (++cnt == 2) {
				return s;
			}
		}
	}
	return NULL;
}
