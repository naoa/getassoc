/*	$Id: mapfile.c,v 1.17 2009/08/27 01:56:48 nis Exp $	*/

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
static char rcsid[] = "$Id: mapfile.c,v 1.17 2009/08/27 01:56:48 nis Exp $";
#endif

#include <config.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <unistd.h>

#include "util.h"

#include <gifnoc.h>

struct mapfile_private {
	int flag;
};

struct mapfile_t *
mapfile(path)
char const *path;
{
	return mapfile_prot(path, PROT_READ, MAP_SHARED);
}

struct mapfile_t *
mapfile_rw(path)
char const *path;
{
	return mapfile_prot(path, PROT_READ|PROT_WRITE, MAP_SHARED);
}

struct mapfile_t *
mapfile_prot(path, prot, flags)
char const *path;
{
	struct stat sb;
	int d;
	void *ptr;
	struct mapfile_t *m;

	if (!(m = calloc(1, sizeof *m))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}
	if (!(m->private = calloc(1, sizeof *m->private))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		goto error;
	}
	if (stat(path, &sb) == -1) {
		syslog(LOG_DEBUG, "%s: %s", path, strerror(errno));
		goto error;
	}
	if (sb.st_size == 0) {
		m->size = 0;
		m->ptr = NULL;
		m->private->flag = 0;
		return m;
	}
	if ((d = open(path, (prot & PROT_WRITE) ? O_RDWR : O_RDONLY)) == -1) {
		syslog(LOG_DEBUG, "%s: %s", path, strerror(errno));
		goto error;
	}

	if ((ptr = mmap(0, sb.st_size, prot, flags, d, 0)) == MAP_FAILED) {
		syslog(LOG_DEBUG, "mmap: %s", strerror(errno));
		close(d);
		goto error;
	}
	close(d);
	m->size = sb.st_size;
	m->ptr = ptr;
	m->private->flag = 1;
	return m;
error:
	free(m->private);
	free(m);
	return NULL;
}

struct mapfile_t *
mmap_alloc(size)
size_t size;
{
	void *ptr;
	struct mapfile_t *m;

	if (!(m = calloc(1, sizeof *m))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		return NULL;
	}
	if (!(m->private = calloc(1, sizeof *m->private))) {
		syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
		goto error;
	}
	if ((ptr = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANON, -1, 0))==MAP_FAILED) {
		syslog(LOG_DEBUG, "mmap: %s", strerror(errno));
		goto try_malloc;
	}
	m->size = size;
	m->ptr = ptr;
	m->private->flag = 1;
	return m;

try_malloc:
syslog(LOG_DEBUG, "mmap MAP_ANON failed. try malloc: size = %"PRIuSIZE_T, (pri_size_t)size);
	if (!(ptr = malloc(size))) {
		syslog(LOG_DEBUG, "malloc: %s", strerror(errno));
		goto error;
	}
	m->size = size;
	m->ptr = ptr;
	m->private->flag = 2;
	return m;

error:
	free(m->private);
	free(m);
	return NULL;
}

void
mapfile_destroy(m)
struct mapfile_t *m;
{
	switch (m->private->flag) {
	case 0:
		break;
	case 1:
		munmap((void *)m->ptr, m->size);
		break;
	case 2:
		free((void *)m->ptr);
		break;
	default:
		syslog(LOG_DEBUG, "mapfile_destroy: invalid argument");
		break;
	}
	free(m->private);
	free(m);
}
