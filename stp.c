/*	$Id: stp.c,v 1.129 2011/09/14 03:06:09 nis Exp $	*/

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
static char rcsid[] = "$Id: stp.c,v 1.129 2011/09/14 03:06:09 nis Exp $";
#endif

/* XXX: need a cleanup routine which handles an abnormal exit */

#include <config.h>

#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <assert.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "nwam.h"
#include "util.h"

#include "common.h"

#include "stpP.h"

#include <gifnoc.h>

#define	ERR	0
#if defined ENABLE_GETA
#if defined BIN_PAX
#define	STP	1
#endif
#endif
#define	STB	2
#define	RMV	3

#define	STT	4

#if defined ENABLE_GETA
#define	CAT	5
#endif

#define	STO	7
#define	STU	8

#if ! defined ENABLE_GETA
#define	DEL	9
#endif

#if ! defined ENABLE_GETA
#define DWAM	NWAMDIR
#else
#define DWAM	"dwam"		/* XXX: a clone is in fss.c */
#endif

#if ! defined ENABLE_NEWLAYOUT
#define	LIBEXEC	DIRECTORY_DELIMITER LIBEXECDIRNAME
#endif

/* GETAROOT RELATIVE */
#if defined ENABLE_GETA
#define MKDW	SBIN DIRECTORY_DELIMITER "mkdw"
#define MKPI	SBIN DIRECTORY_DELIMITER "mkpi"
XXX #define SCHED_CONF	ETC DIRECTORY_DELIMITER "sched.conf"
#endif

/* PREFIX RELATIVE */
#if defined ENABLE_GETA
XXX ENABLE_NEWLAYOUT #define ITB2FRQ	LIBEXEC DIRECTORY_DELIMITER "itb2frq"
XXX ENABLE_NEWLAYOUT #define MKRI	LIBEXEC DIRECTORY_DELIMITER "mkri"
#endif

#if ! defined OTMI2ITB
#define OTMI2ITB	LIBEXEC DIRECTORY_DELIMITER "otmi2itb"
#endif

#if ! defined TSV2ITB
#define TSV2ITB	LIBEXEC DIRECTORY_DELIMITER "tsv2itb"
#endif

#if ! defined ENABLE_GETA
#if ! defined XGETASSOC
#define XGETASSOC	LIBEXEC DIRECTORY_DELIMITER "xgetassoc"
#endif
#define	XGETASSOC_SETUP		"setup"
#define	XGETASSOC_DELARTS	"delarts"
#define	XGETASSOC_DROP		"drop"
#define	XGETASSOC_RENAME	"rename"
#endif

#define	FSSKEY	"@fss"
	/* NOTE: this FSSKEY must NOT match what defined in nwamP.h.
		 but to avoid confusion, we use same value */

static void usage(void);
static int main0(int, char *[], char *, int, int);
static int decode_command(char *);
static int prepare_workdir(char const *, char const *, char *, size_t, int, int);
static int cat_itb(char const *);
#if ! defined ENABLE_GETA
static int gen_wam(char const *, char const *, char const *, char const *, char *, size_t, char *, int, char const *, char const *);
static int rename_wam(char const *, char const *, char const *);
static int rename_wam_0(char const *, char const *, char const *);
static int drop_wam(char const *, char const *);
static int del_arts(char const *, char const *, char const *, char *, size_t);
#else /* ! ENABLE_GETA */
static int gen_wam(char const *, char const *, char * const *, size_t, char const *, int);
static int rename_dir(char const *, char const *);
static int remove_wam(char const *, char const *);
#endif /* ! ENABLE_GETA */
static void sigint(int);

char *progname;

#if ! defined ENABLE_NEWLAYOUT
#define CGIPOST	0
#endif
#define	SSH	1
#define	LOCAL	2

static char wamdir_tmp[MAXPATHLEN] = "";
extern char catalogue_tmp[];

static void
usage()
{
#if 0
#define SHOW_REMOTE_INSTALL_MENU 1
#endif
#if defined SHOW_REMOTE_INSTALL_MENU
	fprintf(stderr, "usage: SCRIPT_NAME=/stb PATH_INFO=/${handle}/${properites} SCRIPT_FILENAME=${getaroot}/XX/YY %s < ${itb}\n", progname);
#if defined ENABLE_GETA
	fprintf(stderr, "       SCRIPT_NAME=/stp PATH_INFO=/${handle}/${properites} SCRIPT_FILENAME=${getaroot}/XX/YY %s < ${tar}\n", progname);
#endif
	fprintf(stderr, "       SCRIPT_NAME=/rmv PATH_INFO=/${handle} SCRIPT_FILENAME=${getaroot}/XX/YY %s\n", progname);
	fprintf(stderr, "       (echo /stb; echo ${handle}; echo ${properties}; cat ${itb}) | %s -s ${dirname}\n", progname);
	fprintf(stderr, "       (echo -stb; echo ${handle}; echo ${properties}; cat ${itb}) | %s -s ${dirname}\n", progname);
	fprintf(stderr, "       (echo /stu; echo ${handle}; echo ${properties}; cat ${tsv}) | %s -s ${dirname}\n", progname);
	fprintf(stderr, "       (echo -stu; echo ${handle}; echo ${properties}; cat ${tsv}) | %s -s ${dirname}\n", progname);
#if defined ENABLE_GETA
	fprintf(stderr, "XXX    (echo /stp; echo ${handle}; echo ${properties}; cat ${tar}) | %s -s ${dirname}\n", progname);
#endif
	fprintf(stderr, "       (echo /rmv; echo ${handle}) | %s -s ${dirname}\n", progname);
	fprintf(stderr, "       (echo /cat; echo ${handle}) | %s -s ${dirname}\n", progname);
#endif
#if defined ENABLE_GETA
	fprintf(stderr, "XXX    %s -p ${dirname} ${handle} ${properties} < ${tar}\n", progname);
#endif
	fprintf(stderr, "       %s [-o,xgopts] [-U] -b ${dirname} ${handle} ${properties} [stemmer [${itb},...]]\n", progname);
	fprintf(stderr, "       %s [-o,xgopts] [-U] -u ${dirname} ${handle} ${properties} [stemmer [${tsv}]]\n", progname);
	fprintf(stderr, "       %s -d ${dirname} ${handle}\n", progname);
	fprintf(stderr, "       %s -r ${dirname} ${handle}\n", progname);
	exit(1);
}

