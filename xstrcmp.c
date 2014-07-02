/*	$Id: xstrcmp.c,v 1.38 2011/09/14 02:36:16 nis Exp $	*/

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
static char rcsid[] = "$Id: xstrcmp.c,v 1.38 2011/09/14 02:36:16 nis Exp $";
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

#include <unicode/utf.h>
#include <unicode/utf8.h>

#include "nwam.h"
#include "util.h"

#include "xstrcmpP.h"

#include <gifnoc.h>

static size_t prefetch(unsigned int *, char const *, int, size_t, xsc_opt_t);
static unsigned int xstrcmp_sngl_nrm(unsigned int, xsc_opt_t);
static int uc_ignore_p(unsigned int, xsc_opt_t);
static unsigned int uc_normalize(unsigned int, unsigned int, xsc_opt_t);
static int uc_ignore_p_post(unsigned int, unsigned int, xsc_opt_t);
static unsigned int uc_normalize_post(unsigned int, unsigned int, xsc_opt_t);

#if defined CACHE_PREFETCH
static unsigned int astream_getc_noskip_fast(struct astream *, ssize_t *, int *);
static size_t prefetch_fast(unsigned int *, char const *, int, size_t, xsc_opt_t);
#endif

#if defined CACHE_PREFETCH
#if ! defined FLWD_UTF32
configuration error!!!
#endif
#endif

#if defined FLWD_UTF32
#define	NEXT_CHAR(s, ptr, len, u) ((ptr) < (len) ? NEXT_CHAR_UNSAFE(s, ptr, u) : ((u) = 0))
#define	NEXT_CHAR_UNSAFE(s, ptr, u) ((u) = *(UChar32 *)((s) + (ptr)) & 0x1fffff, (ptr) += 4)
#define PREV_CHAR_UNSAFE(s, ptr, u) ((ptr) -= 4, (u) = *(UChar32 *)((s) + (ptr)) & 0x1fffff)
#else
#define	NEXT_CHAR(s, ptr, len, u) U8_NEXT(s, ptr, len, u)
#define	NEXT_CHAR_UNSAFE(s, ptr, u) U8_NEXT_UNSAFE(s, ptr, u)
#define PREV_CHAR_UNSAFE(s, ptr, u) U8_PREV_UNSAFE(s, ptr, u)
#endif

#if ! defined ENABLE_RIGIDSTRCMP

int
xstrncmp(a, b, alen, o)		/* alen => physical length (byte) */
char const *a;
char const *b;
size_t alen;
xsc_opt_t o;
{
	struct astream as0, bs0, *as = &as0, *bs = &bs0;
	unsigned int au, bu;
	ssize_t ap, bp;
	int e = 0;

	astream_init_r(a, alen, o, as, -1);
	astream_init_r(b, -1, o, bs, -1);

	if (o & MATCH_HEAD_MASK) {
#if defined FLWD_UTF32
		UChar32 u;
		u = *((UChar32 *)b - 1) & 0x1fffff;
		if (u) {
			return 1;
		}
#else
		if (b[-1]) {
			return 1;
		}
#endif
	}

	do {
		au = astream_getc(as, &ap);
		bu = astream_getc(bs, &bp);
	} while (ap < alen && au && au == bu);
	if (ap >= alen) {
		e = 0;
	}
	else if (au < bu) {
		e = -1;
	}
	else if (au > bu) {
		e = 1;
	}

	if (o & MATCH_TAIL_MASK) {
		if (bu) {
			return -1;
		}
	}

	return e;
}

unsigned int
x1stchar_r(b, o, bs)
char const *b;
xsc_opt_t o;
struct astream *bs;
{
	unsigned int bu;

	astream_init_r(b, -1, o, bs, -1);
	bu = astream_getc(bs, NULL);
	return bu;
}

void
astream_init_r(str, len, opt, s, u1ptr)
char const *str;
size_t len;
ssize_t u1ptr;
xsc_opt_t opt;
struct astream *s;
{
	UChar32 u;

	s->s = str;
	s->len = len;
	s->opt = opt;

	s->p[1] = u1ptr;
	if (s->p[1] == -1) {
		s->p[2] = 0;
		s->u[1] = 0;
	}
	else {
		s->p[2] = prefetch(&s->u[1], s->s, s->p[1], s->len, s->opt);
	}
	s->p[3] = prefetch(&s->u[2], s->s, s->p[2], s->len, s->opt);

