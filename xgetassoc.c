/*	$Id: xgetassoc.c,v 1.119 2011/10/24 06:31:53 nis Exp $	*/

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
static char rcsid[] = "$Id: xgetassoc.c,v 1.119 2011/10/24 06:31:53 nis Exp $";
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
#include <netdb.h>
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
#include "print.h"

#include "fssP.h"
#include "nwamP.h"
#include "nrpc.h"
#include "xgserv.h"

#include <gifnoc.h>

/* #define DBG	1 /* */

#if ! defined PF_LOCAL
#if defined 	AF_UNIX
#define PF_LOCAL	AF_UNIX
#endif
#endif

#define	BACKLOG_DFL	128

#if ! defined LOG_PERROR
#define	LOG_PERROR	0
#endif

static void usage(void);

#if ! defined ENABLE_GETA
static int nwam_setup(int, char *[]);
static int nwam_delete_articles(int, char *[]);
static ssize_t read_idl(char const *, idx_t **, FILE *, char const *);
static int dumpall(int, char *[]);
static int xgserver(int, char *[]);
static int xgserver_master(char const *, char const *, char const *, char const *, char const *, int);
static int sockets(char const *, int [], size_t, const char **, char const *, char const *, int);
static void *start_xgserver_loop(void *);
#if defined SIGPIPE
static void sigpipe(int);
#endif
#if defined SIGCHLD
static void sigchld(int);
#endif
static void sigint(int);
static int nwam_drop_db(int, char *[]);
static int nwam_rename_db(int, char *[]);
#endif

char *progname = NULL;
char *getaroot; /* = GETAROOT; */
extern char **environ;

struct cmd {
	char *name;
	int (*fn)(int, char *[]);
	char *args;
};

static struct cmd cmds[] = {
#if ! defined ENABLE_GETA
	{"setup", nwam_setup, "(nwam-setup) [-d] [-k keys] [-a stem_a_no] [-n stem_n_no] [-r mkri_ncpu] [-t tmpdir] [-R getaroot] [-N (getaroot=>NULL)] [-s stemmer] [-h host] [-p port]"
#if ! defined NO_LOCALSOCKET
		" [-u localsocket]"
#endif
#if ! defined NO_FORK
		" [-b stmd-path]"
#endif
#if defined ENABLE_RAWVECLEN_LIMIT
		" [-l rawveclen_limit]"
#endif
		" itb,...|- handle"},
	{"dumpall", dumpall, "(dumpall) [-k keys] [-R getaroot] [-N] handle"},
	{"xgserver", xgserver, "(xgetassoc-server) [-d] [-p port] [-u localsocket (default: NULL)] [-b bind_address (default: NULL)] [-t tmpdir] -r pwam_root [-B backlog]"},
	{"delarts", nwam_delete_articles, "(delete-articles) [-t tmpdir] [-R getaroot] [-N] handle < id-list"},
	{"drop", nwam_drop_db, "(drop) [-R getaroot] [-N] handle"},
	{"rename", nwam_rename_db, "(rename) [-R getaroot] [-N] from to"},
#endif
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
	PRINT_ELAPSED("elapsed", stderr);
	return e;
}

#if ! defined ENABLE_GETA

