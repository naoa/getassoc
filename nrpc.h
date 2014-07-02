/*	$Id: nrpc.h,v 1.16 2009/09/24 08:55:18 nis Exp $	*/

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

#include "marshal.h"

struct nrpc_t;

struct nrpc_fnreg_t {
	int (*mar_arg)(struct mar_t *, void *);
	void *(*serv)(struct nrpc_t *, void *);
	int (*mar_retval)(struct mar_t *, void *);
};

struct nrpc_pkthdr_t {
	size_t size;
	int selector;
};

struct assv_result_t {
	struct s_syminfo *r;
	df_t nd;
	df_t total;
};

struct xfss_result_t {
	struct fss_simple_query rp, rn;
};

struct nrpc_t {
	int s;
	struct {
		void *pkt;
		size_t size;
	} recvbuf;
	struct nrpc_fnreg_t *fnreg;
	size_t nfnreg;
	struct nrpc_pkthdr_t ph;
	struct mar_t *m;
	int flag;
#define	NRPC_DONE	1
	struct {
		int e;
		WAM *w;
		struct distconf_t dc;
		struct assv_result_t r;
		struct xfss_result_t xr;
	} u;
	struct {
		struct {
			size_t bytes, pkts;
		} sent, recv;
	} stat;

	char const *serv_pwam_root;	/* valid only on server side */
	char const *serv_tmpdir;	/* valid only on server side */

	int bulkmode;
	struct {
		void *buf;
		size_t size, n;
	} sendbuf;
};

#define	NRPC_ACK	0x8000
#define	NRPC_NACK	0x4000

int nrpc_init(struct nrpc_t *, int, struct nrpc_fnreg_t *, size_t);
void nrpc_clear(struct nrpc_t *);
int nrpc_call_mux(struct nrpc_t *, int, int, void *);
int nrpc_serv_1(struct nrpc_t *);
int nrpc_setbulk(struct nrpc_t *, size_t);

int nrpc_sendpkt(struct nrpc_t *, struct iovec *, int, int);
int nrpc_recvpkt(struct nrpc_t *);

#define	nrpc_call_1(n, s, d)	(nrpc_call_mux((n), 1, (s), (d)))
