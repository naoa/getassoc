/*	$Id: proc_body.c,v 1.21 2011/09/14 03:06:09 nis Exp $	*/

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
static char rcsid[] = "$Id: proc_body.c,v 1.21 2011/09/14 03:06:09 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <assert.h>
#include <ctype.h>
#include <libgen.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "nwam.h"
#include "util.h"

#include "xstem.h"

#include "itb2frq.h"

#include <gifnoc.h>

int
init_proc_body(g, stemmer)
struct state *g;
char const *stemmer;
{
	if (setup_pstem(g, stemmer) == -1) {
		return -1;
	}
	return 0;
}

extern char *progname;

int
cleanup_proc_body(g)
struct state *g;
{
	int status;
	char cmd[MAXPATHLEN];	/* XXX */
	xstem_destroy(g->ps);
	g->ps = NULL;
	if (waitpid(g->relay_pid, &status, 0) == -1) {
		perror("waitpid");
		return -1;
	}
	snprintf(cmd, sizeof cmd, "%s.relay", progname);
	if (diag_stat(cmd, status) == -1) {
		return -1;
	}
	if (waitpid(g->pstem_pid, &status, 0) == -1) {
		perror("waitpid");
		return -1;
	}
	snprintf(cmd, sizeof cmd, "%s.pstem", progname);
	if (diag_stat(cmd, status) == -1) {
		return -1;
	}
	return 0;
}

int
proc_body(g, r, fp)
struct state *g;
char *r;
struct fs *fp;
{
	int btype;
	char *p, *q;
	if (!(p = xstem_recv(g->ps))) {
		fprintf(stderr, "xstem_recv failed\n");
		return 1;
	}
	btype = r[1] & 0xff;
	if (btype != '1') {
		fprintf(stderr, "invalid btype: '%c'\n", btype);
		return -1;
	}
	if (r[2] == '\0') {
		return 0;	/* ok. do nothing */
	}
/* fprintf(stderr, "."); */
	for (q = p; *p; p++) {
		if (*p == '\n') {
			*p = '\0';
			fputs("1 ", fp->stream);
			fputs(q, fp->stream);
			fputs("\n", fp->stream);
			q = p + 1;
		}
	}
	if (p != q) {
		fputs("1 ", fp->stream);
		fputs(q, fp->stream);
		fputs("\n", fp->stream);
	}
	return 0;
}
