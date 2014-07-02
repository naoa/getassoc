/*	$Id: nrpc.c,v 1.29 2011/09/14 02:36:15 nis Exp $	*/

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
static char rcsid[] = "$Id: nrpc.c,v 1.29 2011/09/14 02:36:15 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/select.h>
#include <sys/uio.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>

#include "nwam.h"
#include "util.h"
#include "pi.h"
#include "nio.h"

#include "fssP.h"
#include "nwamP.h"
#include "nrpc.h"

#include <gifnoc.h>

#define	NRPC_SENDBUF_SIZE	1440

#if defined DBG
static void nrpc_stat(struct nrpc_t *, FILE *);
#endif
static ssize_t nrpc_write_sendbuf_v(struct nrpc_t *, struct iovec *, int);
static ssize_t nrpc_write_sendbuf(struct nrpc_t *, void *, size_t);
static int nrpc_flush_sendbuf(struct nrpc_t *);

/* `s' should be a socket */
int
nrpc_init(n, s, fnreg, nfnreg)
struct nrpc_t *n;
struct nrpc_fnreg_t *fnreg;
size_t nfnreg;
{
	n->s = s;
	n->recvbuf.pkt = NULL;
	n->recvbuf.size = 0;
	n->fnreg = fnreg;
	n->nfnreg = nfnreg;
	if (!(n->m = mar_create())) {
		syslog(LOG_DEBUG, "nrpc_init: mar_create failed");
		return -1;
	}
	n->stat.sent.bytes = n->stat.sent.pkts = 0;
	n->stat.recv.bytes = n->stat.recv.pkts = 0;
	n->bulkmode = 0;
	n->sendbuf.size = NRPC_SENDBUF_SIZE;
	if (!(n->sendbuf.buf = malloc(n->sendbuf.size))) {
		syslog(LOG_DEBUG, "malloc: %s", strerror(errno));
		return -1;
	}
	n->sendbuf.n = 0;
	return 0;
}

void
nrpc_clear(n)
struct nrpc_t *n;
{
#if defined DBG
	nrpc_stat(n, stderr);
#endif
	free(n->sendbuf.buf);
	mar_destroy(n->m);
	n->m = NULL;
	free(n->recvbuf.pkt);
	n->recvbuf.pkt = NULL;
}

#if defined DBG
static void
nrpc_stat(n, stream)
struct nrpc_t *n;
FILE *stream;
{
	fprintf(stream, "S: %"PRIuSIZE_T"/%"PRIuSIZE_T" R: %"PRIuSIZE_T"/%"PRIuSIZE_T"\n", (pri_size_t)n->stat.sent.bytes, (pri_size_t)n->stat.sent.pkts, (pri_size_t)n->stat.recv.bytes, (pri_size_t)n->stat.recv.pkts);
}
#endif

