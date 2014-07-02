/*	$Id: cstem.c,v 1.13 2010/11/06 03:39:22 nis Exp $	*/

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
static char rcsid[] = "$Id: cstem.c,v 1.13 2010/11/06 03:39:22 nis Exp $";
#endif

#include <config.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <unistd.h>

#include <expat.h>

#include "nwam.h"

#include "util.h"
#include "nio.h"

#include "xstem.h"

#include <gifnoc.h>

struct udata {
	int status, line;
	int recording;
	int words;
	char *ptr;
	size_t size, len;
};

#define	FAIL	0
#define	OK	1

#define	OFF	0
#define	ON	1

static char *process_result(void *, int (*)(void *, char *, int), char const *, size_t);

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

static size_t cputs(char const *, char *);
static int send_request(char const *, char const *, struct nfn_file *, char const *);

static ssize_t my_read(void *, void *, size_t);
static ssize_t my_write(void *, const void *, size_t);
static int my_close(void *);

#if defined HAVE_WSASTARTUP
#ifdef close
#undef close
#endif
#define	close(d)	closesocket((d))
#endif

char *
cstem_to_string(text, stemmer)
char const *text, *stemmer;
{
	int s;
	char *host, *serv;
	char *req_path;
	char *e;

	char *cl, *ct;
	ssize_t clen;
#if defined TCP_NODELAY
	int one = 1;
#endif
	struct nfn_file *fio;

	host = CSTMHOST;
	serv = CSTMSERV;
	req_path = CSTMPATH;

	if ((s = ga_nnet_cli(host, serv, NULL)) == -1) {
		fprintf(stderr, "ga_nnet_cli: %s:%s:%s %s", host ? host : "(null)", serv ? serv : "(null)", "(null)", strerror(errno));
		return NULL;
	}

#if 0
#if defined CRLFTEXT
	_setmode(s, _O_BINARY);
#endif
#endif

#if defined TCP_NODELAY
	(void)setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof one);
#endif

	if (!(fio = nfn_fdopen(&s, my_read, my_write, my_close))) {
		return NULL;
	}

	if (send_request(stemmer, text, fio, req_path) == -1) {
		return NULL;
	}

	if (http_parse_header(fio, nfn_fgets, &cl, &clen, &ct) == -1) {
		return NULL;
	}
	if (!cl || !ct) {
		return NULL;
	}
	if (clen == -1) {
		return NULL;
	}

	e = process_result(fio, nfn_read_int_arg, "-", clen);

	nfn_fclose(fio);

	return e;
}