	if (s->p[1] > 0) {
		s->p[0] = s->p[1]; PREV_CHAR_UNSAFE(s->s, s->p[0], u);
		prefetch(&s->u[0], s->s, s->p[0], s->len, s->opt);
		s->u[0] = uc_normalize(s->u[0], s->u[1], s->opt);
	}
	else {
		s->p[0] = -1;
		s->u[0] = 0;
	}

	if (s->u[1]) {
		s->u[1] = uc_normalize(s->u[1], s->u[2], s->opt);
	}
}

unsigned int
astream_getc(s, u1ptr_p)
struct astream *s;
ssize_t *u1ptr_p;
{
	int ignore;
	do {
		astream_getc_noskip(s, u1ptr_p, &ignore);
	} while (ignore || uc_ignore_p_post(s->u[0], s->u[1], s->opt));
	return uc_normalize_post(s->u[0], s->u[1], s->opt);
}

unsigned int
astream_getc_noskip(s, u1ptr_p, ignorep)
struct astream *s;
ssize_t *u1ptr_p;
int *ignorep;
{
	s->u[0] = s->u[1];
	s->u[1] = s->u[2];
	s->p[0] = s->p[1];
	s->p[1] = s->p[2];
	if (u1ptr_p) {
		*u1ptr_p = s->p[1];
	}
	s->p[2] = s->p[3];
	s->p[3] = prefetch(&s->u[2], s->s, s->p[2], s->len, s->opt);

	if (s->u[1] && uc_ignore_p(s->u[1], s->opt)) {
		*ignorep = 1;
		return U_IGNORE_CHAR;	/* return a junk value */
	}
	*ignorep = 0;

	return s->u[1] = uc_normalize(s->u[1], s->u[2], s->opt);
}

static size_t
prefetch(up, s, ptr, len, o)
unsigned int *up;
char const *s;
size_t len;
xsc_opt_t o;
{
	UChar32 u;
	int ptr0 = ptr;

	*up = 0;
	if (len == -1) {
		NEXT_CHAR_UNSAFE(s, ptr, u);
	}
	else if (ptr >= len) {
		u = 0;
	}
	else {
		NEXT_CHAR(s, ptr, len, u);
	}
	if (u <= 0) {
		return ptr0;
	}
	if (o & (COLLAPSE_HIRAKATA_MASK|COLLAPSE_QY_MASK)) {
		u = xstrcmp_sngl_nrm(u, o);
	}
	*up = u;
	return ptr;
}

#if defined CACHE_PREFETCH
void
astream_init_r_fast(str, len, opt, s, u1ptr)
char const *str;
size_t len;
ssize_t u1ptr;
xsc_opt_t opt;
struct astream *s;
{
	UChar32 u;

	s->s = str;
	s->len = len;
	s->opt = opt;

	s->p[1] = u1ptr;
	if (s->p[1] == -1) {
		s->p[2] = 0;
		s->u[1] = 0;
	}
	else {
		s->p[2] = prefetch_fast(&s->u[1], s->s, s->p[1], s->len, s->opt);
	}
	s->p[3] = prefetch_fast(&s->u[2], s->s, s->p[2], s->len, s->opt);

	if (s->p[1] > 0) {
		s->p[0] = s->p[1]; PREV_CHAR_UNSAFE(s->s, s->p[0], u);
		prefetch_fast(&s->u[0], s->s, s->p[0], s->len, s->opt);
	}
	else {
		s->p[0] = -1;
		s->u[0] = 0;
	}
}

unsigned int
astream_getc_fast(s, u1ptr_p)
struct astream *s;
ssize_t *u1ptr_p;
{
	int ignore;
	do {
		astream_getc_noskip_fast(s, u1ptr_p, &ignore);
	} while (ignore || uc_ignore_p_post(s->u[0], s->u[1], s->opt));
	return uc_normalize_post(s->u[0], s->u[1], s->opt);
}

