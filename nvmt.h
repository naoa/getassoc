/*	$Id: nvmt.h,v 1.10 2009/07/18 11:21:47 nis Exp $	*/

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

/*
 * allcsz: size of fragment (byte)
 * r_allcsz: size of last(== bpend-1) map's fragment
 * nmemb: # of entries par fragment
 * r_nmemb: # of entries par fragment of last fragment
 * size: of bases (entries)
 * bpend: last effective element's index + 1. 0 .. size
 * xend: # of entries so far
 */
struct nvmt {
	char *path;
	struct {
		size_t allcsz, r_allcsz, nmemb, r_nmemb, size, bpend, xend;
		char **bases;
		struct mapfile_t **maps;
	} map, mem;
	size_t alignment, membsize;
	int rdonly;
};

int nvmt_drop(char const *);
struct nvmt *nvmt_open(char const *, size_t, int, mode_t);
int nvmt_close(struct nvmt *);
int nvmt_extend(struct nvmt *, size_t, int);
size_t nvmt_alloc(struct nvmt *, size_t, size_t, size_t *, int);

#define	nVMTxend(h) ((h)->mem.xend)
#define	nVMTMMAPEDP(h, i) ((i) < (h)->map.xend)

#define	nVMTmap_bp(h, i) ((i) / (h)->map.nmemb)
#define	nVMTmap_bi(h, i) ((i) % (h)->map.nmemb)

#define	nVMTmem_bp(h, i) (((i) - (h)->map.xend) / (h)->mem.nmemb)
#define	nVMTmem_bi(h, i) (((i) - (h)->map.xend) % (h)->mem.nmemb)

#define	nVMTmap(h, i, t) ((t *)(h)->map.bases[nVMTmap_bp(h, i)] + nVMTmap_bi(h, i))
#define	nVMTmem(h, i, t) ((t *)(h)->mem.bases[nVMTmem_bp(h, i)] + nVMTmem_bi(h, i))

#define	nVMTa(h, i, t) (nVMTMMAPEDP(h, i) ? nVMTmap(h, i, t) : nVMTmem(h, i, t))
