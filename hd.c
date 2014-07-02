/*	$Id: hd.c,v 1.8 2011/09/14 02:36:15 nis Exp $	*/

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
static char rcsid[] = "$Id: hd.c,v 1.8 2011/09/14 02:36:15 nis Exp $";
#endif

#include <config.h>

#include <sys/types.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <gifnoc.h>

int hyperd_mode = 2;

#if 1
double hyperd(N, K, n, k)
{
	return 0;
}
#else

#include <gmp.h>

static void mP(mpq_t *, int, int);
static void mH(mpq_t *, int, int);
static void mC(mpq_t *, int, int);
static double dB(int, double, int);
static double dnor(double, double, double);

#define MIN(a, b)	((a) < (b) ? (a) : (b))

double
hyperd(N, K, n, k)
{
	static mpq_t num, num2, den, q;
	static mpq_t p2, t;
	static mpf_t f;
	static int first = 1;
	double d;
	double p;

	if (first) {
		mpq_init(num);
		mpq_init(num2);
		mpq_init(den);
		mpq_init(q);
		mpf_init(f);
		mpq_init(p2);
		mpq_init(t);
		first = 0;
	}

	switch (hyperd_mode) {
	case 0:
		mP(&num, K, k);

		mP(&p2, N - K, n - k);
		mpq_mul(num2, num, p2);

		mH(&p2, k + 1, n - k);
		mpq_mul(num, num2, p2);

		mP(&den, N, n);

		mpq_div(q, num, den);

		mpf_set_q(f, q);

		d = mpf_get_d(f);

		break;
	case 1:
	xx:

		mC(&num2, K, k);
		mC(&p2, N - K, n - k);
		mpq_mul(num, num2, p2);

		mC(&den, N, n);

		mpq_div(q, num, den);

		mpf_set_q(f, q);

		d = mpf_get_d(f);
		break;
	case 2:
		if (k == K || n - k == N - K || n == N) {
			goto xx;
		}

		if (k == 0 || n - k == 0 || n == 0) {
			goto xx;
		}

		p = (double)n / N;

		d = dB(K, p, k) * dB(N - K, p, n - k) / dB(N, p, n);

		if (d < 3e-14) {
			goto xx;
		}

		break;
	default:
		fprintf(stderr, "internal error\n");
		exit(1);
		/* NOTREACHED */
	}

	return d;
}

static void
mP(r, n, m)
mpq_t *r;
{
	int i;
	static mpq_t r2, p2, t;
	static int first = 1;
	if (first) {
		mpq_init(r2);
		mpq_init(p2);
		mpq_init(t);
		first = 0;
	}
	i = n - m + 1;
	if (m % 2) {
		mpq_set_si(*r, i, 1);
		i++;
	}
	else {
		mpq_set_si(*r, 1, 1);
	}
	for (; i <= n; ) {
		mpq_set_si(p2, i, 1);
		mpq_mul(r2, *r, p2);
		i++;
		mpq_set_si(p2, i, 1);
		mpq_mul(*r, r2, p2);
		i++;
	}
}

static void
mH(r, n, m)
mpq_t *r;
{
	static mpq_t d, d2, mm, num, t;
	static int first = 1;
	if (first) {
		mpq_init(d);
		mpq_init(d2);
		mpq_init(mm);
		mpq_init(num);
		mpq_init(t);
		first = 0;
	}
	mP(&num, n + m - 1, m);
	if (m % 2) {
		mpq_set_si(d, m, 1);
		m--;
	}
	else {
		mpq_set_si(d, 1, 1);
	}
	for (; m > 0; ) {
		mpq_set_si(mm, m, 1);
		mpq_mul(d2, d, mm);
		m--;
		mpq_set_si(mm, m, 1);
		mpq_mul(d, d2, mm);
		m--;
	}
	mpq_div(*r, num, d);
}

static void
mC(r, n, m)
mpq_t *r;
{
	static mpq_t d, d2, mm, num, t;
	static int first = 1;
	if (first) {
		mpq_init(d);
		mpq_init(d2);
		mpq_init(mm);
		mpq_init(num);
		mpq_init(t);
		first = 0;
	}
	mP(&num, n, m);
	if (m % 2) {
		mpq_set_si(d, m, 1);
		m--;
	}
	else {
		mpq_set_si(d, 1, 1);
	}
	for (; m > 0; ) {
		mpq_set_si(mm, m, 1);
		mpq_mul(d2, d, mm);
		m--;
		mpq_set_si(mm, m, 1);
		mpq_mul(d, d2, mm);
		m--;
	}
	mpq_div(*r, num, d);
}

double
dB(N, p, r)
double p;
{
	double d;
	if (N * p > 10 && N * (1 - p) > 10) {
		d = dnor(N * p, N * p * (1 - p), r);
	}
	else {
		static int first = 1;
		static mpq_t num;
		static mpf_t f, f2, p2;
		if (first) {
			mpq_init(num);
			mpf_init(f);
			mpf_init(f2);
			mpf_init(p2);
			first = 0;
		}
		mC(&num, N, r);
		mpf_set_q(f, num);

		mpf_set_d(p2, pow(p, r));
		mpf_mul(f2, f, p2);

		mpf_set_d(p2, pow(1 - p, N - r));
		mpf_mul(f, f2, p2);

		d = mpf_get_d(f);
	}
	return d;
}

static double
dnor(mu, var, x)
double mu, var, x;
{
	double d, sigma;

	sigma = sqrt(var);

	d = exp(-(x-mu)*(x-mu)/(2*var)) / (sqrt(2 * 3.1415926) * sigma);

	return d;
}
#endif