static int
nwam_setup(argc, argv)
char *argv[];
{
	size_t i;
	char const *handle;
	char const *stemmer = NULL;
	char const *tmpdir = NULL;
	int dflag = 0;
	int ch;
	int role = NDWAM_MONOLITHIC;
	size_t nn0 = 0, nn1 = 0;
	char const *sc[2];
	char scc[2][MAXPATHLEN];
#if defined ENABLE_RAWVECLEN_LIMIT
	int rawveclen_limit = 0;
#endif

	char *stmdhost = STMDHOST;
	char *stmdserv = STMDSERV;
#if ! defined NO_LOCALSOCKET
	char *stmdlocalsocket = STMDLOCALSOCKET;
#endif
#if ! defined NO_FORK
	char *stmdbin = STMDBIN;
#endif

	struct wam_update_args a;

	char *is;
	char *ks = NULL;
	char const **itbs;
	char const **keys;
	size_t ni, nitbs;
	size_t nk, nkeys;

	int stem_a_no = 1, stem_n_no = 1, mkri_ncpu = 1;

	openlog("wam_update", LOG_PID|LOG_PERROR, LOG_LOCAL0);

	sc[0] = NULL;
	sc[1] = NULL;

	while ((ch = getopt(argc, argv, "dk:a:n:r:t:R:Ns:D:Y:C:X:"
		"h:p:"
#if ! defined NO_LOCALSOCKET
		"u:"
#endif
#if ! defined NO_FORK
		"b:"
#endif
#if defined ENABLE_RAWVECLEN_LIMIT
		"l:"
#endif
			)) != -1) {
		switch (ch) {
		case 'D':
			role = NDWAM_ROLE_PARENT;
			nn0 = atoi(optarg);
			break;
		case 'Y':
			role = NDWAM_ROLE_PARENT;
			nn1 = atoi(optarg);
			break;
		case 'C':
			sc[0] = optarg;
			break;
		case 'X':
			sc[1] = optarg;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'k':
			ks = optarg;
			break;
		case 'a':
			stem_a_no = atoi(optarg);
			break;
		case 'n':
			stem_n_no = atoi(optarg);
			break;
		case 'r':
			mkri_ncpu = atoi(optarg);
			break;
		case 't':
			tmpdir = optarg;
			break;
		case 'R':
			getaroot = optarg;
			break;
		case 'N':
			getaroot = NULL;
			break;
		case 's':
			stemmer = optarg;
			break;
		case 'h':
			stmdhost = optarg;
			break;
		case 'p':
			stmdserv = optarg;
			break;
#if ! defined NO_LOCALSOCKET
		case 'u':
			stmdlocalsocket = optarg;
			break;
#endif
#if ! defined NO_FORK
		case 'b':
			stmdbin = optarg;
			break;
#endif
#if defined ENABLE_RAWVECLEN_LIMIT
		case 'l':
			rawveclen_limit = atoi(optarg);
			break;
#endif
		default:
			usage();
		}
	}

	argc -= optind, argv += optind;

	if (!ks) {
		ks = strdup("title,link");
	}

	if (argc != 2) {
		usage();
	}

	if (!tmpdir && !(tmpdir = getenv("TMPDIR"))) {
		tmpdir = "/tmp";
	}
	is = argv[0];
	handle = argv[1];

	wam_init(getaroot);

	if (!stemmer) {
		stemmer = STEMMER_AUTO;
	}

	if (nn0 || nn1) {
		if (!nn0) nn0 = nn1;
		if (!nn1) nn1 = nn0;
		if (!sc[0]) {
			if (sc[1]) {
				sc[0] = sc[1];
			}
			else if (!getaroot) {
				fprintf(stderr, "you must provide GETAROOT\n");
				return 1;
			}
			else {
				snprintf(scc[0], sizeof scc[0], "%s" DIRECTORY_DELIMITER ETCDIR DIRECTORY_DELIMITER DNWAM_CONF, getaroot);
				sc[0] = scc[0];
			}
		}
		if (!sc[1]) {
			if (sc[0]) {
				sc[1] = sc[0];
			}
			else if (!getaroot) {
				fprintf(stderr, "you must provide GETAROOT\n");
				return 1;
			}
			else {
				snprintf(scc[1], sizeof scc[1], "%s" DIRECTORY_DELIMITER ETCDIR DIRECTORY_DELIMITER DNWAM_CONF, getaroot);
				sc[1] = scc[1];
			}
		}
	}
fprintf(stderr, "nn0 = %"PRIuSIZE_T", nn1 = %"PRIuSIZE_T"\n", (pri_size_t)nn0, (pri_size_t)nn1);

	nitbs = count_char(is, ',') + 2;
	if (!(itbs = calloc(nitbs, sizeof *itbs))) {
		perror("calloc");
		return 1;
	}
	ni = splitv(is, ',', itbs, nitbs - 1, 1);
	if (ni == nitbs - 1) {
		fprintf(stderr, "too many itbs\n");
		return 1;
	}
	itbs[ni] = NULL;
	for (i = 0; i < ni; i++) {
		fprintf(stderr, "itb%d = %s\n", (int)i, itbs[i]);
	}

	nkeys = count_char(ks, ',') + 2;
	if (!(keys = calloc(nkeys, sizeof *keys))) {
		perror("calloc");
		return 1;
	}
	nk = splitv(ks, ',', keys, nkeys - 1, 1);
	if (nk == nkeys - 1) {
		fprintf(stderr, "too many keys\n");
		return 1;
	}
	keys[nk] = NULL;
	for (i = 0; i < nk; i++) {
		fprintf(stderr, "key%d = %s\n", (int)i, keys[i]);
	}
fprintf(stderr, "handle = %s\n", handle);
fprintf(stderr, "tmpdir = %s\n", tmpdir);
fprintf(stderr, "getaroot = %s\n", getaroot ? getaroot : "(null)");

	a.command = WAM_UPDATE_FROM_ITB;
	a.handle = handle;
	a.tmpdir = tmpdir;
	a.i.itbs = itbs;
	a.i.s.stemmer = stemmer;
	a.i.s.host = stmdhost;
	a.i.s.serv = stmdserv;
#if ! defined NO_LOCALSOCKET
	a.i.s.sock = stmdlocalsocket;
#endif
#if ! defined NO_FORK
	a.i.s.path = stmdbin;
#endif
	a.i.keys = keys;
	a.i.s.stem_a_no = stem_a_no;
	a.i.s.stem_n_no = stem_n_no;
	a.i.mkri_ncpu = mkri_ncpu;
#if defined ENABLE_RAWVECLEN_LIMIT
	a.i.rawveclen_limit = rawveclen_limit;
#endif

	a.dist.c.role = role;
	a.dist.c.nn[0] = nn0;
	a.dist.c.nn[1] = nn1;
	a.dist.n = NULL;	/* child */
	a.dist.nn = NULL;	/* child */
	a.dist.c.dir = 0;		/* child */
	a.dist.c.node_id = 0;	/* child */
	a.dist.server_conf_paths = sc;

	a.environ = environ;
	if (wam_update(&a, getaroot) == -1) {
		fprintf(stderr, "wam_update failed\n");
		return 1;
	}

	if (dflag) {
		wam_dumpall(handle, keys, stdout);
	}
	free(keys);
	free(itbs);
#if defined ENABLE_MDBG
	mdbg_status(stderr, 1);
#endif
	return 0;
}