int
main(argc, argv)
char *argv[];
{
	int mode;
	int e;
	char *cmd;
	char *xgopt = NULL;
	int uflag = 0;

	progname = basename(argv[0]);

	/* WE CANNOT USE GETOPT HERE */
	for (argc--, argv++; argc > 0; ) {
		if (!strncmp(argv[0], "-o,", 3)) {
			xgopt = argv[0] + 3;
			argc--, argv++;
			continue;
		}
		if (!strcmp(argv[0], "-U")) {
			uflag = 1;
			argc--, argv++;
			continue;
		}
		break;
	}

	cmd = argv[0];


	if (argc == 2 && !strcmp(cmd, "-s")) {
		mode = SSH;
	}
	else if (argc == 3 && (!strcmp(cmd, "-r") || !strcmp(cmd, "-d"))) {
		mode = LOCAL;
	}
	else if (argc == 4 && (!strcmp(cmd, "-p") || !strcmp(cmd, "-b") || !strcmp(cmd, "-u") || !strcmp(cmd, "-t"))) {
		mode = LOCAL;
	}
	else if (argc == 5 && (!strcmp(cmd, "-b") || !strcmp(cmd, "-u"))) {
		mode = LOCAL;
	}
	else if (argc == 6 && (!strcmp(cmd, "-b") || !strcmp(cmd, "-u"))) {
		mode = LOCAL;
	}
	else if (argc != 0) {
		usage();
		/* NOTREACHED */
	}
	else if (isatty(0)) {
		usage();
		/* NOTREACHED */
	}
	/* fprintf(stderr, "mode = %d\n", mode); */
#if ! defined ENABLE_NEWLAYOUT
	else {
		mode = CGIPOST;
	}
#else
	else {
		usage();
		/* NOTREACHED */
	}
#endif

#if ! defined ENABLE_NEWLAYOUT
	if (mode == CGIPOST) {
		setvbuf(stdout, NULL, _IONBF, 0);
		dup2(1, 2);
	}
#endif

#if defined SIGHUP
	signal(SIGHUP, sigint);
#endif
	signal(SIGINT, sigint);
#if defined SIGQUIT
	signal(SIGQUIT, sigint);
#endif
	signal(SIGTERM, sigint);

	switch (mode) {
#if ! defined ENABLE_NEWLAYOUT
	case CGIPOST:
		fprintf(stdout, "Content-Type: text/html\r\n"
			"\r\n"
			"<html><body><pre>\r\n");
		break;
#endif
	default:
		break;
	}

	if (e = main0(argc, argv, xgopt, uflag, mode)) {
		e = 1;
	}

	printf("status = %d\nend.\n", e);

	switch (mode) {
#if ! defined ENABLE_NEWLAYOUT
	case CGIPOST:
		fprintf(stdout, "status=%d\n"
			"</pre><status status=\"%d\"/>"
			"</body></html>\n", e, e);
		return 0;
#endif
	default:
		return e;
	}
}