static unsigned int
astream_getc_noskip_fast(s, u1ptr_p, ignorep)
struct astream *s;
ssize_t *u1ptr_p;
int *ignorep;
{
	s->u[0] = s->u[1];
	s->u[1] = s->u[2];
	s->p[0] = s->p[1];
	s->p[1] = s->p[2];
	if (u1ptr_p) {
		*u1ptr_p = s->p[1];
	}
	s->p[2] = s->p[3];
	s->p[3] = prefetch_fast(&s->u[2], s->s, s->p[2], s->len, s->opt);

	if (s->u[1] == U_IGNORE_CHAR) {
		*ignorep = 1;
		return U_IGNORE_CHAR;	/* return a junk value */
	}
	*ignorep = 0;

	return s->u[1];
}

static size_t
prefetch_fast(up, s, ptr, len, o)
unsigned int *up;
char const *s;
size_t len;
xsc_opt_t o;
{
	UChar32 u;
	int ptr0 = ptr;

	*up = 0;
	if (len == -1) {
		NEXT_CHAR_UNSAFE(s, ptr, u);
	}
	else if (ptr >= len) {
		u = 0;
	}
	else {
		NEXT_CHAR(s, ptr, len, u);
	}
	if (u <= 0) {
		return ptr0;
	}
	*up = u;
	return ptr;
}
#endif

static unsigned int
xstrcmp_sngl_nrm(c, o)
unsigned int c;
xsc_opt_t o;
{
	if (o & COLLAPSE_HIRAKATA_MASK && (
		0x3041 <= c && c <= 0x3094)) {	/* あ -- う゛ */
		c = c - 0x3041 + 0x30a1;	/* -あ +ア */
		/* FALLTHRU */
	}
	if (o & COLLAPSE_QY_MASK) {
		switch (c) {
		case 0x30c3: c = 0x30c4; break; /* ッ ツ */
		case 0x30e3: c = 0x30e4; break; /* ャ ヤ */
		case 0x30e5: c = 0x30e6; break; /* ュ ユ */
		case 0x30e7: c = 0x30e8; break; /* ョ ヨ */
		case 0x3063: c = 0x3064; break; /* っ つ */
		case 0x3083: c = 0x3084; break; /* ゃ や */
		case 0x3085: c = 0x3086; break; /* ゅ ゆ */
		case 0x3087: c = 0x3088; break; /* ょ よ */
		}
		/* FALLTHRU */
	}
	return c;
}

static int
uc_ignore_p(c, o)
unsigned int c;
xsc_opt_t o;
{
	if (o & IGNORE_SPACE_MASK && (
		c == '\t' || c == '\n' || c == '\r' || c == ' ' ||
		c == 0x00a0 ||	/* no-break space */
		0x2000 <= c && c <= 0x200a ||
			/* 0x2000 en quad */
			/* 0x2001 em quad */
			/* 0x2002 en space */
			/* 0x2003 em space */
			/* 0x2004 three-per-em space */
			/* 0x2005 four-per-em space */
			/* 0x2006 six-per-em space */
			/* 0x2007 figure space */
			/* 0x2008 punctuation space */
			/* 0x2009 thin space */
			/* 0x200a hair space */
		c == 0x202f ||	/* narrow no-break space */
		c == 0x205f ||	/* medium mathematical space */
		c == 0x3000)) {	/* ideographic space */
		return 1;
	}
	if (o & IGNORE_PUNCT_MASK && (
		0x0021 <= c && c <= 0x002f ||	/* !"#$%&'()*+,-./ */
		0x003a <= c && c <= 0x0040 ||	/* :;<=>?@ */
		0x005b <= c && c <= 0x0060 ||	/* [\]^_` */
		0x007b <= c && c <= 0x007e ||	/* {|}~ */
		0x00a1 <= c && c <= 0x00bf ||
			/* 00a1-00bf Latin-1 punctuation and symbols */
		0x2016 <= c && c <= 0x2027 ||
			/* 2016-2027 General punctuation */
		0x2030 <= c && c <= 0x2057 ||
			/* 2030-2046 General punctuation */
			/* 2047-2049 Double punctuation for vertical text */
			/* 204a-2057 General punctuation */
		0x3001 <= c && c <= 0x3011 ||
			/* 3000-3007 CJK symbols and punctuation */
			/* 3008-300b CJK angle brackets */
			/* 300c-300f CJK corner brackets */
			/* 3010 3011 CJK brackets */
		0x3014 <= c && c <= 0x3020)) {
			/* 3014-301b CJK brackets */
			/* 301c-3020 CJK symbols and punctuation */
		return 1;
	}
	return 0;
}