static int
nwam_delete_articles(argc, argv)
char *argv[];
{
	char const *handle;
	char const *tmpdir = NULL;
	int ch;
	struct wam_update_args a;
	idx_t *idl;
	size_t ni;

	openlog("wam_update(D)", LOG_PID|LOG_PERROR, LOG_LOCAL0);

	while ((ch = getopt(argc, argv, "t:R:N")) != -1) {
		switch (ch) {
		case 't':
			tmpdir = optarg;
			break;
		case 'R':
			getaroot = optarg;
			break;
		case 'N':
			getaroot = NULL;
			break;
		default:
			usage();
		}
	}

	argc -= optind, argv += optind;

	if (argc != 1) {
		usage();
	}

	if (!tmpdir && !(tmpdir = getenv("TMPDIR"))) {
		tmpdir = "/tmp";
	}
	handle = argv[0];

	wam_init(getaroot);

	if ((ni = read_idl(handle, &idl, stdin, getaroot)) == -1) {
		return 1;
	}

fprintf(stderr, "handle = %s\n", handle);
fprintf(stderr, "tmpdir = %s\n", tmpdir);
fprintf(stderr, "getaroot = %s\n", getaroot ? getaroot : "(null)");

	a.command = WAM_UPDATE_DELIDL;
	a.handle = handle;
	a.tmpdir = tmpdir;
	a.d.id = idl;
	a.d.nid = ni;
	a.d.vecs = NULL;
	a.dist.c.role = NDWAM_INHERIT;
	a.dist.n = NULL;	/* child */
	a.dist.nn = NULL;	/* child */
	a.dist.server_conf_paths = NULL;

	a.environ = environ;
	if (wam_update(&a, getaroot) == -1) {
		fprintf(stderr, "wam_update failed\n");
		return 1;
	}
#if defined ENABLE_MDBG
	mdbg_status(stderr, 1);
#endif
	return 0;
}

