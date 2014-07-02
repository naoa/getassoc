/*	$Id: dumpri.c,v 1.25 2009/07/30 00:08:15 nis Exp $	*/

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
static char rcsid[] = "$Id: dumpri.c,v 1.25 2009/07/30 00:08:15 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>

#include <fcntl.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "nwam.h"
#include "util.h"

#include "fssP.h"

#include <gifnoc.h>

char *progname = NULL;

int
main(argc, argv)
char *argv[];
{
#if ! defined ENABLE_GETA
	WAM *w, *f;
#else
	FSS *f;
#endif
	char *getaroot; /* = GETAROOT; */

	progname = basename(argv[0]);
	if (argc != 3) {
		fprintf(stderr, "usage: %s dirname handle\n", progname);
		return 1;
	}

	getaroot = argv[1];

#if ! defined ENABLE_GETA
	w = wam_open(argv[2], getaroot);
	f = wam_fss_open(w);
#else
	f = fss_open(getaroot, argv[2]);
#endif
	if (!f) {
		return 1;
	}

	if (
#if ! defined ENABLE_GETA
		wam_fss_dump
#else
		fss_dump
#endif
		(f, stdout) == -1) {
		return 1;
	}

#if ! defined ENABLE_GETA
	wam_close(f);
	wam_close(w);
#else
	fss_close(f);
#endif
	return 0;
}