static unsigned int
uc_normalize(c, n, o)
unsigned int c, n;
xsc_opt_t o;
{
#if defined HAVE_ESHK_H
	if (o & COLLAPSE_ESHK_MASK) {
		unsigned int u;
		static unsigned int const eshk[] = {
#include "eshk.h"
		};
		if (c < nelems(eshk) && (u = eshk[c])) {
			return u;
		}
	}
#endif
	if (o & IGNORE_CASE_MASK) {
		if ('a' <= c && c <= 'z') {
			return toupper(c);
		}
		if (0xff41 <= c && c <= 0xff5a) {
			return c - 0xff41 + 0xff21;
		} /* ZENKAKU-HANKAKU form -- ff21-ff3a .. ff41-ff5a */
	}
	if (o & COLLAPSE_ZDZ_MASK) {
		if (c == 0x30c2) {	/* ヂ */
			return 0x30b8;	/* ジ */
		}
		if (c == 0x30c5) {	/* ヅ */
			return 0x30ba;	/* ズ */
		}
	}
	if (o & COLLAPSE_BV_MASK) {
		if (c == 0x30f4 && n == 0x30a1) {	/* ヴァ */
			return 0x30d0;	/* バ */
		}
		if (c == 0x30f4 && n == 0x30a3) {	/* ヴィ */
			return 0x30d3;	/* ビ */
		}
		if (c == 0x30f4 && n == 0x30a7) {	/* ヴェ */
			return 0x30d9;	/* ベ */
		}
		if (c == 0x30f4 && n == 0x30a9) {	/* ヴォ */
			return 0x30dc;	/* ボ */
		}
	}
	if (o & COLLAPSE_TTS_MASK) {
		if (c == 0x30c4 && n == 0x30a3) {	/* ツィ */
			return 0x30c1;	/* チ */
		}
		if (c == 0x30c6 && n == 0x30a3) {	/* ティ */
			return 0x30c1;	/* チ */
		}
		if (c == 0x30c7 && n == 0x30a3) {	/* ディ */
			return 0x30b8;	/* ジ */
		}
	}
	if (o & COLLAPSE_SSH_MASK) {
		if (c == 0x30b7 && n == 0x30a7) {	/* シェ */
			return 0x30bb;	/* セ */
		}
		if (c == 0x30b8 && n == 0x30a7) {	/* ジェ */
			return 0x30bc;	/* ゼ */
		}
	}
	if (o & COLLAPSE_HF_MASK) {
		if (c == 0x30d5 && n == 0x30e5) {	/* フュ */
			return 0x30d2;	/* ヒ */
		}
		if (c == 0x30f4 && n == 0x30e5) {	/* ヴュ */
			return 0x30d3;	/* ビ */
		}
	}
	if (o & (COLLAPSE_HF_MASK|COLLAPSE_QY_MASK)) {
		if (c == 0x30d5 && n == 0x30e6) {	/* フユ */
			return 0x30d2;	/* ヒ */
		}
		if (c == 0x30f4 && n == 0x30e6) {	/* ヴユ */
			return 0x30d3;	/* ビ */
		}
	}
	if (o & COLLAPSE_KX_MASK) {
		if (c == 0x30af && n == 0x30b5) {	/* クサ */
			return 0x30ad;	/* キ */
		}
		if (c == 0x30af && n == 0x30b7) {	/* クシ */
			return 0x30ad;	/* キ */
		}
		if (c == 0x30af && n == 0x30b9) {	/* クス */
			return 0x30ad;	/* キ */
		}
		if (c == 0x30af && n == 0x30bb) {	/* クセ */
			return 0x30ad;	/* キ */
		}
		if (c == 0x30af && n == 0x30bd) {	/* クソ */
			return 0x30ad;	/* キ */
		}
	}
	if (o & COLLAPSE_DASH_MASK && (
		c == 0x00ad ||	/* 0x00ad soft hyphen */
		c == 0x2010 ||	/* 0x2010 hyphen */
		c == 0x2011 ||	/* 0x2011 non-breaking hyphen */
		c == 0x2012 ||	/* 0x2012 figure dash */
		c == 0x2013 ||	/* 0x2013 en-dash */
		c == 0x2014 ||	/* 0x2014 em-dash */
		c == 0x2015 ||	/* 0x2015 horizontal bar */
		c == 0x2212 ||	/* 0x2212 minus sign */
		c == 0x30fc ||	/* 0x30fc katakana-hiragana prolonged */
		c == 0xff0d)) {	/* 0xff0d fullwidth hyphen-minus */
		return 0x002d; /* 0x002d hyphen-minus */
	}
	if (o & COLLAPSE_KA_MASK && (
		c == 0x4e2a || /* 个 */
		c == 0x30f6 || /* ヶ */
		c == 0x30f5)) { /* ヵ */
		return 0x7b87; /* 箇 */
	}
	return c;
}