static int
main0(argc, argv, xgopt, uflag, mode)
char *argv[], *xgopt;
{
	int mkrip = 0;
	char *getaroot; /* = GETAROOT; */
#if defined MKRI_NCPU
	char envs[MAXPATHLEN], *es;
#endif
	char workdir[MAXPATHLEN];
	char inf[MAXPATHLEN];
	char *handle, *ts;
	int cmd;
	char *types[32] = {NULL, };
	size_t ntypes = 0;
#if ! defined ENABLE_GETA
	char wamdir[MAXPATHLEN];
	char *itb = NULL;
	char *tsv = NULL;
	char *otmi = NULL;
#endif
	char itb_s[MAXPATHLEN];
	char otmi_s[MAXPATHLEN];
	char tsv_s[MAXPATHLEN];

	char catalogue[MAXPATHLEN];
	char wamsdir[MAXPATHLEN];
	char index_key[MAXPATHLEN];

#if ! defined ENABLE_GETA
	char *stemmer = NULL;
#endif

	BEGIN();

#if defined S_IWOTH
	/*umask(S_IWGRP|S_IWOTH);*/
	umask(S_IWOTH);
#endif

	switch (mode) {
		char *path_info;
		char *script_name;
		char *script_filename;
		size_t n;
		char *v[3];
		char buf[1024];
#if ! defined ENABLE_NEWLAYOUT
	case CGIPOST:
		if (!(script_name = getenv("SCRIPT_NAME"))) {
			fprintf(stderr, "SCRIPT_NAME is not set\n");
			usage();
		}
		if ((cmd = decode_command(script_name)) == ERR) {
			fprintf(stderr, "%s: unknown command\n", script_name);
			return -1;
		}
		if (!(path_info = getenv("PATH_INFO"))) {
			fprintf(stderr, "PATH_INFO is not set\n");
			usage();
		}
		if (*path_info != '/') {
			fprintf(stderr, "parse error: path_info = [%s]\n", path_info);
			return -1;
		}
		if (!(script_filename = getenv("SCRIPT_FILENAME"))) {
			fprintf(stderr, "SCRIPT_FILENAME is not set\n");
			usage();
			return -1;
		}
#if defined STATIC_GETAROOT
		getaroot = STATIC_GETAROOT;
#else
		if (!(getaroot = strip2(strdup(script_filename)))) {
			return -1;
		}
#endif
		n = splitv(strdup(path_info + 1), '/', v, nelems(v), 0);
		if (n != 1 && n != 2 && n != 3) {
			fprintf(stderr, "arg # mismatch\n");
			return -1;
		}
		handle = v[0];
		switch (cmd) {
#if defined ENABLE_GETA
#if defined BIN_PAX
		case STP:
#endif
#endif
		case STB:
		case STO:
		case STU:
			if (n != 2) {
				fprintf(stderr, "arg # mismatch\n");
				return -1;
			}
			ts = v[1];
			break;
		case RMV:
			if (n != 1) {
				fprintf(stderr, "arg # mismatch\n");
				return -1;
			}
			ts = NULL;
			break;
#if defined ENABLE_GETA
		case CAT:
#endif
		case STT:
			fprintf(stderr, "not allowed\n");
			return -1;
		default:
			fprintf(stderr, "internal error (a)\n");
			return -1;
		}
		break;
#endif
	case SSH:
		getaroot = argv[1];
		if (!(script_name = ngetline(buf, sizeof buf, stdin))) {
			perror("ngetline/scrit_name");
			return -1;
		}
		if ((cmd = decode_command(script_name)) == ERR) {
			fprintf(stderr, "%s: unknown command\n", script_name);
			return -1;
		}
		if (!(handle = ngetline(buf, sizeof buf, stdin))) {
			perror("ngetline/handle");
			return -1;
		}
		switch (cmd) {
#if defined ENABLE_GETA
#if defined BIN_PAX
		case STP:
#endif
#endif
		case STB:
		case STO:
		case STU:
			if (!(ts = ngetline(buf, sizeof buf, stdin))) {
				perror("ngetline/ts");
				return -1;
			}
			break;
		case RMV:
#if defined ENABLE_GETA
		case CAT:
#endif
			ts = NULL;
			break;
		case STT:
			fprintf(stderr, "not allowed\n");
			return -1;
		default:
			fprintf(stderr, "internal error (b)\n");
			return -1;
		}
		break;
	case LOCAL:
		getaroot = argv[1];
		if ((cmd = decode_command(argv[0])) == ERR) {
			fprintf(stderr, "%s: unknown command\n", argv[0]);
			return -1;
		}
		handle = argv[2];
		switch (cmd) {
#if defined ENABLE_GETA
#if defined BIN_PAX
		case STP:
#endif
#endif
		case STB:
		case STO:
		case STU:
		case STT:
			switch (argc) {
#if ! defined ENABLE_GETA
			case 6:
				assert(cmd == STB || cmd == STU);
				if (cmd == STB) {
					itb = argv[5];
				}
				else if (cmd == STU) {
					tsv = argv[5];
				}
				/* FALLTHRU */
			case 5:
				assert(cmd == STB || cmd == STU);
				stemmer = argv[4];
				/* FALLTHRU */
#endif
			case 4:
				ts = argv[3];
				break;
			default:
				fprintf(stderr, "internal error (c)\n");
				return -1;
			}
			break;
#if ! defined ENABLE_GETA
		case DEL:
#endif
		case RMV:
			if (argc != 3) {
				fprintf(stderr, "internal error (d)\n");
				return -1;
			}
			ts = NULL;
			break;
#if defined ENABLE_GETA
		case CAT:
			fprintf(stderr, "not allowed\n");
			return -1;
#endif
		default:
			fprintf(stderr, "internal error (e)\n");
			return -1;
		}
		break;
	default:
		fprintf(stderr, "internal error (f)\n");
		return -1;
	}

#if 0
	snprintf(envs, sizeof envs, "TMPDIR=%s" DIRECTORY_DELIMITER "tmp", getaroot);
	if (!(es = strdup(envs))) {
		perror("strdup");
		return -1;
	}
	if (putenv(es) != 0) {
		perror("putenv");
		return -1;
	}
#endif
#if defined MKRI_NCPU
	snprintf(envs, sizeof envs, "PTHREAD_CONCURRENCY=%d", MKRI_NCPU);
	if (!(es = strdup(envs))) {
		perror("strdup");
		return -1;
	}
	if (putenv(es) != 0) {
		perror("putenv");
		return -1;
	}
#endif

fprintf(stderr, "command = %d\n", cmd);

	if (!is_sound_handle_name(handle)) {
		fprintf(stderr, "soundness check failed: handle = [%s]\n", handle);
		return -1;
	}

	switch (cmd) {
#if defined ENABLE_GETA
#if defined BIN_PAX
	case STP:
#endif
#endif
	case STB:
	case STO:
	case STU:
	case STT:
		if (!ts) {
			fprintf(stderr, "internal error (g)\n");
			return -1;
		}
		break;
#if ! defined ENABLE_GETA
	case DEL:
#endif
	case RMV:
#if defined ENABLE_GETA
	case CAT:
#endif
		if (ts) {
			fprintf(stderr, "internal error (h)\n");
			return -1;
		}
		break;
	default:
		fprintf(stderr, "internal error (i)\n");
		return -1;
	}

	PRINT_ELAPSED(NULL, stderr);

	fprintf(stderr, "handle = %s\n", handle);

	switch (cmd) {
		size_t i;
		char *tsp;
#if defined ENABLE_GETA
#if defined BIN_PAX
	case STP:
#endif
#endif
	case STB:
	case STO:
	case STU:
	case STT:
		fprintf(stderr, "types = %s\n", ts);
		if (!(tsp = strdup(ts))) {
			perror("strdup");
			return -1;
		}
		types[0] = "@frq";
		ntypes = splitv(tsp, ',', types + 1, nelems(types) - 1, 0) + 1;
		for (i = 1; i < ntypes; i++) {
			if (!strcmp(types[i], FSSKEY)) {
				if (cmd == STO) {
					fprintf(stderr, FSSKEY " is not supported in XML format\n");
					return -1;
				}
				else {
					continue;
				}
			}
			if (!is_sound_prop_name(types[i])) {
				fprintf(stderr, "soundness check failed: ts[%d] = [%s]\n", (int)i, types[i]);
				return -1;
			}
		}
		if (prepare_workdir(getaroot, handle, workdir, sizeof workdir, cmd != STT, cmd != STT) == -1) {
			fprintf(stderr, "could not create workdir\n");
			return -1;
		}
		break;
	case RMV:
		break;
#if ! defined ENABLE_GETA
	case DEL:
#endif
#if defined ENABLE_GETA
	case CAT:
#endif
		if (prepare_workdir(getaroot, handle, workdir, sizeof workdir, 0, 0) == -1) {
			fprintf(stderr, "could not change to workdir\n");
			return -1;
		}
		break;
	default:
		fprintf(stderr, "internal error (j)\n");
		return -1;
	}

	snprintf(itb_s, sizeof itb_s, "%s" DIRECTORY_DELIMITER "%s.@itb", workdir, handle);
	snprintf(otmi_s, sizeof otmi_s, "%s" DIRECTORY_DELIMITER "%s.@otmi", workdir, handle);
	snprintf(tsv_s, sizeof tsv_s, "%s" DIRECTORY_DELIMITER "%s.@tsv", workdir, handle);

	switch (cmd) {
#if defined ENABLE_GETA
#if defined BIN_PAX
		size_t i;
	case STP:
		for (i = 1; i < ntypes; i++) {
			if (!strcmp(types[i], FSSKEY)) {
				fprintf(stderr, "stp: cannot use %s\n", types[i]);
				return -1;
			}
		}
		fprintf(stderr, "##### expanding archive...\n");
		if (expand_archive() == -1) {
			fprintf(stderr, "expand archive failed\n");
			return -1;
		}
		break;
#endif
#endif
	case STB:
	case STO:
	case STU:
		switch (cmd) {
			char otmi2itb[MAXPATHLEN];
			char tsv2itb[MAXPATHLEN];
		case STO:
			if (!otmi) {
				otmi = otmi_s;
				receive_file(otmi);
			}
			fprintf(stderr, "##### generating ITB...\n");
#if defined ENABLE_NEWLAYOUT
			snprintf(otmi2itb, sizeof otmi2itb, "%s", OTMI2ITB);
#else
			snprintf(otmi2itb, sizeof otmi2itb, "%s%s", getaroot, OTMI2ITB);
#endif
			if (convert_otmi(otmi, itb_s, otmi2itb) == -1) {
				fprintf(stderr, "otmi2itb failed\n");
				return -1;
			}
			itb = itb_s;
			break;
		case STU:
			if (!tsv) {
				tsv = tsv_s;
				receive_file(tsv);
			}
			fprintf(stderr, "##### generating ITB...\n");
#if defined ENABLE_NEWLAYOUT
			snprintf(tsv2itb, sizeof tsv2itb, "%s", TSV2ITB);
#else
			snprintf(tsv2itb, sizeof tsv2itb, "%s%s", getaroot, TSV2ITB);
#endif
			if (convert_tsv(tsv, itb_s, tsv2itb) == -1) {
				fprintf(stderr, "tsv2itb failed\n");
				return -1;
			}
			itb = itb_s;
			break;
		case STB:
			if (!itb) {
				itb = itb_s;
				receive_file(itb_s);
			}
			break;
		}
#if defined ENABLE_GETA
		for (i = 1; i < ntypes; i++) {
			if (!strcmp(types[i], FSSKEY)) {
				mkrip = 1;
				for (ntypes--; i < ntypes; i++) {
					types[i] = types[i + 1];
				}
				break;
			}
		}
		switch (cmd) {
			char itb2frq[MAXPATHLEN];
		case STO:
		case STU:
		case STB:
			fprintf(stderr, "##### stemming...\n");
			snprintf(itb2frq, sizeof itb2frq, "%s%s", getaroot, ITB2FRQ);
			if (convert_itb(handle, types, ntypes, itb, itb2frq) == -1) {
				fprintf(stderr, "itb2frq failed\n");
				return -1;
			}
			break;
		default:
			fprintf(stderr, "internal error\n");
			return -1;
		}
#endif
		break;
#if ! defined ENABLE_GETA
	case DEL:
		break;
#endif
	case RMV:
#if ! defined ENABLE_GETA
		snprintf(wamdir, sizeof wamdir, "%s" DIRECTORY_DELIMITER DWAM DIRECTORY_DELIMITER "%s", getaroot, handle);
		if (drop_wam(getaroot, wamdir) == -1) {
			perror(wamdir);
			return -1;
		}
#else
		if (remove_wam(getaroot, handle) == -1) {
			fprintf(stderr, "remove wam failed\n");
			return -1;
		}
#endif
		break;
#if defined ENABLE_GETA
	case CAT:
#endif
		if (cat_itb(itb_s) == -1) {
			fprintf(stderr, "cat itb failed\n");
			return -1;
		}
		break;
	case STT:
		break;
	default:
		fprintf(stderr, "internal error (k)\n");
		return -1;
	}

	PRINT_ELAPSED(NULL, stderr);

	switch (cmd) {
		char newinf[MAXPATHLEN];
#if defined ENABLE_GETA
#if defined BIN_PAX
	case STP:
#endif
#endif
#if ! defined ENABLE_GETA
		char wamdir[MAXPATHLEN];
#endif
	case STB:
	case STO:
	case STU:
	case STT:
		fprintf(stderr, "##### creating WAM...\n");
		if (
#if ! defined ENABLE_GETA
			gen_wam(getaroot, handle, ts, workdir, wamdir, sizeof wamdir, xgopt, uflag, itb, stemmer)
#else
			gen_wam(getaroot, handle, types, ntypes, workdir, mkrip)
#endif
			== -1) {
			fprintf(stderr, "wam generation failed\n");
			return -1;
		}
fprintf(stderr, "wamdir = %s\n", wamdir);
#if ! defined ENABLE_GETA
#define	INFKEY	"!.inf"
#define	INFDIR	wamdir
/* XXX: INFKEY MUST match that is defined in nwamP.h */
#else
#define	INFKEY	".@inf"
#define	INFDIR	workdir
#endif
		if (findafile_match_tail(INFDIR, inf, sizeof inf, INFKEY) == -1) {
			return -1;
		}
		if (!*inf) {
			return -1;
		}
#undef	INFKEY
#undef	INFDIR
#define	CATAINF	"@.inf"
		fprintf(stderr, "inf = %s\n", inf);
		snprintf(newinf, sizeof newinf, "%s" DIRECTORY_DELIMITER DWAM DIRECTORY_DELIMITER "%s" DIRECTORY_DELIMITER CATAINF, getaroot, handle);
		fprintf(stderr, "newinf = %s\n", newinf);
		if (gen_newinf(inf, newinf, handle, types, ntypes, mkrip, FSSKEY) == -1) {
			fprintf(stderr, "could not create %s\n", inf);
			return -1;
		}
		break;
#if ! defined ENABLE_GETA
	case DEL:
		if (del_arts(getaroot, handle, workdir, wamdir, sizeof wamdir) == -1) {
			fprintf(stderr, "delete articles failed\n");
			return -1;
		}
		break;
#endif
	case RMV:
#if defined ENABLE_GETA
	case CAT:
#endif
		break;
	default:
		fprintf(stderr, "internal error (l)\n");
		return -1;
	}

	snprintf(catalogue, sizeof catalogue, "%s" DIRECTORY_DELIMITER ETCDIR DIRECTORY_DELIMITER CATALOGUE_XML, getaroot);
	snprintf(wamsdir, sizeof wamsdir, "%s" DIRECTORY_DELIMITER DWAM, getaroot);
	snprintf(index_key, sizeof index_key, "%s" DIRECTORY_DELIMITER ETCDIR DIRECTORY_DELIMITER INDEX_KEY, getaroot);

	if (gen_catalog(
#if defined ENABLE_GETA
		getaroot, 
#endif
		catalogue, wamsdir, index_key, CATAINF) == -1) {
		fprintf(stderr, "catalogue failed\n");
		return -1;
	}

	PRINT_ELAPSED(NULL, stderr);

	return 0;
}

