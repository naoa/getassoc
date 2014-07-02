/*	$Id: rn.c,v 1.24 2010/11/15 10:08:31 nis Exp $	*/

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
static char rcsid[] = "$Id: rn.c,v 1.24 2010/11/15 10:08:31 nis Exp $";
#endif

#include <config.h>

#include <sys/param.h>
#include <sys/time.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>

#include "nwam.h"
#include "util.h"
#include "nio.h"

#include "common.h"
#include "getassocP.h"

#include <gifnoc.h>

#if defined ENABLE_REN
int
rn_html(path_info, argc, argv, out)
char const *path_info;
char *argv[];
{
	return send_file_datadir_relative("rn" DIRECTORY_DELIMITER "rn.html", out, "text/html");
}

int
rn_css(path_info, argc, argv, out)
char const *path_info;
char *argv[];
{
	return send_file_datadir_relative("rn" DIRECTORY_DELIMITER "rn.css", out, "text/css");
}

int
rn_js(path_info, argc, argv, out)
char const *path_info;
char *argv[];
{
	return send_file_datadir_relative("rn" DIRECTORY_DELIMITER "rn.js", out, "text/javascript");
}

int
btn_clear_gif(path_info, argc, argv, out)
char const *path_info;
char *argv[];
{
	return send_file_datadir_relative("rn" DIRECTORY_DELIMITER "btn_clear.gif", out, "image/gif");
}

int
btn_top_imagine_gif(path_info, argc, argv, out)
char const *path_info;
char *argv[];
{
	return send_file_datadir_relative("rn" DIRECTORY_DELIMITER "btn_top_imagine.gif", out, "image/gif");
}

int
top_logo_gif(path_info, argc, argv, out)
char const *path_info;
char *argv[];
{
	return send_file_datadir_relative("rn" DIRECTORY_DELIMITER "top_logo.gif", out, "image/gif");
}

extern char *datadir_getassoc;

int
private_db_js(path_info, argc, argv, out)
char const *path_info;
char *argv[];
{
	char buf[8192], *p, *q;
	char const *request_uri, *http_host;
	char p_url[MAXURLLEN];
	char *p_msg = NULL;
	char path[MAXPATHLEN];
	struct mapfile_t *m = NULL;

	if (!datadir_getassoc) {
		syslog(LOG_DEBUG, "!datadir_getassoc");
		goto err;
	}

	snprintf(path, sizeof path, "%s" DIRECTORY_DELIMITER "rn" DIRECTORY_DELIMITER "sample.txt", datadir_getassoc);

	if (!(m = mapfile(path))) {
		syslog(LOG_DEBUG, "mapfile(%s): %s", path, strerror(errno));
		goto err;
	}
	if (!(p_msg = malloc(m->size + 1))) {
		syslog(LOG_DEBUG, "malloc: %s", strerror(errno));
		mapfile_destroy(m);
		goto err;
	}
	memmove(p_msg, m->ptr, m->size);
	p_msg[m->size] = '\0';
	mapfile_destroy(m);

	for (p = p_msg; q = index(p, '\n'); p = q) {
		*q = ' ';
	}

	if (!(http_host = getenv("HTTP_HOST"))) {
		syslog(LOG_DEBUG, "!HTTP_HOST");
		goto err;
	}

	if (!(request_uri = getenv("REQUEST_URI"))) {
		syslog(LOG_DEBUG, "!REQUEST_URI");
		goto err;
	}

	snprintf(p_url, sizeof p_url, "http://%s%s", http_host, request_uri);
	if (!(p = strstr(p_url, "/rn/private_db.js")) || strcmp(p, "/rn/private_db.js")) {
		syslog(LOG_DEBUG, "!strstr");
		goto err;
	}
	strcpy(p, "/gss3");

	snprintf(buf, sizeof buf,
		"var p_url = \"%s\";\n"
		"var sample_text = \"%s\";\n",
		p_url, p_msg);
	nputs("Content-Type: text/javascript\r\n"
		"Pragma: no-cache\r\n", out);
	send_text_content(&out, NULL, buf, strlen(buf), 1);
	free(p_msg);
	return 0;
err:
	free(p_msg);
	return -1;
}
#endif