/* NOTE: p is sng_normalized && normalized, but not noramlize_post'ed */
static int
uc_ignore_p_post(p, c, o)
unsigned int p, c;
xsc_opt_t o;
{
	if (o & COLLAPSE_BV_MASK && (
		 p == 0x30f4 && c == 0x30a1 ||	/* ヴァ */
		 p == 0x30f4 && c == 0x30a3 ||	/* ヴィ */
		 p == 0x30f4 && c == 0x30a7 ||	/* ヴェ */
		 p == 0x30f4 && c == 0x30a9)) {	/* ヴォ */
		return 1;
	}
	if (o & COLLAPSE_TTS_MASK && (
		 p == 0x30c4 && c == 0x30a3 ||	/* ツィ */
		 p == 0x30c6 && c == 0x30a3 ||	/* ティ */
		 p == 0x30c7 && c == 0x30a3)) {	/* ディ */
		return 1;
	}
	if (o & COLLAPSE_SSH_MASK && (
		 p == 0x30b7 && c == 0x30a7 ||	/* シェ */
		 p == 0x30b8 && c == 0x30a7)) {	/* ジェ */
		return 1;
	}
	return 0;
}

/* NOTE: p is sng_normalized && normalized, but not noramlize_post'ed */
static unsigned int
uc_normalize_post(p, c, o)
unsigned int p, c;
xsc_opt_t o;
{
	if (o & COLLAPSE_AY_MASK) {
		if (c == 0x30e4 && ( /* ヤ */
			p == 0x30a4 || /* イ */
			p == 0x30ad || /* キ */
			p == 0x30ae || /* ギ */
			p == 0x30b7 || /* シ */
			p == 0x30b8 || /* ジ */
			p == 0x30c1 || /* チ */
			p == 0x30c2 || /* ヂ */
			p == 0x30cb || /* ニ */
			p == 0x30d2 || /* ヒ */
			p == 0x30d3 || /* ビ */
			p == 0x30d4 || /* ピ */
			p == 0x30df || /* ミ */
			p == 0x30ea || /* リ */
			p == 0x30f0)) { /* ヰ */
			return 0x30a2;	/* ア */ 
		}
	}
	return c;
}

#else /* ! ENABLE_RIGIDSTRCMP */

int
xstrncmp(a, b, alen, o)		/* alen => physical length (byte) */
char const *a;
char const *b;
size_t alen;
xsc_opt_t o;
{
	unsigned int au, bu;
	ssize_t ap, bp;
	int e = 0;

	ap = 0;
	bp = 0;
	do {
		ap = prefetch(&au, a, ap, alen, 0);
		bp = prefetch(&bu, b, bp, -1, 0);

	} while (ap < alen && au && au == bu);
	if (ap >= alen) {
		e = 0;
	}
	else if (au < bu) {
		e = -1;
	}
	else if (au > bu) {
		e = 1;
	}

	return e;
}

unsigned int
x1stchar_r(b, o, bs)
char const *b;
xsc_opt_t o;
struct astream *bs;
{
	unsigned int bu;

	astream_init_r(b, -1, o, bs, -1);
	bu = astream_getc(bs, NULL);
	return bu;
}

void
astream_init_r(str, len, opt, s, u1ptr)
char const *str;
size_t len;
ssize_t u1ptr;
xsc_opt_t opt;
struct astream *s;
{
	UChar32 u;

	s->s = str;
	s->len = len;
	s->opt = opt;

	s->p[1] = u1ptr;
	if (s->p[1] == -1) {
		s->p[2] = 0;
		s->u[1] = 0;
	}
	else {
		s->p[2] = prefetch(&s->u[1], s->s, s->p[1], s->len, s->opt);
	}
	s->p[3] = prefetch(&s->u[2], s->s, s->p[2], s->len, s->opt);

