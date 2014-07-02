/*	$Id: maxrlimit.c,v 1.11 2009/08/27 01:56:48 nis Exp $	*/

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
static char rcsid[] = "$Id: maxrlimit.c,v 1.11 2009/08/27 01:56:48 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/resource.h>

#include <stdio.h>

#include "util.h"

#include <gifnoc.h>

/* #define VERBOSE_MAXRLIMIT	1 /* */

void
maxrlimit(resource, name)
char const *name;
{
#if ! defined NO_RESOURCE_H
	struct rlimit rl;
	if (getrlimit(resource, &rl) == -1) {
		perror(name);
	}
	else if (rl.rlim_cur < rl.rlim_max) {
#if defined VERBOSE_MAXRLIMIT
		rlim_t prev = rl.rlim_cur;
#endif
		rl.rlim_cur = rl.rlim_max;
		if (setrlimit(resource, &rl) == -1) {
			perror(name);
		}
		else {
#if defined VERBOSE_MAXRLIMIT
			fprintf(stderr, "%s: %"PRIuSIZE_T" => %"PRIuSIZE_T"\n", name, (pri_size_t)prev, (pri_size_t)rl.rlim_cur);
#endif
		}
	}
#if defined VERBOSE_MAXRLIMIT
	else {
			fprintf(stderr, "%s: %"PRIuSIZE_T"\n", name, (pri_size_t)rl.rlim_cur);
	}
#endif
#endif
}
