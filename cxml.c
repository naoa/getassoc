/*	$Id: cxml.c,v 1.12 2009/08/27 23:36:59 nis Exp $	*/

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
static char rcsid[] = "$Id: cxml.c,v 1.12 2009/08/27 23:36:59 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <unistd.h>

#include <expat.h>

#include "cxml.h"

#include <gifnoc.h>

#if ! defined MIN
#define	MIN(a, b)	((a) < (b) ? (a) : (b))
#endif

#if ! defined MAX
#define	MAX(a, b)	((a) < (b) ? (a) : (b))
#endif

struct ewcs {	/* element with content size */
	struct element *e;
	size_t size, ne;
};

struct udata {
	int status, line;
	struct document *d;
	struct ewcs e, stack[256];
	size_t sp;
};

#define	FAIL	0
#define	OK	1

#define	nelems(e)	(sizeof (e)/sizeof *(e))

static int push_e(struct udata *);
static int pop_e(struct udata *);
static int addcont(struct ewcs *, union content *);

static int pabuf(struct udata *, XML_Parser, char *, size_t, char const *);
static int pafile(XML_Parser, void *, int (*)(void *, char *, int), size_t, char const *);

static void start(void *, const XML_Char *, const XML_Char **);
static void end(void *, const XML_Char *);
static void cdatahndl(void *, const XML_Char *, int);

static void cmnthndl(void *, const XML_Char *);
static int extehndl(XML_Parser, const XML_Char *, const XML_Char *, const XML_Char *, const XML_Char *);
static void prcihndl(void *, const XML_Char *, const XML_Char *);
static void dflthndl(void *, const XML_Char *, int);
#if defined XXDEBUG
static void fputdbgs(FILE *, char const *, char const *, int, int);
#else
#define fputdbgs(stream, tag, s, len, nl)
#endif

static int readfn_fread(void *, char *, int);
static int writefn_fwrite(void *, char *, int);

static void *bdup(const void *, size_t);

static ssize_t lfputs(char const *, void *, int (*)(void *, char *, int));
static ssize_t yputs(char const *, int, void *, int (*)(void *, char *, int));
static ssize_t yputc(int, int, void *, int (*)(void *, char *, int));

static int
push_e(u)
struct udata *u;
{
	if (u->sp == 0) {
		return -1;	/* overflow */
	}
	u->stack[--u->sp] = u->e;
	return 0;
}

static int
pop_e(u)
struct udata *u;
{
	if (u->sp == nelems(u->stack)) {
		return -1;	/* underflow */
	}
	u->e = u->stack[u->sp++];
	return 0;
}

static int
addcont(ex, c)
struct ewcs *ex;
union content *c;
{
	if (ex->size <= ex->ne + 1) {
		void *new;
		size_t newsize = ex->ne + 1;
#if ! defined NA
#define	NA(e, x)	((e) + ((e) % (x) ? (x) - (e) % (x) : 0))
#endif
		newsize = NA(newsize + 1, 32);
		if (!(new = realloc(ex->e->contents, newsize * sizeof *ex->e->contents))) {
			return -1;
		}
		ex->size = newsize;
		ex->e->contents = new;
	}
	ex->e->contents[ex->ne++] = c;
	ex->e->contents[ex->ne] = NULL;
	return 0;
}

int
add_content(e, c)
/* restrict */ struct element *e;
union content *c;
{
	struct ewcs ex;

	ex.e = e;
	if (e->contents) {
		int i;
		for (i = 0; e->contents[i]; i++) ;
		ex.size = i + 1;
		ex.ne = i;
	}
	else {
		ex.size = ex.ne = 0;
	}
	return addcont(&ex, c);
}

struct document *
xml2cxml(f, path, clen)
FILE *f;
char const *path;
size_t clen;
{
	return xml2cxmlNS(f, path, clen, 0);
}

/*
 * usage:
 * f = fopen(path, "r");
 * c = xml2cxml(f, path, content_size, namespace_separate_char);
 */
struct document *
xml2cxmlNS(f, path, clen, nsx)
FILE *f;
char const *path;
size_t clen;
{
	return xml2cxmlNSfn(f, readfn_fread, path, clen, nsx);
}

