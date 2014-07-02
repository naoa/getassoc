/*	$Id: simcomp.y,v 1.13 2010/12/14 06:13:32 nis Exp $	*/

%{
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
static char rcsid[] = "$Id: simcomp.y,v 1.13 2010/12/14 06:13:32 nis Exp $";
#endif

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <gifnoc.h>

static int yyparse(void);
static int yyerror(char *);
static int yylex(void);

static void usage(void);

static int tok_r(char const *, char const **, char *, size_t, char *, size_t);

static char *src_p;

struct sexp {
	char *name;			/* if symbol */
	struct sexp *_car, *_cdr;	/* otherwise cons-cell */
};

#define	null(c)	((c) == NULL)
#define	car(c)	((c)->_car)
#define	cdr(c)	((c)->_cdr)
#define	symbolp(c) ((c)->name != NULL)
#define symbol_name(c) ((c)->name)

#define	caar(c)		car(car(c))
#define	cadr(c)		car(cdr(c))
#define	caddr(c)	car(cdr(cdr(c)))

static struct sexp *cons(struct sexp *, struct sexp *);
static struct sexp *intern(char *);
static struct sexp *_reverse(struct sexp *, struct sexp *);
#define	reverse(l)	(_reverse((l), NULL))
static void inorder(struct sexp *, FILE *);

#define list2(a, b)		(cons((a), cons((b), NULL)))
#define	list3(a, b, c)		(cons((a), cons((b), cons((c), NULL))))
#define	list4(a, b, c, d)	(cons((a), cons((b), cons((c), cons((d), NULL)))))

static struct sexp *result;
static char *progname;

#define	nelems(e)	(sizeof (e) / sizeof *(e))

%}

%union {
	char *name;
	struct sexp *list;
}

%start definition

%token <name> TERM
%token <name> STERM
%token <name> DEF
%token <name> SUMT
%token <name> IDENTIFIER
%token <name> FUNCTION_IDENTIFIER
%token <name> NUMBER

%type <void> definition

%type <list> expression
%type <list> assignment_expr_list
%type <list> assignment_expr
%type <list> additive_expr
%type <list> multiplicative_expr
%type <list> unary_expr
%type <list> postfix_expr
%type <list> argument_list
%type <list> primary_expr

%type <list> additive_expr_s
%type <list> multiplicative_expr_s
%type <list> unary_expr_s
%type <list> postfix_expr_s
%type <list> argument_list_s
%type <list> primary_expr_s

%%

definition	:	IDENTIFIER DEF expression '.' {
				result = list3(intern($1), NULL, $3);
			}
		|	IDENTIFIER DEF assignment_expr_list ',' expression '.' {
				result = list3(intern($1), reverse($3), $5);
			}
		;

expression	:	additive_expr {
				$$ = list2($1, NULL);
			}
		|	additive_expr SUMT additive_expr_s {
				$$ = list2($1, $3);
			}
		|	SUMT additive_expr_s {
				$$ = list2(NULL, $2);
			}
		;

assignment_expr_list :	assignment_expr {
				$$ = cons($1, NULL);
			}
		|	assignment_expr_list ',' assignment_expr {
				$$ = cons($3, $1);
			}
		;

assignment_expr :	IDENTIFIER '=' additive_expr {
				$$ = list3(intern($1), intern(" = "), $3);
			}
		;

additive_expr	:	multiplicative_expr
		|	additive_expr '+' multiplicative_expr {
				$$ = list3($1, intern(" + "), $3);
			}
		|	additive_expr '-' multiplicative_expr {
				$$ = list3($1, intern(" - "), $3);
			}
		;

multiplicative_expr	:	unary_expr
		|	unary_expr '*' multiplicative_expr {
				$$ = list3($1, intern(" * "), $3);
			}
		|	unary_expr '/' multiplicative_expr {
				$$ = list3($1, intern(" / "), $3);
			}
		;