static ssize_t
read_idl(handle, rp, stream, getaroot)
idx_t **rp;
char const *handle, *getaroot;
FILE *stream;
{
	WAM *w;
	idx_t *r = NULL;
	size_t n = 0, size = 0;
	char buf[8192];

	if (!(w = wam_open(handle, getaroot))) {
		perror(handle);
		return -1;
	}

	while (fgets(buf, sizeof buf, stream)) {
		char *p;
		idx_t id;
		if (!(p = index(buf, '\n'))) {
			fprintf(stderr, "too long line\n");
			syslog(LOG_DEBUG, "too long line");
			return -1;
		}
		*p = '\0';
		if (buf < p && *--p == '\r') {
			*p = '\0';
		}
		if (!(id = wam_name2id(w, WAM_ROW, buf))) {
			fprintf(stderr, "warning: [%s]: unknown id\n", buf);
			syslog(LOG_DEBUG, "warning: [%s]: unknown id", buf);
			continue;
		}
		if (size <= n) {
			void *new;
			size_t newsize = n;
			newsize = NA(newsize + 1, 128);
			if (!(new = realloc(r, newsize * sizeof *r))) {
				fprintf(stderr, "realloc: %s\n", strerror(errno));
				syslog(LOG_DEBUG, "realloc: %s", strerror(errno));
			}
			size = newsize;
			r = new;
		}
		r[n++] = id;
	}
	wam_close(w);
	*rp = r;
	return n;
}

static int
dumpall(argc, argv)
char *argv[];
{
	int ch;
	char *handle;
	char *ks = NULL;
	char **keys;
	size_t nk, nkeys;

	while ((ch = getopt(argc, argv, "k:R:N")) != -1) {
		switch (ch) {
		case 'k':
			ks = optarg;
			break;
		case 'R':
			getaroot = optarg;
			break;
		case 'N':
			getaroot = NULL;
			break;
		default:
			usage();
		}
	}

	argc -= optind, argv += optind;

	if (argc != 1) {
		usage();
	}

	handle = argv[0];

	if (!ks) {
		ks = strdup("title,link");
	}

	wam_init(getaroot);

	nkeys = count_char(ks, ',') + 2;
	if (!(keys = calloc(nkeys, sizeof *keys))) {
		perror("calloc");
		return 1;
	}
	nk = splitv(ks, ',', keys, nkeys - 1, 1);
	if (nk == nkeys - 1) {
		fprintf(stderr, "too many keys\n");
		return 1;
	}
	keys[nk] = NULL;
	wam_dumpall(handle, keys, stdout);
	return 0;
}

static int
xgserver(argc, argv)
char *argv[];
{
	int ch;
	int dflag = 0;
	char *serv = NULL;
	char *localsocket = NULL;
	char *bind_address = NULL;
	char *xgs_pwam_root = NULL;
	char *xgs_tmpdir = NULL;
	int backlog = BACKLOG_DFL;

	while ((ch = getopt(argc, argv, "B:dp:u:b:t:r:")) != -1) {
		switch (ch) {
		case 'B':
			backlog = atoi(optarg);
			break;
		case 'd':
			dflag = 1;
			break;
		case 'p':
			serv = optarg;
			break;
		case 'u':
			localsocket = optarg;
			break;
		case 'b':
			bind_address = optarg;
			break;
		case 'r':
			xgs_pwam_root = optarg;
			break;
		case 't':
			xgs_tmpdir = optarg;
			break;
		default:
			usage();
		}
	}

	if (!xgs_pwam_root) {
		fprintf(stderr, "ERROR: -r required\n");
		syslog(LOG_DEBUG, "ERROR: -r required");
		return 1;
	}

	if (!serv && !localsocket) {
		fprintf(stderr, "ERROR: -p or -u required\n");
		syslog(LOG_DEBUG, "ERROR: -p or -u required");
		return 1;
	}

	argc -= optind, argv += optind;

	if (argc != 0) {
		usage();
	}

	if (!xgs_tmpdir && !(xgs_tmpdir = getenv("TMPDIR"))) {
		xgs_tmpdir = "/tmp";
	}

#if defined SIGPIPE
	signal(SIGPIPE, sigpipe);
	sigflg(SIGPIPE, SA_RESTART, SA_RESETHAND);
#endif
#if defined SIGCHLD
	signal(SIGCHLD, sigchld);
	sigflg(SIGCHLD, SA_RESTART, SA_RESETHAND);
#endif

#if defined SIGHUP
	signal(SIGHUP, sigint);
#endif
	signal(SIGINT, sigint);
#if defined SIGQUIT
	signal(SIGQUIT, sigint);
#endif
	signal(SIGTERM, sigint);

	if (xgserver_master(serv, localsocket, bind_address, xgs_pwam_root, xgs_tmpdir, backlog) == -1) {
		return 1;
	}

	return 0;
}