static int
decode_command(script_name)
char *script_name;
{
	struct on {
		char *name;
		int cmd;
	};

	static struct on optnames[] = {
#if defined ENABLE_GETA
#if defined BIN_PAX
		{"stp", STP}, {"-p", STP},
#endif
#endif
		{"stb", STB}, {"-b", STB},
		{"sto", STO}, {"-o", STO},
		{"stu", STU}, {"-u", STU},
		{"rmv", RMV}, {"-r", RMV},
		{"-t", STT},
#if ! defined ENABLE_GETA
		{"-d", DEL},
#endif
#if defined ENABLE_GETA
		{"cat", CAT},
#endif
	};
	char *command;
	size_t i;
	if (command = rindex(script_name, '/')) {
		command++;
	}
	else {
		command = script_name;
	}
	for (i = 0; i < nelems(optnames); i++) {
		struct on *o = &optnames[i];
		if (!strcmp(command, o->name)) {
			return o->cmd;
		}
	}
	return ERR;
}

static int
prepare_workdir(getaroot, handle, workdir, size, cleanup, create)
char const *getaroot, *handle;
char *workdir;
size_t size;
{
	struct stat sb;

	snprintf(workdir, size, "%s" DIRECTORY_DELIMITER WORKDIR DIRECTORY_DELIMITER "%s", getaroot, handle);
	if (cleanup) {
		if (stat(workdir, &sb) != -1) {
fprintf(stderr, "%s exists: cleanup\n", workdir);
			if (rm_r(workdir) == -1) {
fprintf(stderr, "rm -r failed!!!!\n");
				return -1;
			}
		}
	}
	if (create) {
		if (stat(workdir, &sb) == -1) {
			if (mkdir(workdir, 0777) == -1) {
				perror(workdir);
				return -1;
			}
		}
	}

#if defined ENABLE_GETA
fprintf(stderr, "chdir %s\n", workdir);
	if (chdir(workdir) == -1) {
		perror("chdir");
		return -1;
	}
#endif
	return 0;
}

