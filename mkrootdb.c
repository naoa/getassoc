/*	$Id: mkrootdb.c,v 1.9 2009/09/01 05:15:39 nis Exp $	*/

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
static char rcsid[] = "$Id: mkrootdb.c,v 1.9 2009/09/01 05:15:39 nis Exp $";
#endif

#include <config.h>

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "nwam.h"

#include "util.h"

#include <gifnoc.h>

int
main(argc, argv)
char *argv[];
{
	char buf[8192];
	int i, l;
	for (i = 1; i < argc; i++) {
		FILE *f;
		char const *path = argv[i];
		if (!(f = fopen(path, "r"))) {
			perror(path);
			return 1;
		}
		while (fgets(buf, sizeof buf, f)) {
			fputs(buf, stdout);
		}
		fputs("\n", stdout);
		fclose(f);
	}
	fputs("function\n"
		"root_db_arts(de)\n"
		"{\n"
		"\treturn [\n\t\t"
		, stdout);
	
	for (l = 0; fgets(buf, sizeof buf, stdin); l++) {
		char *p;
		size_t nf;
		char *v[32];
#define handle	(v[0])
#define props	(v[1])
#define user	(v[2])
#define host	(v[3])
#define path	(v[4])
#define name	(v[5])
#define dflc	(v[6])

		if (p = index(buf, '\n')) {
			*p = '\0';
		}

		nf = splitv(buf, '\t', v, nelems(v), 0);
		if (nf != 7) {
			fprintf(stderr, "Error: line %d: # of fields should be 7\n", l);
			return 1;
		}

		if (l != 0) {
			fputs(",\n\t\t", stdout);
		}
		if (!strcmp(host, "-")) {
			printf("dblent(de, \"%s\", \"%s\", \"%s\", \"%s\", false, %s)", name, path, handle, props, dflc);
		}
		else {
			printf("dblent(de, \"%s\", \"http://%s%s\", \"%s\", \"%s\", false, %s)", name, host, path, handle, props, dflc);
		}
	}
	printf("\n\t];\n"
		"}\n\n"
		"var root_db = {type: 0, key: \"root_db\", title: \"DB list\", url: null, handle: null, props: null, meta: true};\n"
		"var metadb_reference_db = {type: 0, key: \"metadb_reference\", title: \"Metadb Reference DB\", url: \"%s\", handle: \"metadb.reference\", props: \"title,link\", meta: false};\n",
		RN_METADB_URL);

	return 0;
}
