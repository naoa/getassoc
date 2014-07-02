/*	$Id: getassoc.c,v 1.76 2011/10/24 06:31:52 nis Exp $	*/

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
static char rcsid[] = "$Id: getassoc.c,v 1.76 2011/10/24 06:31:52 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#if ! defined NO_REGEX
#include <regex.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "nwam.h"

#include "util.h"
#include "nio.h"

#include "common.h"

#include "getassocP.h"

#include <gifnoc.h>

/* #define DBG	1 /* */
/* #define DEBUG_RELAY 1 /* */

#define	MAXCONFLEN	2048

#if ! defined LOG_PERROR
#define	LOG_PERROR	0
#endif

static int m_post(char const *, size_t, int, int);
static int m_get(char const *, int, char *[], int);
static int decode_query_string(char *, char *[], size_t);
#if defined ENABLE_PROXY
static int xgss3(int, char const *, size_t, int);
static int xgss3Lx(int, char const *, size_t, int, char *, char *, char *, int
#if defined DEBUG_RELAY
, FILE *
#endif
);
static int relay_result(struct nfn_file *, FILE *
#if defined DEBUG_RELAY
, FILE *
#endif
);
static int check_relay_r(char const *, char *, size_t, char *, size_t, char *, size_t, char *, size_t, char *
#if defined DEBUG_RELAY
, FILE *
#endif
);
static int readfn_buf(void *, char *, int);
#endif

static ssize_t my_read(void *, void *, size_t);
static ssize_t my_write(void *, const void *, size_t);
static int my_close(void *);

#if defined HAVE_WSASTARTUP
#ifdef close
#undef close
#endif
#define	close(d)	closesocket((d))
#endif

char *progname = NULL;
char *getaroot; /* = GETAROOT; */
extern char *datadir_getassoc;

struct paction {
	char *name;
	int (*action)(int, char const *, size_t, int);
	int (*cgi_action)(char const *, int, char *[], int);
};

static struct paction ptable[] = {
	{"/gss3", gss3, NULL},	/* gss3 protocol */
			/* URL: http://<host>/getassoc/gss3 */
#if defined ENABLE_PROXY
	{"/xgss3", xgss3, NULL}, /* gss3 (bridge) */
			/* URL: http://<host>/getassoc/xgss3/<host>... */
	{"/xgss3L3", xgss3, NULL}, /* gss3 (router) */
			/* URL: http://<host>/getassoc/xgss3L3/<host>... */
#endif
	{"/sct", NULL, sct},
};

struct gaction {
	char *name;
	int (*action)(char const *, int, char *[], int);
};

static struct gaction gtable[] = {
	{"/index.html", index_html},
	{"/catalogue.html", catalogue_html},
	{"/sct/", sct},		/* html interface */
				/* URL: http://<host>/getassoc/sct/index.html */
	{"/sct.js", sct_js},
	{"/sct_help.html", sct_help},
	{"/search.gif", search_gif},
	{"/geta.gif", geta_gif},

#if defined ENABLE_REN
	{"/rn/rn.css", rn_css},
	{"/rn/rn.js", rn_js},
	{"/rn/img/btn_clear.gif", btn_clear_gif},
	{"/rn/img/btn_top_imagine.gif", btn_top_imagine_gif},
	{"/rn/img/top_logo.gif", top_logo_gif},
	{"/rn/private_db.js", private_db_js},
	{"/rn/", rn_html},
#endif

	{"/", index_html},
};

