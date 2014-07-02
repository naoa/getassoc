/*	$Id: wtouch.c,v 1.10 2011/09/14 03:06:09 nis Exp $	*/

/*-
 * Copyright (c) 2008 Shingo Nishioka.
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
static char rcsid[] = "$Id: wtouch.c,v 1.10 2011/09/14 03:06:09 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <stdio.h>
#include <unistd.h>

/*#include "geta.h"*/

#include "util.h"

#include <gifnoc.h>

int touch(char const *, int);

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

int
main(argc, argv)
char *argv[];
{
        int pagesize = getpagesize();

	for (argc--, argv++; argc > 0; argc--, argv++) {
		touch(argv[0], pagesize);
	}
	return 0;
}

int
touch(path, pagesize)
char const *path;
{
	char const *q, *e;
	int r = 0;
	struct timeval start, end, elap;
	struct mapfile_t *m;

	gettimeofday(&start, NULL);
	if (!(m = mapfile(path))) {
		perror(path);
		return 0;
	}
	fprintf(stderr, "%s: ", path);
	for (q = m->ptr, e = (char const *)m->ptr + m->size; q < e; q += pagesize) {
		r += *q & 0xff;
	}
	gettimeofday(&end, NULL);
	timersub(&end, &start, &elap);
	fprintf(stderr, "%ld.%03ld\n", (long)elap.tv_sec, (long)(elap.tv_usec / 1000));

	mapfile_destroy(m);
	return r;
}