int
nrpc_call_mux(n, nn, selector, data)
struct nrpc_t *n;
void *data;
{
	struct iovec *v;
	int iovcnt;
	int i, done;
	int e;

	/* NOTE: check only the first n's selector, assuming that all n are same */
	if (selector < 0 || n->nfnreg <= selector) {
		syslog(LOG_DEBUG, "nrpc_call_mux: selector %d out of range", selector);
		return -1;
	}

	/* NOTE: use only the first n's m for marshaling */
	if (n->fnreg[selector].mar_arg) {
		mar_reset(n->m);
		if ((*n->fnreg[selector].mar_arg)(n->m, data) == -1) {
			syslog(LOG_DEBUG, "nrpc_call_mux: (*mar_arg) failed");
			return -1;
		}
		if (!(v = mar_finish(n->m, &iovcnt))) {
			syslog(LOG_DEBUG, "nrpc_call_mux: mar_finish failed");
			return -1;
		}
	}
	else {
		v = NULL;
		iovcnt = 0;
	}

	for (i = 0; i < nn; i++) {
		if (nrpc_sendpkt(&n[i], v, iovcnt, selector) == -1) {
			syslog(LOG_DEBUG, "nrpc_call_mux (%d): nrpc_sendpkt failed", i);
			return -1;
		}
		n[i].flag &= ~NRPC_DONE;
	}

	if (!n->fnreg[selector].mar_retval) {
		if (n->recvbuf.size) {
			bzero(n->recvbuf.pkt, n->recvbuf.size);
		}
		return 0;
	}

#if defined DBGxx
fprintf(stderr, "<");
#endif
	e = done = 0;
	for (done = 0; done < nn; ) {
		int nfds;
		fd_set fds;
		FD_ZERO(&fds);
		nfds = 0;
		for (i = 0; i < nn; i++) {
			if (!(n[i].flag & NRPC_DONE)) {
				FD_SET(n[i].s, &fds);
				if (nfds <= n[i].s) {
					nfds = n[i].s + 1;
				}
			}
		}

		if (select(nfds, &fds, NULL, NULL, NULL) == -1) {
			if (errno == EINTR) continue;
			syslog(LOG_DEBUG, "select: %s", strerror(errno));
			return -1;
		}

		for (i = 0; i < nn; i++) {
			if (n[i].flag & NRPC_DONE || !FD_ISSET(n[i].s, &fds)) {
				continue;
			}
#if defined DBGxx
fprintf(stderr, "%d", i);
#endif

			if (nrpc_recvpkt(&n[i]) == -1) {
				syslog(LOG_DEBUG, "nrpc_call_mux (%d): nrpc_recvpkt failed", i);
				return -1;
			}
#if defined DBGxx
fprintf(stderr, ".");
#endif
			n[i].flag |= NRPC_DONE;
			done++;
			if (n[i].ph.selector != selector + NRPC_ACK) {
				syslog(LOG_DEBUG, "nrpc_call_mux: !ACK (%d, %d)", (int)n[i].ph.selector, (int)selector + NRPC_ACK);
				e = -1;
			}
		}
	}

#if defined DBGxx
fprintf(stderr, ">");
#endif
	return e;
}

int
nrpc_serv_1(n)
struct nrpc_t *n;
{
	struct iovec *v;
	int iovcnt;
	void *data;

	if (nrpc_recvpkt(n) == -1) {
		syslog(LOG_DEBUG, "nrpc_serv: nrpc_recvpkt failed");
		return -1;
	}
	if (n->ph.selector < 0 || n->nfnreg <= n->ph.selector) {
		syslog(LOG_DEBUG, "nrpc_serv: selector %d out of range", n->ph.selector);
		goto NAK;
	}

	if (!n->fnreg[n->ph.selector].serv) {
		return 0;
	}
	data = (*n->fnreg[n->ph.selector].serv)(n, unmar(n->recvbuf.pkt));

	if (!n->fnreg[n->ph.selector].mar_retval) {
		return 0;
	}
	if (!data) {
		syslog(LOG_DEBUG, "nrpc_serv: !data");
		goto NAK;
	}

	mar_reset(n->m);
	if ((*n->fnreg[n->ph.selector].mar_retval)(n->m, data) == -1) {
		syslog(LOG_DEBUG, "nrpc_serv: (*mar_retval) failed");
		goto NAK;
	}
	if (!(v = mar_finish(n->m, &iovcnt))) {
		syslog(LOG_DEBUG, "nrpc_serv: mar_finish failed");
		goto NAK;
	}
	if (nrpc_sendpkt(n, v, iovcnt, n->ph.selector + NRPC_ACK) == -1) {
		return -1;
	}

	return 0;
NAK:
	if (!n->fnreg[n->ph.selector].mar_retval) {
		return 0;
	}
	if (nrpc_sendpkt(n, NULL, 0, n->ph.selector + NRPC_NACK) == -1) {
		return -1;
	}

	return 0;
}

int
nrpc_sendpkt(n, v, iovcnt, selector)
struct nrpc_t *n;
struct iovec *v;
{
	size_t size;
	int i;
	struct nrpc_pkthdr_t ph;

	for (size = 0, i = 0; i < iovcnt; i++) {
		size += v[i].iov_len;
	}

	ph.size = size;
	ph.selector = selector;

	if (nrpc_write_sendbuf(n, &ph, sizeof ph) != sizeof ph) {
		syslog(LOG_DEBUG, "write_sendbuf: %s", strerror(errno));
		return -1;
	}
	if (size) {
		if (nrpc_write_sendbuf_v(n, v, iovcnt) != size) {
			syslog(LOG_DEBUG, "nrpc_write_sendbuf_v: %s", strerror(errno));
			return -1;
		}
	}
	if (!n->bulkmode) {
		if (nrpc_flush_sendbuf(n) == -1) {
			return -1;
		}
	}
	return 0;
}