int
main(argc, argv)
char *argv[];
{
	char datadir[MAXPATHLEN];
	size_t i;
	size_t clen;
	char const *request_method;
	char const *path_info = NULL;
	int in, out;
	FILE *fout;
#if defined STATIC_GETAROOT
#elif defined ENABLE_NEWLAYOUT
	int ch;
	char *rcfile = NULL;

	getaroot = NULL;
#else
	char const *script_filename = NULL;
#endif

	in = 0;		/* stdin */
	out = 1;	/* stdout */

#if defined CRLFTEXT
	_setmode(in, _O_BINARY);
	_setmode(out, _O_BINARY);
#endif

#if defined HAVE_WSASTARTUP
	if (wsastartup() == -1) {
		perror("wsastartup");
		return 1;
	}
#endif

	progname = basename(argv[0]);
#if defined ENABLE_NEWLAYOUT
	while ((ch = getopt(argc, argv, "R:"
#if defined ENABLE_NEWLAYOUT
			"r:"
#endif
		)) != -1) {
		switch (ch) {
		case 'R':
			getaroot = optarg;
			break;
		case 'r':
			rcfile = optarg;
			break;
		default:
			return 1;
		}
	}
	/* argc -= optind, argv += optind  oops! we should not fix these variables. */
#endif

	openlog("gss3", LOG_PID|LOG_PERROR, LOG_LOCAL0);

	BEGIN();

#if defined RLIMIT_NOFILE
	MAXRLIMIT(RLIMIT_NOFILE);
#endif

#if defined RLIMIT_CPU
	MAXRLIMIT(RLIMIT_CPU);
#endif

#if defined RLIMIT_VMEM
	MAXRLIMIT(RLIMIT_VMEM);
#endif

#if defined RLIMIT_DATA
	MAXRLIMIT(RLIMIT_DATA);
#endif

	if (!(request_method = getenv("REQUEST_METHOD"))) {
		syslog(LOG_DEBUG, "!request_method");
		goto error;
	}

	if (!(path_info = getenv("PATH_INFO"))) {
		syslog(LOG_DEBUG, "!path_info");
		return 1;
	}

#if defined STATIC_GETAROOT
#elif defined ENABLE_NEWLAYOUT
#else
	if (!(script_filename = getenv("SCRIPT_FILENAME"))) {
		syslog(LOG_DEBUG, "!script_filename");
		return 1;
	}
#endif
#if defined DBG
fprintf(stderr, "REQUEST_METHOD = [%s]\n", request_method ? request_method : "(null)");
fprintf(stderr, "PATH_INFO = [%s]\n", path_info ? path_info : "(null)");
#if defined STATIC_GETAROOT
#elif defined ENABLE_NEWLAYOUT
#else
fprintf(stderr, "SCRIPT_FILENAME = [%s]\n", script_filename ? script_filename : "(null)");
#endif
#endif

#if defined STATIC_GETAROOT
	getaroot = STATIC_GETAROOT;
#elif defined ENABLE_NEWLAYOUT
	if (!getaroot) {
		syslog(LOG_DEBUG, "configuration error: -R required");
		return 1;
	}
#else
	if (!(getaroot = strip2(strdup(script_filename)))) {
		syslog(LOG_DEBUG, "!strip2");
		return 1;
	}
#endif
/*syslog(LOG_DEBUG, "getaroot = [%s]", getaroot);*/
#if defined ENABLE_NEWLAYOUT
	snprintf(datadir, sizeof datadir, "%s", DATADIR);
#else
	snprintf(datadir, sizeof datadir, "%s" DIRECTORY_DELIMITER XDATADIRNAME, getaroot);
#endif
	datadir_getassoc = datadir;

#if defined ENABLE_NEWLAYOUT
	if (rcfile) {
		gss3_init(getaroot, rcfile);
	}
#endif

	if (!strcmp(request_method, "POST")) {
		/* POST */
		char *content_length, *end;
		if (!(content_length = getenv("CONTENT_LENGTH"))) {
			syslog(LOG_DEBUG, "!content_length");
			goto error;
		}
#if defined DBG
fprintf(stderr, "CONTENT_LENGTH = [%s]\n", content_length ? content_length : "(null)");
#endif
		clen = strtoul(content_length, &end, 10);
		if (!(*content_length && !*end)) {
			syslog(LOG_DEBUG, "%s: %s", content_length, strerror(errno));
			goto error;
		}
		if (m_post(path_info, clen, in, out) == -1) {
			syslog(LOG_DEBUG, "!m_post: path_info: [%s]", path_info);
			goto error;
		}
		return 0;
	}
	else if (!strcmp(request_method, "GET")) {
		char *argv[1280];
		char *query_string;
		int argc;
		/* GET */
		query_string = getenv("QUERY_STRING");
#if defined DBG
fprintf(stderr, "QUERY_STRING = [%s]\n", query_string ? query_string : "(null)");
#endif
		if ((argc = decode_query_string(query_string, argv, nelems(argv))) == -1) {
			syslog(LOG_DEBUG, "!decode_query_string");
			goto error;
		}
		if (m_get(path_info, argc, argv, out) == -1) {
			syslog(LOG_DEBUG, "!m_get: path_info: [%s]", path_info);
			goto error;
		}
		return 0;
	}

error:
	if (!(fout = fdopen(out, "w"))) {
		syslog(LOG_DEBUG, "fdopen: %s", strerror(errno));
		return 1;
	}
	fputs("Content-Type: text/plain; charset=utf-8\r\n"
		"Pragma: no-cache\r\n"
		"\r\n", fout);
	for (i = 0; i < argc; i++) {
		fprintf(fout, "argv[%"PRIuSIZE_T"] = %s\r\n", (pri_size_t)i, argv[i]);
	}
	fprintf(fout, "request_method = [%s]\r\n", request_method ? request_method : "(null)");
	fprintf(fout, "path_info = [%s]\r\n", path_info ? path_info : "(null)");
#if defined STATIC_GETAROOT
#elif defined ENABLE_NEWLAYOUT
#else
	fprintf(fout, "script_filename = [%s]\r\n", script_filename ? script_filename : "(null)");
#endif
	return 1;
}

