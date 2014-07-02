/*	$Id: itb2frq.c,v 1.70 2011/10/24 06:31:52 nis Exp $	*/

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
static char rcsid[] = "$Id: itb2frq.c,v 1.70 2011/10/24 06:31:52 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <libgen.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "nwam.h"
#include "util.h"

#include "xstem.h"
#include "fssP.h"

#include "itb2frq.h"

#include <gifnoc.h>

#define	INFKEY		""
#define	FRQKEY		"@frq"
#define	OPTIONSKEY	"@options"
#define	FLWDKEY		"@flwd"
#define	NAMESKEY	"@names"

static int itb_procline(struct state *, char *, struct fs *, int);
struct fs *fskey(struct fs *, int, char const *);
static int setup_pstem(struct state *, char const *, int, int);
static int cleanup_proc_body(struct state *);
static int proc_body(struct state *, char *, struct fs *);
static int proc_flwd(struct state *, char *, struct fs *);
static int check_turn(struct fs *);
int is_xname_subset(char const *);
int is_xletter_subset(int);

static char *progname = NULL;
static int twarn = 0;

void
usage()
{
	fprintf(stderr, "usage: %s [-i:] [-o:] [-s:] basename [aux...]\n", progname);
	exit(1);
}

extern char **environ;

int
main(argc, argv)
char *argv[];
{
	struct fs *fs, *fp;
	extern char *optarg;
	extern int optind;
	int ch;
	char *base = NULL;
	int naux;
	int i;
	size_t tally = 0;	/* modulo 10 * 1024 * 1024 */
	struct state g0, *g = &g0;
	NFILE *n;
int stem_a, stem_n;

	char const *stemmer = NULL;

	progname = basename(argv[0]);

	g->i.seqno = 0;
	g->i.interval = 1;
	g->i.offset = 0;

	while ((ch = getopt(argc, argv, "i:o:s:")) != -1) {
		switch (ch) {
			char *end;
		case 'i':
			g->i.interval = strtoul(optarg, &end, 10);
			if (!(*optarg && !*end)) usage();
			break;
		case 'o':
			g->i.offset = strtoul(optarg, &end, 10);
			if (!(*optarg && !*end)) usage();
			break;
		case 's':
			stemmer = optarg;
			break;
		case '?':
		default:
			usage();
		}
	}
	if (!stemmer) {
		stemmer = STEMMER_AUTO; XXX
	}
	argc -= optind;
	argv += optind;

	if (g->i.offset >= g->i.interval) {
		fprintf(stderr, "warning: offset is g.t. interval.\n");
		g->i.offset %= g->i.interval;
		fprintf(stderr, "using new value: %"PRIuSIZE_T"\n", (pri_size_t)g->i.offset);
	}

	if (argc <= 0) {
		usage();
	}

	base = argv[0];
	if (index(base, '/')) {
		fprintf(stderr, "error: invalid base [%s]\n", base);
		return 1;
	}

	argc--, argv++;
	naux = argc;
	if (!(fs = calloc(naux + 1, sizeof *fs))) {
		perror("calloc");
		return 1;
	}
stem_a = 0;
stem_n = 0;
	for (i = 0, fp = fs; i < naux; i++, fp++) {
		fp->base = base;
		fp->key = argv[i];
		fp->turn = 0;
		if (!(is_xname_subset(fp->key) || *fp->key == '@' && is_xname_subset(fp->key + 1))) {
			fprintf(stderr, "error: invalid key [%s]\n", fp->key);
			return 1;
		}
if (!strcmp(fp->key, FRQKEY)) {
	stem_a = 1;
}
if (!strcmp(fp->key, FLWDKEY)) {
	stem_n = 1;
}
	}
	if (!(fp->key = strdup(INFKEY))) {
		perror("strdup");
		return 1;
	}

/*	if (setup_pstem(g, stemmer, strcmp(fp->key, FRQKEY), strcmp(fp->key, FLWDKEY)) == -1) ??<*/
	if (setup_pstem(g, stemmer, stem_a, stem_n) == -1) {
		return -1;
	}

	for (i = 0, fp = fs; i < naux; i++, fp++) {
		snprintf(fp->path, sizeof fp->path, "%s-%d.#%s", fp->base, fp->turn, fp->key);
		if (!(fp->stream = fopen(fp->path, "w"))) {
			perror(fp->path);
			return 1;
		}
	}
	snprintf(fp->path, sizeof fp->path, "%s.@inf", base);
	if (!(fp->stream = fopen(fp->path, "w"))) {
		perror(fp->path);
		return 1;
	}

	g->id = NULL;
	g->rec_segid = 0;
	g->rec = 0;

	if (!(n = nfopen(stdin))) {
		perror("nfopen");
		return 1;
	}
	for (g->lineno = 1; ; g->lineno++) {
		char *buf, *p;
		if (!(buf = readln(n))) {
			fprintf(stderr, "readln failed at %"PRIuSIZE_T"\n", (pri_size_t)g->lineno);
			return 1;
		}
		if (!*buf) {
			break;	/* EOF */
		}
		if (!index(buf, '\n')) {
			fprintf(stderr, "too long line (or no newline) at %"PRIuSIZE_T"\n", (pri_size_t)g->lineno);
			return 1;
		}
		tally += strlen(buf);
		if ((p = rindex(buf, '\r')) && *(p + 1) == '\n' && *(p + 2) == '\0') {
			/* CRLF -> LF */
			*p = '\n';
			*(p + 1) = '\0';
		}
#define	BOM	"\357\273\277"
		if (g->lineno == 1 && memcmp(buf, BOM, 3) == 0) {
			buf += 3;
		}
		if (itb_procline(g, buf, fs, naux + 1) == -1) {
			fprintf(stderr, "fatal error at %"PRIuSIZE_T"\n", (pri_size_t)g->lineno);
			return 1;
		}
		if (tally >= 10 * 1024 * 1024) {
			fprintf(stderr, "+");
			tally -= 10 * 1024 * 1024;
		}
	}
	if (cleanup_proc_body(g) == -1) {
		return 1;
	}

	free(g->id);

	for (i = 0, fp = fs; i < naux + 1; i++, fp++) {
		fclose(fp->stream);
	}
	return 0;
}

