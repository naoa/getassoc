/*	$Id: mmkri.c,v 1.8 2011/09/14 03:06:09 nis Exp $	*/

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
static char rcsid[] = "$Id: mmkri.c,v 1.8 2011/09/14 03:06:09 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/resource.h>
#include <sys/time.h>

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>

#include "nwam.h"
#include "util.h"

#include "fssP.h"

#include <gifnoc.h>

char *progname;

static void
usage()
{
	fprintf(stderr, "usage: %s dirname handle flwd names out bitmap opts\n", progname);
	exit(1);
}

int
main(argc, argv)
char *argv[];
{
	char *getaroot; /* = GETAROOT; */

	progname = basename(argv[0]);
	if (argc != 8) {
		usage();
	}

#if defined RLIMIT_NOFILE
	MAXRLIMIT(RLIMIT_NOFILE);
#endif

#if defined RLIMIT_CPU
	MAXRLIMIT(RLIMIT_CPU);
#endif

#if defined RLIMIT_VMEM
	MAXRLIMIT(RLIMIT_VMEM);
#endif

#if defined RLIMIT_DATA
	MAXRLIMIT(RLIMIT_DATA);
#endif

	getaroot = argv[1];
	if (mkri(getaroot, argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], MKRI_NCPU) == -1) {
		return 1;
	}
	return 0;
}
