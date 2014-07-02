/*	$Id: util.h,v 1.50 2010/11/12 03:37:26 nis Exp $	*/

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

struct mapfile_private;

struct mapfile_t {
	void *ptr;
	size_t size;
	struct mapfile_private *private;
};

struct mapfile_t *mapfile(char const *);
struct mapfile_t *mapfile_rw(char const *);
struct mapfile_t *mapfile_prot(char const *, int, int);
struct mapfile_t *mmap_alloc(size_t);
void mapfile_destroy(struct mapfile_t *);

int brsearch(const void *, const void *, size_t, size_t, int (*)(const void *, const void *), size_t *, size_t *);

struct nfile_d;
typedef struct nfile_d NFILE;

NFILE *nopen(char const *, char const *);
NFILE *nfopen(FILE *);
#if 0
NFILE *nffunopen(ssize_t (*readfn)(void *, char *, size_t), void *);
#endif
char *readln(NFILE *);
void nfclose(NFILE *);
int nfunread(NFILE *);

struct nfn_file;
struct nfn_file *nfn_fdopen(void *, ssize_t (*)(void *, void *, size_t), ssize_t (*)(void *, const void *, size_t), 
int (*)(void *));
int nfn_read_int_arg(void *, char *, int);
ssize_t nfn_read(void *, void *, size_t);
char *nfn_fgets(char * restrict, int, void *);
ssize_t nfn_write(void *, const void *, size_t);
int nfn_fflush(void *);
int nfn_fclose(void *);

int fsort(FILE *, size_t, size_t, int (*)(const void *, const void *));

int xcharp(unsigned int);
int xletterp(unsigned int);
int xdigitp(unsigned int);
int xcombiningcharp(unsigned int);
int xextenderp(unsigned int);

double hyperd(int, int, int, int);

char *strip2(char *);

int diag_stat(char const *, int);

void maxrlimit(int, char const *);
#define	MAXRLIMIT(resource) maxrlimit(resource, #resource)

int ga_nnet_cli(char const *, char const *, char const *);

void encode_1342_bytes(char const *, size_t, char *);

#define	NA(e, x)	((e) + ((e) % (x) ? (x) - (e) % (x) : 0))

/*
 * make it safe to access ptr[n - 1].
 */
#define NREALLOC0(ptr, size, n, align, err) do {			\
	if ((size) < (n)) {						\
		void *NREALLOC_new;					\
		size_t NREALLOC_newsize = (n);				\
		NREALLOC_newsize = NA(NREALLOC_newsize, (align));	\
		if (!(NREALLOC_new = realloc((ptr), NREALLOC_newsize * sizeof *(ptr)))) { \
			err						\
		}							\
		(size) = NREALLOC_newsize;				\
		(ptr) = NREALLOC_new;					\
	}								\
} while (0)

/*
 * make it safe to access ptr[n].
 */
#define NREALLOC1(ptr, size, n, align, err) do {			\
	if ((size) <= (n)) {						\
		void *NREALLOC_new;					\
		size_t NREALLOC_newsize = (n);				\
		NREALLOC_newsize = NA(NREALLOC_newsize + 1, (align));	\
		if (!(NREALLOC_new = realloc((ptr), NREALLOC_newsize * sizeof *(ptr)))) { \
			err						\
		}							\
		(size) = NREALLOC_newsize;				\
		(ptr) = NREALLOC_new;					\
	}								\
} while (0)

void sigflg(int, int, int);
void dumpmem(void *, size_t);
void chkmem(void *, size_t);

#define	BEGIN()		(timer_init(NULL))
#define	PRINT_ELAPSED(msg, stream)	(timer_print_elapsed(NULL, msg, stream))

void downsample(void *, size_t, size_t, size_t);

#if ! defined(MIN)
#define	MIN(a, b)	((a) < (b) ? (a) : (b))
#endif

#if ! defined(MAX)
#define	MAX(a, b)	((a) > (b) ? (a) : (b))
#endif

#if ! defined(nelems)
#define nelems(e)	(sizeof (e) / sizeof *(e))
#endif

#if ! defined DIRECTORY_DELIMITER
#define	DIRECTORY_DELIMITER	"/"
#endif

#define PRIuSIZE_T	"lu"
#define	pri_size_t	unsigned long

#define PRIdSSIZE_T	"ld"
#define	pri_ssize_t	long