static int
itb_procline(g, r, fs, naux)
struct state *g;
char *r;
struct fs *fs;
{
	char *p, *q;
	int i;

	switch (*r) {
		struct fs *fp;
		char const *id;
	case '@':
		if (fp = fskey(fs, naux, INFKEY)) {
			fputs(r, fp->stream);
		}
		break;
	case '$':
		if (fp = fskey(fs, naux, OPTIONSKEY)) {
			fputs(r, fp->stream);
		}
		break;
	case 'i':
		if (!g->id) {
			g->rec_segid = 1;
		}
		else {
			if (g->rec_segid) {
				g->segid0 = g->segid;
				g->rec_segid = 0;
			}
			if (g->segid != g->segid0) {
				fprintf(stderr, "warning: seg # varies in [%s] %d -- %d\n", g->id, g->segid0, g->segid);
			}
		}
		g->segid = 0;

		free(g->id);
		if (!(g->id = strdup(r + 1))) {
			perror("strdup");
			return -1;
		}
		if (p = index(g->id, '\n')) {
			*p = '\0';
		}

		if (!(g->rec = g->i.seqno++ % g->i.interval == g->i.offset)) break;
		id = r + 1;
		for (i = 0; i < naux; i++) {
			fp = &fs[i];
			if (!strcmp(fp->key, FRQKEY)) {
				if (check_turn(fp) == -1) {
					return -1;
				}
				fputs("@", fp->stream);
				fputs(id, fp->stream);
			}
			else if (!strcmp(fp->key, NAMESKEY)) {
				/* don't check turn on these keys */
				size_t l = strlen(id);
				fwrite(id, 1, l - 1, fp->stream);
				fwrite("", 1, 1, fp->stream);
			}
			else if (!strcmp(fp->key, INFKEY) ||
				 !strcmp(fp->key, OPTIONSKEY) ||
				 !strcmp(fp->key, FLWDKEY)) {
				/* do nothing */
				/* don't check turn on these keys */
			}
			else {
				if (check_turn(fp) == -1) {
					return -1;
				}
				fputs("%", fp->stream);
				fputs(id, fp->stream);
			}
		}
		break;
	case 't':
if (!twarn) {
	fprintf(stderr, "warning: use of `t' is strongly discouraged. use `#ttl=' instead.\n");
	twarn = 1;
}
		/* if (!g->id) return -1; */
		if (!g->rec) break;
		if (fp = fskey(fs, naux, "ttl")) {
			fputs(" ", fp->stream);
			fputs(r + 1, fp->stream);
		}
		break;
	case ' ':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		/* if (!g->id) return -1; */
		if (!g->rec) break;
		if (fp = fskey(fs, naux, FRQKEY)) {
			if (*r == ' ') {
				fputs("1", fp->stream);
			}
			else {
				for (q = r; *q && *q != ' '; q++) {
					if (!isdigit(*q & 0xff)) {
						fprintf(stderr, "malformed line [%s] at %"PRIuSIZE_T"\n", r, (pri_size_t)g->lineno);
						return -1;
					}
				}
				if (*q != ' ') {
					fprintf(stderr, "malformed line [%s] at %"PRIuSIZE_T"\n", r, (pri_size_t)g->lineno);
					return -1;
				}
			}
			fputs(r, fp->stream);
		}
		break;
	case 'b':
		/* if (!g->id) return -1; */
		if (!g->rec) break;
		if (!g->a.ps) break;
		if (fp = fskey(fs, naux, FRQKEY)) {
			if (proc_body(g, r, fp) == -1) {
				return -1;
			}
		}
		break;
	case '!':
		/* if (!g->id) return -1; */
		if (!g->rec) break;
		if (!g->n.ps) break;
		if (fp = fskey(fs, naux, FLWDKEY)) {
			if (g->segid > MAXSEGID) {
				fprintf(stderr, "too many segments\n");
				return -1;
			}
			if (proc_flwd(g, r, fp) == -1) {
				return -1;
			}
			g->segid++;
		}
		break;
	case '#':
		/* if (!g->id) return -1; */
		if (!g->rec) break;
		if (!(p = index(r + 1, '='))) {
			fprintf(stderr, "= expected at line %"PRIuSIZE_T"\n", (pri_size_t)g->lineno);
			return -1;
		}
		q = r + 1;
/*
		for (q = r + 1; q < p; q++) {
			if (!isalnum(*q & 0xff)) {
				fprintf(stderr, "invalid char: %c at line %"PRIuSIZE_T"\n", *q, (pri_size_t)g->lineno);
				return -1;
			}
		}
*/
		*p++ = '\0';
		if (!is_xname_subset(q)) {
			fprintf(stderr, "invalid name: %s at line %"PRIuSIZE_T"\n", q, (pri_size_t)g->lineno);
			return -1;
		}
		if (*p != '\n' && (fp = fskey(fs, naux, r + 1))) {
			fputs(" ", fp->stream);
			fputs(p, fp->stream);
		}
		break;
	default:
		fprintf(stderr, "error: malformed line [%s] at %"PRIuSIZE_T"\n", r, (pri_size_t)g->lineno);
		return -1;
	}
	return 0;
}