int signaled = 0;

struct xserver_loop_arg_t {
	int ss;
	char const *xgs_pwam_root, *xgs_tmpdir;
};

static int
xgserver_master(serv, localsocket, bind_address, xgs_pwam_root, xgs_tmpdir, backlog)
char const *serv, *localsocket, *bind_address, *xgs_pwam_root, *xgs_tmpdir;
{
	int s[16];
	int ns;
	const char *cause;

	if ((ns = sockets(serv, s, nelems(s), &cause, localsocket, bind_address, backlog)) == 0) {
		syslog(LOG_DEBUG, "%s: %s", cause, strerror(errno));
		return -1;
	}

	syslog(LOG_DEBUG, "Server ready");

	for (; !signaled; ) {
		int i;
		fd_set readfds;
		int nfds;

		FD_ZERO(&readfds);

		nfds = 0;
		for (i = 0; i < ns; i++) {
			FD_SET(s[i], &readfds);
			if (s[i] > nfds) {
				nfds = s[i];
			}
		}

		nfds++;

		if (select(nfds, &readfds, NULL, NULL, NULL) == -1) {
			if (errno != EINTR) {
				syslog(LOG_DEBUG, "select: %s", strerror(errno));
			}
			continue;
		}

		for (i = 0; i < ns; i++) {
			struct sockaddr_storage addr;
			socklen_t len;
			int ss;
			struct xserver_loop_arg_t xserver_loop_arg;
#if defined NO_FORK
			pthread_t thread;
#endif
			if (!FD_ISSET(s[i], &readfds)) {
				continue;
			}
			len = sizeof addr;
			if ((ss = accept(s[i], (struct sockaddr *)&addr, &len)) < 0) {
				syslog(LOG_DEBUG, "accept: %s", strerror(errno));
				continue;
			}
			xserver_loop_arg.ss = ss;
			xserver_loop_arg.xgs_pwam_root = xgs_pwam_root;
			xserver_loop_arg.xgs_tmpdir = xgs_tmpdir;
#if ! defined NO_FORK
			switch (fork()) {
			case -1:
				syslog(LOG_DEBUG, "fork: %s", strerror(errno));
				return -1;
			case 0:
				for (i = 0; i < ns; i++) {
					close(s[i]);
				}
				if (start_xgserver_loop(&xserver_loop_arg)) {
					close(ss);
					_exit(1);
				}
				close(ss);
				_exit(0);
			default:
				close(ss);
				break;
			}
#else
			if (pthread_create(&thread, NULL, start_xgserver_loop, &xserver_loop_arg)) {
				perror("pthread_create");
				return -1;
			}
#endif
		}
	}
	if (localsocket) {
		unlink(localsocket);
	}
	return 0;
}

