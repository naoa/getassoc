/*	$Id: 1342.c,v 1.9 2009/08/09 23:47:53 nis Exp $	*/

/*-
 * Copyright (c) 2004 Shingo Nishioka. 
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
static char rcsid[] = "$Id: 1342.c,v 1.9 2009/08/09 23:47:53 nis Exp $";
#endif

#include <config.h>

#include <sys/types.h>
#include <sys/time.h>

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

#include <gifnoc.h>

static void encode_3byte_1342(char const *, char *);

static int table_1342[] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3',
	'4', '5', '6', '7', '8', '9', '+', '/',
	'='};

static void
encode_3byte_1342(s, r)
char const *s;
char *r;
{
	int x;

	x = (s[0]&0xff)<<16 | (s[1]&0xff)<<8 | (s[2]&0xff);

	r[0] = table_1342[x >> 18 & 0x3f];
	r[1] = table_1342[x >> 12 & 0x3f];
	r[2] = table_1342[x >> 6 & 0x3f];
	r[3] = table_1342[x >> 0 & 0x3f];
}

void
encode_1342_bytes(plain_text, len, encoded_text)
char const *plain_text;
size_t len;
char *encoded_text;
{
	char const *end = plain_text + len;

	while (plain_text < end) {
/*#warning "len%3!=0 no toki, 1 or 2 byte overrun suru kotoga aru..."*/
		encode_3byte_1342(plain_text, encoded_text);
		plain_text += 3;
		encoded_text += 4;
	}
	switch (len % 3) {
	case 1:
		encoded_text[-1] = '=';
		encoded_text[-2] = '=';
		break;
	case 2:
		encoded_text[-1] = '=';
		break;
	}
	encoded_text[0] = '\0';
}