struct fs *
fskey(fs, naux, key)
struct fs *fs;
char const *key;
{
	int i;
	for (i = 0; i < naux; i++) {
		struct fs *fp = &fs[i];
		if (!strcmp(key, fp->key)) {
			return fp;
		}
	}
	return NULL;
}

static int
setup_pstem(g, stemmer, stem_a, stem_n)
struct state *g;
char const *stemmer;
{
	int fdsA0[2], fdsA1[2];
	int fdsN0[2], fdsN1[2];
	int fds2[2];

	g->a.ps = NULL;
	g->n.ps = NULL;

	if (stem_a) {
		if (pipe(fdsA1) == -1) {
			perror("pipe");
			return -1;
		}
		if (pipe(fdsA0) == -1) {
			perror("pipe");
			return -1;
		}
		switch (g->a.pid = fork()) {
			char *path;
			char *argv[3];
		case -1:
			perror("fork");
			return -1;
		case 0:
			path = STMDBIN;
			close(fdsA1[1]);
			close(fdsA0[0]);
			dup2(fdsA1[0], 0);
			if (fdsA1[0] != 0) {
				close(fdsA1[0]);
			}
			dup2(fdsA0[1], 1);
			if (fdsA0[1] != 1) {
				close(fdsA0[1]);
			}
			argv[0] = path;
			argv[1] = "-q";
			argv[2] = NULL;
	fprintf(stderr, "%s.pstem A: %d\n", progname, getpid());
			if (execve(path, argv, environ) == -1) {
				perror(path);
				_exit(1);
			}
			_exit(0);
		default:
			close(fdsA1[0]);
			close(fdsA0[1]);
			break;
		}
	}
	else {
		g->a.pid = -1;
	}

	if (stem_n) {
		if (pipe(fdsN1) == -1) {
			perror("pipe");
			return -1;
		}
		if (pipe(fdsN0) == -1) {
			perror("pipe");
			return -1;
		}
		switch (g->n.pid = fork()) {
			char *path;
			char *argv[3];
		case -1:
			perror("fork");
			return -1;
		case 0:
			if (stem_a) {
				close(fdsA1[1]);
				close(fdsA0[0]);
			}
			path = STMDBIN;
			close(fdsN1[1]);
			close(fdsN0[0]);
			dup2(fdsN1[0], 0);
			if (fdsN1[0] != 0) {
				close(fdsN1[0]);
			}
			dup2(fdsN0[1], 1);
			if (fdsN0[1] != 1) {
				close(fdsN0[1]);
			}
			argv[0] = path;
			argv[1] = "-q";
			argv[2] = NULL;
	fprintf(stderr, "%s.pstem N: %d\n", progname, getpid());
			if (execve(path, argv, environ) == -1) {
				perror(path);
				_exit(1);
			}
			_exit(0);
		default:
			close(fdsN1[0]);
			close(fdsN0[1]);
			break;
		}
	}
	else {
		g->n.pid = -1;
	}

	if (pipe(fds2) == -1) {
		perror("pipe");
		return -1;
	}

	switch (g->r.pid = fork()) {
		size_t lineno;
		NFILE *n;
		int fds[2];
		FILE *relay_in;
	case -1:
		perror("fork");
		return -1;
	case 0:
		close(fds2[0]);
		if (stem_a) close(fdsA0[0]);
		if (stem_n) close(fdsN0[0]);
		if (!(relay_in = fdopen(fds2[1], "w"))) {
			perror("fdoen");
			_exit(1);
		}
fprintf(stderr, "%s.relay: %d\n", progname, getpid());
		if (stem_a) {
			fds[0] = -1;
			fds[1] = fdsA1[1];
			if (!(g->a.ps = xstem_create_pipe(fds, stemmer, 0))) {
				perror("xstem_create_pipe");
				_exit(1);
			}
		}
		if (stem_n) {
			fds[0] = -1;
			fds[1] = fdsN1[1];
			if (!(g->n.ps = xstem_create_pipe(fds, NORMALIZER, 0))) {
				perror("xstem_create_pipe");
				_exit(1);
			}
		}
		if (!(n = nfopen(stdin))) {
			perror("nfopen");
			_exit(1);
		}
		for (lineno = 1; ; lineno++) {
			char *buf;
			if (!(buf = readln(n))) {
				fprintf(stderr, "readln failed at %"PRIuSIZE_T"\n", (pri_size_t)lineno);
				_exit(1);
			}
			if (!*buf) {
				break;	/* EOF */
			}
			/* note: thru CRLF, BOM */
			fputs(buf, relay_in);
			if (*buf == 'b' && g->a.ps) {
				fflush(relay_in);
				if (xstem_send(g->a.ps, buf + 2) == -1) {
					fprintf(stderr, "xstem_send failed\n");
					_exit(1);
				}
			}
			if (*buf == '!' && g->n.ps) {
				fflush(relay_in);
				if (xstem_send(g->n.ps, buf + 1) == -1) {
					fprintf(stderr, "xstem_send failed\n");
					_exit(1);
				}
			}
		}
		/* close(fds2[1]); */
		fclose(relay_in);
		if (g->a.ps) {
			xstem_destroy(g->a.ps);
			g->a.ps = NULL;
		}
		if (g->n.ps) {
			xstem_destroy(g->n.ps);
			g->n.ps = NULL;
		}
		_exit(0);
	default:
fprintf(stderr, "%s: %d\n", progname, getpid());
		close(fds2[1]);
		dup2(fds2[0], 0);	/* relay_out => stdin */
		if (fds2[0] != 0) {
			close(fds2[0]);
		}
		if (stem_a) close(fdsA1[1]);
		if (stem_n) close(fdsN1[1]);
		if (stem_a) {
			fds[0] = fdsA0[0];
			fds[1] = -1;
			if (!(g->a.ps = xstem_create_pipe(fds, stemmer, 0))) {
				perror("xstem_create_pipe");
				return -1;
			}
		}
		if (stem_n) {
			fds[0] = fdsN0[0];
			fds[1] = -1;
			if (!(g->n.ps = xstem_create_pipe(fds, NORMALIZER, 0))) {
				perror("xstem_create_pipe");
				return -1;
			}
		}
		break;
	}
	return 0;
}

