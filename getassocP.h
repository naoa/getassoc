/*	$Id: getassocP.h,v 1.38 2010/12/15 06:03:33 nis Exp $	*/

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

struct document;
struct element;
union content;
struct fss_query;

#define GSS3_PARSER_XNS	0
#define GSS3_PARSER_NS	1
struct gss3_parser_t;
struct gss3_parser_t *gss3_create_parser(int);
void gss3_parser_setxns(struct gss3_parser_t *);
void gss3_destroy_parser(struct gss3_parser_t *);

struct gss3_parser_t;

#if defined ENABLE_NEWLAYOUT
int gss3_init(char const *, char const *);
#endif
int gss3(int, char const *, size_t, int);
int gss3fl(FILE *, char const *, size_t, int, int);
#if defined ENABLE_PROXY
struct document *gss3xtargets(void *, int (*)(void *, char *, int), char const *, size_t, char ***, size_t *);
#endif
#define	GSS3FN_HDR	1
int gss3fn(void *, int (*)(void *, char *, int), char const *, size_t, int, int);
/*
int sct_post(int, char const *, int, char *[], int);
*/
int sct(char const *, int, char *[], int);
int catalogue_html(char const *, int, char *[], int);
int index_html(char const *, int, char *[], int);
int standalone(int, char *[]);
int parse_search(struct element *, struct fss_query *, struct gss3_parser_t *);
int search_gif(char const *, int, char *[], int);
int geta_gif(char const *, int, char *[], int);
int sct_js(char const *, int, char *[], int);
int sct_help(char const *, int, char *[], int);
#if defined ENABLE_REN
int rn_html(char const *, int, char *[], int);
int rn_css(char const *, int, char *[], int);
int rn_js(char const *, int, char *[], int);
int btn_clear_gif(char const *, int, char *[], int);
int btn_top_imagine_gif(char const *, int, char *[], int);
int top_logo_gif(char const *, int, char *[], int);
int private_db_js(char const *, int, char *[], int);
#endif
struct cs_elem *csb1(struct syminfo *, df_t, WAM *, int, df_t *, int, int, int);