static int
sockets(servname, s, size, cause, localsocket, bind_address, backlog)
char const *servname;
int s[];
size_t size;
const char **cause;
char const *localsocket;
char const *bind_address;
{
	int nsock = 0;

	*cause = "";

#if ! defined NO_LOCALSOCKET
	do {
		struct sockaddr_un myaddr;
		int sun_len;

		if (!localsocket) {
			continue;
		}

		sun_len = sizeof myaddr.sun_family + strlen(localsocket) + 1;
		s[nsock] = socket(PF_LOCAL, SOCK_STREAM, 0);
		if (s[nsock] < 0) {
			*cause = "socket";
			continue;
		}
		memset(&myaddr, 0, sizeof myaddr);
		myaddr.sun_family = PF_LOCAL;
#if HAVE_SUN_LEN
		myaddr.sun_len = sun_len;
#endif
		if (strlen(localsocket) >= sizeof myaddr.sun_path) {
			close(s[nsock]);
			continue;
		}
		strcpy(myaddr.sun_path, localsocket);
		if (bind(s[nsock], (struct sockaddr *)&myaddr, sun_len)==-1) {
			close(s[nsock]);
			*cause = "bind";
			continue;
		}
		(void) listen(s[nsock++], backlog);
		chmod(localsocket, 0777);
	} while (0);
#endif

	if (servname) {
		struct addrinfo hints, *res, *res0;
		int e;
		hints.ai_family = PF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE;
		hints.ai_protocol = 0;
		hints.ai_addrlen = 0;
		hints.ai_canonname = NULL;
		hints.ai_addr = NULL;
		hints.ai_next = NULL;
		if (e = getaddrinfo(bind_address, servname, &hints, &res0)) {
			syslog(LOG_DEBUG, "%s", gai_strerror(e));
			*cause = "getaddrinfo";
			return 0;
		}

		for (res = res0; res && nsock < size; res = res->ai_next) {
			s[nsock] = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
			if (s[nsock] < 0) {
				*cause = "socket";
				continue;
			}

			if (bind(s[nsock], res->ai_addr, res->ai_addrlen) < 0) {
				close(s[nsock]);
				*cause = "bind";
				continue;
			}
			(void) listen(s[nsock++], backlog);
		}
		freeaddrinfo(res0);
	}
	return nsock;
}

static void *
start_xgserver_loop(v)
void *v;
{
#if defined TCP_NODELAY
	int one = 1;
#endif
	struct xserver_loop_arg_t *xserver_loop_arg = v;

#if defined TCP_NODELAY
#if defined DBG
syslog(LOG_DEBUG, "tcp_nodelay");
#endif
	(void)setsockopt(xserver_loop_arg->ss, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof one);
#endif

	if (xgserv_loop(xserver_loop_arg->ss, xserver_loop_arg->xgs_pwam_root, xserver_loop_arg->xgs_tmpdir, environ) == -1) {
#if defined ENABLE_MDBG
		mdbg_status(stderr, 1);
#endif
		return "error";
	}
#if defined ENABLE_MDBG
	mdbg_status(stderr, 1);
#endif

	return NULL;
}

#if defined SIGPIPE
static void
sigpipe(n)
{
	syslog(LOG_DEBUG, "sigpipe");
	_exit(1);
}
#endif

#if defined SIGCHLD
static void
sigchld(n)
{
	int status;

	for (; wait3(&status, WNOHANG, NULL) > 0; ) {
#if defined DBG
		syslog(LOG_DEBUG, "# wait3: status = %d", status);
#endif
	}
}
#endif

static void
sigint(n)
{
	signaled = 1;
}

static int
nwam_drop_db(argc, argv)
char *argv[];
{
	char const *handle;
	int ch;

	openlog("xgetassoc: wam_drop", LOG_PID|LOG_PERROR, LOG_LOCAL0);
	while ((ch = getopt(argc, argv, "R:N")) != -1) {
		switch (ch) {
		case 'R':
			getaroot = optarg;
			break;
		case 'N':
			getaroot = NULL;
			break;
		default:
			usage();
		}
	}

	argc -= optind, argv += optind;

	if (argc != 1) {
		usage();
	}

	handle = argv[0];

	wam_init(getaroot);

	if (wam_drop(handle, NULL) == -1) {
		return 1;
	}

	return 0;
}

static int
nwam_rename_db(argc, argv)
char *argv[];
{
	char const *from, *to;
	int ch;

	openlog("xgetassoc: wam_rename", LOG_PID|LOG_PERROR, LOG_LOCAL0);
	while ((ch = getopt(argc, argv, "R:N")) != -1) {
		switch (ch) {
		case 'R':
			getaroot = optarg;
			break;
		case 'N':
			getaroot = NULL;
			break;
		default:
			usage();
		}
	}

	argc -= optind, argv += optind;

	if (argc != 2) {
		usage();
	}

	from = argv[0];
	to = argv[1];

	wam_init(getaroot);

	if (wam_rename(from, to, NULL) == -1) {
		return 1;
	}

	return 0;
}

#endif /* !ENABLE_GETA */
