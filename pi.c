/*	$Id: pi.c,v 1.8 2009/08/27 01:56:48 nis Exp $	*/

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
static char rcsid[] = "$Id: pi.c,v 1.8 2009/08/27 01:56:48 nis Exp $";
#endif

#include <config.h>

#include <sys/types.h>

#include <math.h>
#include <stdio.h>

#include "util.h"
#include "pi.h"

#include <gifnoc.h>

void
pi_init(pi, n, c, c2, wid, stream)
struct pi *pi;
size_t n;
FILE *stream;
{
	pi->n = n;
	pi->step = 0;
	pi->ch = c;
	pi->ch2 = c2;
	pi->wid = wid;
	pi->stream = stream;
	if (pi->n) {
		pi->next_report = 0;
	}
	else {
		pi->next_report = exp(pi->step * log(10) / 3);
	}
}

void
pi_progress(pi)
struct pi *pi;
{
	if (pi->n) {
		putc(pi->ch, pi->stream);
		pi->next_report = (int64_t)pi->n * ++pi->step / pi->wid;
	}
	else {
		fprintf(pi->stream, "%c%c%"PRIuSIZE_T"%c%c", pi->ch, pi->ch, (pri_size_t)pi->next_report, pi->ch2, pi->ch2);
		pi->step++;
		pi->next_report = exp(pi->step * log(10) / 3);
	}
	fflush(pi->stream);
}

void
pi_redisplay(pi)
struct pi *pi;
{
	int j;
	for (j = 0; j < pi->step; j++) {
		putc(' ', pi->stream);
	}
}
