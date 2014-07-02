/*	$Id: libgss3.js,v 1.3 2009/07/18 11:21:48 nis Exp $	*/

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

/* Request Constructor */

function
cons_catalog()
{
	var q, g;

	q = createXMLDocument();
	g = ea(q, "inquire", "type", "catalogue");
	nc(q, g);
	return q;
}

function
cons_getvec(handle, id)
{
	var q, g, a;

	q = createXMLDocument();
	g = ea(q, "getprop", "target", handle, "a-props", "vec");
	a = ea(q, "article", "name", id);
	nc(q, nc(g, a));
	return q;
}

function
cons_assoc(db, q0, na, nk)
{
	var q, a, i;

	q = createXMLDocument();
	a = ea(q, "assoc",
		"target", db.handle,
		"narticles", na,
		"nkeywords", nk,
		"nacls", "1",
		"nkcls", "1",
		"a-offset", "0",
		"a-props", db.props,
		"cross-ref", "aw,wa");
	if (q0.text) {
		var f, tt;
		f = ea(q, "freetext", "cutoff-df", "0");
		tt = q.createTextNode(q0.text);
		nc(a, nc(f, tt));
	}
	for (i = 0; i < q0.vecs.length; i++) {
		var av, tt, vv;
		av = ea(q, "article", "vec", q0.vecs[i]);
		nc(a, av);
	}
	nc(q, a);

	return q;
}

/* Response Analyzer (DOM-Tree) */

function
getResultNode(rx)
{
	return rx.getElementsByTagName("result")[0];
}

function
getArticlesNode(rx)
{
	return rx.getElementsByTagName("articles")[0];
}

function
getKeywordsNode(rx)
{
	return rx.getElementsByTagName("keywords")[0];
}

function
getFirstArticleNode(rx)
{
	return rx.getElementsByTagName("article")[0];
}

function
gss3_status(rx)
{
	if (!rx) {
		return null;
	}
	return getResultNode(rx).getAttribute("status");
}

function
getArticleList(aclss)
{
	var r;
	r = new Array();
	for (i = 0; i < aclss.childNodes.length; i++) {
		e = aclss.childNodes[i];
		for (j = 0; j < e.childNodes.length; j++) {
			c = e.childNodes[j];
			r[r.length] = c;
		}
	}
	return r;
}

function
getKeywordList(kclss)
{
	var r;
	r = new Array();
	for (i = 0; i < kclss.childNodes.length; i++) {
		e = kclss.childNodes[i];
		for (j = 0; j < e.childNodes.length; j++) {
			c = e.childNodes[j];
			r[r.length] = c;
		}
	}
	return r;
}

/* Response Analyzer (Articles / Keywords) */

function
as(db, arts)
{
	var r, i;
	var has_type, has_desc, has_img;

	has_type = db.props.match(/\btype\b/);
	has_desc = db.props.match(/\bdesc\b/);
	has_img = db.props.match(/\bimg\b/);

	r = new Array();
	for (i = 0; i < arts.length; i++) {
		var e, title, link, a;
		e = arts[i];

		if (has_type && e.getAttribute("type") == "0") {
			var handle, props, key, meta;
			title = e.getAttribute("title");
			link = e.getAttribute("link");
			handle = e.getAttribute("handle");
			props = e.getAttribute("props");
			meta = false;	// XXX
			key = db.key + "/" + link + "/" + handle;
			a = {type: 0, key: key, title: title, url: link, handle: handle, props: props, arts: null, meta: meta};
		}
		else {
			var name, desc, img;
			name = e.getAttribute("name");
			title = e.getAttribute("title");
			link = e.getAttribute("link");
			desc = null;
			img = null;

			if (has_desc) {
				desc = e.getAttribute("desc");
			}
			if (has_img) {
				img = e.getAttribute("img");
			}

			a = {type: 1, db: db, id: name, title: title, url: link, vec: null, desc: desc, img: img};
		}

		r[r.length] = a;
	}
	return r;
}

function
anames(arts)
{
	return mapcar(function (e) { return e.getAttribute("name"); }, arts);
}

function
ks(db, kwds)
{
	return mapcar(function (e) {
		var name;
		name = e.getAttribute("name");
		return {type: 2, db: db, id: name, title: name};
	}, kwds);
		
}

function
personal_db_arts(de, corpora, url)
{
	return mapcar(function (e) { return pd_ent(de, e, url); }, corpora);
}

function
pd_ent(de, e, url)
{
	return dblent(de, e.getAttribute("title"), url, e.getAttribute("name"), e.getAttribute("properties"), false, true);
}

function
dblent(de, title, url, handle, props, meta, default_checked)
{
	var key;
	key = de.key + "/" + url + "/" + handle;

	return {type: 0, key: key, title: title, url: url, handle: handle, props: props, meta: meta, dflc: default_checked};
}
