/*	$Id: archive.c,v 1.5 2011/09/14 03:06:08 nis Exp $	*/

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
static char rcsid[] = "$Id: archive.c,v 1.5 2011/09/14 03:06:08 nis Exp $";
#endif

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

#include "stpP.h"

#include <gifnoc.h>

int
convert_otmi(otmi, itb, otmi2itb)
char const *otmi, *itb, *otmi2itb;
{
	char *cmd;
	char *argv[32];
	int k;

	k = 0;
	cmd = otmi2itb;
	argv[k++] = cmd;
	argv[k++] = "-o";
	argv[k++] = itb;
	argv[k++] = otmi;
	argv[k++] = NULL;
	assert(k <= nelems(argv));
	if (systemv(cmd, argv) == -1) {
		return -1;
	}
	return 0;
}

int
convert_tsv(tsv, itb, tsv2itb)
char const *tsv, *itb, *tsv2itb;
{
	char *cmd;
	char *argv[32];
	int k;

	k = 0;
	cmd = tsv2itb;
	argv[k++] = cmd;
	argv[k++] = "-o";
	argv[k++] = itb;
	argv[k++] = tsv;
	argv[k++] = NULL;
	assert(k <= nelems(argv));
	if (systemv(cmd, argv) == -1) {
		return -1;
	}
	return 0;
}

#if defined BIN_RM	/* ==> /bin/rm will not be used */
static int rm(char const *, void *);

static int
rm(path, cookie)
char const *path;
void *cookie;
{
	fprintf(stderr, "unlink(\"%s\")\n", path);
	return unlink(path);
}

int
rm_r(path)
char const *path;
{
	if (traverse_fn(path, rm, NULL) != 0) {
		return -1;
	}
	fprintf(stderr, "rmdir(\"%s\")\n", path);
	return rmdir(path);
#if 0
	char *cmd;
	char *argv[4];
	int k;

	k = 0;
	cmd = BIN_RM;
	argv[k++] = cmd;
	argv[k++] = "-r";
	argv[k++] = (char *)path;
	argv[k++] = NULL;
	assert(k <= nelems(argv));
	if (systemv(cmd, argv) == -1) {
		return -1;
	}
	return 0;
#endif
}
#endif

#if defined ENABLE_GETA
int
mv(source, target)
char const *source, *target;
{
	char *cmd;
	char *argv[4];
	int k;

	k = 0;
	cmd = BIN_MV;
	argv[k++] = cmd;
	argv[k++] = (char *)source;
	argv[k++] = (char *)target;
	argv[k++] = NULL;
	assert(k <= nelems(argv));
	if (systemv(cmd, argv) == -1) {
		return -1;
	}
	return 0;
}

int
convert_itb(handle, types, ntypes, itb, itb2frq)
char const *handle, *itb, *bin;
char * const *types;
size_t ntypes;
{
	struct popenv_t *p;
	int d;

	if ((d = open(itb, O_RDONLY)) != -1) {
		char *xtypes[] = {"@flwd", "@names"};
#if 0
		size_t i;
#endif
		char *cmd;
		char *argv[32];
		int k;
		k = 0;
		cmd = itb2frq;
		argv[k++] = cmd;
		argv[k++] = (char *)handle;
		if (nelems(argv) <= k + 1 + ntypes + (mkrip ? nelems(xtypes) : 0)) {
			fprintf(stderr, "too many types\n");
			return -1;
		}
		argv[k++] = "@options";
#if 1
		if (ntypes) {
			memcpy(&argv[k], &types[0], ntypes * sizeof *argv);
			k += ntypes;
		}
		if (mkrip) {
			assert(nelems(xtypes));
			memcpy(&argv[k], &xtypes[0], nelems(xtypes) * sizeof *argv);
			k += nelems(xtypes);
		}
#else
		for (i = 0; i < ntypes; i++) {
			argv[k++] = types[i];
		}
		if (mkrip) {
			for (i = 0; i < nelems(xtypes); i++) {
				argv[k++] = xtypes[i];
			}
		}
#endif
		argv[k++] = NULL;
		assert(k <= nelems(argv));
		if (!(p = popenv(cmd, argv))) {
			return -1;
		}
		if (copy(stdin, p->stream) == -1) {
			perror("fwrite");
			pclosev(p);
			return -1;
		}
		pclosev(p);
	}
	else {
		perror(itb);
		return -1;
	}
	return 0;
}

