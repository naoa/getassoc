/*	$Id: xstrcmpP.h,v 1.3 2010/01/16 00:31:36 nis Exp $	*/

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

#define	MATCH_HEAD_N		0
#define	MATCH_TAIL_N		1
#define	WORD_BOUNDARY_N		2

#define	IGNORE_CASE_N		3

#define	COLLAPSE_HIRAKATA_N	4
#define	COLLAPSE_QY_N		5	
#define	COLLAPSE_KA_N		6	

#define	COLLAPSE_DASH_N		7
#define	COLLAPSE_NOMA		8
#define	COLLAPSE_KANJI_VARIANT	9
#define	COLLAPSE_KANA_VARIANT	10

#define	COLLAPSE_PS_MARK	11

#define	COLLAPSE_ZDZ_N		12
#define	COLLAPSE_BV_N		13
#define	COLLAPSE_TTS_N		14
#define	COLLAPSE_SSH_N		15
#define	COLLAPSE_AY_N		16
#define	COLLAPSE_KX_N		17
#define	COLLAPSE_HF_N		18

#define	IGNORE_PUNCT_N		19
#define	IGNORE_SPACE_N		20

#define	COLLAPSE_ESHK_N		21

#define	XSC_OPT_NAMES { \
	"match-head", \
	"match-tail", \
	"word-boundary", \
	"ignore-case", \
	"collapse-hirakata", \
	"collapse-qy", \
	"collapse-ka", \
	"collapse-dash", \
	"collapse-noma", \
	"collapse-kanji-variant", \
	"collapse-kana-variant", \
	"collapse-ps-mark", \
	"collapse-zdz", \
	"collapse-bv", \
	"collapse-tts", \
	"collapse-ssh", \
	"collapse-ay", \
	"collapse-kx", \
	"collapse-hf", \
	"ignore-punct", \
	"ignore-space", \
	"collapse-eshk", \
	}

#define	MATCH_HEAD_MASK			(1<<MATCH_HEAD_N)
#define	MATCH_TAIL_MASK			(1<<MATCH_TAIL_N)

#define	IGNORE_CASE_MASK		(1<<IGNORE_CASE_N)

#define	COLLAPSE_HIRAKATA_MASK		(1<<COLLAPSE_HIRAKATA_N)

#define	COLLAPSE_QY_MASK		(1<<COLLAPSE_QY_N)
#define	COLLAPSE_KA_MASK		(1<<COLLAPSE_KA_N)
#define	COLLAPSE_DASH_MASK		(1<<COLLAPSE_DASH_N)
#define	COLLAPSE_NOMA_MASK		(1<<COLLAPSE_NOMA_N)
#define	COLLAPSE_KANJI_VARIANT_MASK	(1<<COLLAPSE_KANJI_VARIANT_N)
#define	COLLAPSE_KANA_VARIANT_MASK	(1<<COLLAPSE_KANA_VARIANT_N)
#define	COLLAPSE_PS_MARK_MASK		(1<<COLLAPSE_PS_MARK_N)

#define	COLLAPSE_ZDZ_MASK		(1<<COLLAPSE_ZDZ_N)
#define	COLLAPSE_BV_MASK		(1<<COLLAPSE_BV_N)
#define	COLLAPSE_TTS_MASK		(1<<COLLAPSE_TTS_N)
#define	COLLAPSE_SSH_MASK		(1<<COLLAPSE_SSH_N)
#define	COLLAPSE_AY_MASK		(1<<COLLAPSE_AY_N)
#define	COLLAPSE_KX_MASK		(1<<COLLAPSE_KX_N)
#define	COLLAPSE_HF_MASK		(1<<COLLAPSE_HF_N)

#define	IGNORE_PUNCT_MASK		(1<<IGNORE_PUNCT_N)
#define	IGNORE_SPACE_MASK		(1<<IGNORE_SPACE_N)

#define	COLLAPSE_ESHK_MASK		(1<<COLLAPSE_ESHK_N)

#define	WORD_BOUNDARY_MASK		(1<<WORD_BOUNDARY_N)

struct astream {
	char const *s;
	size_t len;
	xsc_opt_t opt;
	unsigned int u[3];
	ssize_t p[4];
};

unsigned int x1stchar_r(char const *, xsc_opt_t, struct astream *);
void astream_init_r(char const *, size_t, xsc_opt_t, struct astream *, ssize_t);
unsigned int astream_getc(struct astream *, ssize_t *);
unsigned int astream_getc_noskip(struct astream *, ssize_t *, int *);
#if defined CACHE_PREFETCH
void astream_init_r_fast(char const *, size_t, xsc_opt_t, struct astream *, ssize_t);
unsigned int astream_getc_fast(struct astream *, ssize_t *);
#endif
#define	U_IGNORE_CHAR	0xfffe	/* noncharacter */
