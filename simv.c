/*	$Id: simv.c,v 1.5 2010/12/08 01:44:08 nis Exp $	*/

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
static char rcsid[] = "$Id: simv.c,v 1.5 2010/12/08 01:44:08 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/time.h>

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>

#include "nwam.h"
#include "util.h"
#include "pi.h"

#include "print.h"

#include "assvP.h"
#include "fssP.h"
#include "nwamP.h"
#include "xgserv.h"

#include <gifnoc.h>

double
simv(q, nq, w, qdir, type, d, nd)
struct syminfo *q, *d;
WAM *w;
df_t nq, nd;
{
	df_t i, j;
	struct assv_g g0, *g;
	freq_t freq_sum;
	double wsum;
	struct ovec_t dd;
	df_t DF_d;
	freq_t TF_d;

	df_t nh, k;
	struct hvec_t *h = NULL;
	struct pq_container *c = NULL;
	struct xr_elem *e = NULL;

	bzero(&g0, sizeof g0);
	g = &g0;

	g->nd = nd;

	g->w = w;
	g->qdir = qdir;
	g->rdir = WAM_REVERT_DIRECTION(qdir);
	g->q = q;
	g->nq = nq;
	g->Nq = wam_size(g->w, g->qdir);
	g->Nr = wam_size(g->w, g->rdir);

	g->ntests = 0;

	for (i = 0; i < nq; i++) {
		if (!q[i].v && q[i].id != 0) {
			q[i].DF = wam_elem_num(w, qdir, q[i].id);
			q[i].TF = wam_freq_sum(w, qdir, q[i].id);
		}
		else {
			syslog(LOG_DEBUG, "simv: assertion failed: v and id should be mutually exclusive");
			return 0;
		}
	}

	freq_sum = 0;
	for (i = 0; i < g->nq; i++) {
		freq_sum += q[i].TF_d;
	}
	g->tfq = freq_sum;

	if (simdef_set_type(g, type) == -1) {
		syslog(LOG_DEBUG, "unsupported type: %d", type);
		return 0;
	}

	nh = MAX(nq, nd);
	k = 0;
	if (g->f.weight) {
		if (!(c = calloc(nh, sizeof *c))) {
			syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
			return 0;
		}
		if (!(e = calloc(nh, sizeof *e))) {
			syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
			return 0;
		}
		if (!(h = calloc(nh, sizeof *h))) {
			syslog(LOG_DEBUG, "calloc: %s", strerror(errno));
			return 0;
		}
	}

	wsum = 0;
	DF_d = 0;
	TF_d = 0;
	for (i = 0, j = 0; i < nq && j < nd; ) {
		/*struct pq_container *t = h[i].t;*/
		if (q[i].id < d[j].id) {
			i++;
		}
		else if (q[i].id > d[j].id) {
			j++;
		}
		else {
			double wq, wd;
			if (g->f.wq) {
				wq = (*g->f.wq)(&q[i], g);
			}
			else {
				wq = 1;
			}
			if (g->f.wd) {
				struct xr_elem e;
				e.id = d[j].id;
				e.freq = d[j].TF_d;
				wd = (*g->f.wd)(&e, &q[i], g);
			}
			else {
				wd = 1;
			}

			if (g->f.weight) {
				h[k].t = &c[k];
				c[k].qidx = i;
				h[k].dp = &e[k];
				e[k].id = d[j].id;
				e[k].freq = d[j].TF_d;
				k++;
			}

			wsum += wq * wd;
			TF_d += d[j].TF_d;
			DF_d++;
			i++, j++;
		}
	}
	nh = k;

	dd.id = 0;	/* XXX: dd.id is unavailable */
	dd.weight = 0;
	dd.TF_d = TF_d;
	dd.DF_d = DF_d;	/* != nh */
	if (wsum) {
		double norm;
		if (g->f.norm && (norm = (*g->f.norm)(dd.id, &dd, g)) != 0) {
			wsum /= norm;
		}
	}
	if (g->f.weight) {
/*
	h->t
	h->t->qidx
	h->t->ep	UNDEF.
	h->t->ep_end	UNDEF.
	h->t->epid	UNDEF.
	h->t->wq	UNDEF.

	h->dp
	h->dp->id
	h->dp->freq
*/
		wsum = (*g->f.weight)(wsum, dd.id, &dd, h, nh, g);
		free(h);
		free(e);
		free(c);
	}
	dd.weight = wsum;

	if (g->f.cleanup) {
		(*g->f.cleanup)(g);
	}

	return dd.weight;
}