#if defined BIN_PAX
int
expand_archive()
{
	char *cmd;
	char *argv[5];
	int k;
	struct popenv_t *p;

	k = 0;
	cmd = BIN_PAX;
	argv[k++] = cmd;
	argv[k++] = "-r";
	argv[k++] = NULL;
	assert(k <= nelems(argv));

	if (!(p = popenv(cmd, argv))) {
		return -1;
	}
	if (copy(stdin, p->stream) == -1) {
		perror("fwrite");
		pclosev(p);
		return -1;
	}
	pclosev(p);

	return 0;
}
#endif /* BIN_PAX */
#endif /* ENABLE_GETA */

#if defined BIN_UNCOMPRESS
int
uncompress(path)
char const *path;
{
	char *cmd;
	char *argv[4];
	int k;

	k = 0;
	cmd = BIN_UNCOMPRESS;
	argv[k++] = cmd;
	argv[k++] = (char *)path;
	argv[k++] = NULL;
	assert(k <= nelems(argv));
	if (systemv(cmd, argv) == -1) {
		return -1;
	}
	return 0;
}
#endif

#if defined BIN_GZIP
int
ungzip(path)
char const *path;
{
	char *cmd;
	char *argv[4];
	int k;

	k = 0;
	cmd = BIN_GZIP;
	argv[k++] = cmd;
	argv[k++] = (char *)path;
	argv[k++] = "-d";
	argv[k++] = NULL;
	assert(k <= nelems(argv));
	if (systemv(cmd, argv) == -1) {
		return -1;
	}
	return 0;
}
#endif

#if defined BIN_BZIP2
int
unbzip2(path)
char const *path;
{
	char *cmd;
	char *argv[4];
	int k;

	k = 0;
	cmd = BIN_BZIP2;
	argv[k++] = cmd;
	argv[k++] = (char *)path;
	argv[k++] = "-d";
	argv[k++] = NULL;
	assert(k <= nelems(argv));
	if (systemv(cmd, argv) == -1) {
		return -1;
	}
	return 0;
}
#endif

int
receive_file(path)
char const *path;
{
	char pathz[MAXPATHLEN];
	FILE *f;
	size_t c;
	char buf[8192];
#define	C_NONE	0
#if defined BIN_UNCOMPRESS
#define	C_COMPRESS	1
#endif
#if defined BIN_GZIP
#define	C_GZIP	2
#endif
#if defined BIN_BZIP2
#define	C_BZIP2	3
#endif
	int compressed = C_NONE;

	if (!(f = fopen(path, "w"))) {
		perror(path);
		return -1;
	}
	if ((c = fread(buf, 1, sizeof buf, stdin)) == 0) {
		/* empty input is invalid */
		return -1;
	}
/*
0	string		\037\235	compress'd data
0       string          \037\213        gzip compressed data
0	string		BZh		bzip2 compressed data
*/
	if (0) { }
#if defined BIN_UNCOMPRESS
	else if (!memcmp(buf, "\037\235", 2)) {
		compressed = C_COMPRESS;
	}
#endif
#if defined BIN_GZIP
	else if (!memcmp(buf, "\037\213", 2)) {
		compressed = C_GZIP;
	}
#endif
#if defined BIN_BZIP2
	else if (!memcmp(buf, "BZh", 3)) {
		compressed = C_BZIP2;
	}
#endif
	if (fwrite(buf, 1, c, f) != c) {
		perror(path);
		fclose(f);
		return -1;
	}
	if (copy(stdin, f) == -1) {
		perror(path);
		fclose(f);
		return -1;
	}
	fclose(f);
	switch (compressed) {
	case C_NONE:
		break;
#if defined BIN_UNCOMPRESS
	case C_COMPRESS:
		snprintf(pathz, sizeof pathz, "%s.Z", path);
		if (rename(path, pathz) == -1) {
			perror(pathz);
			return -1;
		}
		if (uncompress(pathz) == -1) {
			return -1;
		}
		break;
#endif
#if defined BIN_GZIP
	case C_GZIP:
		snprintf(pathz, sizeof pathz, "%s.gz", path);
		if (rename(path, pathz) == -1) {
			perror(pathz);
			return -1;
		}
		if (ungzip(pathz) == -1) {
			return -1;
		}
		break;
#endif
#if defined BIN_BZIP2
	case C_BZIP2:
		snprintf(pathz, sizeof pathz, "%s.bz2", path);
		if (rename(path, pathz) == -1) {
			perror(pathz);
			return -1;
		}
		if (unbzip2(pathz) == -1) {
			return -1;
		}
		break;
#endif
	default:
		fprintf(stderr, "internal error: unknown compression type\n");
		return -1;
	}
	return 0;
}

int
copy(from, to)
FILE *from, *to;
{
	size_t c;
	char buf[8192];

	while (c = fread(buf, 1, sizeof buf, from)) {
		if (fwrite(buf, 1, c, to) != c) {
			return -1;
		}
	}
	return 0;
}