int
nrpc_recvpkt(n)
struct nrpc_t *n;
{
	size_t size, l;
	if ((l = recvn(n->s, &n->ph, sizeof n->ph, 0)) != sizeof n->ph) {
		syslog(LOG_DEBUG, "nrpc_recvpkt: recvn failed (h), read=%"PRIuSIZE_T, (pri_size_t)l);
		return -1;
	}
	n->stat.recv.bytes += sizeof n->ph + n->ph.size;
	n->stat.recv.pkts++;
	if (size = n->ph.size) {
		if (n->recvbuf.size < size) {
			void *new;
			size_t newsize = size;
			newsize = NA(newsize, 1024);
			if (!(new = realloc(n->recvbuf.pkt, newsize))) {
				syslog(LOG_DEBUG, "realloc: %s", strerror(errno));
				return -1;
			}
			n->recvbuf.size = newsize;
			n->recvbuf.pkt = new;
		}
		if ((l = recvn(n->s, n->recvbuf.pkt, size, 0)) != size) {
			syslog(LOG_DEBUG, "nrpc_recvpkt: recvn failed (b) socket = %d, expected: %"PRIuSIZE_T", read: %"PRIuSIZE_T", reason: %s", n->s, (pri_size_t)size, (pri_size_t)l, strerror(errno));
			return -1;
		}
	}
	return 0;
}

void
dumpmem(p, size)
void *p;
size_t size;
{
	size_t i, o;
	char buf[131];
	for (i = o = 0; i < size; i++) {
		int c;
		if (i && i % 8 == 0) {
			syslog(LOG_DEBUG, "%s", buf);
			o = 0;
		}
		c = *((char *)p + i) & 0xff;
		if (' ' <= c && c < 0177) {
			sprintf(buf + o, "%02x(%c) ", c, c);
		}
		else {
			sprintf(buf + o, "%02x     ", c);
		}
		o += strlen(buf + o);
	}
	syslog(LOG_DEBUG, "%s", buf);
}

static ssize_t
nrpc_write_sendbuf_v(n, iov, iovcnt)
struct nrpc_t *n;
struct iovec *iov;
{
	int i;
	size_t sent = 0;
	for (i = 0; i < iovcnt; i++) {
		if (nrpc_write_sendbuf(n, iov[i].iov_base, iov[i].iov_len) != iov[i].iov_len) {
			return -1;
		}
		sent += iov[i].iov_len;
	}
	return sent;
}

static ssize_t
nrpc_write_sendbuf(n, buf, size)
struct nrpc_t *n;
void *buf;
size_t size;
{
	size_t sent = 0;
	if (n->sendbuf.size == 0) {
		return -1;
	}
	while (sent < size) {
		size_t r = size - sent;
		size_t c = MIN(r, n->sendbuf.size - n->sendbuf.n);
		memcpy((char *)n->sendbuf.buf + n->sendbuf.n, (char *)buf + sent, c);
		n->sendbuf.n += c;
		if (n->sendbuf.n == n->sendbuf.size) {
			if (nrpc_flush_sendbuf(n) == -1) {
				return -1;
			}
		}
		sent += c;
	}
	return size;
}

static int
nrpc_flush_sendbuf(n)
struct nrpc_t *n;
{
	if (!n->sendbuf.size) {
		return 0;
	}
	if (n->sendbuf.n) {
		n->stat.sent.bytes += n->sendbuf.n;
		n->stat.sent.pkts++;
		if (sendn(n->s, n->sendbuf.buf, n->sendbuf.n, 0) != n->sendbuf.n) {
			syslog(LOG_DEBUG, "sendn: %s", strerror(errno));
			return -1;
		}
	}
	n->sendbuf.n = 0;
	return 0;
}

int
nrpc_setbulk(n, size)
struct nrpc_t *n;
size_t size;
{
	if (nrpc_flush_sendbuf(n) == -1) {
		return -1;
	}
	free(n->sendbuf.buf);

	if (size) {
		n->bulkmode = 1;
		n->sendbuf.size = size;
	}
	else {
		n->bulkmode = 0;
		n->sendbuf.size = NRPC_SENDBUF_SIZE;
	}

	if (!(n->sendbuf.buf = malloc(n->sendbuf.size))) {
		syslog(LOG_DEBUG, "malloc: %s", strerror(errno));
		return -1;
	}
	n->sendbuf.n = 0;
	return 0;
}