static int
cleanup_proc_body(g)
struct state *g;
{
	int status;
	char cmd[MAXPATHLEN];	/* XXX */
	if (g->a.ps) {
		xstem_destroy(g->a.ps);
		g->a.ps = NULL;
	}
	if (g->n.ps) {
		xstem_destroy(g->n.ps);
		g->n.ps = NULL;
	}
	if (waitpid(g->r.pid, &status, 0) == -1) {
		perror("waitpid");
		return -1;
	}
	snprintf(cmd, sizeof cmd, "%s.relay", progname);
	if (diag_stat(cmd, status) == -1) {
		return -1;
	}
if (g->a.pid !=-1) {
	if (waitpid(g->a.pid, &status, 0) == -1) {
		perror("waitpid");
		return -1;
	}
	snprintf(cmd, sizeof cmd, "%s.pstem A", progname);
	if (diag_stat(cmd, status) == -1) {
		return -1;
	}
}
if (g->n.pid != -1) {
	if (waitpid(g->n.pid, &status, 0) == -1) {
		perror("waitpid");
		return -1;
	}
	snprintf(cmd, sizeof cmd, "%s.pstem N", progname);
	if (diag_stat(cmd, status) == -1) {
		return -1;
	}
}
	return 0;
}

static int
proc_body(g, r, fp)
struct state *g;
char *r;
struct fs *fp;
{
	int btype;
	char *p, *q;
	assert(g->a.ps);
	if (!(p = xstem_recv(g->a.ps))) {
		fprintf(stderr, "xstem_recv failed\n");
		return 1;
	}
	btype = r[1] & 0xff;
	if (btype != '1') {
		fprintf(stderr, "invalid btype: '%c'\n", btype);
		return -1;
	}
	if (r[2] == '\0') {
		return 0;	/* ok. do nothing */
	}
/* fprintf(stderr, "."); */
	for (q = p; *p; p++) {
		if (*p == '\n') {
			*p = '\0';
			fputs("1 ", fp->stream);
			fputs(q, fp->stream);
			fputs("\n", fp->stream);
			q = p + 1;
		}
	}
	if (p != q) {
		fputs("1 ", fp->stream);
		fputs(q, fp->stream);
		fputs("\n", fp->stream);
	}
	return 0;
}

