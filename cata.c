/*	$Id: cata.c,v 1.5 2011/09/14 03:06:08 nis Exp $	*/

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
static char rcsid[] = "$Id: cata.c,v 1.5 2011/09/14 03:06:08 nis Exp $";
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

static int reserved_attr(char const *);
static int inf2xml(char **, size_t, FILE *
#if defined ENABLE_GETA
, char const *
#endif
);
static void fputxs(char *, FILE *);

char catalogue_tmp[MAXPATHLEN] = "";

int
gen_newinf(inf, newinf, handle, types, ntypes, mkrip, fsskey)
char const *inf, *newinf, *handle, *fsskey;
char * const *types;
size_t ntypes;
{
	FILE *f;
	size_t i;
	int has_title;
	char props[4096];
	size_t l;

	strcpy(props, "");	/* good */
	l = 0;
	for (i = 1; i < ntypes; i++) {
		if (i >= 2) {
			if (l + 1 >= sizeof props) {
				return -1;
			}
			strcpy(props + l, ",");	/* good */
			l += 1;
		}
		if (l + strlen(types[i]) >= sizeof props) {
			return -1;
		}
		strcpy(props + l, types[i]);	/* good */
		l += strlen(props + l);
	}
	if (mkrip) {
		if (i >= 2) {
			if (l + 1 >= sizeof props) {
				return -1;
			}
			strcpy(props + l, ",");	/* good */
			l += 1;
		}
		if (l + strlen(fsskey) >= sizeof props) {
			return -1;
		}
		strcpy(props + l, fsskey);	/* good */
		l += strlen(props + l);
	}

	if (!(f = fopen(newinf, "w"))) {
		perror(newinf);
		return -1;
	}
	fprintf(f, "@name=%s\n", handle);
	fprintf(f, "@properties=%s\n", props);
	has_title = 0;
	if (*inf) {
		NFILE *g;
		char *buf;
		if (!(g = nopen(inf, "r"))) {
			perror(inf);
			return -1;
		}
		while ((buf = readln(g)) && *buf) {
			char *k, *v;
			if (*buf != '@') {
				continue;
			}
			k = buf + 1;
			if (!(v = index(k, '='))) {
				continue;
			}
			*v++ = '\0';
			if (reserved_attr(k) || !is_sound_prop_name(k)) {
				continue;
			}
			fprintf(f, "@%s=%s", k, v);
			if (!strncmp(k, "title", 5)) {
				has_title = 1;
			}
		}
		nfclose(g);
	}
	if (!has_title) {
		fprintf(f, "@title=%s\n", handle);
	}
	fclose(f);
	return 0;
}

static int
reserved_attr(a)
char const *a;
{
/* NOTE: see nwam.c reserved_prop */
	return !strncmp(a, "name", 4) ||		/* stp.c */
	       !strncmp(a, "created", 7) ||		/* inf2xml.c */
		/* stemmer -- nwam.c */
		/* servers -- nwam.c */
		/* stat -- nwam.c */
	       !strncmp(a, "properties", 10) ||		/* nwam.c -> stp.c */
	       !strncmp(a, "number-of-articles", 18);	/* inf2xml.c */
}

int
gen_catalog(
#if defined ENABLE_GETA
getaroot, 
#endif
catalogue, wamsdir, index_key, catainf)
char const 
#if defined ENABLE_GETA
*getaroot, 
#endif
*catalogue, *wamsdir, *index_key, *catainf;
{
	char wamsdirs[MAXPATHLEN];
	char scatainf[MAXPATHLEN];
	int d;
	size_t j;
	struct spl spl = {-1, };
	FILE *f;
	struct find_pat f0;

	snprintf(wamsdirs, sizeof wamsdirs, "%s" DIRECTORY_DELIMITER, wamsdir);
	snprintf(scatainf, sizeof scatainf, DIRECTORY_DELIMITER "%s", catainf);

	SPL0(&spl);
	snprintf(catalogue_tmp, sizeof catalogue_tmp, "%s.XXXXXX", catalogue);
	if ((d = mkstemp(catalogue_tmp)) == -1) {
		perror("mkstemp");
		return -1;	/* ! goto fail */
	}
	SPLX(&spl);
	if (!(f = fdopen(d, "w+"))) {
		perror("fdopen");
		goto fail;
	}

	f0.n = 0;
	f0.size = 0;
	f0.p = NULL;
	f0.t = scatainf;
	f0.tlen = strlen(f0.t);
	if (traverse_fn(wamsdir, fn_find_pat_match_tail, &f0)) {
		perror(wamsdir);
		goto fail;
	}

	if (pxsort(f0.p, f0.n, wamsdirs, scatainf, index_key) == -1) {
		fprintf(stderr, "pxsort failed\n");
		goto fail;
	}

	if (inf2xml(f0.p, f0.n, f
#if defined ENABLE_GETA
		, getaroot
#endif
		) == -1) {
		fprintf(stderr, "inf2xml failed\n");
		goto fail;
	}

	fclose(f);

	for (j = 0; j < f0.n; j++) {
		free(f0.p[j]);
	}
	free(f0.p);

	if (chmod(catalogue_tmp, 0666) == -1) {
		perror(catalogue_tmp);
	}

	SPL0(&spl);
#if defined NO_OVERWRITE_ON_RENAME
	unlink(catalogue);
#endif
	if (rename(catalogue_tmp, catalogue) == -1) {
		perror("rename");
		unlink(catalogue_tmp);
	}
	catalogue_tmp[0] = '\0';
	SPLX(&spl);

	return 0;
fail:
	if (catalogue_tmp[0]) unlink(catalogue_tmp);
	return -1;
}