struct document *
xml2cxmlNSfn(cookie, readfn, path, clen, nsx)
void *cookie;
int (*readfn)(void *, char *, int);
char const *path;
size_t clen;
{
	struct document *d;
	struct udata u0, *u = &u0;
	XML_Parser p = NULL;
	struct element e0, *e = &e0;

	if (!(d = u->d = calloc(1, sizeof *u->d))) {
		return NULL;
	}
	e->type = CElem;
	e->name = NULL;
	e->attributes = NULL;
	e->contents = NULL;
	u->e.e = e;
	u->e.size = u->e.ne = 0;
	u->sp = nelems(u->stack);

	if (nsx) {
		if (!(p = XML_ParserCreateNS("UTF-8", nsx))) {
			fprintf(stderr, "XML_ParserCreateNS failed\n");
			goto error;
		}
	}
	else {
		if (!(p = XML_ParserCreate("UTF-8"))) {
			fprintf(stderr, "XML_ParserCreate failed\n");
			goto error;
		}
	}

	XML_SetElementHandler(p, start, end);
	XML_SetCharacterDataHandler(p, cdatahndl);

	XML_SetCommentHandler(p, cmnthndl);
	XML_SetExternalEntityRefHandler(p, extehndl);
	XML_SetProcessingInstructionHandler(p, prcihndl);
	XML_SetDefaultHandler(p, dflthndl);

/*
XML_SetCdataSectionHandler
*XML_SetCharacterDataHandler
*XML_SetCommentHandler
+XML_SetDefaultHandler
XML_SetDefaultHandlerExpand
*XML_SetElementHandler
*XML_SetExternalEntityRefHandler
XML_SetNamespaceDeclHandler
XML_SetNotationDeclHandler
XML_SetNotStandaloneHandler
*XML_SetProcessingInstructionHandler
-XML_SetUnknownEncodingHandler
-XML_SetUnparsedEntityDeclHandler
*/

	XML_SetUserData(p, u);
	u->status = OK;
	u->line = 1;
	if (pafile(p, cookie, readfn, clen, path) == -1) {
		goto error;
	}

	XML_ParserFree(p);

	if (u->status == FAIL) {
		goto error;
	}

	if (u->e.e != e) {
		return NULL;
	}
	if (!e->contents || e->contents[0]->element.type != CElem) {
		return NULL;
	}
	if (!(d->prolog = calloc(1, sizeof *d->prolog))) {
		return NULL;
	}
	d->prolog->xmldecl = NULL;
	d->prolog->doctypedecl = NULL;
	d->element = &e->contents[0]->element;
	free(e->contents);
	return d;

error:
	if (p) XML_ParserFree(p);
	return NULL;
}

static int
pabuf(u, p, b, size, path)
struct udata *u;
XML_Parser p;
char *b;
size_t size;
char const *path;
{
	size_t i, j;

	for (i = 0; i < size; i = j) {
		void *buf;
		int nl = 0;
		for (j = i; j < size && b[j] != '\n'; j++) ;
		if (j < size) {
			nl = 1;
			j++;
		}
		if (!(buf = XML_GetBuffer(p, j - i))) {
			char const *msg = XML_ErrorString(XML_GetErrorCode(p));
			fprintf(stderr, "XML_GetBuffer failed: %s at line %d\n", msg, u->line);
			return -1;
		}
		memmove(buf, b + i, j - i);
		if (!XML_ParseBuffer(p, j - i, 0)) {
			char const *msg = XML_ErrorString(XML_GetErrorCode(p));
			fprintf(stderr, "XML_ParseBuffer failed:%s: %s at line %d\n", path, msg, u->line);
/*				stacktrace(XML_GetUserData(p), stderr); */
			return -1;
		}
		if (u->status == FAIL) {
			return -1;
		}
		if (nl) {
			u->line++;
		}
	}
	return 0;
}

static int
pafile(p, cookie, readfn, clen, path)
XML_Parser p;
void *cookie;
int (*readfn)(void *, char *, int);
size_t clen;
char const *path;
{
	ssize_t len;
	char b[8192];
	struct udata *u = XML_GetUserData(p);
	for (; clen; clen -= len) {
		size_t size = MIN(sizeof b, clen);
		if (!(len = (*readfn)(cookie, b, size))) {
			break;
		}
		if (pabuf(u, p, b, len, path) == -1) {
			return -1;
		}
	}
	XML_Parse(p, NULL, 0, 1);

	return 0;
}

