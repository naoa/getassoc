/*	$Id: nht.h,v 1.17 2009/07/26 03:28:16 nis Exp $	*/

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

typedef idx_t hashval_t;
#define	PRIuHASHVAL_T PRIuIDX_T

int nht_drop(char const *);
struct nht *nht_open(char const *, size_t, mode_t);
int nht_close(struct nht *);
void *nht_enter(struct nht *, char const *, void *);
void *nht_retrieve(struct nht *, char const *);
void *nht_seq(struct nht *);
int nht_delete(struct nht *, char const *);
void nht_rewind(struct nht *);

int nit_drop(char const *);
struct nit *nit_open(char const *, mode_t);
int nit_close(struct nit *);
void *nit_enter(struct nit *, hashval_t, void *, size_t);
void *nit_retrieve(struct nit *, hashval_t, size_t *);
void *nit_seq(struct nit *, hashval_t *, size_t *);
int nit_delete(struct nit *, hashval_t);
void nit_rewind(struct nit *);
