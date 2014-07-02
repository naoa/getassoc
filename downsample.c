/*	$Id: downsample.c,v 1.1 2009/08/10 06:25:20 nis Exp $	*/

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
static char rcsid[] = "$Id: downsample.c,v 1.1 2009/08/10 06:25:20 nis Exp $";
#endif

#include <config.h>

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "util.h"

#include <gifnoc.h>

void
downsample(base, nmemb, n, size)
void *base;
size_t nmemb, n, size;
{
	int adj1, adj2, e;
	int xd, yd;
	int x, y;

	xd = nmemb;
	yd = n;
	adj2 = 2 * yd;
	e = adj2 - xd;
	adj1 = e - xd;
	for(x = 0, y = 0; x < nmemb; ++x) {
		if(e > 0) {
			memmove((char *)base + y * size, (char *)base + x * size, size);
			y++;
			e += adj1;
		}
		else {
			e += adj2;
		}
	}
}