static void
start(data, el, attr)
void *data;
const XML_Char *el;
const XML_Char **attr;
{
	size_t n2, i;
	struct udata *u = data;
#if 0
	char *q;
#endif
	struct element *e;
	struct attribute *a;
/*** ***/
	fputdbgs(stdout, "start", el, strlen(el), 1);
	if (u->status == FAIL) {
		return;
	}
	if (!(e = alloc_element())) {
		goto error;
	}
	if (addcont(&u->e, (union content *)e) == -1) {
		goto error;
	}
	if (!(e->name = strdup(el))) {
		goto error;
	}
#if 0
	if (q = index(el, 1)) *q = ':';
#endif
	for (n2 = 0; attr[n2]; n2 += 2) ;
	if (!(e->attributes = calloc(n2 / 2 + 1, sizeof *e->attributes))) {
		goto error;
	}
	for (i = 0, a = e->attributes; i < n2; i += 2, a++) {
		if (!(a->name = strdup(attr[i]))) {
			goto error;
		}
		if (!(a->value = strdup(attr[i + 1]))) {
			goto error;
		}
		fputdbgs(stdout, "attr/val", a->name, strlen(a->name), 0);
		fputdbgs(stdout, " ", a->value, strlen(a->value), 1);
	}
	a->name = NULL;
	a->value = NULL;
	e->contents = NULL;
	if (push_e(u) == -1) {
		goto error;
	}
	u->e.e = e;
	u->e.size = u->e.ne = 0;
	return;
error:
	u->status = FAIL;
	return;
}

static void
end(data, el)
void *data;
const XML_Char *el;
{
	struct udata *u = data;
/*** ***/
	struct element *e = u->e.e;
	fputdbgs(stdout, "end", el, strlen(el), 1);
	if (u->status == FAIL) {
		return;
	}
	if (strcmp(e->name, el)) {
		fprintf(stderr, "internal error");
		u->status = FAIL;
	}
	if (pop_e(u) == -1) {
		u->status = FAIL;
	}
}

static void
cdatahndl(data, s, len)
void *data;
const XML_Char *s;
int len;
{
	struct udata *u = data;
	struct chardata *d;
/*** ***/
	if (u->status == FAIL) {
		return;
	}
	if (!(d = alloc_chardata())) {
		u->status = FAIL;
		return;
	}
	fputdbgs(stdout, "cdatahndl", d->value, d->len, 1);
	if (addcont(&u->e, (union content *)d) == -1) {
		u->status = FAIL;
		return;
	}
	d->len = len;
	if (!(d->value = bdup(s, len + 1))) {
		u->status = FAIL;
		return;
	}
	d->value[len] = '\0';
}

static void
cmnthndl(data, s)
void *data;
const XML_Char *s;
{
/* XXX */
	fputdbgs(stdout, "cmnthndl", s, strlen(s), 1);
}

static int
extehndl(parser, context, base, systemId, publicId)
XML_Parser parser;
const XML_Char *context, *base, *systemId, *publicId;
{
	fputdbgs(stdout, "extehndl", systemId, strlen(systemId), 1);
#if 0
	XML_Parser p;
	char path[MAXPATHLEN];
	FILE *f;
	struct stat sb;

	strlcpy(path, systemId, sizeof path);
	if (stat(path, &sb) == -1) {
		return 0;
	}

	if (!(p = XML_ExternalEntityParserCreate(parser, context, "UTF-8"))) {
		fprintf(stderr, "XML_ExternalEntityParserCreate failed\n");
		return 0;
	}
	if (!(f = fopen(path, "rb"))) {
		return 0;
	}
	if (XXXpafile(p, f, readfn_fread, sb.st_size, path) == -1) {
		fclose(f);
		XML_ParserFree(p);
		return 0;
	}
	fclose(f);

	XML_ParserFree(p);
#endif
	return 1;
}

static void
prcihndl(data, target, s)
void *data;
const XML_Char *target, *s;
{
	fputdbgs(stdout, "prcihndl", s, strlen(s), 1);
}

static void
dflthndl(data, s, len)
void *data;
const XML_Char *s;
int len;
{
	fputdbgs(stdout, "dflthndl", s, len, 1);
}

#if ! defined fputdbgs
static void
fputdbgs(stream, tag, s, len, nl)
FILE *stream;
char const *tag, *s;
{
	size_t i;
	fputs(tag, stream);
	fputs(": ", stream);
	for (i = 0; i < len; i++) {
		fputc(s[i], stream);
	}
	if (nl) {
		fputc('\n', stream);
	}
}
#endif

static int
readfn_fread(cookie, buf, size)
void *cookie;
char *buf;
{
	return fread(buf, 1, size, cookie);
}

