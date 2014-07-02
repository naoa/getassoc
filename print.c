/*	$Id: print.c,v 1.22 2009/09/24 08:55:18 nis Exp $	*/

/*-
 * Copyright (c) 2005 Shingo Nishioka.
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
static char rcsid[] = "$Id: print.c,v 1.22 2009/09/24 08:55:18 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/time.h>

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include <expat.h>

#include "nwam.h"

#include "print.h"

#include <gifnoc.h>

#define PRINT_ID	1 /* */

void
print_xr_vec(v, max, w, dir, stream)
struct xr_vec const *v;
df_t max;
WAM *w;
FILE *stream;
{
	df_t i;
	if (!v) {
		fprintf(stream, "(null)\n");
		return;
	}
	fprintf(stream, "%"PRIdDF_T", %"PRIdFREQ_T, v->elem_num, v->freq_sum);

	for (i = 0; i < v->elem_num && (max == 0 || i < max); i++) {
		struct xr_elem const *e = &v->elems[i];
		fprintf(stream, ", ["
#if defined PRINT_ID
				"%"PRIuIDX_T", "
#endif
				"\"%s\", %"PRIdFREQ_T"]",
#if defined PRINT_ID
			e->id,
#endif
			w ? wam_id2name(w, dir, e->id) : "(nil)",
			e->freq);
	}
	if (i < v->elem_num) {
		fprintf(stream, ", ...");
	}

	fprintf(stream, "\n");
}

void
print_syminfo(d, nd, total, max, w, dir, stream)
struct syminfo const *d;
df_t nd, max;
df_t total;
WAM *w;
FILE *stream;
{
	df_t i;
	if (!d) {
		fprintf(stream, "(null)\n");
		return;
	}
	fprintf(stream, "%"PRIdDF_T", %"PRIdDF_T, nd, total);

	for (i = 0; i < nd && (max == 0 || i < max); i++) {
		struct syminfo const *r = &d[i];
		char const *name;
		name = w ? wam_id2name(w, dir, r->id) : NULL;
		fprintf(stream, ", ["
#if defined PRINT_ID
				"%"PRIuIDX_T", "
#endif
				"\"%s\", %"PRIdFREQ_T", %"PRIdFREQ_T", %"PRIdDF_T", %"PRIdDF_T", %f, %p]",
#if defined PRINT_ID
			r->id,
#endif
			name ? name : "(nil)",
			r->TF, r->TF_d,
			r->DF, r->DF_d,
			r->weight, r->v);
	}
	if (i < nd) {
		fprintf(stream, ", ...");
	}

	fprintf(stream, "\n");
}

void
print_s_syminfo(d, nd, total, max, w, dir, stream)
struct s_syminfo const *d;
df_t nd, max;
df_t total;
WAM *w;
FILE *stream;
{
	df_t i;
	if (!d) {
		fprintf(stream, "(null)\n");
		return;
	}
	fprintf(stream, "%"PRIdDF_T", %"PRIdDF_T, nd, total);

	for (i = 0; i < nd && (max == 0 || i < max); i++) {
		struct s_syminfo const *r = &d[i];
		char const *name;
		name = w ? wam_id2name(w, dir, r->id) : NULL,
		fprintf(stream, ", ["
#if defined PRINT_ID
				"%"PRIuIDX_T", "
#endif
				"\"%s\", %"PRIdFREQ_T", %"PRIdFREQ_T", %"PRIdDF_T", %"PRIdDF_T", %f, %p]",
#if defined PRINT_ID
			r->id,
#endif
			name ? name : "(nil)",
			r->TF, r->TF_d,
			r->DF, r->DF_d,
			r->weight, r->v);
	}
	if (i < nd) {
		fprintf(stream, ", ...");
	}

	fprintf(stream, "\n");
}

size_t
snprint_xr_vec(v, max, w, dir, s, size)
struct xr_vec const *v;
df_t max;
WAM *w;
char *s;
size_t size;
{
	df_t i;
	size_t t;
	if (!v) {
		return snprintf(s, size, "(null)");
	}
	t = snprintf(s, size, "%"PRIdDF_T", %"PRIdFREQ_T, v->elem_num, v->freq_sum);

	for (i = 0; i < v->elem_num && (max == 0 || i < max); i++) {
		struct xr_elem const *e = &v->elems[i];
		char const *name;
		name = w ? wam_id2name(w, dir, e->id) : NULL,
		t += snprintf(s + t, size - t, ", ["
#if defined PRINT_ID
				"%"PRIuIDX_T", "
#endif
				"\"%s\", %"PRIdFREQ_T"]",
#if defined PRINT_ID
			e->id,
#endif
			name ? name : "(nil)",
			e->freq);
	}
	if (i < v->elem_num) {
		t += snprintf(s + t, size - t, ", ...");
	}
	return t;
}

size_t
snprint_syminfo(d, nd, total, max, w, dir, s, size)
struct syminfo const *d;
df_t nd, max;
df_t total;
WAM *w;
char *s;
size_t size;
{
	df_t i;
	size_t t;
	if (!d) {
		return snprintf(s, size, "(null)\n");
	}
	t = snprintf(s, size, "%"PRIdDF_T", %"PRIdDF_T, nd, total);

	for (i = 0; i < nd && (max == 0 || i < max); i++) {
		struct syminfo const *r = &d[i];
		char const *name;
		name = w ? wam_id2name(w, dir, r->id) : NULL,
		t += snprintf(s + t, size - t, ", ["
#if defined PRINT_ID
				"%"PRIuIDX_T", "
#endif
				"\"%s\", %"PRIdFREQ_T", %"PRIdFREQ_T", %"PRIdDF_T", %"PRIdDF_T", %f, %p]",
#if defined PRINT_ID
			r->id,
#endif
			name ? name : "(nil)",
			r->TF, r->TF_d,
			r->DF, r->DF_d,
			r->weight, r->v);
	}
	if (i < nd) {
		t += snprintf(s + t, size - t, ", ...");
	}

	return t;
}

size_t
snprint_s_syminfo(d, nd, total, max, w, dir, s, size)
struct s_syminfo const *d;
df_t nd, max;
df_t total;
WAM *w;
char *s;
size_t size;
{
	df_t i;
	size_t t;
	if (!d) {
		return snprintf(s, size, "(null)\n");
	}
	t = snprintf(s, size, "%"PRIdDF_T", %"PRIdDF_T, nd, total);

	for (i = 0; i < nd && (max == 0 || i < max); i++) {
		struct s_syminfo const *r = &d[i];
		char const *name;
		name = w ? wam_id2name(w, dir, r->id) : NULL,
		t += snprintf(s + t, size - t, ", ["
#if defined PRINT_ID
				"%"PRIuIDX_T", "
#endif
				"\"%s\", %"PRIdFREQ_T", %"PRIdFREQ_T", %"PRIdDF_T", %"PRIdDF_T", %f, %p]",
#if defined PRINT_ID
			r->id,
#endif
			name ? name : "(nil)",
			r->TF, r->TF_d,
			r->DF, r->DF_d,
			r->weight, r->v);
	}
	if (i < nd) {
		t += snprintf(s + t, size - t, ", ...");
	}

	return t;
}