static char *
process_result(cookie, readfn, path, clen)
void *cookie;
int (*readfn)(void *, char *, int);
char const *path;
size_t clen;
{
	struct udata u0, *u = &u0;
	XML_Parser p = NULL;

	u->recording = OFF;
	u->words = 0;
	u->ptr = NULL;
	u->size = u->len = 0;

/*
	if (!(p = XML_ParserCreateNS("UTF-8", '\1'))) {
		fprintf(stderr, "XML_ParserCreate failed\n");
		goto error;
	}
*/
	if (!(p = XML_ParserCreate("UTF-8"))) {
		fprintf(stderr, "XML_ParserCreate failed\n");
		goto error;
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

	XML_ParserFree(p), p = NULL;

	if (u->status == FAIL) {
		goto error;
	}

	if (!u->words) {
		return NULL;
	}
	if (!u->ptr) {
		return strdup("");
	}
	return u->ptr;

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
	struct udata *u = data;
/*** ***/
	if (u->status == FAIL) {
		return;
	}
	if (!strcmp(el, "words")) {
		if (u->recording) {
			fprintf(stderr, "nested words\n");
			u->status = FAIL;
			return;
		}
		u->recording = ON;
		u->words = 1;
	}
	return;
}

static void
end(data, el)
void *data;
const XML_Char *el;
{
	struct udata *u = data;
/*** ***/
	if (u->status == FAIL) {
		return;
	}
	if (!strcmp(el, "text")) {
		u->recording = OFF;
	}
}

static void
cdatahndl(data, s, len)
void *data;
const XML_Char *s;
int len;
{
	struct udata *u = data;
/*** ***/
	if (u->status == FAIL) {
		return;
	}
	if (u->recording == ON) {
		if (u->size <= u->len + len) {
			size_t newsize = u->len + len + 1024;
			void *new;
			if (!(new = realloc(u->ptr, newsize))) {
				perror("realloc");
				u->status = FAIL;
				return;
			}
			u->ptr = new;
			u->size = newsize;
		}
		memmove(u->ptr + u->len, s, len);
		u->len += len;
		u->ptr[u->len] = '\0';
	}
}

static void
cmnthndl(data, s)
void *data;
const XML_Char *s;
{
/* XXX */
	/*fputdbgs(stdout, "cmnthndl", s, strlen(s), 1);*/
}

static int
extehndl(parser, context, base, systemId, publicId)
XML_Parser parser;
const XML_Char *context, *base, *systemId, *publicId;
{
	/*fputdbgs(stdout, "extehndl", systemId, strlen(systemId), 1);*/
#if 0
	XML_Parser p;
	char path[MAXPATHLEN];
	/*FILE *f;*/
	int d;
	struct stat sb;

	strlcpy(path, systemId, sizeof path);
	if (stat(path, &sb) == -1) {
		return 0;
	}

	if (!(p = XML_ExternalEntityParserCreate(parser, context, "UTF-8"))) {
		fprintf(stderr, "XML_ExternalEntityParserCreate failed\n");
		return 0;
	}
	XXX if (!(f = fopen(path, "r"))) {
		return 0;
	}
	if (pafile_d(p, d, sb.st_size, path) == -1) {
		close(d);
		XML_ParserFree(p);
		return 0;
	}
	close(d);

	XML_ParserFree(p);
#endif
	return 1;
}

static void
prcihndl(data, target, s)
void *data;
const XML_Char *target, *s;
{
	/*fputdbgs(stdout, "prcihndl", s, strlen(s), 1);*/
}

static void
dflthndl(data, s, len)
void *data;
const XML_Char *s;
int len;
{
	/*fputdbgs(stdout, "dflthndl", s, len, 1);*/
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

static size_t
cputs(s, t)
char const *s;
char *t;
{
	size_t l;
	for (l = 0; *s; s++) {
		if (t) {
			*t++ = *s;
		}
		l++;
		if (!strncmp("]]>", s, 3)) {
#define	CDATA_END_START	"]]><![CDATA["
			if (t) {
				memcpy(t, CDATA_END_START, sizeof CDATA_END_START - 1);
				t += sizeof CDATA_END_START - 1;
			}
			l += sizeof CDATA_END_START - 1;
		}
	}
	return l;
}

#define	REQUEST_PROLOG1	"<?xml version=\"1.0\"?>\n<text rtype=\"cdata\""
#define	REQUEST_PROLOG2	"><![CDATA["
#define	REQUEST_TRAILER	"]]></text>\n"
#define	STEMMER1	" stemmer=\""
#define	STEMMER2	"\""

static int
send_request(stemmer, text, out, req_path)
char const *stemmer, *text, *req_path;
struct nfn_file *out;
{
	ssize_t l, o;
	char *s;
	char buf[1024];	/** XXX **/
	char *type = "text/xml";

	if (stemmer) {
		if (!strcmp(stemmer, STEMMER_AUTO) || !strcmp(stemmer, "default")) {
			stemmer = NULL;
		}
	}

	l = 0;
	l += sizeof REQUEST_PROLOG1 - 1;
	if (stemmer) {
		l += strlen(stemmer) ;
		l += sizeof STEMMER1 - 1;
		l += sizeof STEMMER2 - 1;
	}
	l += sizeof REQUEST_PROLOG2 - 1;
	l += cputs(text, NULL);
	l += sizeof REQUEST_TRAILER - 1;

	if (!(s = malloc(l))) {
		return -1;
	}

	o = 0;
	memcpy(s + o, REQUEST_PROLOG1, sizeof REQUEST_PROLOG1 - 1), o += sizeof REQUEST_PROLOG1 - 1;
	if (stemmer) {
		size_t ll;
		memcpy(s + o, STEMMER1, sizeof STEMMER1 - 1), o += sizeof STEMMER1 - 1;
		ll = strlen(stemmer);
		memcpy(s + o, stemmer, ll), o += ll;
		memcpy(s + o, STEMMER2, sizeof STEMMER2 - 1), o += sizeof STEMMER2 - 1;
	}
	memcpy(s + o, REQUEST_PROLOG2, sizeof REQUEST_PROLOG2 - 1), o += sizeof REQUEST_PROLOG2 - 1;
	o += cputs(text, s + o);
	memcpy(s + o, REQUEST_TRAILER, sizeof REQUEST_TRAILER - 1), o += sizeof REQUEST_TRAILER - 1;
	assert(l == o);
	snprintf(buf, sizeof buf, "POST %s HTTP/1.0\r\n", req_path);
	nfn_write(out, buf, strlen(buf));
	snprintf(buf, sizeof buf, "Content-Type: %s\r\n", type);
	nfn_write(out, buf, strlen(buf));
	send_text_content(out, nfn_write, s, l, 1);
	free(s);

	return 0;
}

static ssize_t
my_read(sp, buf, size)
void *sp, *buf;
size_t size;
{
	return recv(*(int *)sp, buf, size, 0);
}

static ssize_t
my_write(sp, buf, size)
void *sp;
const void *buf;
size_t size;
{
	return sendn(*(int *)sp, buf, size, 0);
}

static int
my_close(sp)
void *sp;
{
	return close(*(int *)sp);
}
