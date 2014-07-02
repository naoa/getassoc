/*	$Id: simdef.h,v 1.10 2009/08/17 00:30:20 nis Exp $	*/

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

#if ! defined _GETASSOC_SIMDEF_H
#define _GETASSOC_SIMDEF_H

#define	WT_NONE	0
#define	WT_HITS	1
#define	WT_HYPGSUMD	2
#define	WT_HYPGSUMT	3
#define	WT_PEAK3	4
#define	WT_SMART	5
#define	WT_SMARTAW	6
#define	WT_SMARTWA	7
#define	WT_COSINE	8
#define WT_DOT_PRODUCT	9
#define	WT_COSINE_TFIDF	10
#define WT_DOT_PRODUCT_TFIDF	11
#define	WT_OKAPI_BM25	12

#define WEIGHT_TYPE_NAMES { \
	"WT_NONE", \
	"WT_HITS", \
	"WT_HYPGSUMD", \
	"WT_HYPGSUMT", \
	"WT_PEAK3", \
	"WT_SMART", \
	"WT_SMARTAW", \
	"WT_SMARTWA", \
	"WT_COSINE", \
	"WT_DOT_PRODUCT", \
	"WT_COSINE_TFIDF", \
	"WT_DOT_PRODUCT_TFIDF", \
	"WT_Okapi_BM25", \
	}

#endif /* _GETASSOC_SIMDEF_H */