unary_expr	:	postfix_expr
		|	'-' unary_expr { $$ = list2(intern(" - "), $2); }
		;

postfix_expr	:	primary_expr
		|	FUNCTION_IDENTIFIER '(' argument_list ')' {
				$$ = list4(intern($1), intern("("), $3, intern(")"));
			}
		;

argument_list	:	additive_expr
		|	argument_list ',' additive_expr {
				$$ = list3($1, intern(","), $3);
			}
		;

primary_expr	:	TERM { $$ = intern($1); }
		|	NUMBER { $$ = intern($1); }
		|	IDENTIFIER { $$ = intern($1); }
		|	'(' additive_expr ')' {
				$$ = list3(intern("("), $2, intern(")"));
			}
		|	'{' additive_expr '}' {
				$$ = list3(intern("("), $2, intern(")"));
			}
		|	'[' additive_expr ']' {
				$$ = list3(intern("("), $2, intern(")"));
			}
		;


/***** CLONE CLONE CLONE *****/

additive_expr_s	:	multiplicative_expr_s
		|	additive_expr_s '+' multiplicative_expr_s {
				$$ = list3($1, intern(" + "), $3);
			}
		|	additive_expr_s '-' multiplicative_expr_s {
				$$ = list3($1, intern(" - "), $3);
			}
		;

multiplicative_expr_s	:	unary_expr_s
		|	unary_expr_s '*' multiplicative_expr_s {
				$$ = list3($1, intern(" * "), $3);
			}
		|	unary_expr_s '/' multiplicative_expr_s {
				$$ = list3($1, intern(" / "), $3);
			}
		;

unary_expr_s	:	postfix_expr_s
		|	'-' unary_expr_s { $$ = list2(intern(" - "), $2); }
		;

postfix_expr_s	:	primary_expr_s
		|	FUNCTION_IDENTIFIER '(' argument_list_s ')' {
				$$ = list4(intern($1), intern("("), $3, intern(")"));
			}
		;

argument_list_s	:	additive_expr_s
		|	argument_list_s ',' additive_expr_s {
				$$ = list3($1, intern(","), $3);
			}
		;

primary_expr_s	:	TERM { $$ = intern($1); }
		|	STERM { $$ = intern($1); }
		|	NUMBER { $$ = intern($1); }
		|	IDENTIFIER { $$ = intern($1); }
		|	'(' additive_expr_s ')' {
				$$ = list3(intern("("), $2, intern(")"));
			}
		|	'{' additive_expr_s '}' {
				$$ = list3(intern("("), $2, intern(")"));
			}
		|	'[' additive_expr_s ']' {
				$$ = list3(intern("("), $2, intern(")"));
			}
		;

%%

static int
yyerror(s)
char *s;
{
	fprintf(stderr, "%s\n", s);
	exit(1);
}

static int
yylex()
{
	char t[256], u[sizeof t];
	int c;
	if (c = tok_r(src_p, &src_p, t, sizeof t, u, sizeof u)) {
		yylval.name = strdup(t);
		fprintf(stderr, "<%s> ", *u ? u : t);
	}
	else {
		yylval.name = NULL;
	}
	return c;
}

static void
usage()
{
	fprintf(stderr, "usage: %s [-o out] path\n", progname);
	exit(1);
}

