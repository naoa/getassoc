/*	$Id: cxml.h,v 1.4 2009/08/27 23:36:59 nis Exp $	*/

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

#define CElem			1
#define CString			2
#define CComment		3
#define CPi			4

struct attribute {
	char *name, *value;
};

struct element {
	int type;	/* CElem */
	char *name;
	struct attribute *attributes;
	union content **contents;
};

struct chardata {
	int type;	/* CString */
	size_t len;
	char *value;
};

struct misc {
	int type;	/* CComment | CPi */
	char *pitarget;	/* CPi */
	size_t len;
	char *string;
};

union content {
	struct element element;
	struct chardata chardata;
/*
	struct reference reference;
*/
	struct misc misc;
};

struct prolog {
	void *xmldecl;
	void *doctypedecl;
};

struct document {
	struct prolog *prolog;
	struct element *element;
};

ssize_t unparse(struct element *, FILE *);
ssize_t unparse_fn(struct element *, void *, int (*)(void *, char *, int));
union content *alloc_content(int);
/*char *fwstrdup(char const *);*/
int add_attribute(struct element *, char *, char *);
#define	alloc_element()		((struct element *)alloc_content(CElem))
#define	alloc_chardata()	((struct chardata *)alloc_content(CString))
#define	alloc_comment()		((struct misc *)alloc_content(CComment))
#define	alloc_pi()		((struct misc *)alloc_content(CPi))

int add_content(/* restrict */ struct element *, union content *);

union content *string2chardata(char const *);

struct document *xml2cxmlNSfn(void *, int (*)(void *, char *, int), char const *, size_t, int);
struct document *xml2cxmlNS(FILE *, char const *, size_t, int);
struct document *xml2cxml(FILE *, char const *, size_t);

void free_document(struct document *);
void free_element(struct element *);