static int
writefn_fwrite(cookie, buf, size)
void *cookie;
char *buf;
{
	return fwrite(buf, 1, size, cookie);
}

static void *
bdup(src, len)
const void *src;
size_t len;
{
	void *r;
	if (!(r = malloc(len))) {
		return NULL;
	}
	memcpy(r, src, len);
	return r;
}

ssize_t
unparse(e, stream)
struct element *e;
FILE *stream;
{
	return unparse_fn(e, stream, stream ? writefn_fwrite : NULL);
}

ssize_t
unparse_fn(e, cookie, fn)
struct element *e;
void *cookie;
int (*fn)(void *, char *, int);
{
	struct attribute *a;
	union content **c;
	size_t l = 0, ll;
	if ((ll = lfputs("<", cookie, fn)) == -1) {
		return -1;
	}
	l += ll;
	if ((ll = lfputs(e->name, cookie, fn)) == -1) {
		return -1;
	}
	l += ll;
	if (e->attributes) {
		for (a = e->attributes; a->name; a++) {
			int c1, c2;
			char *qc;
			char const *p;
			for (c1 = c2 = 0, p = a->value; *p; p++) {
				if (*p == '"') {
					c1++;
				}
				else if (*p == '\'') {
					c2++;
				}
			}
			if ((ll = lfputs(" ", cookie, fn)) == -1) {
				return -1;
			}
			l += ll;
			if ((ll = yputs(a->name, 0, cookie, fn)) == -1) {
				return -1;
			}
			l += ll;
			if ((ll = lfputs("=", cookie, fn)) == -1) {
				return -1;
			}
			l += ll;
			if (c1 > c2) {
				qc = "'";
			}
			else {
				qc = "\"";
			}
			if ((ll = lfputs(qc, cookie, fn)) == -1) {
				return -1;
			}
			l += ll;
			if (a->value) {
				if ((ll = yputs(a->value, *qc, cookie, fn)) == -1) {
					return -1;
				}
				l += ll;
			}
			if ((ll = lfputs(qc, cookie, fn)) == -1) {
				return -1;
			}
			l += ll;
		}
	}
	if (e->contents && *e->contents) {
		if ((ll = lfputs("\n>", cookie, fn)) == -1) {
			return -1;
		}
		l += ll;
		for (c = e->contents; *c; c++) {
			switch ((*c)->element.type) {
			case CElem:
				if ((ll = unparse_fn(&(*c)->element, cookie, fn)) == -1) {
					return -1;
				}
				l += ll;
				break;
			case CString:
				if ((ll = yputs((*c)->chardata.value, 0, cookie, fn)) == -1) {
					return -1;
				}
				l += ll;
				break;
			case CComment:
				if ((ll = lfputs("<!--", cookie, fn)) == -1) {
					return -1;
				}
				l += ll;
				if ((ll = lfputs((*c)->misc.string, cookie, fn)) == -1) {
					return -1;
				}
				l += ll;
				if ((ll = lfputs("-->", cookie, fn)) == -1) {
					return -1;
				}
				l += ll;
				break;
			case CPi:
				if ((ll = lfputs("<?", cookie, fn)) == -1) {
					return -1;
				}
				l += ll;
				ll = (*c)->misc.len;
				if (fn) {
					if ((*fn)(cookie, (*c)->misc.pitarget, ll) != ll) {
						return -1;
					}
				}
				l += ll;
				if ((ll = lfputs("?>", cookie, fn)) == -1) {
					return -1;
				}
				l += ll;
				break;
			default:
				break;
			}
		}
		if ((ll = lfputs("</", cookie, fn)) == -1) {
			return -1;
		}
		l += ll;
		if ((ll = lfputs(e->name, cookie, fn)) == -1) {
			return -1;
		}
		l += ll;
		if ((ll = lfputs("\n>", cookie, fn)) == -1) {
			return -1;
		}
		l += ll;
	}
	else {
		if ((ll = lfputs("\n/>", cookie, fn)) == -1) {
			return -1;
		}
		l += ll;
	}
	return l;
}

static ssize_t
lfputs(s, cookie, fn)
char const *s;
void *cookie;
int (*fn)(void *, char *, int);
{
	size_t l = strlen(s);
	if (fn) {
		if ((*fn)(cookie, s, l) != l) {
			return -1;
		}
	}
	return l;
}

