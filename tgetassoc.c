/*	$Id: tgetassoc.c,v 1.53 2011/10/24 06:31:53 nis Exp $	*/

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
static char rcsid[] = "$Id: tgetassoc.c,v 1.53 2011/10/24 06:31:53 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include <expat.h>

#include <unicode/utf.h>
#include <unicode/utf8.h>

#include "nwam.h"
#include "util.h"
#include "pi.h"

#include "cxml.h"

#include "common.h"
#include "gorj.h"
#include "print.h"
#include "getassocP.h"

#include "fssP.h"
#include "nwamP.h"
#include "nrpc.h"
#include "xgserv.h"

#if defined VSTAT
#include "vmstat.h"
#endif
#if defined ISTAT
#include "istt.h"
#endif
#if defined NSTAT
#include "gstat.h"
#endif

#include <gifnoc.h>

#if ! defined NO_FORK
#define FRK_GSS3_LOOP	1
#define	DFLT_FRK_NCPU	1
#endif

#define	TEST_ASSV	0
#define	TEST_ASSV_VEC	1
#define	TEST_WWSH	2

#if ! defined LOG_PERROR
#define	LOG_PERROR	0
#endif

#if ! defined ENABLE_GETA
#define	sym_compar	n_sym_compar
#endif

struct cookie;

static void usage(void);
ssize_t readfn_init(struct cookie *);
static int readfn(void *, char *, int);
static int intrinsic_test(int, char *[]);
static int gss3_file(int, char *[]);
static int gss3_loop(int, char *[]);
#if defined FRK_GSS3_LOOP
static int waitone(void);
#endif
static int clustering_test(int, char *[]);
static void cluster_all(char const *, size_t, int);
static unsigned int ccshash(unsigned char const *, size_t, unsigned int);
static int sct_test(int, char *[]);
static int assv_test_drv(int, char *[]);
static void assv_test_gen(char const *, size_t, int, char const *, size_t, size_t);
static void assv_test_exec(char const *, int, int, size_t, int, unsigned int, int);
static int catalogue_test(int, char *[]);
static int json_test(int, char *[]);
static int cstem_test(int, char *[]);
#if ! defined FLWD_UTF32
static int xstrncmp_test(int, char *[]);
#endif

#if ! defined ENABLE_GETA
static int nrpc_test(int, char *[]);
static int compare_syminfo(struct syminfo *, size_t, struct syminfo *, size_t);
#endif
static int miscellaneous_test(int, char *[]);

char *progname = NULL;
char *getaroot; /* = GETAROOT; */

struct cmd {
	char *name;
	int (*fn)(int, char *[]);
	char *args;
};

static struct cmd cmds[] = {
	{"libnwam-test", intrinsic_test, "[-R root]"},
	{"gss3", gss3_file, "[-R root]"
#if defined ENABLE_NEWLAYOUT
		" [-r rcfile]"
#endif
		" [-h] [-n #] path"},
	{"gss3-loop", gss3_loop, "[-R root]"
#if defined ENABLE_NEWLAYOUT
		" [-r rcfile]"
#endif
		" [-h] [-l #loop]"
#if defined NSTAT
		" [-n (w/nstat)]"
#endif
#if defined FRK_GSS3_LOOP
		" [-c #cpu] [-d]"
#endif
		},
	{"assv-test-gen", assv_test_drv, "-g #queries [-R root] [-d WAM_COL|WAM_ROW] [-t weight-type] [-m maxsize (default: 31) | -s fixsize] handle"},
	{"assv-test-exec", assv_test_drv, "-a|-v|-w [-R root] [-n] [-r request_number (default: 128)] [-e allowerror] [-D] handle"},
	{"clustering-test", clustering_test, "[-R root] [-n #articles] [-v] handle"},
	{"sct-test", sct_test, "[-R root]"},
	{"catalogue-test", catalogue_test, "[-R root]"},
	{"json-test", json_test, NULL},
	{"cstem-test", cstem_test, "[-R root]"},
#if ! defined FLWD_UTF32
	{"xstrncmp-test", xstrncmp_test, "[-R root]"},
#endif
#if ! defined ENABLE_GETA
	{"nrpc-test", nrpc_test, "[-R root]"},
#endif
	{"test", miscellaneous_test, "[-R root]"},
};

static void
usage()
{
	size_t i;
	fprintf(stderr, "usage: %s command [arg...]\n", progname);
	for (i = 0; i < nelems(cmds); i++) {
		if (cmds[i].args) {
			fprintf(stderr, "\t%s %s\n", cmds[i].name, cmds[i].args);
		}
		else {
			fprintf(stderr, "\t%s\n", cmds[i].name);
		}
	}
	exit(1);
}

int
main(argc, argv)
char *argv[];
{
	int e = 0;
	size_t i;
	progname = basename(argv[0]);

#if defined HAVE_WSASTARTUP
	if (wsastartup() == -1) {
		perror("wsastartup");
		return 1;
	}
#endif

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

#if defined STATIC_GETAROOT
	getaroot = STATIC_GETAROOT;
#elif defined ENV_GETAROOT
	getaroot = getenv("GETAROOT");
#else
	getaroot = NULL;
#endif
/*syslog(LOG_DEBUG, "getaroot = [%s]", getaroot);*/
	if (argc < 2) {
		usage();
	}
	argc--, argv++;
	for (i = 0; i < nelems(cmds); i++) {
		if (!strcmp(cmds[i].name, argv[0])) {
			e = cmds[i].fn(argc, argv);
			break;
		}
	}
	if (i == nelems(cmds)) {
		usage();
	}
	PRINT_ELAPSED("elapsed", stdout);
	return e;
}

#if defined FRK_GSS3_LOOP
struct cookie {
	FILE *stream;
	char buf[1024 * 1024], *p, *e;
};

ssize_t
readfn_init(c)
struct cookie *c;
{
	int j, n;
	size_t rest, len;
	size_t size;
	char *p, *ep;
	size = 0;
	if (!fgets(c->buf, sizeof c->buf, c->stream)) {
		return 0;
	}
	if (p = index(c->buf, '\n')) *p = '\0';
	n = strtoul(c->buf, &ep, 10);
	if (!(*c->buf && !*ep)) {
		fprintf(stderr, "malformed input: not a number: [%s]\n", c->buf);
		return -1;
	}
	rest = sizeof c->buf;
	c->p = c->e = c->buf;
	*c->e = '\0';
	for (j = 0; j < n; j++) {
		if (rest == 1) {
			fprintf(stderr, "too large segment\n");
			return -1;
		}
		if (!fgets(c->e, rest, c->stream)) {
			fprintf(stderr, "unexpected EOF\n");
			return -1;
		}
		len = strlen(c->e);
		rest -= len;
		c->e += len;
		size += len;
	}
	return size;
}

static int
readfn(d, buf, nbytes)
void *d;
char *buf;
{
	struct cookie *c;
	int len, rest;

	c = d;

	rest = c->e - c->p;
	len = MIN(nbytes, rest);

	memmove(buf, c->p, len);
	c->p += len;

	return len;
}
#else
struct cookie {
	FILE *stream;
	int n;
	char buf[65536], *p, *e;
};

ssize_t
readfn_init(c)
struct cookie *c;
{
	char *p, *ep;
	if (!fgets(c->buf, sizeof c->buf, c->stream)) {
		return 0;
	}
	if (p = index(c->buf, '\n')) *p = '\0';
	c->n = strtoul(c->buf, &ep, 10);
	if (!(*c->buf && !*ep)) {
		fprintf(stderr, "malformed input: not a number: [%s]\n", c->buf);
		return -1;
	}
	c->p = c->e = NULL;
	return 0x80000;
}

static int
readfn(d, buf, nbytes)
void *d;
char *buf;
{
	struct cookie *c;

	c = d;
readfn:
	if (c->p < c->e) {
		size_t l;
		l = MIN(nbytes, c->e - c->p);
		memmove(buf, c->p, l);
		c->p += l;
		return l;
	}

	if (c->n == 0) {
		return 0;
	}
	if (!fgets(c->buf, sizeof c->buf, c->stream)) {
		return 0;
	}
	if (!index(c->buf, '\n')) {
		fprintf(stderr, "too long line\n");
		exit(1);
	}
	c->p = c->buf;
	c->e = c->buf + strlen(c->buf);
	c->n--;
	goto readfn;
}
#endif