static int
inf2xml(paths, n, stream
#if defined ENABLE_GETA
, getaroot
#endif
)
char **paths;
size_t n;
FILE *stream;
#if defined ENABLE_GETA
char const *getaroot;
#endif
{
	struct {
		char *name, *value;
	} *attrs = NULL;
	size_t attrs_size = 0;
	char buf[8192];
	char *name, *value, *e;
	FILE *f;
	struct stat sb;
	char *path;
	size_t k;
#if defined ENABLE_GETA
	WAM *w;

	wam_init(getaroot);
#endif
fprintf(stderr, "handles:");
	fprintf(stream, "<catalogue\n>");
	for (k = 0; k < n; k++) {
#if defined ENABLE_GETA
		df_t an;
#endif
		char *p, *handle;
		size_t nattrs, i;
		char s[32];
		path = paths[k];
		if (p = rindex(path, '\n')) *p = '\0';

		if (stat(path, &sb)) {
			perror(path);
			fprintf(stderr, "cannot access %s ... contiune\n", path);
			continue;
		}

		if (!(f = fopen(path, "r"))) {
			continue;
		}
		while (fgets(buf, sizeof buf, f)) {
			if (!strncmp(buf, "@name=", 6)) {
				handle = buf + 6;
				if (p = rindex(handle, '\n')) *p = '\0';
				goto ok;
			}
		}
		fclose(f);
		continue;

	ok:
#if defined ENABLE_GETA
		if (!(w = wam_open(handle))) {
			perror(handle);
			fclose(f);
			continue;
		}
		an = wam_size(w, WAM_ROW);
		wam_close(w);
#endif
fprintf(stderr, " %s", handle);

		nattrs = 0;

		rewind(f);
		while (fgets(buf, sizeof buf, f)) {
			if (*buf != '@') {
				continue;
			}
			name = buf + 1;
			if (!(value = index(name, '='))) {
				continue;
			}
			*value++ = '\0';
			if (!(e = index(value, '\n'))) {
				continue;
			}
			*e = '\0';
			if (!*name) {
				continue;
			}
#define ADDATTR(NAME, VALUE) do { \
		if (attrs_size <= nattrs) { \
			void *new; \
			size_t newsize = nattrs; \
			newsize = NA(newsize + 1, 32); \
			if (!(new = realloc(attrs, newsize * sizeof *attrs))) { \
				return -1; \
			} \
			attrs_size = newsize; \
			attrs = new; \
		} \
		if (!(attrs[nattrs].name = strdup((NAME)))) { \
			perror("strdup"); \
			return -1; \
		} \
		if (!(attrs[nattrs].value = strdup((VALUE)))) { \
			perror("strdup"); \
			return -1; \
		} \
		nattrs++; \
	} while (0)
			ADDATTR(name, value);

#if ! defined ENABLE_GETA
			if (!strcmp(name, "stat.row_num")) {
				ADDATTR("number-of-articles", value);
			}
#endif
		}

		sprintf(s, "%"PRIuSIZE_T, (pri_size_t)sb.st_mtime); /* XXX: 2038 problem */
		ADDATTR("created", s);
#if defined ENABLE_GETA
		sprintf(s, "%"PRIdDF_T, an);
		ADDATTR("number-of-articles", s);
#endif

		for (i = 0; i < nattrs; i++) {
			size_t j;
			if (!namep(attrs[i].name)) {
				fprintf(stderr, "invalid name: %s\n", attrs[i].name);
				goto cont;
			}
			for (j = 0; j < i; j++) {
				if (!strcmp(attrs[i].name, attrs[j].name)) {
					fprintf(stderr, "duplicated attribute: %s\n", attrs[i].name);
					goto cont;
				}
			}
		}

		fprintf(stream, "<corpus");

		for (i = 0; i < nattrs; i++) {
			fprintf(stream, " %s=\"", attrs[i].name);
			fputxs(attrs[i].value, stream);
			fprintf(stream, "\"");
			free(attrs[i].name), attrs[i].name = NULL;
			free(attrs[i].value), attrs[i].value = NULL;
		}

		fprintf(stream, "\n/>");
	cont:
		fclose(f);
	}
	fprintf(stream, "</catalogue>\n");
fprintf(stderr, "\n");
	return 0;
}

static void
fputxs(s, stream)
char *s;
FILE *stream;
{
	for (; *s; s++) {
		switch (*s) {
		case '&':
			fputs("&amp;", stream);
			break;
		case '"':
			fputs("&quot;", stream);
			break;
		case '<':
			fputs("&lt;", stream);
			break;
		default:
			putc(*s, stream);
			break;
		}
	}
}
