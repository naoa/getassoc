/*	$Id: otmi2itb.c,v 1.25 2011/10/24 06:31:53 nis Exp $	*/

/*-
 * Copyright (c) 2007 Shingo Nishioka.
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
static char rcsid[] = "$Id: otmi2itb.c,v 1.25 2011/10/24 06:31:53 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>

#include <ctype.h>
#include <getopt.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <expat.h>

#include "nstring.h"

#include <gifnoc.h>

#define	nelems(e)	(sizeof (e) / sizeof *(e))

struct udata {
	int status, line;
	struct {
		unsigned int meta, opt, entry, id, title, link, attr, fss, data, vectors, vector, snippets, snippet;
	} rec;
	struct {
		struct nstring meta, opt, id, title, link, attr, attrs, fss, fsss, vectors, vector, snippets, snippet;
	} v;
	struct nstring count;
	FILE *g;
};

#define FAIL	0
#define	OK	1

struct elname {
	char const *name;
	unsigned int id;
};

#define	M_META	1
#define	M_OPT	2

#define	M_ENTRY	10
#define	M_ID	11
#define	M_TITLE	12
#define	M_LINK	13

#define	M_DATA	20
#define	M_VECTORS	21
#define	M_VECTOR	22
#define	M_SNIPPETS	23
#define	M_SNIPPET	24

#define	M_ATTR	30
#define	M_FSS	31

#define	NS_ITB	"http://itb.cs.nii.ac.jp/"
#define	NS_OTMI	"http://www.nature.com/schema/2006/03/otmi"
#define	NS_PRISM	"http://prismstandard.org/namespaces/1.2/basic/"
#define	NS_ATOM	"http://www.w3.org/2005/Atom"

#define	NSX	"\001"

struct elname elnames[] = {
	{NS_ITB NSX "meta", M_META},
	{NS_ITB NSX "opt", M_OPT},

	{NS_ATOM NSX "entry", M_ENTRY},
	{NS_ATOM NSX "id", M_ID},
	{NS_ATOM NSX "title", M_TITLE},
	{NS_ATOM NSX "link", M_LINK},

	{NS_OTMI NSX "data", M_DATA},
	{NS_OTMI NSX "vectors", M_VECTORS},
	{NS_OTMI NSX "vector", M_VECTOR},
	{NS_OTMI NSX "snippets", M_SNIPPETS},
	{NS_OTMI NSX "snippet", M_SNIPPET},

	{NS_ITB NSX "attr", M_ATTR},
	{NS_ITB NSX "fss", M_FSS},
};

#if ! defined MIN
#define	MIN(a, b)	((a) < (b) ? (a) : (b))
#endif

static void usage(void);
static int p_main(FILE *, FILE *);
static void write_entry(struct udata *, FILE *);
static int pabuf(struct udata *, XML_Parser, char *, size_t, char const *);
static int pafile(struct udata *, XML_Parser, FILE *, char const *);
static void start(void *, const XML_Char *, const XML_Char **);
static void end(void *, const XML_Char *);
static void cdatahndl(void *, const XML_Char *, int);
static unsigned int eltype(const XML_Char *, struct elname *, size_t);
static int elcompar(const void *, const void *);
static void kill_nl(char *);

char *progname;

static void
usage()
{
	fprintf(stderr, "usage: %s [-o out] otmi.xml\n", basename(progname));
	exit(1);
}

int
main(argc, argv)
char *argv[];
{
	int ch;
	char *inf, *outf = NULL;
	FILE *in, *out;

	progname = basename(argv[0]);

	while ((ch = getopt(argc, argv, "o:")) != -1) {
		switch (ch) {
		case 'o':
			outf = optarg;
			break;
		default:
			usage();
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1) {
		usage();
		return 1;
	}
	inf = argv[0];

	if (!(in = fopen(inf, "rb"))) {
		perror(inf);
		return 1;
	}
	if (outf) {
		if (!(out = fopen(outf, "wb"))) {
			perror(outf);
			return 1;
		}
	}
	else {
		out = stdout;
	}

	if (p_main(in, out) == -1) {
		return 1;
	}
	return 0;
}

static int
p_main(in, out)
FILE *in, *out;
{
	XML_Parser p = NULL;
	struct udata u0, *u = &u0;

	bzero(u, sizeof *u);

	u->g = out;

	qsort(elnames, nelems(elnames), sizeof *elnames, elcompar);

	if (!(p = XML_ParserCreateNS("UTF-8", NSX[0]))) {
		fprintf(stderr, "XML_ParserCreateNS failed\n");
		goto error;
	}

	XML_SetElementHandler(p, start, end);
	XML_SetCharacterDataHandler(p, cdatahndl);

	bzero(&u->rec, sizeof u->rec);

	XML_SetUserData(p, u);
	u->status = OK;
	u->line = 1;
	if (pafile(u, p, in, "stdin") == -1) {
		fprintf(stderr, "pafile failed\n");
		goto error;
	}

	XML_ParserFree(p);

	if (u->status == FAIL) {
		return -1;
	}

	return 0;

error:
	if (p) XML_ParserFree(p);
	return -1;
}

static void
write_entry(u, stream)
struct udata *u;
FILE *stream;
{
	kill_nl(u->v.id.ptr);
	kill_nl(u->v.title.ptr);
	kill_nl(u->v.link.ptr);

	if (!u->v.id.ptr) {
		fprintf(stderr, "`id' is missing: line %d\n", u->line);
	}
	if (!u->v.title.ptr) {
		fprintf(stderr, "`title' is missing: line %d\n", u->line);
	}
	if (!u->v.link.ptr) {
		fprintf(stderr, "`link' is missing: line %d\n", u->line);
	}

	fputs(u->v.id.ptr, stream), fputs("\n", stream);
	fputs(u->v.title.ptr, stream), fputs("\n", stream);
	fputs(u->v.link.ptr, stream), fputs("\n", stream);

	if (u->v.vectors.ptr) {
		fputs(u->v.vectors.ptr, stream);
	}

	if (u->v.snippets.ptr) {
		fputs(u->v.snippets.ptr, stream);
	}

	if (u->v.attrs.ptr) {
		fputs(u->v.attrs.ptr, stream);
	}

	if (u->v.fsss.ptr) {
		fputs(u->v.fsss.ptr, stream);
	}
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
pafile(u, p, f, path)
struct udata *u;
XML_Parser p;
FILE *f;
char const *path;
{
	ssize_t len;
	char b[8192];
	int first;
	snprintf(b, sizeof b, "<itb:itb xmlns:itb=\"" NS_ITB "\">");
	if (pabuf(u, p, b, strlen(b), path) == -1) {
		return -1;
	}
#define	BOM	"\357\273\277"
	for (first = 1; (len = fread(b, 1, sizeof b, f)) > 0; first = 0) {
		if (first && len >= 3 && memcmp(b, BOM, 3) == 0) {
			memmove(b, b + 3, len - 3);
			len -= 3;
		}
		if (pabuf(u, p, b, len, path) == -1) {
			return -1;
		}
	}
	snprintf(b, sizeof b, "</itb:itb>");
	if (pabuf(u, p, b, strlen(b), path) == -1) {
		return -1;
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
	struct udata *u = data;
	unsigned int type;
	int i;

	if (u->status == FAIL) {
		return;
	}

	type = eltype(el, elnames, nelems(elnames));

	switch (type) {
	case M_META:
		NS_ZERO(&u->v.meta);
		for (i = 0; attr[i]; i += 2) {
			if (!strcmp(attr[i], "type")) {
				ns_append(&u->v.meta, "@", -1);
				ns_append(&u->v.meta, attr[i + 1], -1);
				ns_append(&u->v.meta, "=", -1);
				break;
			}
		}
		if (!attr[i]) {
			fprintf(stderr, "no `type' in `meta' at line %d\n", u->line);
			goto err;
		}
		u->rec.meta++;
		break;
	case M_OPT:
		NS_ZERO(&u->v.opt);
		for (i = 0; attr[i]; i += 2) {
			if (!strcmp(attr[i], "type")) {
				ns_append(&u->v.opt, "$", -1);
				ns_append(&u->v.opt, attr[i + 1], -1);
				ns_append(&u->v.opt, "=", -1);
				break;
			}
		}
		if (!attr[i]) {
			fprintf(stderr, "no `type' in `opt' at line %d\n", u->line);
			goto err;
		}
		u->rec.opt++;
		break;
	case M_ENTRY:
		if (u->rec.entry) goto err;

		NS_ZERO(&u->v.id);
		NS_ZERO(&u->v.title);
		NS_ZERO(&u->v.link);

		NS_ZERO(&u->v.vectors);
		NS_ZERO(&u->v.snippets);
		NS_ZERO(&u->v.attrs);
		NS_ZERO(&u->v.fsss);

		u->rec.entry++;
		break;
	case M_ID:
		if (!u->rec.entry || u->rec.id || u->v.id.ptr) goto err;
		ns_append(&u->v.id, "i", -1);
		u->rec.id++;
		break;
	case M_TITLE:
		if (!u->rec.entry || u->rec.title || u->v.title.ptr) goto err;
		ns_append(&u->v.title, "#title=", -1);
		u->rec.title++;
		break;
	case M_LINK:
		if (!u->rec.entry || u->rec.link || u->v.link.ptr) goto err;
		ns_append(&u->v.link, "#link=", -1);
		u->rec.link++;
		break;

	case M_DATA:
		if (!u->rec.entry || u->rec.data) goto err;
		u->rec.data++;
		break;

	case M_VECTORS:
		if (!u->rec.data || u->rec.vectors || u->v.vectors.ptr) goto err;
		u->rec.vectors++;
		break;
	case M_VECTOR:
		if (!u->rec.vectors || u->rec.vector) goto err;
		NS_ZERO(&u->v.vector);
		NS_ZERO(&u->count);
		for (i = 0; attr[i]; i += 2) {
			if (!strcmp(attr[i], "count")) {
				char const *p = attr[i + 1];
				char *end;
				strtoul(p, &end, 10);
				if (!(isdigit(*p & 0xff) && !*end)) {
					fprintf(stderr, "not a number\n");
					goto err;
				}
				ns_append(&u->count, attr[i + 1], -1);
				break;
			}
		}
		if (!attr[i]) {
			ns_append(&u->count, "1", -1);
		}
		u->rec.vector++;
		break;
	case M_SNIPPETS:
		if (!u->rec.data || u->rec.snippets || u->v.snippets.ptr) goto err;
		u->rec.snippets++;
		break;
	case M_SNIPPET:
		if (!u->rec.snippets || u->rec.snippet) goto err;
		NS_ZERO(&u->v.snippet);
		u->rec.snippet++;
		break;

	case M_ATTR:
		if (!u->rec.data || u->rec.attr) goto err;
		NS_ZERO(&u->v.attr);
		for (i = 0; attr[i]; i += 2) {
			if (!strcmp(attr[i], "type")) {
				ns_append(&u->v.attr, "#", -1);
				ns_append(&u->v.attr, attr[i + 1], -1);
				ns_append(&u->v.attr, "=", -1);
				break;
			}
		}
		if (!attr[i]) {
			fprintf(stderr, "no `type' in `attr' at line %d\n", u->line);
			goto err;
		}
		u->rec.attr++;
		break;

	case M_FSS:
		if (!u->rec.data || u->rec.fss) goto err;
		NS_ZERO(&u->v.fss);
		ns_append(&u->v.fss, "!", -1);
		u->rec.fss++;
		break;

	default:
		break;
	}
	return;

err:
	fprintf(stderr, "error at line %d\n", u->line);
	u->status = FAIL;
}

static void
end(data, el)
void *data;
const XML_Char *el;
{
	struct udata *u = data;
	unsigned int type;

	if (u->status == FAIL) {
		return;
	}

	type = eltype(el, elnames, nelems(elnames));

	switch (type) {
	case M_META:
		kill_nl(u->v.meta.ptr);
		fputs(u->v.meta.ptr, u->g), fputs("\n", u->g);
		NS_FREE(&u->v.meta);
		u->rec.meta--;
		break;
	case M_OPT:
		kill_nl(u->v.opt.ptr);
		fputs(u->v.opt.ptr, u->g), fputs("\n", u->g);
		NS_FREE(&u->v.opt);
		u->rec.opt--;
		break;
	case M_ENTRY:
		write_entry(u, u->g);

		NS_FREE(&u->v.id);
		NS_FREE(&u->v.title);
		NS_FREE(&u->v.link);

		NS_FREE(&u->v.vectors);
		NS_FREE(&u->v.snippets);
		NS_FREE(&u->v.attrs);
		NS_FREE(&u->v.fsss);

		u->rec.entry--;
		break;
	case M_ID:
		u->rec.id--;
		break;
	case M_TITLE:
		u->rec.title--;
		break;
	case M_LINK:
		u->rec.link--;
		break;

	case M_DATA:
		u->rec.data--;
		break;
	case M_VECTORS:
		u->rec.vectors--;
		break;
	case M_VECTOR:
		if (u->v.vector.ptr) {
			kill_nl(u->v.vector.ptr);
			ns_append(&u->v.vectors, u->count.ptr, -1);
			ns_append(&u->v.vectors, " ", -1);
			ns_append(&u->v.vectors, u->v.vector.ptr, -1);
			ns_append(&u->v.vectors, "\n", -1);
		}
		NS_FREE(&u->v.vector);
		NS_FREE(&u->count);
		u->rec.vector--;
		break;
	case M_SNIPPETS:
		u->rec.snippets--;
		break;
	case M_SNIPPET:
		if (u->v.snippet.ptr) {
			kill_nl(u->v.snippet.ptr);
			ns_append(&u->v.snippets, "b1", -1);
			ns_append(&u->v.snippets, u->v.snippet.ptr, -1);
			ns_append(&u->v.snippets, "\n", -1);
		}
		NS_FREE(&u->v.snippet);
		u->rec.snippet--;
		break;

	case M_ATTR:
		kill_nl(u->v.attr.ptr);
		ns_append(&u->v.attrs, u->v.attr.ptr, -1);
		ns_append(&u->v.attrs, "\n", -1);
		NS_FREE(&u->v.attr);
		u->rec.attr--;
		break;
	case M_FSS:
		kill_nl(u->v.fss.ptr);
		ns_append(&u->v.fsss, u->v.fss.ptr, -1);
		ns_append(&u->v.fsss, "\n", -1);
		NS_FREE(&u->v.fss);
		u->rec.fss--;
		break;

	default:
		break;
	}
}

static void
cdatahndl(data, s, len)
void *data;
const XML_Char *s;
int len;
{
	struct udata *u = data;
	if (u->status == FAIL) {
		return;
	}
	else if (u->rec.meta) {
		ns_append(&u->v.meta, s, len);
	}
	else if (u->rec.opt) {
		ns_append(&u->v.opt, s, len);
	}
	else if (u->rec.id) {
		ns_append(&u->v.id, s, len);
	}
	else if (u->rec.title) {
		ns_append(&u->v.title, s, len);
	}
	else if (u->rec.link) {
		ns_append(&u->v.link, s, len);
	}
	else if (u->rec.vector) {
		ns_append(&u->v.vector, s, len);
	}
	else if (u->rec.snippet) {
		ns_append(&u->v.snippet, s, len);
	}
	else if (u->rec.attr) {
		ns_append(&u->v.attr, s, len);
	}
	else if (u->rec.fss) {
		ns_append(&u->v.fss, s, len);
	}
}

static unsigned int
eltype(el, names, nmemb)
const XML_Char *el;
struct elname *names;
size_t nmemb;
{
	struct elname key, *e;
	key.name = el;
	if (e = bsearch(&key, names, nmemb, sizeof *names, elcompar)) {
		return e->id;
	}
	return 0;
}

static int
elcompar(va, vb)
const void *va;
const void *vb;
{
	struct elname const *a = va;
	struct elname const *b = vb;

	return strcmp(a->name, b->name);
}

static void
kill_nl(s)
char *s;
{
	char *t;
	if (!s) {
		return;
	}
	for (t = s; *s; s++) {
		if (*s != '\n') {
			*t++ = *s;
		}
	}
	*t = '\0';
}