static int
proc_flwd(g, r, fp)
struct state *g;
char *r;
struct fs *fp;
{
	struct flwd_idx idx;
	size_t l;
	size_t pad;
	char *p;

	if (*r != '!') {
		fprintf(stderr, "internal error\n");
		return -1;
	}
	r++;
	assert(g->n.ps);
	if (!(p = xstem_recv(g->n.ps))) {
		fprintf(stderr, "xstem_recv failed\n");
		return -1;
	}
	r = p;
	l = strlen(r);
	if (l > 0 && r[l - 1] == '\n') l--;
	idx.id = 0;
	idx.segid = g->segid;
	if (fwrite(&idx, sizeof idx, 1, fp->stream) != 1) {
		return -1;
	}
	if (fwrite("", 1, 1, fp->stream) != 1) {
		return -1;
	}
	if (fwrite(r, 1, l, fp->stream) != l) {
		return -1;
	}
	if (fwrite("", 1, 1, fp->stream) != 1) {
		return -1;
	}
	pad = FWIDXALIGN - (sizeof idx + 1 + l + 1) % FWIDXALIGN;
	if (pad != FWIDXALIGN) {
		if (fwrite(FWPAD, 1, pad, fp->stream) != pad) {
			return -1;
		}
	}
	return 0;
}

static int
check_turn(fp)
struct fs *fp;
{
	if (ftell(fp->stream) > 512 * 1024 * 1024) {
		if (fclose(fp->stream) == EOF) {
			perror(fp->path);
			return -1;
		}
		fp->turn++;
		fprintf(stderr, "%s.#%s turned over\n", fp->base, fp->key);
		snprintf(fp->path, sizeof fp->path, "%s-%d.#%s", fp->base, fp->turn, fp->key);
		if (!(fp->stream = fopen(fp->path, "w"))) {
			perror(fp->path);
			return -1;
		}
	}
	return 0;
}

int
is_xname_subset(s)
char const *s;
{
	if (!(is_xletter_subset(*s&0xff) || *s == '_' || *s == ':')) {
		return 0;
	}
	for (s++; *s; s++) {
		if (!(is_xletter_subset(*s&0xff) || *s == '.' || *s == '-' || *s == '_' || *s == ':')) {
			return 0;
		}
	}
	return 1;
}

int
is_xletter_subset(c)
{
	return isalnum(c);
}
