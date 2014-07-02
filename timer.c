/*	$Id: timer.c,v 1.10 2009/08/27 01:56:48 nis Exp $	*/

/*-
 * Copyright (c) 2005 Shingo Nishioka.
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
static char rcsid[] = "$Id: timer.c,v 1.10 2009/08/27 01:56:48 nis Exp $";
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

#if ! defined(timeradd)
#define	timeradd(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec + (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec + (uvp)->tv_usec;	\
		if ((vvp)->tv_usec >= 1000000) {			\
			(vvp)->tv_sec++;				\
			(vvp)->tv_usec -= 1000000;			\
		}							\
	} while (0)
#endif

#if ! defined(timersub)
#define	timersub(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;	\
		if ((vvp)->tv_usec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_usec += 1000000;			\
		}							\
	} while (0)
#endif

struct timer {
	struct timeval begin, end, elapsed;
};

static struct timer timers;

struct timer *
timer_new()
{
	struct timer *t;
	t = calloc(1, sizeof *t);
	timer_init(t);
	return t;
}

void
timer_init(t)
struct timer *t;
{
	if (!t) t = &timers;
	bzero(t, sizeof *t);
	gettimeofday(&t->begin, NULL);
}

void
timer_check_elapsed(t)
struct timer *t;
{
	if (!t) t = &timers;
	gettimeofday(&t->end, NULL);
	timersub(&t->end, &t->begin, &t->elapsed);
}

void
timer_print_elapsed(t, msg, stream)
struct timer *t;
char const *msg;
FILE *stream;
{
	if (!t) t = &timers;
	timer_check_elapsed(t);
	fprintf(stream, msg ? "%s: %"PRIuSIZE_T".%03"PRIuSIZE_T"\n" : "(%s%"PRIuSIZE_T".%03"PRIuSIZE_T")\n", msg ? msg : "", (pri_size_t)t->elapsed.tv_sec, (pri_size_t)(t->elapsed.tv_usec / 1000L));
}

void
timer_sprint_elapsed(t, msg, s, size)
struct timer *t;
char const *msg;
char *s;
size_t size;
{
	if (!t) t = &timers;
	timer_check_elapsed(t);
	snprintf(s, size, msg ? "%s: %"PRIuSIZE_T".%03"PRIuSIZE_T : "%s%"PRIuSIZE_T".%03"PRIuSIZE_T, msg ? msg : "", (pri_size_t)t->elapsed.tv_sec, (pri_size_t)(t->elapsed.tv_usec / 1000L));
}