static ssize_t
yputs(s, qc, cookie, fn)
char const *s;
void *cookie;
int (*fn)(void *, char *, int);
{
	size_t l, ll;
	for (l = 0; *s; s++, l += ll) {
		if ((ll = yputc(*s, qc, cookie, fn)) == -1) {
			return -1;
		}
	}
	return l;
}

static ssize_t
yputc(c, qc, cookie, fn)
void *cookie;
int (*fn)(void *, char *, int);
{
	switch (c) {
	case '<':
		return lfputs("&lt;", cookie, fn);
	case '>':
		return lfputs("&gt;", cookie, fn);
	case '&':
		return lfputs("&amp;", cookie, fn);
	case '"':
		if (qc != '"') goto noescape;
		return lfputs("&quot;", cookie, fn);
	case '\'':
		if (qc != '\'') goto noescape;
		return lfputs("&apos;", cookie, fn);
	noescape:
	default:
		if (fn) {
			char cc = c;
			if ((*fn)(cookie, &cc, 1) != 1) {
				return -1;
			}
		}
		return 1;
	}
}

/*#define	BULKSIZE	256 */
union content *
alloc_content(type)
{
	union content *c;
/*
	static union content *pool = NULL;
	if (!pool) {
		int i;
		if (!(pool = calloc(BULKSIZE, sizeof *pool))) {
			return NULL;
		}
		for (i = 0; i < BULKSIZE - 1; i++) {
			*(union content **)&pool[i] = &pool[i + 1];
		}
		*(union content **)&pool[i] = NULL;
	}
	c = pool;
	pool = *(union content **)pool;
*/
	c = calloc(1, sizeof *c);
	c->element.type = type;
	return c;
}

#if 0
#define FWBLKSZ	8192

char *
fwstrdup(s)
char const *s;
{
	static char *fullword = NULL;
	static size_t fullword_size = 0;
	size_t l;
	char *p;

	if (!s) {
		return NULL;
	}
	if (!*s) {
		return "";
	}
	l = strlen(s) + 1;
	if (fullword_size < l) {
		fullword_size = MAX(FWBLKSZ, l);
		if (!(fullword = malloc(FWBLKSZ))) {
			return NULL;
		}
	}

	p = fullword;
	fullword += l;
	fullword_size -= l;
	memmove(p, s, l);

	return p;
}
#endif

int
add_attribute(e, name, value)
struct element *e;
char *name;
char *value;
{
	int i;
	if (!name || !value) {
		return -1;
	}
	if (!e->attributes) {
		if (!(e->attributes = calloc(2, sizeof *e->attributes))) {
			return -1;
		}
		i = 0;
	}
	else {
		void *new;
		for (i = 0; e->attributes[i].name; i++) ;
		if (!(new = realloc(e->attributes, (i + 2) * sizeof *e->attributes))) {
			return -1;
		}
		e->attributes = new;
	}
	e->attributes[i].name = name;
	e->attributes[i].value = value;
	e->attributes[i + 1].name = NULL;
	e->attributes[i + 1].value = NULL;
	return 0;
}

union content *
string2chardata(s)
char const *s;
{
	struct chardata *t;
	if (!(t = alloc_chardata())) {
		syslog(LOG_DEBUG, "alloc_chardata: %s", strerror(errno));
		return NULL;
	}
	if (!(t->value = strdup(s))) {
		syslog(LOG_DEBUG, "strdup: %s", strerror(errno));
		return NULL;
	}
	t->len = strlen(t->value);
	return (union content *)t;
}

void
free_document(d)
struct document *d;
{
	free(d->prolog->xmldecl);
	free(d->prolog->doctypedecl);
	free(d->prolog);
	free_element(d->element);
	free(d);
}

void
free_element(e)
struct element *e;
{
	struct attribute *a;
	union content **c;
	if (!e) return;
	if (e->contents) {
		for (c = e->contents; *c; c++) {
			switch ((*c)->element.type) {
			case CElem:
				free_element(&(*c)->element);
				break;
			case CString:
				free((*c)->chardata.value);
				free(&(*c)->chardata);
				break;
			case CComment:
				free((*c)->misc.string);
				free(&(*c)->misc);
				break;
			case CPi:
				free((*c)->misc.pitarget);
				free(&(*c)->misc);
				break;
			default:
				break;
			}
		}
	}
	if (e->attributes) {
		for (a = e->attributes; a->name; a++) {
			free(a->name);
			free(a->value);
		}
	}
	free(e->contents);
	free(e->attributes);
	free(e->name);
	free(e);
}