static int
intrinsic_test(argc, argv)
char *argv[];
{
	char const *handle = "mai2001";
	static char *names[2] = { "\357\274\220\357\274\220\357\274\223\357\274\225\357\274\221\357\274\223\357\274\230\357\274\220",  "\345\256\210"}; /* ００３５１３８０守 */ 
	static char *dn[2] = { "WAM_ROW", "WAM_COL"};

	WAM *w;
	int wt;
	static char const *weight_type_names[] = WEIGHT_TYPE_NAMES;
	struct xr_vec const *v;
	int d;

	char const *nm;
	idx_t id;

	df_t df;
	df_t sz;
	freq_t tf;

	int ch;

	openlog("libnwam_test", LOG_PID|LOG_PERROR, LOG_LOCAL0);

	while ((ch = getopt(argc, argv, "R:")) != -1) {
		switch (ch) {
		case 'R':
			getaroot = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0) {
		usage();
	}

	wam_init(getaroot);

	if (!(w = wam_open(handle, NULL))) {
		perror(handle);
		return 1;
	}

	for (wt = 0; wt < nelems(weight_type_names); wt++) {
		fprintf(stdout, "%s = %d\n", weight_type_names[wt], wt);
	}
	for (d = 0; d < 2; d++) {

		id = 1;
		names[d] = strdup(wam_id2name(w, d, id));
		nm = names[d];

		id = wam_name2id(w, d, nm);
		fprintf(stdout, "id(%s, \"%s\") = %"PRIuIDX_T"\n", dn[d], nm, id);
		nm = wam_id2name(w, d, id);
		fprintf(stdout, "name(%s, %"PRIuIDX_T") = \"%s\"\n", dn[d], id, nm);

		sz = wam_size(w, d);
		fprintf(stdout, "size(%s) = %"PRIdDF_T"\n", dn[d], sz);

		df = wam_elem_num(w, d, id);
		fprintf(stdout, "elem_um(%s, %"PRIuIDX_T") = %"PRIdDF_T"\n", dn[d], id, df);

		tf = wam_freq_sum(w, d, id);
		fprintf(stdout, "freq_sum(%s, %"PRIuIDX_T") = %"PRIdFREQ_T"\n", dn[d], id, tf);

		tf = wam_total_elem_num(w, d);
		fprintf(stdout, "total_elem_num(%s) = %"PRIdFREQ_T"\n", dn[d], tf);

		tf = wam_total_freq_sum(w, d);
		fprintf(stdout, "total_freq_sum(%s) = %"PRIdFREQ_T"\n", dn[d], tf);

		if (wam_get_vec(w, d, id, &v) != -1) {
			print_xr_vec(v, 8, w, WAM_REVERT_DIRECTION(d), stdout);
		}
		else {
			printf("vec = NULL\n");
		}
	}

	wam_close(w);
	return 0;
}

static int
gss3_file(argc, argv)
char *argv[];
{
	char const *path;
	size_t size;
	FILE *f;
	size_t n = 1, i;
	int ch;
	int flags = 0;
#if defined ENABLE_NEWLAYOUT
	char *rcfile = NULL;
#endif

	openlog("gss3_file", LOG_PID|LOG_PERROR, LOG_LOCAL0);

	while ((ch = getopt(argc, argv, "hn:R:"
#if defined ENABLE_NEWLAYOUT
			"r:"
#endif
		)) != -1) {
		switch (ch) {
			char *end;
		case 'h':
			flags |= GSS3FN_HDR;
			break;
		case 'n':
			n = strtoul(optarg, &end, 10);
			if (!(*optarg && !*end)) {
				fprintf(stderr, "invalued number: %s\n", optarg);
				return 1;
			}
			break;
		case 'R':
			getaroot = optarg;
			break;
#if defined ENABLE_NEWLAYOUT
		case 'r':
			rcfile = optarg;
			break;
#endif
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		usage();
	}

	path = argv[0];
	if (!strcmp(path, "-")) {
		if (n != 1) {
			fprintf(stderr, "-n and - are mutually exclusive\n");
			return 1;
		}
		f = stdin;
		size = 0x80000;
	}
	else {
		struct stat sb;
		if (stat(path, &sb) == -1) {
			perror(path);
			return 1;
		}
		size = sb.st_size;
		if (!(f = fopen(path, "r"))) {
			perror(path);
			return 1;
		}
	}
#if defined ENABLE_NEWLAYOUT
	if (rcfile) {
		gss3_init(getaroot, rcfile);
	}
#endif
	for (i = 0; i < n; i++) {
		gss3fl(f, path, size, 1, flags);
		if (f != stdin) {
			rewind(f);
		}
	}
	fclose(f);
#if defined ENABLE_MDBG
	mdbg_status(stderr, 1);
#endif
	return 0;
}

static int
gss3_loop(argc, argv)
char *argv[];
{
	int flags = 0;
	int dflag = 0;
	struct cookie *c;
	size_t cnt, loop_limit = 0;
	char *end_optarg;
#if defined FRK_GSS3_LOOP
	pid_t pid;
	int active = 0;
	int frk_ncpu = DFLT_FRK_NCPU;
#endif
#if defined VSTAT
	struct vmst *v;
	struct iost *i;
#endif
#if defined ISTAT
	struct istt *t;
#endif
#if defined VSTAT || defined ISTAT
	struct timer *tm;
#endif
#if defined NSTAT
	int nstat_nbndry = 0;
#endif
#if defined ENABLE_NEWLAYOUT
	char *rcfile = NULL;
#endif

#if defined VSTAT || defined ISTAT
	tm = timer_new();
#endif

	int ch;
	while ((ch = getopt(argc, argv, "hl:"
#if defined NSTAT
			"n"
#endif
#if defined FRK_GSS3_LOOP
			"c:d"
#endif
			"R:"
#if defined ENABLE_NEWLAYOUT
			"r:"
#endif
		)) != -1) {
		switch (ch) {
		case 'h':
			flags |= GSS3FN_HDR;
			break;
		case 'l':
			loop_limit = strtol(optarg, &end_optarg, 10);
			if (!(*optarg && !*end_optarg)) {
				fprintf(stderr, "%s: invalid number\n", optarg);
				usage();
			}
			break;
#if defined NSTAT
		case 'n':
			/* nstat_init("nstat.out", NULL); */
			nstat_init("136.187.115.119", "nstatd");
			break;
#endif
#if defined FRK_GSS3_LOOP
		case 'c':
			frk_ncpu = strtol(optarg, &end_optarg, 10);
			if (!(*optarg && !*end_optarg)) {
				fprintf(stderr, "%s: invalid number\n", optarg);
				usage();
			}
			break;
		case 'd':
			dflag = 1;
			break;
#endif
		case 'R':
			getaroot = optarg;
			break;
#if defined ENABLE_NEWLAYOUT
		case 'r':
			rcfile = optarg;
			break;
#endif
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0) {
		usage();
	}

	if (!(c = calloc(1, sizeof *c))) {
		perror("calloc");
		return 1;
	}

	c->stream = stdin;

#if defined VSTAT
	fprintf(stderr, "      time ");
	v = vinit(stderr);
	fprintf(stderr, " ");
	i = iinit(stderr);
	fprintf(stderr, "\n");
#define	vstat() do { \
	timer_print_elapsed(tm, NULL, stderr); \
	dovmstat(v, stderr); \
	fprintf(stderr, " "); \
	dsget(i, stderr); \
	fprintf(stderr, "\n"); \
} while (0)
#endif
#if defined ISTAT
	fprintf(stderr, "      time ");
	t = istt_init(stderr);
	fprintf(stderr, "\n");
#define	istat() do { \
	timer_print_elapsed(tm, NULL, stderr); \
	istt_show(t, stderr); \
	fprintf(stderr, "\n"); \
} while (0)
#endif

#if defined VSTAT || defined ISTAT
	timer_init(tm);
#endif

#if defined VSTAT
	vstat();
#endif
#if defined ISTAT
	istat();
#endif
#if defined ENABLE_NEWLAYOUT
	if (rcfile) {
		gss3_init(getaroot, rcfile);
	}
#endif
	for (cnt = 0; loop_limit == 0 || cnt < loop_limit; cnt++) {
		ssize_t size;
		if (cnt % 100 == 0) fprintf(stderr, ".");
		size = readfn_init(c);
		if (size == -1) {
			return 1;
		}
		if (size == 0) {
			fprintf(stderr, "EOF\n");
			break;
		}
#if defined FRK_GSS3_LOOP
		if (active >= frk_ncpu) {
			if (waitone() == -1) {
				fprintf(stderr, "waitone failed\n");
				return 1;
			}
			active--;
#if defined VSTAT
			vstat();
#endif
#if defined ISTAT
			istat();
#endif
		}
		switch (pid = fork()) {
		case -1:
			perror("fork");
			return 1;
		case 0:
#if defined NSTAT
			nstat(0, 0, 0, 0);
#endif
			if (!dflag) {
				gss3fn(c, readfn, "-", size, 1, flags);
			}
			printf("\n");
			fflush(stdout);
			_exit(0);
			/* NOTREACHED */
		default:
			active++;
#if defined NSTAT
			nstat_nbndry++;
#endif
			break;
		}
#else
#if defined NSTAT
		nstat(0, 0, 0, 0);
		nstat_nbndry++;
#endif
		gss3fn(c, readfn, "-", size, 1, flags);
		printf("\n");
#endif
#if ! defined FRK_GSS3_LOOP
#if VSTAT
		vstat();
#endif
#if defined ISTAT
		istat();
#endif
#endif
	}
#if defined FRK_GSS3_LOOP
	for (; active > 0; ) {
		if (waitone() == -1) {
			fprintf(stderr, "waitone failed\n");
			return 1;
		}
		active--;
#if defined VSTAT
		vstat();
#endif
#if defined ISTAT
		istat();
#endif
	}
#endif
#if defined ENABLE_MDBG
	mdbg_status(stderr, 1);
#endif
#if defined NSTAT
	fprintf(stderr, "nstat_nbndry: %d\n", nstat_nbndry);
#endif
#if defined VSTAT || defined ISTAT
	free(tm);
#endif
	PRINT_ELAPSED(NULL, stderr);
	/*fprintf(stderr, "\n>>>> %f (Trans./s)\n", cnt / (timers.elapsed.tv_sec + timers.elapsed.tv_usec / 1000000.0));*/
	return 0;
#undef vstat
}

#if defined FRK_GSS3_LOOP
static int
waitone()
{
	pid_t pid;
	int status;
waitone:
	if ((pid = wait(&status)) == -1) {
		if (errno == EINTR) {
			perror("EINTR");
			goto waitone;
		}
		else {
			perror("wait");
			return -1;
		}
	}
	if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
		fprintf(stderr, "%d aborted\n", pid);
		return -1;
	}
	return 0;
}
#endif