static int
m_post(path_info, clen, in, out)
char const *path_info;
size_t clen;
{
#if defined TCP_NODELAY
	int one = 1;
#endif
	size_t i;

#if defined TCP_NODELAY
	(void)setsockopt(0, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof one);
#endif

	for (i = 0; i < nelems(ptable); i++) {
		struct paction *ap = &ptable[i];
		if (!strncmp(ap->name, path_info, strlen(ap->name))) {
			if (ap->action) {
				return ap->action(in, path_info, clen, out);
			}
			else if (ap->cgi_action) {
				char *argv[1280];
				char *query_string;
				int argc;
				/* GET */

				if (!(query_string = malloc(clen + 1))) {
					return -1;
				}
				readn(in, query_string, clen);
				query_string[clen] = '\0';
				if ((argc = decode_query_string(query_string, argv, nelems(argv))) == -1) {
					return -1;
				}
				return ap->cgi_action(path_info, argc, argv, out);
			}
		}
	}
	return -1;
}

static int
m_get(path_info, argc, argv, out)
char const *path_info;
char *argv[];
{
#if defined TCP_NODELAY
	int one = 1;
#endif
	size_t i;

#if defined TCP_NODELAY
	(void)setsockopt(0, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof one);
#endif
/*syslog(LOG_DEBUG, "m_get: path_info = %s", path_info);*/

	for (i = 0; i < nelems(gtable); i++) {
		struct gaction *ap = &gtable[i];
		if (!strncmp(ap->name, path_info, strlen(ap->name))) {
			return ap->action(path_info, argc, argv, out);
		}
	}
	syslog(LOG_DEBUG, "!m_get: no-match");
	return -1;
}

static int
decode_query_string(s, v, size)
char *s;
char *v[];
size_t size;
{
	char *p, *q, *r;
	int i = 0;

	for (p = q = r = s; *p; ) {
		if (*p == '&') {
			*q++ = '\0';
			p++;
			if (size <= i) {
				return -1;
			}
			v[i++] = r;
			v[i] = NULL;
			r = q;
		}
		else if (*p == '+') {
			*q++ = ' ';
			p++;
		}
		else if (*p == '%') {
			char t[3];
			p++;
			t[0] = *p++;
			t[1] = *p++;
			t[2] = '\0';
			*q++ = strtoul(t, NULL, 16) & 0xff;
		}
		else {
			*q++ = *p++;
		}
	}
	*q++ = '\0';
	if (size <= i) {
		return -1;
	}
	v[i++] = r;
	v[i] = NULL;
	r = q;

	return i;
}

#if defined ENABLE_PROXY
struct cookie {
	char *buf, *p, *e;
};