	if (s->p[1] > 0) {
		s->p[0] = s->p[1]; PREV_CHAR_UNSAFE(s->s, s->p[0], u);
		prefetch(&s->u[0], s->s, s->p[0], s->len, s->opt);
	}
	else {
		s->p[0] = -1;
		s->u[0] = 0;
	}
}

unsigned int
astream_getc(s, u1ptr_p)
struct astream *s;
ssize_t *u1ptr_p;
{
	astream_getc_noskip(s, u1ptr_p, NULL);
	return s->u[1];
}

unsigned int
astream_getc_noskip(s, u1ptr_p, ignorep)
struct astream *s;
ssize_t *u1ptr_p;
int *ignorep;
{
	s->u[0] = s->u[1];
	s->u[1] = s->u[2];
	s->p[0] = s->p[1];
	s->p[1] = s->p[2];
	if (u1ptr_p) {
		*u1ptr_p = s->p[1];
	}
	s->p[2] = s->p[3];
	s->p[3] = prefetch(&s->u[2], s->s, s->p[2], s->len, s->opt);

	return s->u[1];
}

static size_t
prefetch(up, s, ptr, len, o)
unsigned int *up;
char const *s;
size_t len;
xsc_opt_t o;
{
	UChar32 u;
	int ptr0 = ptr;

	*up = 0;
	if (len == -1) {
		NEXT_CHAR_UNSAFE(s, ptr, u);
	}
	else if (ptr >= len) {
		u = 0;
	}
	else {
		NEXT_CHAR(s, ptr, len, u);
	}
	if (u <= 0) {
		return ptr0;
	}
	*up = u;
	return ptr;
}

#if defined CACHE_PREFETCH
void
astream_init_r_fast(str, len, opt, s, u1ptr)
char const *str;
size_t len;
ssize_t u1ptr;
xsc_opt_t opt;
struct astream *s;
{
	UChar32 u;

	s->s = str;
	s->len = len;
	s->opt = opt;

	s->p[1] = u1ptr;
	if (s->p[1] == -1) {
		s->p[2] = 0;
		s->u[1] = 0;
	}
	else {
		s->p[2] = prefetch_fast(&s->u[1], s->s, s->p[1], s->len, s->opt);
	}
	s->p[3] = prefetch_fast(&s->u[2], s->s, s->p[2], s->len, s->opt);

	if (s->p[1] > 0) {
		s->p[0] = s->p[1]; PREV_CHAR_UNSAFE(s->s, s->p[0], u);
		prefetch_fast(&s->u[0], s->s, s->p[0], s->len, s->opt);
	}
	else {
		s->p[0] = -1;
		s->u[0] = 0;
	}
}

unsigned int
astream_getc_fast(s, u1ptr_p)
struct astream *s;
ssize_t *u1ptr_p;
{
	astream_getc_noskip_fast(s, u1ptr_p, NULL);
	return s->u[1];
}

static unsigned int
astream_getc_noskip_fast(s, u1ptr_p, ignorep)
struct astream *s;
ssize_t *u1ptr_p;
int *ignorep;
{
	s->u[0] = s->u[1];
	s->u[1] = s->u[2];
	s->p[0] = s->p[1];
	s->p[1] = s->p[2];
	if (u1ptr_p) {
		*u1ptr_p = s->p[1];
	}
	s->p[2] = s->p[3];
	s->p[3] = prefetch_fast(&s->u[2], s->s, s->p[2], s->len, s->opt);

	return s->u[1];
}

/* XXX: code clone: this code is exactly same as prefetch */
static size_t
prefetch_fast(up, s, ptr, len, o)
unsigned int *up;
char const *s;
size_t len;
xsc_opt_t o;
{
	UChar32 u;
	int ptr0 = ptr;

	*up = 0;
	if (len == -1) {
		NEXT_CHAR_UNSAFE(s, ptr, u);
	}
	else if (ptr >= len) {
		u = 0;
	}
	else {
		NEXT_CHAR(s, ptr, len, u);
	}
	if (u <= 0) {
		return ptr0;
	}
	*up = u;
	return ptr;
}
#endif
#endif /* ! ENABLE_RIGIDSTRCMP */