static int
cat_itb(itb)
char const *itb;
{
	FILE *f;
	if (!(f = fopen(itb, "r"))) {
		perror(itb);
		return -1;
	}
	if (copy(f, stdout) == -1) {
		perror(itb);
		fclose(f);
		return -1;
	}
	fclose(f);

	return 0;
}

#if ! defined ENABLE_GETA

static int
gen_wam(getaroot, handle, types, workdir, wamdir, size, xgopt, uflag, itblist, stemmer)
char const *getaroot, *handle, *types, *workdir, *itblist, *stemmer;
char *wamdir;
size_t size;
char *xgopt;
{
	char xgetassoc[MAXPATHLEN];
	char *cmd;
	char *xgargv[32];
	size_t nxgargv = 0;
	char *argv[nelems(xgargv) + 32];
	int k;
	size_t i;

	char *handle_tmp = NULL;
	char *dtmp;
	struct spl spl = {-1, };

	if (xgopt) {
		nxgargv = splitv(strdup(xgopt), ',', xgargv, nelems(xgargv), 0);
		if (nxgargv >= nelems(xgargv)) {
			fprintf(stderr, "too may arguments to xgetassoc");
			goto fail;
		}
	}
	if (!uflag) {
		SPL0(&spl);
		snprintf(wamdir_tmp, sizeof wamdir_tmp, "%s" DIRECTORY_DELIMITER DWAM DIRECTORY_DELIMITER "XXXXXX", getaroot);
		if (!(dtmp = mkdtemp(wamdir_tmp))) {
			perror(wamdir_tmp);
			wamdir_tmp[0] = '\0';
		}
		SPLX(&spl);

		if (!dtmp) {
			goto fail;
		}

		if (!(handle_tmp = rindex(wamdir_tmp, DIRECTORY_DELIMITER[0]))) {
			fprintf(stderr, "internal error: !handle_tmp\n");
			goto fail;
		}
		handle_tmp++;
		fprintf(stderr, "handle_tmp = %s\n", handle_tmp);
	}

	snprintf(wamdir, size, "%s" DIRECTORY_DELIMITER DWAM DIRECTORY_DELIMITER "%s", getaroot, handle);
#if defined ENABLE_NEWLAYOUT
	snprintf(xgetassoc, sizeof xgetassoc, "%s", XGETASSOC);
#else
	snprintf(xgetassoc, sizeof xgetassoc, "%s%s", getaroot, XGETASSOC);
#endif

fprintf(stderr, "gen_wam(\"%s\", \"%s\", \"%s\", \"%s\")\n", getaroot, handle, types, workdir);
fprintf(stderr, "getaroot = %s\n", getaroot);
fprintf(stderr, "wamdir = %s\n", wamdir);
fprintf(stderr, "xgetassoc = %s\n", xgetassoc);
fprintf(stderr, "wamdir_tmp = %s\n", wamdir_tmp);
fprintf(stderr, "handle_tmp = %s\n", handle_tmp ? handle_tmp : "(null)");

	k = 0;
	cmd = xgetassoc;
	argv[k++] = cmd;
	argv[k++] = XGETASSOC_SETUP;
	argv[k++] = "-k";
	argv[k++] = types;
	for (i = 0; i < nxgargv; i++) {
		argv[k++] = xgargv[i];
	}
	argv[k++] = "-t";
	argv[k++] = workdir;
	argv[k++] = "-N";
	if (stemmer) {
		argv[k++] = "-s";
		argv[k++] = stemmer;
	}
	argv[k++] = itblist;
	if (uflag) {
		argv[k++] = wamdir;
	}
	else {
		argv[k++] = wamdir_tmp;
	}
	argv[k++] = NULL;
	assert(k <= nelems(argv));
	if (systemv(cmd, argv) == -1) {
		goto fail;
	}

	if (!uflag) {
		chmod(wamdir_tmp, 0777);

		SPL0(&spl);
		if (rename_wam(getaroot, wamdir_tmp, wamdir) == -1) {
			drop_wam(getaroot, wamdir_tmp);
		}
		wamdir_tmp[0] = '\0';
		SPLX(&spl);
	}

	return 0;
fail:
	if (wamdir_tmp[0]) rm_r(wamdir_tmp);
	return -1;
}