/* URL: http://<host>/getassoc/xgss3[L3]/<host>[:port]/... */
static int
xgss3(in, path_info, clen, out)
char const *path_info;
size_t clen;
{
	int layer;
	char *p;
	size_t l;
	char host[MAXHOSTNAMELEN], serv[MAXHOSTNAMELEN], path[MAXPATHLEN];

#if defined DEBUG_RELAY
FILE *f;
f = fopen("/tmp/dbg", "a");
if (f) fprintf(f, "\n\n---\npath_info = %s\n", path_info), fflush(f), fflush(f);
#endif
	if (!strncmp(path_info, "/xgss3/", 7)) {
		path_info += 7;
		layer = 2;
	}
	else if (!strncmp(path_info, "/xgss3L3/", 9)) {
		path_info += 9;
		layer = 3;
	}
	else {
#if defined DEBUG_RELAY
if (f) fprintf(f, "configuration error 1\n"), fflush(f);
#endif
		syslog(LOG_DEBUG, "configuration error 1");
		return -1;
	}
	if (p = index(path_info, '/')) {
		l = p - path_info;
	}
	else {
		l = strlen(path_info);
	}
	if (l >= sizeof host) {
#if defined DEBUG_RELAY
if (f) fprintf(f, "invalid request 1\n"), fflush(f);
#endif
		syslog(LOG_DEBUG, "invalid request 1");
		return -1;
	}
	memmove(host, path_info, l); host[l] = '\0';
	l = strlen(path_info) - l;
	if (l >= sizeof path) {
#if defined DEBUG_RELAY
if (f) fprintf(f, "invalid request 2\n"), fflush(f);
#endif
		syslog(LOG_DEBUG, "invalid request 2");
		return -1;
	}
	memmove(path, p, l); path[l] = '\0';
	if (p = index(host, ':')) {
		*p = '\0';
		strcpy(serv, p + 1);
	}
	else {
		strcpy(serv, "80");
	}

#if defined DEBUG_RELAY
if (f) fprintf(f, "host = %s\n", host), fflush(f);
if (f) fprintf(f, "serv = %s\n", serv), fflush(f);
if (f) fprintf(f, "path = %s\n", path), fflush(f);
#endif

	assert(layer == 3 || layer == 2);
	return xgss3Lx(in, path_info, clen, out, host, serv, path, layer
#if defined DEBUG_RELAY
, f
#endif
);
}

