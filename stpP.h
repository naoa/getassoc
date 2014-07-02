/*	$Id: stpP.h,v 1.3 2010/11/05 02:53:24 nis Exp $	*/

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

/* sutil.c */
struct find_pat {
	char **p, *t;
	size_t n, size, tlen;
};
int fn_find_pat_match_tail(const char *, void *);
int findafile_match_tail(char const *, char *, size_t, char const *);
int pxsort(char **, size_t, char const *, char const *, char const *);
char *ngetline(char *, size_t, FILE *);
void chop(char *);
#if ! defined HAVE_MKDTEMP
char *mkdtemp(char *);
#endif
int is_sound_handle_name(char const *);
int is_sound_prop_name(char const *);
int namep(char const *);

/* archive.c */
int convert_otmi(char const *, char const *, char const *);
int convert_tsv(char const *, char const *, char const *);
#if defined BIN_RM
int rm_r(char const *);
#endif
#if defined ENABLE_GETA
int mv(char const *, char const *);
int convert_itb(char const *, char const *, char * const *, size_t, char const *, char const *);
#if defined BIN_PAX
int expand_archive(void);
#endif
#endif
#if defined BIN_UNCOMPRESS
int uncompress(char const *);
#endif
#if defined BIN_GZIP
int ungzip(char const *);
#endif
#if defined BIN_BZIP2
int unbzip2(char const *);
#endif
int receive_file(char const *);
int copy(FILE *, FILE *);

/* cata.c */
int gen_newinf(char const *, char const *, char const *, char * const *, size_t, int, char const *);
int gen_catalog(
#if defined ENABLE_GETA
	char const *, 
#endif
	char const *, char const *, char const *, char const *);

/* traverse.c */
int traverse_fn(char const *, int (*)(char const *, void *), void *);

/* systemv.c */
int systemv(char const *, char * const []);
#if defined ENABLE_GETA
struct popenv_t {
	char *path;
	FILE *stream;
	pid_t pid;
};
struct popenv_t *popenv(char const *, char * const []);
int pclosev(struct popenv_t *);
#endif

struct spl {
	int e;
#if ! defined NO_SIGPROCMASK
	sigset_t set, oset;
#else
	int dummy;
#endif
};

#if ! defined NO_SIGPROCMASK
#define	SPL0(s) do { \
	sigemptyset(&(s)->set); \
	sigemptyset(&(s)->oset); \
	sigaddset(&(s)->set, SIGHUP); \
	sigaddset(&(s)->set, SIGINT); \
	sigaddset(&(s)->set, SIGQUIT); \
	sigaddset(&(s)->set, SIGTERM); \
	if (((s)->e = sigprocmask(SIG_BLOCK, &(s)->set, &(s)->oset)) == -1) { \
		perror("sigprocmask"); \
	} \
} while (0)

#define SPLX(s) do { \
	if ((s)->e == 0 && sigprocmask(SIG_SETMASK, &(s)->oset, NULL) == -1) { \
		perror("sigprocmask"); \
	} \
	(s)->e = -1; \
} while (0)

#else /* NO_SIGPROCMASK */
#define	SPL0(s) do { } while (0)
#define	SPLX(s) do { } while (0)
#endif /* NO_SIGPROCMASK */
