/*	$Id: traverse.c,v 1.3 2011/09/14 03:06:09 nis Exp $	*/

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
static char rcsid[] = "$Id: traverse.c,v 1.3 2011/09/14 03:06:09 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <assert.h>
#include <fcntl.h>
#if defined HAVE_FTS_OPEN
#include <fts.h>
#elif defined HAVE_NFTW
#include <ftw.h>
#else
configureation error!
#endif
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "nwam.h"
#include "util.h"

#include "stpP.h"

#include <gifnoc.h>

#if defined HAVE_FTS_OPEN
#elif defined HAVE_NFTW
static int fnw(const char *, const struct stat *, int, struct FTW *);
#endif

#if defined HAVE_FTS_OPEN
int
traverse_fn(path, fn, cookie)
const char *path;
int (*fn)(char const *, void *);
void *cookie;
{
	char *paths[2];
	FTS *ftsp;
	FTSENT *cur;
	int e = 0;

	paths[0] = (char *)path;
	paths[1] = NULL;
	if (!(ftsp = fts_open(paths, FTS_COMFOLLOW|FTS_LOGICAL, NULL))) {
		perror(path);
		return -1;
	}
	while (cur = fts_read(ftsp)) {
		if (cur->fts_info == FTS_F && (*fn)(cur->fts_path, cookie) != 0) {
			e = -1;
			goto error;
		}
	}
error:
	if (fts_close(ftsp) == -1) {
		return -1;
	}
	return e;
}

#elif defined HAVE_NFTW

static int (*fnx)(char const *, void *);
static void *fnc;

int
traverse_fn(path, fn, cookie)
const char *path;
int (*fn)(char const *, void *);
void *cookie;
{
	fnx = fn;
	fnc = cookie;
	return nftw(path, fnw, 1, FTW_DEPTH);
}

static int
fnw(path, sb, flag, ftw)
const char *path;
const struct stat *sb;
struct FTW *ftw;
{
	if (flag == FTW_F) {
		return (*fnx)(path, fnc);
	}
	return 0;
}
#endif