static int
xgss3Lx(in, path_info, clen, out, host0, serv0, path0, layer
#if defined DEBUG_RELAY
, f
#endif
)
char const *path_info;
size_t clen;
char *host0, *serv0, *path0;
#if defined DEBUG_RELAY
FILE *f;
#endif
{
	struct document *d = NULL;
	int e;
	char host[MAXHOSTNAMELEN], serv[MAXHOSTNAMELEN];
	char path[MAXPATHLEN];
	char auth[128];	/* XXX => plain */
	struct cookie c;
	size_t rest;
	int s;
	char buf[MAX(MAXURLLEN + sizeof auth + 64, 4096)];
	size_t len, offs;
#if 0
	FILE *t;
#else
	struct nfn_file *t;
#endif
	FILE *fin, *fout;
	char relay_conf[MAXPATHLEN];
#if defined DEBUG_RELAY
if (f) fprintf(f, "xgss3Lx\n"), fflush(f);
#endif

	snprintf(relay_conf, sizeof relay_conf, "%s" DIRECTORY_DELIMITER ETCDIR DIRECTORY_DELIMITER RELAY_CONF, getaroot);

	if (!(fin = fdopen(in, "rb"))) {
		syslog(LOG_DEBUG, "fdopen: %s", strerror(errno));
		return -1;
	}
	if (!(fout = fdopen(out, "wb"))) {
		syslog(LOG_DEBUG, "fdopen: %s", strerror(errno));
		return -1;
	}

	if (strlen(host0) >= sizeof host) return -1;
	if (strlen(serv0) >= sizeof serv) return -1;
	if (strlen(path0) >= sizeof path) return -1;

	strcpy(host, host0);
	strcpy(serv, serv0);
	strcpy(path, path0);

	switch (layer) {
		char **targets;
		size_t n, i;
	case 2:
		e = check_relay_r(relay_conf, host, sizeof host, serv, sizeof serv, path, sizeof path, auth, sizeof auth, NULL
#if defined DEBUG_RELAY
, f
#endif
);
		break;
	case 3:
		if (!(c.buf = malloc(clen))) {
			syslog(LOG_DEBUG, "malloc: %s", strerror(errno));
			return -1;
		}
		for (rest = clen, offs = 0; rest > 0; rest -= len, offs += len) {
			len = fread(buf, 1, MIN(rest, sizeof buf), fin);
			memmove(c.buf + offs, buf, len);
		}
#if defined DEBUG_RELAY
if (f) fprintf(f, "read: %"PRIuSIZE_T" bytes\n", (pri_size_t)offs), fflush(f);
if (f) fprintf(f, "query = \n"), fflush(f);
if (f) fwrite(c.buf, 1, clen, f), fflush(f);
#endif
		c.p = c.buf;
		c.e = c.buf + clen;
		if (!(d = gss3xtargets(&c, readfn_buf, path_info, clen, &targets, &n))) {
#if defined DEBUG_RELAY
if (f) fprintf(f, "gss3xtargets failed\n"), fflush(f);
#endif
			return -1;
		}
#if defined DEBUG_RELAY
if (f) fprintf(f, "start check relay\n"), fflush(f);
if (f) { int i; for (i = 0; i < n; i++) fprintf(f, "target%d: %s\n", i, targets[i]); fflush(f); }
#endif
		for (i = 0, e = 1; i < n; i++) {
			e = e && check_relay_r(relay_conf, host, sizeof host, serv, sizeof serv, path, sizeof path, auth, sizeof auth, targets[i]
#if defined DEBUG_RELAY
, f
#endif
);
		}
		for (i = 0; i < n; i++) free(targets[i]);
		free(targets);
		break;
	default:
		return -1;
	}

	if (!e) {
		syslog(LOG_DEBUG, "relay denied");

#if defined DEBUG_RELAY
if (f) fprintf(f, "relay denied\n"), fflush(f);
#endif
		return -1;
	}
#if defined DEBUG_RELAY
if (f) fprintf(f, "relay allowed\n"), fflush(f);
#endif

	if ((s = ga_nnet_cli(host, serv, NULL)) == -1) {
		syslog(LOG_DEBUG, "nnet_cli: %s", strerror(errno));
#if defined DEBUG_RELAY
if (f) fprintf(f, "nnet_cli: %s\n", strerror(errno)), fflush(f);
#endif
		return -1;
	}
#if 0
#if defined CRLFTEXT
	_setmode(s, _O_BINARY);
#endif
#endif
	snprintf(buf, sizeof buf, "POST %s HTTP/1.0\r\n"
		"Content-Length: %"PRIuSIZE_T"\r\n"
		"%s"
		"\r\n",
		path, (pri_size_t)clen, auth);

#if 0
	if (!(t = fdopen(s, "wb"))) {
#else
	if (!(t = nfn_fdopen(&s, my_read, my_write, my_close))) {
#endif
		syslog(LOG_DEBUG, "fdopen: %s", strerror(errno));
#if defined DEBUG_RELAY
if (f) fprintf(f, "fdopen failed\n"), fflush(f);
#endif
		return -1;
	}
	len = strlen(buf);
	if (nfn_write(t, buf, len) != len) {
		syslog(LOG_DEBUG, "fwrite: %s", strerror(errno));
		return -1;
	}
#if defined DEBUG_RELAY
if (f) fprintf(f, "auth = [%s]\n", auth), fflush(f);
if (f) fwrite(buf, 1, len, f), fflush(f);
#endif
	switch (layer) {
	case 2:
		for (rest = clen; rest > 0; rest -= len) {
			len = fread(buf, 1, MIN(rest, sizeof buf), fin);
			if (nfn_write(t, buf, len) != len) {
				syslog(LOG_DEBUG, "fwrite: %s", strerror(errno));
#if defined DEBUG_RELAY
if (f) fprintf(f, "fwrite failed\n"), fflush(f);
#endif
				return -1;
			}
#if defined DEBUG_RELAY
/*if (f) fwrite(buf, 1, len, f);*/
#endif
		}
		break;
	case 3:
		if (nfn_write(t, c.buf, clen) != clen) {
			return -1;
		}
		free(c.buf);
		/* free(d) */
		break;
	default:
		return -1;
	}
	nfn_fflush(t);

	e = relay_result(t, fout
#if defined DEBUG_RELAY
, f
#endif
		);

	nfn_fclose(t);

	return e;
}

static int
relay_result(t, stream
#if defined DEBUG_RELAY
, f
#endif
)
struct nfn_file *t;
FILE *stream;
#if defined DEBUG_RELAY
FILE *f;
#endif
{
	/*FILE *t;*/
	char *cl, *ct;
	ssize_t clen;
	size_t len;
	char buf[4096];

#if defined DEBUG_RELAY
if (f) fprintf(f, "\n-----\n"), fflush(f);
#endif

#if 0
	if (!(t = fdopen(s, "rb"))) {
		syslog(LOG_DEBUG, "fdopen: %s", strerror(errno));
#if defined DEBUG_RELAY
if (f) fprintf(f, "fdopen failed\n"), fflush(f);
#endif
		return -1;
	}
#endif

	if (http_parse_header(t, nfn_fgets, &cl, &clen, &ct) == -1) {
		return -1;
	}
#if defined DEBUG_RELAY
if (f) fprintf(f, "clen = %"PRIdSSIZE_T"\n-----\n", (pri_ssize_t)clen), fflush(f);
#endif
	if (!cl || !ct) {
#if defined DEBUG_RELAY
if (f) fprintf(f, "invalid request 4\n"), fflush(f);
#endif
		return -1;
	}

	fprintf(stream, "%s\r\n%s\r\nConnection: close\r\n\r\n", ct, cl);
#if defined DEBUG_RELAY
if (f) fprintf(f, "%s\r\n%s\r\nConnection: close\r\n\r\n", ct, cl), fflush(f);
#endif

	if (clen == -1) {
#if defined DEBUG_RELAY
if (f) fprintf(f, "Content-Length not found\n"), fflush(f);
#endif
		return -1;
	}

	for (; clen > 0; clen -= len) {
/*		len = fread(buf, 1, MIN(clen, sizeof buf), t);*/
		len = nfn_read(t, buf, MIN(clen, sizeof buf));
		fwrite(buf, 1, len, stream);
#if defined DEBUG_RELAY
/* if (f) fwrite(buf, 1, len, f); */
#endif
	}
	fflush(stream);
/*	shutdown(s, SHUT_RDWR);*/

#if defined DEBUG_RELAY
if (f) fclose(f);
#endif
	return 0;
}

#if ! defined REG_BASIC
#define	REG_BASIC	0
#endif

static int
check_relay_r(relay_conf, host, host_size, serv, serv_size, path, path_size, auth, auth_size, target
#if defined DEBUG_RELAY
, f
#endif
)
char const *relay_conf;
char *host, *serv, *path, *auth, *target;
size_t host_size, serv_size, path_size, auth_size;
#if defined DEBUG_RELAY
FILE *f;
#endif
{
	char str[MAXURLLEN];
	char const *kwd_allow = "allow: ";
	char const *kwd_deny = "deny: ";
	char const *kwd_auth = "auth: ";
	char const *kwd_rdr = "rdr: ";
	char buf[MAXCONFLEN];
#if ! defined NO_REGEX
	regex_t preg;
#endif
	size_t kwd_allow_len = strlen(kwd_allow);
	size_t kwd_deny_len = strlen(kwd_deny);
	size_t kwd_auth_len = strlen(kwd_auth);
	size_t kwd_rdr_len = strlen(kwd_rdr);
	FILE *conf;
	int r = 0;
	char plain[64];	/* XXX => auth */
	char rdr[MAXCONFLEN];
	char authkey[sizeof plain * 4 / 3 + 2];
	int line, rdr_line = 0;

	bzero(plain, sizeof plain);
	bzero(rdr, sizeof rdr);
	bzero(auth, auth_size);
#if defined DEBUG_RELAY
if (f) fprintf(f, "check_relay: %s %s %s %s\n", host, serv, path, target ? target : "(null)"), fflush(f);
#endif

	if (!(conf = fopen(relay_conf, "r"))) {
#if defined DEBUG_RELAY
if (f) fprintf(f, "%s: %s\n", relay_conf, strerror(errno)), fflush(f);
#endif
		syslog(LOG_DEBUG, "%s: %s", relay_conf, strerror(errno));
		return 0;
	}

	if (target) {
		snprintf(str, sizeof str, "http://%s:%s%s:%s", host, serv, path, target);
	}
	else {
		snprintf(str, sizeof str, "http://%s:%s%s", host, serv, path);
	}
#if defined DEBUG_RELAY
if (f) fprintf(f, "check_relay: str = [%s]\n", str), fflush(f);
#endif

#define	END(v)	do { (r = (v)); goto end; } while (0)

	for (line = 1; fgets(buf, sizeof buf, conf); line++) {
		char *p, *re;
#if defined DEBUG_RELAY
		char const *typ;
#endif
		int retval;
		if (p = index(buf, '\n')) {
			*p = '\0';
			if (buf < p && *(p - 1) == '\r') {
				*(p - 1) = '\0';
			}
		}
		if (!strncmp(buf, kwd_allow, kwd_allow_len)) {
			re = buf + kwd_allow_len;
			retval = 1;
#if defined DEBUG_RELAY
			typ = "allow";
#endif
		}
		else if (!strncmp(buf, kwd_deny, kwd_deny_len)) {
			re = buf + kwd_deny_len;
			retval = 0;
#if defined DEBUG_RELAY
			typ = "deny";
#endif
		}
		else if (!strncmp(buf, kwd_auth, kwd_auth_len)) {
			bzero(plain, sizeof plain);
			if (strlen(buf + kwd_auth_len) >= sizeof plain) {
				continue;
			}
			strcpy(plain, buf + kwd_auth_len);
			continue;
		}
		else if (!strncmp(buf, kwd_rdr, kwd_rdr_len)) {
			bzero(rdr, sizeof rdr);
			if (strlen(buf + kwd_rdr_len) >= sizeof rdr) {
				continue;
			}
			strcpy(rdr, buf + kwd_rdr_len);
			rdr_line = line;
			continue;
		}
		else {
			syslog(LOG_DEBUG, "%s: malformed line: %d", relay_conf, rdr_line);
			return 0;
		}
#if defined DEBUG_RELAY
if (f) fprintf(f, "checking %s against %s\n", typ, re);
#endif
#if ! defined NO_REGEX
		if (regcomp(&preg, re, REG_BASIC) != 0) {
			syslog(LOG_DEBUG, "regcomp: %s", strerror(errno));
			END(0);
		}
		if (regexec(&preg, str, 0, NULL, 0) == 0) { /* match */
#if defined DEBUG_RELAY
if (f) fprintf(f, "+ %s: [%s] [%s]\n", typ, re, str);
#endif
			regfree(&preg);
			END(retval);
		}
#if defined DEBUG_RELAY
if (f) fprintf(f, "- %s: [%s] [%s]\n", typ, re, str);
#endif
		regfree(&preg);
#else /* ! NO_REGEX */
		if (!strcmp(re, str)) {
#if defined DEBUG_RELAY
if (f) fprintf(f, "+ %s: [%s] [%s]\n", typ, re, str);
#endif
			END(retval);
		}
#endif /* ! NO_REGEX */
	}
#if defined DEBUG_RELAY
if (f) fprintf(f, "no match\n");
#endif
end:
	fclose(conf);
	if (plain[0]) {
#if defined DEBUG_RELAY
if (f) fprintf(f, "AUTH = %s\n", plain);
#endif
		encode_1342_bytes(plain, strlen(plain), authkey);
		snprintf(auth, auth_size, "Authorization: Basic %s\r\n", authkey);
	}
	if (rdr[0]) {
		char *p, *q;
		int cnt;
#if defined DEBUG_RELAY
if (f) fprintf(f, "RDR = %s\n", rdr);
#endif

		for (cnt = 0, p = rdr; *p; p++) {
			if (*p == '\t') {
				cnt++;
			}
		}
		if (cnt != 2) {
			syslog(LOG_DEBUG, "%s: malformed rdr format at line %d", relay_conf, rdr_line);
			return 0;
		}
		p = rdr;
		if (!(q = index(p, '\t'))) {
			syslog(LOG_DEBUG, "check_relay_r: internal error");
			return 0;
		}
		*q++ = '\0';
		if (strlen(p) >= host_size) {
			syslog(LOG_DEBUG, "check_relay_r: too long host at line %d", rdr_line);
			return 0;
		}
		strcpy(host, p);
		p = q;

		if (!(q = index(p, '\t'))) {
			syslog(LOG_DEBUG, "check_relay_r: internal error A");
			return 0;
		}
		*q++ = '\0';
		if (strlen(p) >= serv_size) {
			syslog(LOG_DEBUG, "check_relay_r: too long serv at line %d", rdr_line);
			return 0;
		}
		strcpy(serv, p);
		p = q;

		if (q = index(p, '\t')) {
			syslog(LOG_DEBUG, "check_relay_r: internal error B");
			return 0;
		}
		if (strlen(p) >= path_size) {
			syslog(LOG_DEBUG, "check_relay_r: too long path at line %d", rdr_line);
			return 0;
		}
		strcpy(path, p);
	}
	return r;
}

static int
readfn_buf(d, buf, nbytes)
void *d;
char *buf;
{
	struct cookie *c = d;
	size_t len, rest;

	rest = c->e - c->p;
	len = MIN(nbytes, rest);

	memmove(buf, c->p, len);
	c->p += len;

	return len;
}
#endif

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