static int
clustering_test(argc, argv)
char *argv[];
{
	char const *handle;
	int vflag = 0;
	size_t narts = 0;
	int ch;

	while ((ch = getopt(argc, argv, "n:vR:")) != -1) {
		switch (ch) {
		case 'n':
			narts = strtoul(optarg, NULL, 10);
			break;
		case 'v':
			vflag = 1;
			break;
		case 'R':
			getaroot = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		usage();
	}

	handle = argv[0];
	cluster_all(handle, narts, vflag);
	return 0;
}

extern int cs_use_new_ism;

static void
cluster_all(handle, narts, verbose)
char const *handle;
size_t narts;
{
	struct timer *t;
	int dir;
	df_t i, j;
	struct syminfo *rw;
#if ! defined ENABLE_GETA
	df_t nr, nc, th, ckkn;
#else
	df_t nr, th, ckkn;
	size_t nc;
#endif
	struct cs_elem *cslst;
	df_t elemn;
	int bias, cs_type, wt_type;
	WAM *w;

	t = timer_new();

	wam_init(getaroot);
	if (!(w = wam_open(handle, NULL))) {
		perror(handle);
		return;
	}

	dir = WAM_ROW;

	nr = wam_size(w, dir);
	if (narts != 0) {
		nr = MIN(nr, narts);
	}
	/* cs_use_new_ism = 0; */
	fprintf(stdout, "%"PRIdDF_T"\n", nr);

	if (!(rw = calloc(nr, sizeof *rw))) {
		perror("calloc (rw)");
		return;
	}
	for (i = j = 0; i < nr; i++) {
		idx_t id;
		df_t DF;
		id = i + 1;
		DF = wam_elem_num(w, dir, id);
		if (DF != 0) {
			struct syminfo *r = &rw[j++];
			r->id = id;
			r->TF = wam_freq_sum(w, dir, id);
			r->DF = DF;
			r->TF_d = 1;
			r->DF_d = 1;
			r->weight = 1;
			r->v = NULL;
#if defined ENABLE_GETA
			r->attr = WSH_OR;
#endif
		}
	}
	if (j != nr) {
		fprintf(stdout, "%"PRIdDF_T" => %"PRIdDF_T"\n", nr, j);
		nr = j;
	}

#if 0
#define CS_HBC 1    /* hbc */
#define CS_WARD 2   /* Ward's method */
#define CS_GAVE 3   /* group average link */
#define CS_SLINK 4  /* single link */
#define CS_CLINK 5  /* complete link */
#endif

	bias = 0;	/* バイアス情報, 不明なら 0 */
	cs_type = CS_HBC;
	wt_type = WT_SMART;
wt_type = WT_SMARTAW;
	elemn = 256;
	nc = 3;	/* 要求クラスタ数 */
	th = 1;	/* クラスタの最小要素数 */
	ckkn = 128;	/* クラスタ内単語リスト最大長 */

	fprintf(stdout, "start clustering: %"PRIdDF_T"\n", nr);
#if ! defined ENABLE_GETA
	cslst = ncsb(rw, nr, cs_type, w, dir, wt_type, elemn, &nc, wt_type, ckkn);
#else
	cslst = csb(rw, nr, bias, w, dir, cs_type, wt_type, elemn, &nc, th, ckkn, NULL);
#endif
	timer_print_elapsed(t, "end", stdout);

#if ! defined ENABLE_GETA
	fprintf(stdout, "nc = %"PRIdDF_T"\n", nc);
#else
	fprintf(stdout, "nc = %"PRIuSIZE_T"\n", (pri_size_t)nc);
#endif
	for (i = 0; i < nc; i++) {
		unsigned int sum;
		struct cs_elem *e = &cslst[i];
		fprintf(stdout, "cls: %"PRIdDF_T"\n", i);
#if ! defined ENABLE_GETA
		fprintf(stdout, "csa.n = %"PRIdDF_T"\n", e->csa.n);
#else
		fprintf(stdout, "csalen = %"PRIuSIZE_T"\n", (pri_size_t)e->csalen);
#endif

		sum = 0;
		for (j = 0; j < e->csa.n; j++) {
			char s[12];
			idx_t id = e->csa.s[j].id;
			snprintf(s, sizeof s, "%"PRIuIDX_T" ", id);
			if (verbose) {
				fprintf(stdout, "%s", s);
			}
			sum = ccshash((unsigned char *)s, strlen(s), sum);
		}

		fprintf(stdout, "checksum: %08x\n", sum);
#if ! defined ENABLE_GETA
		fprintf(stdout, "csa.n = %"PRIdDF_T"\n", e->csa.n);
#else
		fprintf(stdout, "csalen = %"PRIuSIZE_T"\n", (pri_size_t)e->csalen);
#endif
		for (j = 0; j < e->csa.n; j++) {
			idx_t id = e->csa.s[j].id;
			fprintf(stdout, "%"PRIuIDX_T" ", id);
		}
		fprintf(stdout, "\n");
#if 1
#if ! defined ENABLE_GETA
		fprintf(stdout, "csw.n = %"PRIdDF_T"\n", e->csw.n);
#else
		fprintf(stdout, "cswlen = %"PRIuSIZE_T"\n", (pri_size_t)e->cswlen);
#endif
		for (j = 0; j < e->csw.n; j++) {
			idx_t id = e->csw.s[j].id;
			fprintf(stdout, "%"PRIuIDX_T" ", id);
		}
		fprintf(stdout, "\n");
#endif
	}
	free(t);
}

static unsigned int table[256] = 
{ /* 5744ee9b/d97722ea */
 0x00000000, 0xde0e75b8, 0x0ef2aea5, 0xd0fcdb1d, 0x1de55d4a, 0xc3eb28f2, 
 0x1317f3ef, 0xcd198657, 0x3bcaba94, 0xe5c4cf2c, 0x35381431, 0xeb366189, 
 0x262fe7de, 0xf8219266, 0x28dd497b, 0xf6d33cc3, 0x77957528, 0xa99b0090, 
 0x7967db8d, 0xa769ae35, 0x6a702862, 0xb47e5dda, 0x648286c7, 0xba8cf37f, 
 0x4c5fcfbc, 0x9251ba04, 0x42ad6119, 0x9ca314a1, 0x51ba92f6, 0x8fb4e74e, 
 0x5f483c53, 0x814649eb, 0xef2aea50, 0x31249fe8, 0xe1d844f5, 0x3fd6314d, 
 0xf2cfb71a, 0x2cc1c2a2, 0xfc3d19bf, 0x22336c07, 0xd4e050c4, 0x0aee257c, 
 0xda12fe61, 0x041c8bd9, 0xc9050d8e, 0x170b7836, 0xc7f7a32b, 0x19f9d693, 
 0x98bf9f78, 0x46b1eac0, 0x964d31dd, 0x48434465, 0x855ac232, 0x5b54b78a, 
 0x8ba86c97, 0x55a6192f, 0xa37525ec, 0x7d7b5054, 0xad878b49, 0x7389fef1, 
 0xbe9078a6, 0x609e0d1e, 0xb062d603, 0x6e6ca3bb, 0x6cbb9175, 0xb2b5e4cd, 
 0x62493fd0, 0xbc474a68, 0x715ecc3f, 0xaf50b987, 0x7fac629a, 0xa1a21722, 
 0x57712be1, 0x897f5e59, 0x59838544, 0x878df0fc, 0x4a9476ab, 0x949a0313, 
 0x4466d80e, 0x9a68adb6, 0x1b2ee45d, 0xc52091e5, 0x15dc4af8, 0xcbd23f40, 
 0x06cbb917, 0xd8c5ccaf, 0x083917b2, 0xd637620a, 0x20e45ec9, 0xfeea2b71, 
 0x2e16f06c, 0xf01885d4, 0x3d010383, 0xe30f763b, 0x33f3ad26, 0xedfdd89e, 
 0x83917b25, 0x5d9f0e9d, 0x8d63d580, 0x536da038, 0x9e74266f, 0x407a53d7, 
 0x908688ca, 0x4e88fd72, 0xb85bc1b1, 0x6655b409, 0xb6a96f14, 0x68a71aac, 
 0xa5be9cfb, 0x7bb0e943, 0xab4c325e, 0x754247e6, 0xf4040e0d, 0x2a0a7bb5, 
 0xfaf6a0a8, 0x24f8d510, 0xe9e15347, 0x37ef26ff, 0xe713fde2, 0x391d885a, 
 0xcfceb499, 0x11c0c121, 0xc13c1a3c, 0x1f326f84, 0xd22be9d3, 0x0c259c6b, 
 0xdcd94776, 0x02d732ce, 0xd97722ea, 0x07795752, 0xd7858c4f, 0x098bf9f7, 
 0xc4927fa0, 0x1a9c0a18, 0xca60d105, 0x146ea4bd, 0xe2bd987e, 0x3cb3edc6, 
 0xec4f36db, 0x32414363, 0xff58c534, 0x2156b08c, 0xf1aa6b91, 0x2fa41e29, 
 0xaee257c2, 0x70ec227a, 0xa010f967, 0x7e1e8cdf, 0xb3070a88, 0x6d097f30, 
 0xbdf5a42d, 0x63fbd195, 0x9528ed56, 0x4b2698ee, 0x9bda43f3, 0x45d4364b, 
 0x88cdb01c, 0x56c3c5a4, 0x863f1eb9, 0x58316b01, 0x365dc8ba, 0xe853bd02, 
 0x38af661f, 0xe6a113a7, 0x2bb895f0, 0xf5b6e048, 0x254a3b55, 0xfb444eed, 
 0x0d97722e, 0xd3990796, 0x0365dc8b, 0xdd6ba933, 0x10722f64, 0xce7c5adc, 
 0x1e8081c1, 0xc08ef479, 0x41c8bd92, 0x9fc6c82a, 0x4f3a1337, 0x9134668f, 
 0x5c2de0d8, 0x82239560, 0x52df4e7d, 0x8cd13bc5, 0x7a020706, 0xa40c72be, 
 0x74f0a9a3, 0xaafedc1b, 0x67e75a4c, 0xb9e92ff4, 0x6915f4e9, 0xb71b8151, 
 0xb5ccb39f, 0x6bc2c627, 0xbb3e1d3a, 0x65306882, 0xa829eed5, 0x76279b6d, 
 0xa6db4070, 0x78d535c8, 0x8e06090b, 0x50087cb3, 0x80f4a7ae, 0x5efad216, 
 0x93e35441, 0x4ded21f9, 0x9d11fae4, 0x431f8f5c, 0xc259c6b7, 0x1c57b30f, 
 0xccab6812, 0x12a51daa, 0xdfbc9bfd, 0x01b2ee45, 0xd14e3558, 0x0f4040e0, 
 0xf9937c23, 0x279d099b, 0xf761d286, 0x296fa73e, 0xe4762169, 0x3a7854d1, 
 0xea848fcc, 0x348afa74, 0x5ae659cf, 0x84e82c77, 0x5414f76a, 0x8a1a82d2, 
 0x47030485, 0x990d713d, 0x49f1aa20, 0x97ffdf98, 0x612ce35b, 0xbf2296e3, 
 0x6fde4dfe, 0xb1d03846, 0x7cc9be11, 0xa2c7cba9, 0x723b10b4, 0xac35650c, 
 0x2d732ce7, 0xf37d595f, 0x23818242, 0xfd8ff7fa, 0x309671ad, 0xee980415, 
 0x3e64df08, 0xe06aaab0, 0x16b99673, 0xc8b7e3cb, 0x184b38d6, 0xc6454d6e, 
 0x0b5ccb39, 0xd552be81, 0x05ae659c, 0xdba01024, };

static unsigned int
ccshash(c, n, r)
unsigned char const *c;
size_t n;
unsigned int r;
{
#define	MASK	0xffffffff
	size_t i;

	r = ~r & MASK;
	for (i = 0; i < n; i++) {
		unsigned int d = c[i] & 0xff;
		unsigned int v;
		r ^= d;
		v = table[r & 0xff];
		r >>= 8;
		r ^= v;
	}
	return ~r & MASK;
#undef MASK
}

static int
sct_test(argc, argv)
char *argv[];
{
	char *path_info = "/sct/lq";
	int ac = 0;
	char *av[128];
	int ch;

	while ((ch = getopt(argc, argv, "R:")) != -1) {
		switch (ch) {
		case 'R':
			getaroot = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0) {
		usage();
	}

	av[ac++] = strdup("text=451");
	av[ac++] = "i1.x=0";
	av[ac++] = "i1.y=0";
	av[ac++] = "ohandle=senya";
	av[ac++] = "nhandle=senya";
	av[ac++] = "dmax=20";
	av[ac++] = "qmax=15";
	av[ac++] = "ctype=0";
	av[ac] = NULL;
	sct(path_info, ac, av, 1);
	return 0;
}

static int
assv_test_drv(argc, argv)
char *argv[];
{
	int ch;
	char *handle;
	int gflag = 0;
	size_t gnum = 0;
	int type = TEST_ASSV;
	int need_result = 0;
	char const *wtn = "WT_SMARTWA";
	int qdir = WAM_COL;
	size_t maxsize = 0;
	size_t fixsize = 0;
	size_t request_number = 128;
	int eflag = 0;
	int Dflag = 0;
	unsigned int allowerror = 0;

	while ((ch = getopt(argc, argv, "m:s:d:g:avwnt:r:D"
#if ! defined ENABLE_GETA
		"e:"
#endif
		"R:"
		)) != -1) {
		switch (ch) {
		case 'm':
			maxsize = strtoul(optarg, NULL, 10);
			break;
		case 's':
			fixsize = strtoul(optarg, NULL, 10);
			break;
		case 'g':
			gflag = 1;
			gnum = strtoul(optarg, NULL, 10);
			break;
#if ! defined ENABLE_GETA
		case 'e':
			eflag = 1;
			allowerror = strtoul(optarg, NULL, 10);
			break;
#endif
		case 'D':
			Dflag = 1;
			break;
		case 'a':
			type = TEST_ASSV;
			break;
		case 'v':
			type = TEST_ASSV_VEC;
			break;
		case 'w':
			type = TEST_WWSH;
			break;
		case 'n':
			need_result = 1;
			break;
		case 't':
			wtn = optarg;
			break;
		case 'r':
			request_number = strtoul(optarg, NULL, 10);
			break;
		case 'd':
			if (!strcmp(optarg, "WAM_COL")) qdir = WAM_COL;
			else if (!strcmp(optarg, "WAM_ROW")) qdir = WAM_ROW;
			else {
				fprintf(stderr, "WAM_COL or WAM_ROW expected");
				usage();
			}
			break;
		case 'R':
			getaroot = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		fprintf(stderr, "argc != 1\n");
		usage();
	}

	if (maxsize && fixsize) {
		usage();
	}
	if (maxsize == 0 && fixsize == 0) {
		maxsize = 31;
	}

	handle = argv[0];
	if (gflag) {
		assv_test_gen(handle, gnum, qdir, wtn, maxsize, fixsize);
	}
	else {
		assv_test_exec(handle, type, need_result, request_number, eflag, allowerror, Dflag);
	}
	return 0;
}

static void
assv_test_gen(handle, gnum, qdir, wtn, maxsize, fixsize)
char const *handle;
size_t gnum;
char const *wtn;
size_t maxsize, fixsize;
{
	size_t i, j;
	WAM *w;
	df_t qsz;
	static char const *weight_type_names[] = WEIGHT_TYPE_NAMES;
	int wt;

	/* check weight type name. */
	for (wt = 0; wt < nelems(weight_type_names); wt++) {
		if (!strcmp(wtn, weight_type_names[wt])) break;
	}
	if (wt == nelems(weight_type_names)) {
		fprintf(stderr, "%s: undefined weight type\n", wtn);
		return;
	}

	wam_init(getaroot);
	if (!(w = wam_open(handle, NULL))) {
		perror(handle);
		return;
	}
	qsz = wam_size(w, qdir);

	for (i = 0; i < gnum; i++) {
		size_t n;
		if (maxsize) {
			n = random() % maxsize + 1;
		}
		else {
			n = fixsize;
		}
		fprintf(stdout, "%s\n", wtn);
		fprintf(stdout, "%s\n", qdir == WAM_COL ? "WAM_COL" : "WAM_ROW");
		fprintf(stdout, "%"PRIuSIZE_T"\n", (pri_size_t)n);
		for (j = 0; j < n; j++) {
			idx_t k;
			char const *name;
			k = random() % qsz + 1;
			if (!(name = wam_id2name(w, qdir, k))) {
				fprintf(stderr, "%"PRIuIDX_T": name2id failed\n", k);
				return;
			}
			fprintf(stdout, "%s\n", name);
		}
	}
	wam_close(w);
}

static size_t safe_request_num(size_t, double, int, size_t);

static void
assv_test_exec(handle, type, need_result, request_number, eflag, allowerror, Dflag)
char const *handle;
size_t request_number;
unsigned int allowerror;
{
size_t nq0 = 0, nr0 = 0;
#if defined DBG_DIST
size_t slen;
size_t slen_total = 0, ns = 0;
size_t maxslen = 0;
df_t j;
#endif
	int d;
	size_t merror = 0, count = 0, false_positive = 0, error = 0;
size_t short_results = 0;
	struct timer *t;
	WAM *w;
	int wt;
	static char const *weight_type_names[] = WEIGHT_TYPE_NAMES;
	char wtn[128], buf[8192];
	int one = 1;
	size_t nn[2];
	struct diststat *dists[2];

/*
	openlog("assv_test_exec", LOG_PID|LOG_PERROR, LOG_LOCAL0);
*/
	t = timer_new();

	wam_init(getaroot);
	if (!(w = wam_open(handle, NULL))) {
		perror(handle);
		return;
	}

#if ! defined ENABLE_GETA
	if (eflag) {
		if (wam_setopt(w, WAM_SETOPT_ALLOWERROR, &allowerror) == -1) {
			perror("wam_setopt");
			return;
		}
	}
	if (Dflag) {
		wam_setopt(w, WAM_SETOPT_NEED_DISTSTAT, &one);
		d = 0;
		if (wam_getopt(w, WAM_GETOPT_DISTSTAT_NN_ROW, &nn[d]) == -1) {
			fprintf(stderr, "wam_getopt failed\n");
			return;
		}
		d = 1;
		if (wam_getopt(w, WAM_GETOPT_DISTSTAT_NN_COL, &nn[d]) == -1) {
			fprintf(stderr, "wam_getopt failed\n");
			return;
		}
		for (d = 0; d < 2; d++) {
			if (!(dists[d] = calloc(nn[d], sizeof *dists[d]))) {
				fprintf(stderr, "!calloc\n");
				return;
			}
		}
	}
#endif

	for (; fgets(wtn, sizeof wtn, stdin); count++) {
		struct syminfo *q, *r;
#if ! defined ENABLE_GETA
		struct syminfo *r_save;
#endif
		df_t nq, nr;
#if ! defined ENABLE_GETA
		df_t nr_save;
		df_t total;
		df_t total_save;
#else
		df_t r0total;
		ssize_t total;
#endif
		df_t i;
		df_t qsz;
		int qdir, rdir;
		char *p, *e;
#if ! defined ENABLE_GETA
		int errorlevel;
		unsigned int allowerror_save;
#endif
if (count % 1000 == 0) fprintf(stderr, ".");

		if (p = index(wtn, '\n')) *p = '\0';
		for (wt = 0; wt < nelems(weight_type_names); wt++) {
			if (!strcmp(wtn, weight_type_names[wt])) break;
		}
		if (wt == nelems(weight_type_names)) {
			fprintf(stderr, "[%s]: undefined weight type\n", wtn);
			break;
		}
/*fprintf(stderr, "%s: %d\n", wtn, wt);*/

		if (!fgets(buf, sizeof buf, stdin)) break;
		if (p = index(buf, '\n')) *p = '\0';
		if (!strcmp(buf, "WAM_COL")) qdir = WAM_COL, rdir = WAM_ROW;
		else if (!strcmp(buf, "WAM_ROW")) qdir = WAM_ROW, rdir = WAM_COL;
		else {
fprintf(stderr, "invalid dir: %s\n", buf);
			return;
		}

		qsz = wam_size(w, qdir);

		if (!fgets(buf, sizeof buf, stdin)) break;
		if (p = index(buf, '\n')) *p = '\0';

		r = NULL;

		nq = strtol(buf, &e, 10);
if (nq0 == 0) nq0 = nq;
if (nr0 == 0) nr0 = nr;
		nr = request_number;

		if (!(*buf && !*e)) {
			fprintf(stderr, "%s: not a number\n", buf);
			break;
		}

		if (!(q = calloc(nq, sizeof *q))) {
			perror("calloc (q)");
			return;
		}

		for (i = 0; i < nq; i++) {
			idx_t id;
			if (!fgets(buf, sizeof buf, stdin)) {
				fprintf(stderr, "unexpected EOF\n");
				goto end;
			}
			if (p = index(buf, '\n')) *p = '\0';
			if (!(id = wam_name2id(w, qdir, buf))) {
				fprintf(stderr, "%s: name2id failed\n", buf);
				goto end;
			}
			switch (type) {
				struct xr_vec const *v;
			case TEST_ASSV:
				q[i].id = id;
				q[i].v = NULL;
				break;
			case TEST_ASSV_VEC:
				wam_get_vec(w, qdir, id, &v);
				q[i].id = 0;
				q[i].v = dxr_dup(v);
				break;
			case TEST_WWSH:
				q[i].id = id;
				break;
			}
			q[i].TF = 0;
			q[i].TF_d = 1;
			q[i].weight = 1;
#if defined ENABLE_GETA
			q[i].attr = WSH_OR;
#endif
		}

#if ! defined ENABLE_GETA
		errorlevel = 0;
	restart:
#endif
		switch (type) {
#if defined ENABLE_GETA
			ssize_t tmp;
#endif
		case TEST_ASSV:
		case TEST_ASSV_VEC:
			if (need_result) {
				timer_init(t);
				fprintf(stdout, "start assv: %"PRIdDF_T" %"PRIdDF_T"\n", nq, nr);
			}
#if ! defined ENABLE_GETA
			r = assv(q, nq, w, qdir, wt, &nr, &total, NULL);
#else
			r = assv(q, nq, w, qdir, wt, &nr, &r0total, NULL);
			total = r0total;
#endif
			break;

		case TEST_WWSH:
			if (need_result) {
				timer_init(t);
				fprintf(stdout, "start wwsh: %"PRIdDF_T" %"PRIdDF_T"\n", nq, nr);
			}
#if ! defined ENABLE_GETA
			r = wsh(q, nq, w, qdir, wt, &nr, &total, NULL);
#else
			tmp = nr;
			r = wsh(q, nq, w, qdir, wt, &tmp, &total);
			nr = tmp;
#endif
			if (need_result) {
				qsort(r, nr, sizeof *r, sym_compar);
			}
			break;
		}
#if defined DBG_DIST

for (i = nr; 1 < i; ) {
	i--;
	if (r[i].weight != r[i - 1].weight) break;
}
j = i;
for (; 1 < i; ) {
	i--;
	if (r[i].weight != r[i - 1].weight) break;
}
slen = j - i;
if (slen) {
	slen_total += slen;
	if (maxslen < slen) maxslen = slen;
	ns++;
}

#endif
#if ! defined ENABLE_GETA

		if (errorlevel) {	/* oops! we restarted the process */
if (nr < request_number) {
	short_results++;
} else
			if (compare_syminfo(r, nr, r_save, nr_save) == 0) {
				false_positive++;
			}
			else {
				error++;
			}
			free(r);
#if defined DBG_DIST
fprintf(stderr, "nr>> %"PRIdDF_T" %"PRIdDF_T"\n", nr_save, nr);
fprintf(stderr, "total>> %"PRIdDF_T" %"PRIdDF_T"\n", total_save, total);
fprintf(stderr, "slen>> %"PRIuSIZE_T"\n", (pri_size_t)slen);
#endif
			wam_setopt(w, WAM_SETOPT_ALLOWERROR, &allowerror_save);
			r = r_save;
			nr = nr_save;
			total = total_save;
			/* errorlevel = 0; resume from restart */
		}
		else {
			int mi;
			if (wam_getopt(w, WAM_GETOPT_MAY_INCOMPLETE, &mi) == -1) {
				fprintf(stderr, "wam_getopt failed\n");
				return;
			}
			if (mi) {
				unsigned int u_zero = 0;
				merror++;
				errorlevel = 1;
				r_save = r;
				nr_save = nr;
				total_save = total;
#if defined DBG_DIST
fprintf(stderr, "!\n");
#if 0
fprintf(stderr, "Q>>");
print_syminfo(q, nq, 0, 0, w, qdir, stderr);
fprintf(stderr, "\n");
fprintf(stderr, "R>>");
print_syminfo(r, nr, total, 0, w, rdir, stderr);
fprintf(stderr, "\n");
#endif
if (Dflag) {
	struct diststat *dist;
	size_t ii;
	d = qdir == WAM_ROW ? 1 : 0;
	if (wam_getopt(w, WAM_GETOPT_DISTSTAT, &dist) == -1) {
		fprintf(stderr, "wam_getopt failed\n");
		return;
	}
}
#endif
				wam_getopt(w, WAM_GETOPT_ALLOWERROR, &allowerror_save);
				wam_setopt(w, WAM_SETOPT_ALLOWERROR, &u_zero);
				goto restart;
			}
		}

		if (Dflag) {
			struct diststat *dist;
			size_t ii;
			d = qdir == WAM_ROW ? 1 : 0;
			if (wam_getopt(w, WAM_GETOPT_DISTSTAT, &dist) == -1) {
				fprintf(stderr, "wam_getopt failed\n");
				return;
			}
			for (ii = 0; ii < nn[d]; ii++) {
				dists[d][ii].j += dist[ii].j;
				dists[d][ii].nd += dist[ii].nd;
				dists[d][ii].total += dist[ii].total;
			}
		}
#endif

		if (need_result) {
			timer_print_elapsed(t, "end", stdout);
			print_syminfo(r, nr, total, 8, w, rdir, stdout);
		}

		switch (type) {
		case TEST_ASSV_VEC:
			for (i = 0; i < nq; i++) {
				free(q[i].v);
			}
			break;
		}
		free(q);
		free(r);
	}
end:
#if ! defined ENABLE_GETA
	if (Dflag) {
		fprintf(stderr, "\n");
		for (d = 0; d < 2; d++) {
			size_t ii;
			for (ii = 0; ii < nn[d]; ii++) {
				fprintf(stderr, " (%"PRIdDF_T, dists[d][ii].j);
				fprintf(stderr, " %"PRIdDF_T, dists[d][ii].nd);
				fprintf(stderr, " %"PRIdDF_T")", dists[d][ii].total);
			}
			fprintf(stderr, "\n");
			free(dists[d]);
		}
	}
#endif
	free(t);
#if defined DBG_DIST
fprintf(stderr, "slen_total = %"PRIuSIZE_T"  maxslen = %"PRIuSIZE_T"  ns = %"PRIuSIZE_T"\n", (pri_size_t)slen_total, (pri_size_t)maxslen, (pri_size_t)ns);

#endif
	fprintf(stderr, "l = %"PRIuSIZE_T"  c = %"PRIuSIZE_T"  n = %"PRIuSIZE_T"  a = %d  N = %"PRIdDF_T"\n", (pri_size_t)nn[0], (pri_size_t)count, (pri_size_t)nr0, allowerror, wam_size(w, WAM_ROW));

	fprintf(stderr, "h = %s  x = %s\n", handle, XPART_TYPE);
	fprintf(stderr, "k = %"PRIuSIZE_T"\n", (pri_size_t)safe_request_num(nr0, 1.0 / nn[0], allowerror, nn[0]));
	count -= short_results;
	fprintf(stderr, "%"PRIuSIZE_T" (%"PRIuSIZE_T" - %"PRIuSIZE_T") / %"PRIuSIZE_T"  short_results = %"PRIuSIZE_T"\n", (pri_size_t)error, (pri_size_t)merror, (pri_size_t)false_positive, (pri_size_t)count, (pri_size_t)short_results);
	fprintf(stderr, "%e\n", (double)error / count);

	wam_close(w);
}

static int
catalogue_test(argc, argv)
char *argv[];
{
	char *path_info = "/catalogue.html";
	int ac = 0;
	char *av[128];
	int ch;

	while ((ch = getopt(argc, argv, "R:")) != -1) {
		switch (ch) {
		case 'R':
			getaroot = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0) {
		usage();
	}

	av[ac] = NULL;
	catalogue_html(path_info, ac, av, 1);
	return 0;
}

static int
json_test(argc, argv)
char *argv[];
{
	df_t i;
	char const *v;
	struct xr_vec *vec;
	char *handle;
	WAM *w;
	int ch;

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch (ch) {
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0) {
		usage();
	}

	handle = "hiro";
	handle = "hdhcx";

	v = "[[\"a\",3],[\"b\",5],[\"\\\"\",1],[\"a\\\\b\",8]]";
	v = "[[\"xyzzy\",3],[\"xyzzy\",5],[\"\\\"\",1],[\"a\\\\b\",8]]";

	wam_init(NULL);

	printf(">>> %s <<<\n", v);
	if (!(w = wam_open(handle, NULL))) {
		perror(handle);
		return 1;
	}

	vec = jsvec2vec(v, w, WAM_COL);

	printf("%"PRIdDF_T",%"PRIdFREQ_T", ", vec->elem_num, vec->freq_sum);
	for (i = 0; i < vec->elem_num; i++) {
		printf("[%"PRIuIDX_T",%"PRIdFREQ_T"]", vec->elems[i].id, vec->elems[i].freq);
	}
	printf("\n");

	printf("::: %s :::\n", vec2jsvec(w, WAM_COL, vec));	

	return 0;
}

static int
cstem_test(argc, argv)
char *argv[];
{
	char const *stemmer = NULL;
	char *text = "いまだ第二次世界大戦の戦火が激しいなか、サン=テグジュペリはコルシカ島から南仏グルノーブルおよびアヌシー方面の偵察飛行あるいは出撃に飛び立ったまま行方不明となり、そのまま大空の不帰の人となった。この年、ぼくが生まれた。てすと";
	char *ptr;
	char *handle = "mai2001";
	WAM *w;
	df_t i;
	struct xr_vec *v;
	int ch;

	while ((ch = getopt(argc, argv, "R:")) != -1) {
		switch (ch) {
		case 'R':
			getaroot = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0) {
		usage();
	}

	wam_init(getaroot);

	if (!stemmer) {
		stemmer = STEMMER_AUTO;
	}

	/*if (ptr = cstem_to_string(text, stemmer)) ??<*/
	if (ptr = normalize_text(text)) {
		printf("%s\n", ptr);
		free(ptr);
	}

	if (!(w = wam_open(handle, NULL))) {
		perror(handle);
		return 1;
	}

	if (!(v = text2vec(text, w, WAM_COL, stemmer))) {
		perror("text2vec");
		return 1;
	}

	for (i = 0; i < v->elem_num; i++) {
		printf("%"PRIuIDX_T" ", v->elems[i].id);
	}
	printf("\n");

	free(v);
	return 0;
}

struct pat {
	char *a, *b;
	size_t alen;
	char *options;
	int expected;
};
struct pat pats[] = {
	{"",		"",		0,	"",			0},
	{"a",		"",		0,	"",			0},
	{"a",		"",		1,	"",			1},
	{"",		"a",		0,	"",			0},
	{"",		"a",		1,	"",			-1},
	{"ab",		"ax",		1,	"",			0},
	{"ab",		"ax",		2,	"",			-1},
	{"ab",		"ax",		-1,	"",			-1},
	{"ax",		"ab",		2,	"",			1},
	{"ax",		"ab",		-1,	"",			1},
	{"aA",		"aa",		-1,	"",			-1},
	{"aA",		"aa",		-1,	"ignore-case",		0},
	{"ab",		"a b",		-1,	"ignore-space",		0},
	{"ab",		"a b",		-1,	"",			1},
	{"a b",		"ab",		-1,	"",			-1},
	{"a b",		"ab",		-1,	"ignore-space",		0},
	{"a  b",	"ab ",		-1,	"ignore-space",		0},
	{"a  b",	" ab ",		-1,	"ignore-space",		1},
	{" ab ",	" ab ",		-1,	"ignore-space",		0},
#if ! defined NO_UTF8_SOURCE
	{"ジ",		"ヂ",		3,	"",			-1},
	{"ジ",		"ヂ",		3,	"collapse-zdz",		0},
	{"ビルジング",	"ビルヂング",	-1,	"",			-1},
	{"ビルジング",	"ビルヂング",	-1,	"collapse-zdz",		0},
	{"ヴァイオリン", "バイオリン",	-1,	"",			1},
	{"バイオリン", "ヴァイオリン",	-1,	"collapse-bv",		0},
	{"バイオリソ", "ヴァイオリン",	-1,	"collapse-bv",		-1},
	{"ヴァイオリン", "バイオリン",	-1,	"collapse-bv",		0},
	{"ヴィオラ",	"ビオラ",	-1,	"",			1},
	{"ヴィオラ", 	"ビオラ",	-1,	"collapse-bv",		0},
	{"ァイオリン", "イオリン",	-1,	"collapse-bv",		-1},
	{"ィオラ", 	"オラ",		-1,	"collapse-bv",		-1},
	{"ベートーベン", "ベートーヴェン",	-1,	"",		-1},
	{"ベートーベン", "ベートーヴェン",	-1,	"collapse-bv",	0},
	{"ボーカリーズ", "ヴォーカリーズ",	-1,	"collapse-bv",	0},

	{"ボーカリーズ", "ヴォーカリーズ",	-1,	"collapse-bv",	0},
	{"ツィター", "ティター", 	-1, "collapse-tts", 0},
	{"チター", "ティター", 	-1, "collapse-tts", 0},
	{"ツィター", "チター", 	-1, "collapse-tts", 0},
	{"ディス", "ジス", 	-1, "collapse-tts", 0},
	{"ディス", "デス", 	-1, "collapse-tts", -1},

	{"セ", "シェ", -1, "collapse-ssh", 0},
	{"ゼノバ", "ジェノバ", -1, "collapse-ssh", 0},
	{"ゼシカ", "ジェシカ", -1, "collapse-ssh", 0},


	{"ヒュ", "フュ", -1, "collapse-hf", 0},
	{"ビュ", "ヴュ", -1, "collapse-hf", 0},

	{"ヒ", "ヒュ", -1, "collapse-hf", -1},
	{"ビ", "ビュ", -1, "collapse-hf", -1},
	{"ヒ", "フュ", -1, "collapse-hf", -1},
	{"ビ", "ヴュ", -1, "collapse-hf", -1},

	{"シラクサ", "シラキサ", -1, "", 1},
	{"シラクサ", "シラキサ", -1, "collapse-kx", 0},

	{"プロキシ", "プロクシ", -1, "collapse-kx", 0},
	{"プロキ", "プロキシ", 9, "collapse-kx", 0},
	{"プロキ", "プロクシ", 9, "collapse-kx", 0},	/* kono pattern to */
	{"プロクシ", "プロキ", 9, "collapse-kx", 1},	/* kono pattern no tigai ni tyuui! */
	{"プロキシ", "プロク", 9, "collapse-kx", -1},
	{"プロキシ", "プロキ", 9, "collapse-kx", 0},

	{"フェニクス", "フェニキス", -1, "collapse-kx", 0},

	{"ジュニヤ", "ジュニア", -1, "collapse-ay", 0},

	{"クセ", "キセ", -1, "collapse-kx", 0},
	{"キソ", "クソ", -1, "collapse-kx", 0},

	{"ブック", "ブツク", -1, "collapse-kx", -1},
	{"ブック", "ブツク", -1, "collapse-qy", 0},
	{"きょう", "きよう", -1, "collapse-kx", -1},
	{"きょう", "きよう", -1, "collapse-qy", 0},

	{"きょう", "キヨウ", -1, "", -1},
	{"きょう", "キヨウ", -1, "collapse-qy", -1},
	{"きょう", "キヨウ", -1, "collapse-hirakata", -1},
	{"きょう", "キヨウ", -1, "collapse-qy,collapse-hirakata", 0},

	{"ＡＢＣ", "ABC", -1, "", 1},

#if 0
	{"ＡＢＣ", "ABC", -1, "collapse-zenhan", 0},
	{"！＠＃", "!@#", -1, "collapse-zenhan", 0},
	{"０１２", "012", -1, "collapse-zenhan", 0},
	{"ａｂｃ", "abc", -1, "collapse-zenhan", 0},
	{"ＡＢＣ", "abc", -1, "collapse-zenhan", -1},
#endif

	{"ＡＢＣ", "abc", -1, "ignore-case", 1},
#if 0
	{"ＡＢＣ", "abc", -1, "collapse-zenhan,ignore-case", 0},
#endif

	{"ＡＢＣ", "ａｂｃ", -1, "", -1},
	{"ＡＢＣ", "ａｂｃ", -1, "ignore-case", 0},
#if 0
	{"ＡＢＣ", "ａｂｃ", -1, "collapse-zenhan,ignore-case", 0},
#endif

	{"ベートーヴェン", "べーとーべん", -1, "collapse-hirakata,collapse-bv", 0},
	{"ベートーヴェン", "べーとーべん", -1, "collapse-hirakata", 1},
	{"ベートーヴェン", "べーとーべん", -1, "collapse-bv", 1},
	{"ベートーヴェン", "べーとーべん", -1, "", 1},

	{"ｳｴﾉ", "ウエノ", -1, "", 1},
#if 0
	{"ｳｴﾉ", "ウエノ", -1, "collapse-zenhan", 0},
	{"ｷﾞﾝｻﾞ､", "ギンザ、", -1, "collapse-zenhan", 0},
	{"ｷﾞﾝｻﾞ", "ギンザ", -1, "collapse-zenhan", 0},
	{"｢ﾛｯﾎﾟﾝｷﾞ｣｡", "「ロッポンギ」。", -1, "collapse-zenhan", 0},
	{"･", "・", -1, "collapse-zenhan", 0},

	{"べートーヴェン", "ﾍﾞｰﾄｰﾍﾞﾝ", -1, "collapse-zenhan,collapse-bv", -1},
	{"べートーヴェン", "ﾍﾞｰﾄｰﾍﾞﾝ", -1, "collapse-zenhan,collapse-hirakata,collapse-bv", 0},

	{"ﾌﾟﾛｷｼ", "プロクシ", -1, "collapse-zenhan,collapse-kx", 0},
#endif

	{"一个", "一箇", -1, "collapse-ka", 0},
	{"一ヶ", "一箇", -1, "collapse-ka", 0},
	{"一ヵ", "一箇", -1, "collapse-ka", 0},
	{"一ヶ", "一ヵ", -1, "collapse-ka", 0},

	{"ｷﾞｻﾞ", "ギザ", -1, "", 1},
#if 0
	{"ｷﾞｻﾞ", "ギザ", -1, "collapse-zenhan", 0},
#endif

	{"一「」ヵ", "一ヵ", -1, "ignore-punct", 0},
	{"イタリア", "イタリヤ", -1, "collapse-ay", 0},
	{"ギリシャ", "ギリシヤ", -1, "collapse-qy", 0},
	{"ギリシャ", "ギリシア", -1, "collapse-qy,collapse-ay", 0},

	{"イヤ", "イア", -1, "collapse-ay", 0},
	{"ノキヤ", "ノキア", -1, "collapse-ay", 0},
	{"エネルギヤ", "エネルギア", -1, "collapse-ay", 0},
	{"アカシヤ", "アカシア", -1, "collapse-ay", 0},
	{"チランジヤ", "チランジア", -1, "collapse-ay", 0},
	{"クロアチヤ", "クロアチア", -1, "collapse-ay", 0},
	{"ヂヤ", "ヂア", -1, "collapse-ay", 0},
	{"ニヤ", "ニア", -1, "collapse-ay", 0},
	{"ヒヤリング", "ヒアリング", -1, "collapse-ay", 0},
	{"ビヤパーティー", "ビアパーティー", -1, "collapse-ay", 0},
	{"ピヤ", "ピア", -1, "collapse-ay", 0},
	{"ミヤ", "ミア", -1, "collapse-ay", 0},
	{"リヤ王", "リア王", -1, "collapse-ay", 0},
	{"ヰヤ", "ヰア", -1, "collapse-ay", 0},

	{"–—−ー－", "-----", -1, "collapse-dash", 0}, /* en-dash, em-dash, minus sign, katakana-hiragana prolonged, fullwidth hyphen-minus */
	{"–—−ー", "----", -1, "collapse-dash", 0}, /* en-dash, em-dash, minus sign, katakana-hiragana prolonged */
#if defined HAVE_ESHK_H
#include "eshk-test.h"
#endif
#endif
};

#if ! defined FLWD_UTF32
static int
xstrncmp_test(argc, argv)
char *argv[];
{
	int fail = 0;
	size_t i;
	int ch;
	int vflag = 0;

#if 0
	struct astream as;
	unsigned int c;
	ssize_t u1ptr;
	xsc_opt_t opt;
	UBool isError;
	int o;
	uint8_t u1[8];
#endif

	while ((ch = getopt(argc, argv, "vR:")) != -1) {
		switch (ch) {
		case 'v':
			vflag = 1;
			break;
		case 'R':
			getaroot = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0) {
		usage();
	}

	for (i = 0; i < nelems(pats); i++) {
		struct pat *p = &pats[i];
		int e;
		xsc_opt_t opt;
		if (parse_opts(p->options, &opt) == -1) {
			fprintf(stderr, "option parse error: \"%s\"\n", p->options);
			continue;
		}
		e = xstrncmp(p->a, p->b, p->alen, opt);
		if (e != p->expected) {
			printf("failed: a = \"%s\", b = \"%s\", len = %"PRIuSIZE_T", opt = \"%s\"(%x), expected = %d, result = %d\n", p->a, p->b, (pri_size_t)p->alen, p->options, opt, p->expected, e);
			fail = 1;
		}
		else if (vflag) {
			printf("success: a = \"%s\", b = \"%s\", len = %"PRIuSIZE_T", opt = \"%s\"(%x), expected = %d, result = %d\n", p->a, p->b, (pri_size_t)p->alen, p->options, opt, p->expected, e);
		}
	}
	if (!fail) {
		printf("all tests passwd\n");
	}

#if 0
	opt = 0;
	c = astream_init_r("いろは", -1, opt, &as, -1);
	c = astream_getc(&as, &u1ptr);
	o = 0; U8_APPEND(u1, o, sizeof u1, c, isError);
	fprintf(stderr, "c = %d(%s) u1 = %d\n", c, u1, (int)u1ptr);
	c = astream_getc(&as, &u1ptr);
	o = 0; U8_APPEND(u1, o, sizeof u1, c, isError);
	fprintf(stderr, "c = %d(%s) u1 = %d\n", c, u1, (int)u1ptr);
	c = astream_getc(&as, &u1ptr);
	o = 0; U8_APPEND(u1, o, sizeof u1, c, isError);
	fprintf(stderr, "c = %d(%s) u1 = %d\n", c, u1, (int)u1ptr);
	c = astream_getc(&as, &u1ptr);
	o = 0; U8_APPEND(u1, o, sizeof u1, c, isError);
	fprintf(stderr, "c = %d(%s) u1 = %d\n", c, u1, (int)u1ptr);
	c = astream_init_r("いろは", -1, opt, &as, 6);
	c = astream_ungetc(&as, &u1ptr);
	o = 0; U8_APPEND(u1, o, sizeof u1, c, isError);
	fprintf(stderr, "c = %d(%s) u1 = %d\n", c, u1, (int)u1ptr);
	c = astream_ungetc(&as, &u1ptr);
	o = 0; U8_APPEND(u1, o, sizeof u1, c, isError);
	fprintf(stderr, "c = %d(%s) u1 = %d\n", c, u1, (int)u1ptr);
	c = astream_ungetc(&as, &u1ptr);
	o = 0; U8_APPEND(u1, o, sizeof u1, c, isError);
	fprintf(stderr, "c = %d(%s) u1 = %d\n", c, u1, (int)u1ptr);
#endif

	return 0;
}
#endif

#if ! defined ENABLE_GETA

static int
nrpc_test(argc, argv)
char *argv[];
{
	WAM *w, *p;
	struct xr_vec const *v;
	idx_t i, j, k;
	char const *s;
	df_t np1, nr1, r1total;
	struct syminfo p1[3], *r1;
	int ch;

	while ((ch = getopt(argc, argv, "R:")) != -1) {
		switch (ch) {
		case 'R':
			getaroot = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0) {
		usage();
	}

	i = 1;
	j = 2;
	k = 3;

	if (!(w = wam_open("small", PREFIX))) {
		perror("small");
		return 1;
	}
	fprintf(stderr, "w = %p\n", w);

	if (wam_get_vec(w, WAM_COL, i, &v) == -1) {
		perror("wam_get_vec");
		return 1;
	}

	fprintf(stderr, "v = %p\n", v);
	fprintf(stderr, "e = %"PRIdDF_T"\n", v->elem_num);

	print_xr_vec(v, 8, w, WAM_ROW, stdout);

	if (!(p = wam_prop_open(w, WAM_ROW, "title"))) {
		perror("small - title");
		return 1;
	}
	fprintf(stderr, "p = %p\n", p);

	if (wam_prop_gets(p, i, &s) == -1) {
		perror("wam_prop_gets");
		return 1;
	}
	fprintf(stderr, "s = %p\n", s);
	fprintf(stderr, "s = %s\n", s);

	p1[0].id = i; p1[0].v = NULL; p1[0].TF_d = 1;
	p1[1].id = j; p1[1].v = NULL; p1[1].TF_d = 1;
	p1[2].id = k; p1[2].v = NULL; p1[2].TF_d = 1;
	np1 = 3;
	nr1 = 8;
	r1 = assv(p1, np1, w, WAM_ROW, WT_SMARTAW, &nr1, &r1total, NULL);
	fprintf(stderr, "r1 = %p\n", r1);
	fprintf(stderr, "nr1 = %"PRIdDF_T"\n", nr1);
	fprintf(stderr, "r1total = %"PRIdDF_T"\n", r1total);

	print_syminfo(r1, nr1, r1total, 8, w, WAM_COL, stderr);

	return 0;
}

#endif /* !ENABLE_GETA */

#if ! defined ENABLE_GETA
static int
compare_syminfo(r, nr, s, ns)
struct syminfo *r, *s;
size_t nr, ns;
{
	size_t i;

	for (i = 0; i < nr && i < ns; i++) {
		if (r[i].id < s[i].id) {
#if 0
fprintf(stderr, "%"PRIuIDX_T" < %"PRIuIDX_T"\n", r[i].id, s[i].id);
#endif
			return -1;
		}
		else if (r[i].id > s[i].id) {
#if 0
fprintf(stderr, "%"PRIuIDX_T" > %"PRIuIDX_T"\n", r[i].id, s[i].id);
#endif
			return 1;
		}
	}
	if (i < nr) {
#if 0
fprintf(stderr, "SHORT R\n");
#endif
		return 1;
	}
	if (i < ns) {
#if 0
fprintf(stderr, "SHORT S\n");
#endif
		return -1;
	}
	return 0;
}
#endif

static int
miscellaneous_test(argc, argv)
char *argv[];
{
	void *p;
	int ch;

	while ((ch = getopt(argc, argv, "R:")) != -1) {
		switch (ch) {
		case 'R':
			getaroot = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0) {
		usage();
	}

	if (!(p = malloc(4000000000))) {
		perror("malloc");
	}
	getchar();
	return 0;
}

/*
 * n: number of result
 * p: ratio of population of objects of the host: useally 1/l
 * b: 1/b == average error rate to achieve 
 * l: number of partitions
 * RETURN VALUE: number of request
 */
static size_t
safe_request_num(n, p, b, l)
size_t n, l;
double p;
{
	double a, q, s, t, r;
	size_t k;

	a = 1 - 1.0 / b;
	r = 1.0 / b;
	q = 1 - p;
	s = t = pow(q, n);
	if (s == 0) {
		return n;
	}
	for (k = 0; k < n && 1 - s > r / l; k++) {
		t *= (n - k) * p / ((k + 1) * q);
		s += t;
	}
	return k;
}