static int
rename_wam(getaroot, from, to)
char const *getaroot, *from, *to;
{
	char to_xxx[MAXPATHLEN];
	struct stat sb;

	if (stat(to, &sb) == -1) {
		return rename_wam_0(getaroot, from, to);
	}
	else {	/* to exists! */
		char *p;
		if (strlen(from) >= sizeof to_xxx) {
			fprintf(stderr, "too long name\n");
			return -1;
		}
		strcpy(to_xxx, from);
#define	TMPL	"XXXXXX"
		if (p = rindex(to_xxx, DIRECTORY_DELIMITER[0])) {
			p++;
			if (strlen(TMPL) >= sizeof to_xxx - (p - to_xxx)) {
				fprintf(stderr, "too long name\n");
				return -1;
			}
			strcpy(p, TMPL);
		}
		else {
			strcpy(to_xxx, TMPL); /* good */
		}

		if (!mkdtemp(to_xxx)) {		/* make empty backup */
			perror(to_xxx);
			return -1;
		}
#if defined NO_OVERWRITE_ON_RENAME
		rm_r(to_xxx);
#endif
		if (rename_wam_0(getaroot, to, to_xxx) == -1) {	/* backup failed */
			rmdir(to_xxx);
			perror("rename");
			return -1;
		}
		if (rename_wam_0(getaroot, from, to) == -1) {
			rename_wam_0(getaroot, to_xxx, to);	/* undo */
			return -1;
		}
		drop_wam(getaroot, to_xxx);			/* remove backup */
	}
	return 0;
}

static int
rename_wam_0(getaroot, from, to)
char const *getaroot, *from, *to;
{
	char xgetassoc[MAXPATHLEN];
	char *cmd;
	char *argv[32];
	int k;

#if defined ENABLE_NEWLAYOUT
	snprintf(xgetassoc, sizeof xgetassoc, "%s", XGETASSOC);
#else
	snprintf(xgetassoc, sizeof xgetassoc, "%s%s", getaroot, XGETASSOC);
#endif

fprintf(stderr, "rename(\"%s\", \"%s\")\n", from, to);

	k = 0;
	cmd = xgetassoc;
	argv[k++] = cmd;
	argv[k++] = XGETASSOC_RENAME;
	argv[k++] = "-N";
	argv[k++] = from;
	argv[k++] = to;
	argv[k++] = NULL;
	assert(k <= nelems(argv));
	if (systemv(cmd, argv) == -1) {
		return -1;
	}
	return 0;
}

static int
drop_wam(getaroot, handle)
char const *getaroot, *handle;
{
	char xgetassoc[MAXPATHLEN], inf[MAXPATHLEN];
	char *cmd;
	char *argv[32];
	int k;

fprintf(stderr, "drop(\"%s\")\n", handle);
fprintf(stderr, "getaroot = \"%s\"\n", getaroot);

	snprintf(inf, sizeof inf, "%s" DIRECTORY_DELIMITER CATAINF, handle);
fprintf(stderr, "unlink(\"%s\")\n", inf);

	if (unlink(inf) == -1) {
		perror(inf);
		/* return -1; */
	}

#if defined ENABLE_NEWLAYOUT
	snprintf(xgetassoc, sizeof xgetassoc, "%s", XGETASSOC);
#else
	snprintf(xgetassoc, sizeof xgetassoc, "%s%s", getaroot, XGETASSOC);
#endif

	k = 0;
	cmd = xgetassoc;
	argv[k++] = cmd;
	argv[k++] = XGETASSOC_DROP;
	argv[k++] = "-N";
	argv[k++] = handle;
	argv[k++] = NULL;
	assert(k <= nelems(argv));
	if (systemv(cmd, argv) == -1) {
		perror(handle);
		return -1;
	}
	return 0;
}

