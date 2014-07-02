/*	$Id: systemv.c,v 1.3 2011/09/14 03:06:09 nis Exp $	*/

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
static char rcsid[] = "$Id: systemv.c,v 1.3 2011/09/14 03:06:09 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <assert.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <unicode/utf.h>
#include <unicode/utf8.h>

#include "nwam.h"
#include "util.h"

#include "stpP.h"

#include <gifnoc.h>

#if ! defined NO_FORK
extern char **environ;
#endif

int
systemv(path, argv)
char const *path;
char * const argv[];
{
#if ! defined NO_FORK
	int k;
	pid_t pid;
	int status;

	fprintf(stderr, "cmd: %s (%s)\n", argv[0], path);
	fprintf(stderr, "arg:");
	for (k = 1; argv[k]; k++) {
		fprintf(stderr, " %s", argv[k]);
	}
	fprintf(stderr, "\n");

	switch (pid = fork()) {
	case -1:
		perror("fork");
		return -1;
	case 0:
		execve(path, argv, environ);
		perror(path);
		_exit(1);
	default:
		break;
	}

	if (wait4(pid, &status, 0, NULL) == -1) {
		perror("wait4");
		return -1;
	}
	if (diag_stat(path, status) == -1) {
		perror("diag_stat");
		return -1;
	}
	return 0;
#else
	return spawn(path, argv);
#endif
}

#if defined ENABLE_GETA
struct popenv_t *
popenv(path, argv)
char const *path;
char * const argv[];
{
	int k;
	struct popenv_t *p;
	int d[2];

	fprintf(stderr, "cmd: %s (%s)\n", argv[0], path);
	fprintf(stderr, "arg:");
	for (k = 1; argv[k]; k++) {
		fprintf(stderr, " %s", argv[k]);
	}
	fprintf(stderr, "\n");

	if (!(p = calloc(1, sizeof *p))) {
		perror("calloc");
		return NULL;
	}

	if (!(p->path = strdup(path))) {
		free(p);
		perror("strdup");
		return NULL;
	}

	if (pipe(d) == -1) {
		free(p->path);
		free(p);
		perror("pipe");
		return NULL;
	}

	switch (p->pid = fork()) {
	case -1:
		perror("fork");
		free(p->path);
		free(p);
		return NULL;
	case 0:
		dup2(0, d[0]);
		close(d[1]);
		execve(path, argv, environ);
		perror(path);
		_exit(1);
	default:
		close(d[0]);
		if (!(p->stream = fdopen(d[1], "wb"))) {
			close(d[1]);
			free(p->path);
			free(p);
			perror("fdopen");
			return NULL;
		}
		break;
	}

	return p;
}

static int
pclosev(p)
struct popenv_t *p;
{
	int status;
	fclose(p->stream);
	if (wait4(p->pid, &status, 0, NULL) == -1) {
		perror("wait4");
		return -1;
	}
	if (diag_stat(p->path, status) == -1) {
		perror("diag_stat");
		return -1;
	}
	free(p->path);
	return 0;
}
#endif
