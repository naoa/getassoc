/*	$Id: marshal.c,v 1.18 2011/09/14 02:36:15 nis Exp $	*/

/*-
 * Copyright (c) 2009 Shingo Nishioka.
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
static char rcsid[] = "$Id: marshal.c,v 1.18 2011/09/14 02:36:15 nis Exp $";
#endif

#include <config.h>

#include <sys/types.h>
#include <sys/time.h>

#include <sys/uio.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

#include "marshal.h"

#include <gifnoc.h>

struct pkthdr_t {
	size_t objsize, nrelent;
};

struct relent_t {
	size_t offset;
	size_t address;
};

struct basemem_t {
	void *ptr;
	size_t offset;
};

struct mar_t {
	struct pkthdr_t pkthdr;
	void *object;
	struct relent_t *relocation_table;
	struct basemem_t *base_table;
	size_t nbases;
	size_t object_size, relocation_table_size, base_table_size;
	struct iovec v[3];
};


struct mar_t *
mar_create()
{
	struct mar_t *m;
	if (!(m = calloc(1, sizeof *m))) {
		return NULL;
	}

	m->object_size = m->relocation_table_size = m->base_table_size = 0;

	m->object = NULL;
	m->relocation_table = NULL;
	m->base_table = NULL;

	mar_reset(m);
	return m;
}

void
mar_reset(m)
struct mar_t *m;
{
	m->pkthdr.objsize = 0;
	m->pkthdr.nrelent = 0;
	m->nbases = 0;
}

int
mar_append_p(m, base, ptr, size)
struct mar_t *m;
void *base, *ptr;
size_t size;
{
	struct relent_t *e;
	struct basemem_t *b;
	size_t i;
	size_t base_o, ptr_o;

	if (!base) {
		assert(m->pkthdr.objsize == 0);
	}
	else {
		assert(m->pkthdr.objsize != 0);
	}

	ptr_o = m->pkthdr.objsize;
	if (m->object_size < ptr_o + size) {
		void *new;
		size_t newsize = ptr_o + size;
		newsize = NA(newsize, 1024);
		if (!(new = realloc(m->object, newsize))) {
			return -1;
		}
		m->object_size = newsize;
		m->object = new;
	}
	memmove((char *)m->object + ptr_o, *(void **)ptr, size);

	if (m->base_table_size <= m->nbases) {
		void *new;
		size_t newsize = m->nbases;
		newsize = NA(newsize + 1, 1024); 
		if (!(new = realloc(m->base_table, newsize * sizeof *m->base_table))) {
			return -1;
		}
		m->base_table_size = newsize;
		m->base_table = new;
	}
	b = &m->base_table[m->nbases++];
	b->ptr = *(void **)ptr;
	b->offset = ptr_o;

	if (size % 4) {
		size += (4 - size % 4);
	}
	m->pkthdr.objsize += size;

	if (base) {
		for (i = 0; i < m->nbases; i++) {
			b = &m->base_table[i];
			if (b->ptr == base) {
				base_o = b->offset;
				goto found;
			}
		}
		return -1;
	found:
		if (m->relocation_table_size <= m->pkthdr.nrelent) {
			void *new;
			size_t newsize = m->pkthdr.nrelent;
			newsize = NA(newsize + 1, 8);
			if (!(new = realloc(m->relocation_table, newsize * sizeof *m->relocation_table))) {
				return -1;
			}
			m->relocation_table_size = newsize;
			m->relocation_table = new;
		}
		e = &m->relocation_table[m->pkthdr.nrelent++];
		e->offset = base_o + ((char *)ptr - (char *)base);
		e->address = ptr_o;
	}
	return 0;
}

struct iovec *
mar_finish(m, iovcnt)
struct mar_t *m;
int *iovcnt;
{
	struct iovec *v;

	if (m->pkthdr.objsize == 0) {
		return NULL;
	}
	assert(m->nbases == m->pkthdr.nrelent + 1);

	v = m->v;
	v[0].iov_base = &m->pkthdr;
	v[0].iov_len = sizeof m->pkthdr;
	v[1].iov_base = m->object;
	v[1].iov_len = m->pkthdr.objsize;
	*iovcnt = 2;
	if (m->pkthdr.nrelent != 0) {
		v[2].iov_base = m->relocation_table;
		v[2].iov_len = m->pkthdr.nrelent * sizeof (struct relent_t);
		*iovcnt = 3;
	}
	return v;
}

void
mar_destroy(m)
struct mar_t *m;
{
	free(m->object);
	free(m->relocation_table);
	free(m->base_table);
	free(m);
}

void *
unmar(p)
void *p;
{
	struct pkthdr_t *h;
	struct relent_t *relocation_table;
	char *obj;
	size_t i;

	if (!p) {
		return NULL;
	}
	h = p;
	obj = (char *)(h + 1);
	relocation_table = (struct relent_t *)(obj + h->objsize);
	for (i = 0; i < h->nrelent; i++) {
		struct relent_t *e = &relocation_table[i];
		*(void **)(obj + e->offset) = (void *)(obj + e->address);
	}
	return obj;
}

void
chkmem(v, size)
void *v;
size_t size;
{
	size_t i;
	int sum = 0;
	char *p = v;
	for (i = 0; i < size; i++) {
		sum += p[i];
	}
	printf("ok: %p %"PRIuSIZE_T" %x\n", v, (pri_size_t)size, sum);
}