static int
del_arts(getaroot, handle, workdir, wamdir, size)
char const *getaroot, *handle, *workdir;
char *wamdir;
size_t size;
{
	char xgetassoc[MAXPATHLEN];
	char *cmd;
	char *argv[32];
	int k;

	snprintf(wamdir, size, "%s" DIRECTORY_DELIMITER DWAM DIRECTORY_DELIMITER "%s", getaroot, handle);
#if defined ENABLE_NEWLAYOUT
	snprintf(xgetassoc, sizeof xgetassoc, "%s", XGETASSOC);
#else
	snprintf(xgetassoc, sizeof xgetassoc, "%s%s", getaroot, XGETASSOC);
#endif

fprintf(stderr, "del_arts(\"%s\", \"%s\", \"%s\")\n", getaroot, handle, workdir);
fprintf(stderr, "getaroot = %s\n", getaroot);
fprintf(stderr, "wamdir = %s\n", wamdir);
fprintf(stderr, "xgetassoc = %s\n", xgetassoc);

	k = 0;
	cmd = xgetassoc;
	argv[k++] = cmd;
	argv[k++] = XGETASSOC_DELARTS;
	argv[k++] = "-t";
	argv[k++] = workdir;
	argv[k++] = "-N";
	argv[k++] = wamdir;
	argv[k++] = NULL;
	assert(k <= nelems(argv));
	if (systemv(cmd, argv) == -1) {
		return -1;
	}
	return 0;
}

#else /* ! ENABLE_GETA */

static int
gen_wam(getaroot, handle, types, ntypes, workdir, mkrip)
char const *getaroot, *handle, *workdir;
char * const *types;
size_t ntypes;
{
	struct find_pat f0;
	size_t i, j;
	char wamdir[MAXPATHLEN];
	char mkdw[MAXPATHLEN];
	char mkpi[MAXPATHLEN];
	char sched_conf[MAXPATHLEN];
	char mkri[MAXPATHLEN];

	char *cmode = "3";
	char *v0size = "536870912"; /* 512 * 1024 * 1024 */
	char *compress = "none";
	char *hash = "-h";
	char *realm = "local";
	char *np = "1";
	char *ioptions = ""; /* = "ignore-case,ignore-space,collapse-zdz"; */

	char *cmd;
	char *argv[32];
	int k;

	char *handle_tmp;
	char *dtmp;
	struct spl spl = {-1, };

	SPL0(&spl);
	snprintf(wamdir_tmp, sizeof wamdir_tmp, "%s" DIRECTORY_DELIMITER DWAM DIRECTORY_DELIMITER "XXXXXX", getaroot);
	if (!(dtmp = mkdtemp(wamdir_tmp))) {
		perror(wamdir_tmp);
		wamdir_tmp[0] = '\0';
	}
	SPLX(&spl);

	if (!dtmp) {
		goto fail;
	}

	if (!(handle_tmp = rindex(wamdir_tmp, DIRECTORY_DELIMITER[0]))) {
		fprintf(stderr, "internal error: !handle_tmp\n");
		goto fail;
	}
	handle_tmp++;
	fprintf(stderr, "handle_tmp = %s\n", handle_tmp);

	snprintf(wamdir, sizeof wamdir, "%s" DIRECTORY_DELIMITER DWAM DIRECTORY_DELIMITER "%s", getaroot, handle);
	snprintf(mkdw, sizeof mkdw, "%s%s", getaroot, MKDW);
	snprintf(mkpi, sizeof mkpi, "%s%s", getaroot, MKPI);
	snprintf(mkri, sizeof mkri, "%s%s", getaroot, MKRI);
	snprintf(sched_conf, sizeof sched_conf, "%s%s", getaroot, SCHED_CONF);

	{
		NFILE *g;
		char waoptions[MAXPATHLEN];

		snprintf(waoptions, sizeof waoptions, "%s" DIRECTORY_DELIMITER "%s-0.#@options", workdir, handle);
		if (g = nopen(waoptions, "r")) {
			char *buf;
			while ((buf = readln(g)) && *buf) {
				chop(buf);
				if (!strncmp(buf, "$cmode=", 7)) {
					cmode = strdup(buf + 7);
				}
				else if (!strncmp(buf, "$v0size=", 8)) {
					v0size = strdup(buf + 8);
				}
				else if (!strncmp(buf, "$compress=", 10)) {
					compress = strdup(buf + 10);
				}
				else if (!strncmp(buf, "$hash=", 6)) {
					hash = strdup(buf + 6);
				}
				else if (!strncmp(buf, "$realm=", 7)) {
					realm = strdup(buf + 7);
				}
				else if (!strncmp(buf, "$np=", 4)) {
					np = strdup(buf + 4);
				}
				else if (!strncmp(buf, "$ioptions=", 10)) {
					ioptions = strdup(buf + 10);
				}
			}
			nfclose(g);
		}
fprintf(stderr, "cmode = %s\n", cmode);
fprintf(stderr, "v0size = %s\n", v0size);
fprintf(stderr, "compress = %s\n", compress);
fprintf(stderr, "hash = %s\n", hash);
fprintf(stderr, "realm = %s\n", realm);
fprintf(stderr, "np = %s\n", np);
fprintf(stderr, "ioptions = %s\n", ioptions);
	}

	for (i = 0; i < ntypes; i++) {
		char path[MAXPATHLEN];
		char *type = types[i];
		FILE *g;
		char typei[MAXPATHLEN], apath[MAXPATHLEN];
		char stype[MAXPATHLEN];

		snprintf(path, sizeof path, "%s" DIRECTORY_DELIMITER "#%s", workdir, type);
		f0.n = 0;
		f0.size = 0;
		f0.p = NULL;
		snprintf(stype, sizeof stype, ".#%s", type);
		f0.t = stype;
		f0.tlen = strlen(f0.t);
fprintf(stderr, "workdir = %s\n", workdir);
		if (traverse_fn(workdir, fn_find_pat_match_tail, &f0)) {
			perror(workdir);
		}
		if (!(g = fopen(path, "w"))) {
			perror(path);
			goto fail;
		}
		for (j = 0; j < f0.n; j++) {
			fprintf(g, "%s\n", f0.p[j]);
		}
		fclose(g);
		for (j = 0; j < f0.n; j++) {
			free(f0.p[j]);
		}
		free(f0.p);

		if (!strcmp(type, "@frq")) {
			char schedule[MAXPATHLEN];
			assert(i == 0);
			if (!strcmp(realm, "local")) {
				snprintf(schedule, sizeof schedule, "1.1.l");
			}
			else {
				snprintf(schedule, sizeof schedule, "%s.%s.hosts.%s" DIRECTORY_DELIMITER "etc" DIRECTORY_DELIMITER "hosts.%s", np, np, getaroot, realm);
			}
			k = 0;
			snprintf(apath, sizeof apath, "@%s", path);
			cmd = mkdw;
			argv[k++] = cmd;
			argv[k++] = "-sched-conf"; argv[k++] = sched_conf;
			argv[k++] = "-compress"; argv[k++] = compress;
			argv[k++] = "-c"; argv[k++] = cmode;
			if (hash && *hash) argv[k++] = hash;
			argv[k++] = "-v0size"; argv[k++] = v0size;
			argv[k++] = (char *)getaroot;
			argv[k++] = (char *)handle_tmp;
			argv[k++] = apath;
			argv[k++] = schedule;
			argv[k++] = NULL;
			assert(k <= nelems(argv));
		}
		else {
			assert(i != 0);
			k = 0;
			snprintf(typei, sizeof typei, "%si", type);
			snprintf(apath, sizeof apath, "@%s", path);
			cmd = mkpi;
			argv[k++] = cmd;
			argv[k++] = (char *)getaroot;
			argv[k++] = (char *)handle_tmp;
			argv[k++] = "row";
			argv[k++] = typei;
			argv[k++] = apath;
			argv[k++] = NULL;
			assert(k <= nelems(argv));
		}
		if (systemv(cmd, argv) == -1) {
			goto fail;
		}
	}
	if (mkrip) {
		char waflwd[MAXPATHLEN], wanames[MAXPATHLEN];
		char aflwd[MAXPATHLEN], anames[MAXPATHLEN], aidx[MAXPATHLEN], amap[MAXPATHLEN];

		PRINT_ELAPSED(NULL, stderr);

		fprintf(stderr, "##### creating index for free text search...\n");

		k = 0;
		snprintf(waflwd, sizeof waflwd, "%s" DIRECTORY_DELIMITER "%s-0.#@flwd", workdir, handle);
		snprintf(wanames, sizeof wanames, "%s" DIRECTORY_DELIMITER "%s-0.#@names", workdir, handle);
		snprintf(aflwd, sizeof aflwd, "%s" DIRECTORY_DELIMITER "@.flwd", wamdir_tmp);
		snprintf(anames, sizeof anames, "%s" DIRECTORY_DELIMITER "@.names", wamdir_tmp);

		/* note that we cannot use `rename' here! */
		mv(waflwd, aflwd);
		mv(wanames, anames);

		snprintf(aidx, sizeof aidx, "%s" DIRECTORY_DELIMITER "@.idx", wamdir_tmp);
		snprintf(amap, sizeof amap, "%s" DIRECTORY_DELIMITER "@.map", wamdir_tmp);
		cmd = mkri;
		argv[k++] = cmd;
		argv[k++] = (char *)getaroot;
		argv[k++] = (char *)handle_tmp;
		argv[k++] = aflwd;
		argv[k++] = anames;
		argv[k++] = aidx;
		argv[k++] = amap;
		argv[k++] = ioptions;
		argv[k++] = NULL;
		assert(k <= nelems(argv));
		if (systemv(cmd, argv) == -1) {
			goto fail;
		}
	}

	chmod(wamdir_tmp, 0777);

	SPL0(&spl);
	if (rename_dir(wamdir_tmp, wamdir) == -1) {
		rm_r(wamdir_tmp);
	}
	wamdir_tmp[0] = '\0';
	SPLX(&spl);

	if (strcmp(realm, "local")) {
		fprintf(stderr, "%s" DIRECTORY_DELIMITER "sbin" DIRECTORY_DELIMITER "dwdctl rename %s %s %s\n", getaroot, realm, handle_tmp, handle);
	}

	return 0;
fail:
	if (wamdir_tmp[0]) rm_r(wamdir_tmp);
	return -1;
}

