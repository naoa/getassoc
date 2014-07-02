/*	$Id: diag.c,v 1.7 2009/07/30 00:08:15 nis Exp $	*/

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
static char rcsid[] = "$Id: diag.c,v 1.7 2009/07/30 00:08:15 nis Exp $";
#endif

#include <config.h>

#include <sys/types.h>
#include <sys/time.h>

#include <sys/wait.h>

#include <stdio.h>

#include "util.h"

#include <gifnoc.h>

int
diag_stat(cmd, status)
char const *cmd;
{
	if (WIFEXITED(status)) {
		if (WEXITSTATUS(status) != 0) {
			fprintf(stderr, "%s exit(%d)\n", cmd, WEXITSTATUS(status));
			return -1;
		}
fprintf(stderr, "%s exit(%d)\n", cmd, WEXITSTATUS(status));
	}
	else if (WIFSIGNALED(status)) {
		fprintf(stderr, "%s killed(%d)%s\n", cmd, WTERMSIG(status), WCOREDUMP(status) ? " (core dumped)" : "");
		return -1;
	}
#if 0
	else if (WIFCONTINUED(status)) {
		fprintf(stderr, "continued  %s\n", cmd);
		return -1;
	}
#endif
	return 0;
}