int
main(argc, argv)
char *argv[];
{
	struct sexp *nm, *as, *ex, *exl, *exr;
	char *path;
	int d;
	struct stat sb;
	char *src;
	struct sexp *q, *r;
	int ch;
	char *outf = NULL;
	FILE *out;

	progname = basename(argv[0]);

	while ((ch = getopt(argc, argv, "o:")) != -1) {
		switch (ch) {
		case 'o':
			outf = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind, argv += optind;

	if (argc != 1) {
		usage();
	}

	path = argv[0];

	if ((d = open(path, O_RDONLY)) == -1) {
		perror(path);
		return 1;
	}
	if (fstat(d, &sb) == -1) {
		perror(path);
		return 1;
	}

	if (!(src = malloc(sb.st_size))) {
		perror("malloc");
		return 1;
	}

	if (read(d, src, sb.st_size) != sb.st_size) {
		perror("read");
		return 1;
	}
	close(d);

	src_p = src;
	result = NULL;
	yyparse();
	nm = car(result);
	as = cadr(result);
	ex = caddr(result);
	exl = car(ex);
	exr = cadr(ex);
	fprintf(stderr, "\n");

	if (outf) {
		if (!(out = fopen(outf, "w"))) {
			perror(outf);
			return 1;
		}
	}
	else {
		out = stdout;
	}

	fprintf(out, "#include \"xsimdef.h\"\n");
	fprintf(out, "double " DLSFN_PREFIX "%s(double, idx_t, struct ovec_t *, struct hvec_t *, df_t, struct assv_g *);\n", symbol_name(nm));
	fprintf(out, "double " DLSFN_PREFIX "%s(DLSIM_wsum, DLSIM_d, DLSIM_dd, DLSIM_h, DLSIM_nh, DLSIM_g)\n"
		"double DLSIM_wsum;\n"
		"idx_t DLSIM_d;\n"
		"struct ovec_t *DLSIM_dd;\n"
		"struct hvec_t *DLSIM_h;\n"
		"df_t DLSIM_nh;\n"
		"struct assv_g *DLSIM_g;\n"
		"{\n", symbol_name(nm));
	if (exr) {
		fprintf(out, "\tdf_t DLSIM_i;\n");
	}

	for (q = as; q; q = cdr(q)) {
		char *nam = symbol_name(caar(q));
		for (r = cdr(q); r; r = cdr(r)) {
			if (!strcmp(nam, symbol_name(caar(r)))) {
				goto cont;
			}
		}
		fprintf(out, "\tdouble %s;\n", nam);
	cont: ;
	}

	for (q = as; q; q = cdr(q)) {
		fprintf(out, "\t");
		inorder(car(q), out);
		fprintf(out, ";\n");
	}
	if (exr) {
		fprintf(out, "\tfor (DLSIM_wsum = 0, DLSIM_i = 0; DLSIM_i < DLSIM_nh; DLSIM_i++) {\n"
			"\t\tstruct pq_container *DLSIM_t = DLSIM_h[DLSIM_i].t;\n"
			"\t\tif (DLSIM_t->qidx < DLSIM_g->nq) {\n"
			"\t\t\tstruct xr_elem const *DLSIM_dp = DLSIM_h[DLSIM_i].dp;\n"
			"\t\t\tstruct syminfo *DLSIM_qp = &DLSIM_g->q[DLSIM_t->qidx];\n"
			);

		fprintf(out, "\t\t\tDLSIM_wsum += (");
		inorder(exr, out);
		fprintf(out, ");\n"
			"\t\t}\n"
			"\t}\n");
	}
	if (exl) {
		if (exr) {
			fprintf(out, "\tDLSIM_wsum *= (");
		}
		else {
			fprintf(out, "\tDLSIM_wsum = (");
		}
		inorder(exl, out);
		fprintf(out, ");\n");
	}
	fprintf(out, "\treturn DLSIM_wsum;\n"
			"}\n");
	fclose(out);
	return 0;
}

struct term_t {
	char *name;
	size_t len;
	char *alt;
};

#define	ENT(name, alt) { name, sizeof name - 1, alt }
static struct term_t terms[] = {
	ENT("Nq(w)", "((double)DLSIM_g->Nq)"),
	ENT("Nr(w)", "((double)DLSIM_g->Nr)"),
	ENT("DF(w)", "((double)wam_total_elem_num(DLSIM_g->w))"),
	ENT("TF(w)", "((double)wam_total_freq_sum(DLSIM_g->w))"),
	ENT("maxDFq(w)", "((double)wam_max_elem_num(DLSIM_g->w, DLSIM_g->qdir))"),
	ENT("maxTFq(w)", "((double)wam_max_freq_sum(DLSIM_g->w, DLSIM_g->qdir))"),
	ENT("maxDFr(w)", "((double)wam_max_elem_num(DLSIM_g->w, DLSIM_g->rdir))"),
	ENT("maxTFr(w)", "((double)wam_max_freq_sum(DLSIM_g->w, DLSIM_g->rdir))"),
	ENT("DF(.|q)", "((double)DLSIM_g->nq)"),
	ENT("TF(.|q)", "((double)DLSIM_g->tfq)"),
	ENT("DF(.|d)", "((double)wam_elem_num(DLSIM_g->w, DLSIM_g->rdir, DLSIM_d))"),
	ENT("TF(.|d)", "((double)wam_freq_sum(DLSIM_g->w, DLSIM_g->rdir, DLSIM_d))"),
	ENT("DF(d&q)", "((double)DLSIM_dd->DF_d)"),
	ENT("TF(d&q)", "((double)DLSIM_dd->TF_d)"),
};

static struct term_t sterms[] = {
	ENT("TF(t|q)", "((double)DLSIM_qp->TF_d)"),
	ENT("DF(.|t)", "((double)DLSIM_qp->DF)"),
	ENT("TF(.|t)", "((double)DLSIM_qp->TF)"),
	ENT("idf(t)", "(log1p((double)DLSIM_g->Nr / DLSIM_qp->DF))"),
	ENT("TF(t|d)", "((double)DLSIM_dp->freq)"),
	ENT("weight(t|q)", "(DLSIM_qp->weight)"),
};
#undef ENT

struct func_t {
	char *name;
	size_t len;
};

#define	ENT(name) { name, sizeof name - 1 }
struct func_t funcs[] = {
	ENT("acos"),
	ENT("acosh"),
	ENT("asin"),
	ENT("asinh"),
	ENT("atan"),
	ENT("atanh"),
	ENT("atan2"),
	ENT("cabs"),
	ENT("cbrt"),
	ENT("ceil"),
	ENT("copysign"),
	ENT("cos"),
	ENT("cosh"),
	ENT("erf"),
	ENT("erfc"),
	ENT("exp"),
	ENT("expm1"),
	ENT("fabs"),
	ENT("finite"),
	ENT("floor"),
	ENT("fmod"),
	ENT("hypot"),
	ENT("ilogb"),
	ENT("isinf"),
	ENT("isnan"),
	ENT("j0"),
	ENT("j1"),
	ENT("jn"),
	ENT("lgamma"),
	ENT("log"),
	ENT("log10"),
	ENT("log1p"),
	ENT("nan"),
	ENT("nextafter"),
	ENT("pow"),
	ENT("remainder"),
	ENT("rint"),
	ENT("scalbn"),
	ENT("sin"),
	ENT("sinh"),
	ENT("sqrt"),
	ENT("tan"),
	ENT("tanh"),
	ENT("trunc"),
	ENT("y0"),
	ENT("y1"),
	ENT("yn"),
};
#undef ENT

#define	isnamec(c)	(isalnum(c) || (c) == '_')
#define	strneq_wb(s, t, l) (!strncmp((s), (t), (l)) && !isnamec((s)[(l)] & 0xff))

static int
tok_r(s, end, t, size, u, usize)
char const *s, **end;
char *t, *u;
size_t size, usize;
{
	size_t i, l;

	if (size < 36) return 0;

	*t = *u = '\0';

tok_r:
	while (*s && isspace(*s & 0xff)) s++;

	if (!strncmp(s, "--", 2)) {
		while (*s && *s != '\n') s++;
		if (*s) s++;
		goto tok_r;
	}
	else if (!strncmp(s, "/*", 2)) {
		while (*s && strncmp(s, "*/", 2)) s++;
		if (*s) s += 2;
		goto tok_r;
	}

	if (!*s) {
		*end = NULL;
		return 0;
	}
	switch (*s & 0xff) {
	case '.':
		if (isdigit(*(s + 1) & 0xff)) break;
		/* FALLTHRU */
	case ',':
	case '=':
	case '+':
	case '-':
	case '*':
	case '/':
	case '{':
	case '}':
	case '(':
	case ')':
	case '[':
	case ']':
		*end = s + 1;
		t[0] = *s, t[1] = '\0';
		return t[0] & 0xff;
	case '\\':
#define	SUMT_STR	"\\sum_t"
		l = sizeof SUMT_STR - 1;
		if (strneq_wb(s, SUMT_STR, l)) {
			*end = s + l;
			memcpy(t, SUMT_STR, l), t[l] = '\0';
			return SUMT;
		}
		break;
	case ':':
#define	DEF_STR	"::="
		l = sizeof DEF_STR - 1;
		if (!strncmp(s, DEF_STR, l)) {
			*end = s + l;
			memcpy(t, DEF_STR, l), t[l] = '\0';
			return DEF;
		}
		break;
	}

	if (isdigit(*s & 0xff) || *s == '.') {
		strtod(s, end);
		if (s == *end) {
			return 0;
		}
		l = *end - s;
		if (size < l + 1) return 0;
		memcpy(t, s, l), t[l] = '\0';
		return NUMBER;
	}

	for (i = 0; i < nelems(terms); i++) {
		struct term_t *tp = &terms[i];
		if (strneq_wb(s, tp->name, tp->len)) {
			l = strlen(tp->alt);
			*end = s + tp->len;
			if (size < l + 1) return 0;
			memcpy(t, tp->alt, l), t[l] = '\0';

			l = tp->len;
			if (usize < l + 1) return 0;
			memcpy(u, tp->name, l), u[l] = '\0';

			return TERM;
		}
	}

	for (i = 0; i < nelems(sterms); i++) {
		struct term_t *tp = &sterms[i];
		if (strneq_wb(s, tp->name, tp->len)) {
			l = strlen(tp->alt);
			*end = s + tp->len;
			if (size < l + 1) return 0;
			memcpy(t, tp->alt, l), t[l] = '\0';

			l = tp->len;
			if (usize < l + 1) return 0;
			memcpy(u, tp->name, l), u[l] = '\0';

			return STERM;
		}
	}

	for (i = 0; i < nelems(funcs); i++) {
		struct func_t *fp = &funcs[i];
		if (strneq_wb(s, fp->name, fp->len)) {
			l = fp->len;
			*end = s + l;
			if (size < l + 1) return 0;
			memcpy(t, fp->name, l), t[l] = '\0';
			return FUNCTION_IDENTIFIER;
		}
	}

	if (isalpha(*s & 0xff) || *s == '_') {
		char *p;
		for (p = s; isnamec(*p & 0xff); p++) ;
		*end = p;
		l = *end - s;
		if (size < l + 1) return 0;
		memcpy(t, s, l), t[l] = '\0';
		return IDENTIFIER;
	}

	return 0;
}

static struct sexp *
cons(_car, _cdr)
struct sexp *_car, *_cdr;
{
	struct sexp *r;
	if (!(r = calloc(1, sizeof *r))) {
		return NULL;
	}
	r->name = NULL;
	r->_car = _car;
	r->_cdr = _cdr;
	return r;
}

static struct sexp *
intern(name)
char *name;
{
	struct sexp *r;
	r = cons(NULL, NULL);
	r->name = name;
	return r;
}

static struct sexp *
_reverse(l, r)
struct sexp *l, *r;
{
	if (!l) return r;
	return _reverse(cdr(l), cons(car(l), r));
}

static void
inorder(l, stream)
struct sexp *l;
FILE *stream;
{
	if (null(l)) return;
	if (symbolp(l)) {
		fputs(symbol_name(l), stream);
	}
	else {
		inorder(car(l), stream);
		inorder(cdr(l), stream);
	}
}