static int
rename_dir(from, to)
char const *from, *to;
{
	char to_xxx[MAXPATHLEN];
	struct stat sb;

	if (stat(to, &sb) == -1) {
		return rename(from, to);
	}
	else {	/* to exists! */
		char *p;
		if (strlen(from) >= sizeof to_xxx) {
			fprintf(stderr, "too long name\n");
			return -1;
		}
		strcpy(to_xxx, from);
#define	TMPL	"XXXXXX"
		if (p = rindex(to_xxx, DIRECTORY_DELIMITER[0])) {
			p++;
			if (strlen(TMPL) >= sizeof to_xxx - (p - to_xxx)) {
				fprintf(stderr, "too long name\n");
				return -1;
			}
			strcpy(p, TMPL);
		}
		else {
			strcpy(to_xxx, TMPL); /* good */
		}

		if (!mkdtemp(to_xxx)) {		/* make empty backup */
			perror(to_xxx);
			return -1;
		}
		if (rename(to, to_xxx) == -1) {	/* backup failed */
			rmdir(to_xxx);
			perror("rename");
			return -1;
		}
		if (rename(from, to) == -1) {
			rename(to_xxx, to);	/* undo */
			return -1;
		}
		rm_r(to_xxx);			/* remove backup */
	}
	return 0;
}

static int
remove_wam(getaroot, handle)
char const *getaroot, *handle;
{
	char wamdir[MAXPATHLEN];
	snprintf(wamdir, sizeof wamdir, "%s" DIRECTORY_DELIMITER DWAM DIRECTORY_DELIMITER "%s", getaroot, handle);
fprintf(stderr, "remove: %s\n", wamdir);
	return rm_r(wamdir);
}
#endif /* ! ENABLE_GETA */

static void
sigint(n)
{
	if (wamdir_tmp[0]) {
		fprintf(stderr, "remove %s\n", wamdir_tmp);
		rm_r(wamdir_tmp);
	}
	if (catalogue_tmp[0]) {
		fprintf(stderr, "remove %s\n", catalogue_tmp);
		unlink(catalogue_tmp);
	}
	fprintf(stderr, "%s: killed %d\n", progname, n);
	_exit(1);
}
