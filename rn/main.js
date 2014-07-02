/*	$Id: main.js,v 1.7 2009/07/18 11:21:48 nis Exp $	*/

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

var last_query = null;

var t1 = null;
var h1 = null;
var div0 = null;
var ckdb0 = null;

function
sigonload()
{
	var d, top, q;

	check_platform();

	d = document;
	div0 = d.getElementById("div0");
	top = generateTopPage();
	div0.appendChild(top);
	if (xIE) {
		var v;
		v = d.getElementById("ckdb1");
		v.checked = true;
	}
	xreqandcb(p_url, cons_catalog(), function (m) { init_root_db(m); });
}

function
init_root_db(msg)
{
	var j, je, arts;
	arts = root_db_arts(root_db);
	if (gss3_status(msg) == "OK") {
		var p_arts;
		p_arts = personal_db_arts(root_db, getResultNode(msg).childNodes, p_url);
		arts = nconc(p_arts, arts);
	}

	je = {db: root_db, arts: arts, kwds: null, curn: 0};
	j = jik(je, false, false, false);
	update_jik(j, h1);
	mapc(init_check, j.childNodes);
}

function
init_check(o)
{
	var n, e;

	o.mouseEnabled = false;
	n = gn(o, "node mnode", 0, "anode-hw", 0, "checkbox");
	if (!n) {
		return;
	}
	e = n.entity;
	if (!e) {
		return;
	}
	o.onmousedown = function (e) { mouseDown(e, this); return false; };
	if (e.dflc) {
		n.checked = true;
		chk(n);
	}
}

function
chk(o)
{
	var e;
	e = o.entity;
	switch (e.type) {
	case 0:
		db_chk(o, e, o.checked);
		break;
	case 1:
		ar_chk(o, e, o.checked);
		break;
	}
}

function
db_chk(o, db, checked)
{
	var v0, kwd_db;
	v0 = pickElementById(h1, db.key);
	kwd_db = pickElementById(h1, "kwd_db");
	if (checked && !v0) {
		var je, j;
		je = {db: db, arts: null, kwds: null, curn: 0};
		j = jik(je, true, true, false);
		nc(h1, j);
		reset_h1_size();
		gathj(pickElementById(h1, "root_db"));
		if (last_query) {
			xqq(je, last_query, false);
		}
	}
	else if (!checked && v0) {
		delete_from(h1, v0);
		reset_h1_size();
		if (kwd_db) {
			var id, kj;
			id = "KWDS/" + db.key;
			kj = pickElementById(kwd_db, id);
			if (kj) {
				delete_from(kwd_db, kj);
			}
		}
	}
}

function
ar_chk(o, a, checked)
{
	if (checked) {
		getvec(o, a);
	}
	else {
		a.vec = null;
		setClass(o.abox, "node anode");
	}
}

function
getvec(o, a)
{
	xreqandcb(a.db.url, cons_getvec(a.db.handle, a.id), function (m) { getvec_done(o, a, m)});
}

function
getvec_done(o, a, rx)
{
	var ar;
	if (gss3_status(rx) != "OK") {
		return;
	}
	ar = getFirstArticleNode(rx);
	a.vec = ar.getAttribute("vec");
	setClass(o.abox, "node anode anode-armed");
}

function
db_vecs()
{
	return mapcan(jvecs, h1.childNodes);
}

function
checked_name(n)
{
	var b;
	if ((b = getAnodeCheckbox(n)) && b.checked) {
		return [b.entity.id];
	}
	return [];
}

function
checked_names(j)
{
	var je;
	if (!(classp(j, "jik kwd") && j.entity)) {
/* "jik art" => should not happen */
		return [];
	}
	je = j.entity;
	if (!je.db.meta) {
		return mapcan(checked_name, j.childNodes);
	}
	return [];
}

function
kwd2vec_l(j)
{
	var nl;
	nl = mapcan(checked_names, j.childNodes);
	if (nl.length == 0) return [];
	return [names2vec(nl)];
}

function
jvecs(j)
{
	var je;
	if (!((classp(j, "jik art") || classp(j, "jik kwd")) && j.entity)) {
		return [];
	}
	je = j.entity;
	if (je.db.key == "kwd_db") {
		return kwd2vec_l(j);
	}
	else if (!je.db.meta) {
		var a;
		a = je.arts;
		if (a) {
			return mapcan(function (e) { return e.vec ? [e.vec] : []; }, a);
		}
	}
	return [];
}

function
ren()
{
	var text, vecs;

	text = t1.value;
	vecs = db_vecs();

	if (!vecs.length && !text) {
		alert("気に入った項目にチェックしてから、もう一度押してください");
	}
	else {
		last_query = {text: text, vecs: vecs};
		mapc(jren, h1.childNodes);
		t1.value = "";
	}
}

function
jren(j)
{
	var je;
	if (!(classp(j, "jik art") && j.entity)) {
/* "jik kwd" => do nothing */
		return;
	}
	je = j.entity;
	if (!je.db.meta || unlocked_meta_db_p(j)) {
		mapc(disable_anode, j.childNodes);
		je.curn = 0;
		xqq(je, last_query, false);
	}
}

function
xren(o)
{
	var db, je, j;

	db = o.entity;
	j = pickElementById(h1, db.key);
	je = j.entity;
	xqq(je, last_query, true);
}

function
unlocked_meta_db_p(j)
{
	var c;

	c = gn(j, "jik art", 1, "inode", 0, "cblb", 0, "cb");
	return c && !c.checked;
}

function
xqq(je, q0, req_more)
{
	if (je.db.meta) {
		var mrdb;
		mrdb = metadb_reference_db;
		xreqandcb(mrdb.url, cons_assoc(mrdb, q0, "200", "0"), function (m) { mren_done(je, m); });
	}
	else {
		xreqandcb(je.db.url, cons_assoc(je.db, q0, "" + (je.curn + 10), "10"), function (m) { ren_done(je, m, req_more); });
	}
}

function
mren_done(je, rx)
{
	var aclss, atotal, a;
	var kclss, ktotal, k;

	if (gss3_status(rx) != "OK") {
		return;
	}

	aclss = getArticlesNode(rx);
	atotal = aclss.getAttribute("total");
	a = anames(getArticleList(aclss));

	q0 = {text: null, vecs: [names2vec(a)]};
	xreqandcb(je.db.url, cons_assoc(je.db, q0, "" + (je.curn + 10), "0"), function (m) { ren_done(je, m, false); });
}

function
names2vec(a)
{
	var r, i, j;
	a.sort();
	r = "[";
	for (i = 0; i < a.length; i = j) {
		var as, k;
		for (j = i + 1; j < a.length && a[i] == a[j]; j++) ;
		as = a[i].replace(/\\/g, "\\\\").replace(/"/g, "\\\"");
		k = j - i;
		r += "[\"" + as + "\"," + k + "]";
		if (j < a.length) {
			r += ",";
		}
	}
	return r + "]";
}

function
ren_done(je, rx, req_more)
{
	var aclss, atotal, a;
	var kclss, ktotal, k;
	var j;

	if (gss3_status(rx) != "OK") {
		return;
	}

	aclss = getArticlesNode(rx);
	atotal = aclss.getAttribute("total");
	a = as(je.db, getArticleList(aclss));

	if (req_more) {
		var v0;
		v0 = pickElementById(h1, je.db.key);
		if (v0) {
			var je0, oa, i;
			je0 = v0.entity;
/* assert(je0 && je0.arts) */
			oa = je0.arts;
/* assert(oa && oa.length <= a.length); */
			for (i = 0; i < oa.length; i++) {
				a[i].vec = oa[i].vec;
			}
		}
	}

	kclss = getKeywordsNode(rx);
	ktotal = kclss.getAttribute("total");
	k = ks(je.db, getKeywordList(kclss));

	je.curn += 10;
	je.arts = a;
	je.kwds = k;
	je.atotal = atotal;
	je.ktotal = atotal;
	j = jik(je, true, true, false);

	update_jik(j, h1);

	if (!req_more) {
		update_kwd_jik(j);
	}
	/* else do nothing, if this query is triggerd by clicking a more button */
}

function
update_jik(j, h)
{
	var id, v0;

	id = j.getAttribute("id");
	v0 = pickElementById(h, id);

	if (v0) {
		subst(h, j, v0);
	}
	else {
		nc(h, j);
		reset_h1_size();
	}
}

function
reset_h1_size()
{
	var w;

	w = 0;
	mapc(function (e) {
			w += 210;
		},
	h1.childNodes);
	w += 8;
	h1.style.width = w + "px";
}

function
disable_anode(e)
{
	var nc;
	nc = e.getAttribute("class") + " anode-dim";
	setClass(e, nc);
}

function
create_anode(d, e, kwdp, target, meta, force_chk)
{
	var v, h, c, a0, a, t, tr, td, checked;

	checked = e.type == 0 && pickElementById(h1, e.key) || force_chk;

	v = newDiv(d, kwdp ? "node knode" : meta ? "node mnode" : "node anode");

	h = newDiv(d, kwdp ? "knode-hw" : "anode-hw");

	c = newCheckbox(d, "checkbox", kwdp ? "" : "chk(this);", checked);

	t = d.createTextNode(e.title);
	if (kwdp || meta) {
		a0 = newAnode(d, "mnode-title", null, null);
	}
	else {
		a0 = newAnode(d, "tnode " + (kwdp ? "knode" : "anode") + "-title", e.url, null);
		a0.setAttribute("target", target);
	}
	a = nc(a0, t);
	nc(h, c, a);

	nc(v, h);
	if (e.img) {
		var bx, im;
/* assert(!kwdp); */
		bx = newDiv(d, "anode-image");
		im = newImg(d, null, e.img);
		nc(v, nc(bx, im));
	}
	if (e.desc) {
		var de;
/* assert(!kwdp); */
		de = newDText(d, "anode-desc", e.desc);
		nc(v, de);
	}

	c.entity = e;
	c.abox = v;
	a.entity = e;

	return v;
}

function
clear_check()
{
	var kwd_db;

	t1.value = "";

	mapc(clrallchk, h1.childNodes);
	kwd_db = pickElementById(h1, "kwd_db");
	if (kwd_db) {
		mapc(clrallchk, kwd_db.childNodes);
	}
}

function
clrallchk(j)
{
	var je;

	if (!((classp(j, "jik art") || classp(j, "jik kwd")) && j.entity)) {
		return;
	}

	je = j.entity;
	if (je.db.meta) {
		return;
	}
	mapc(function (e) {
			var cck;
			if ((cck = getAnodeCheckbox(e)) && cck.checked) {
				cck.checked = false;
				chk(cck);
			}
		}, j.childNodes);
}

function
gath(o)
{
	var db, j;

	db = o.entity;
	j = pickElementById(h1, db.key);

	gathj(j);
}

function
gathj(j)
{
	var kwd_db;

	kwd_db = pickElementById(h1, "kwd_db");
	mapc(function (e) {
		var cck;
		if ((cck = getAnodeCheckbox(e)) && cck.checked) {
			var jk;
			jk = pickElementById(h1, cck.entity.key);
			if (jk) {
				bMoveToLast(h1, jk);
			}
			if (kwd_db) {
				var id, kj;
				id = "KWDS/" + cck.entity.key;
				kj = pickElementById(kwd_db, id);
				if (kj) {
					bMoveToLast(kwd_db, kj);
				}
			}
		}
	}, j.childNodes);
	if (kwd_db) {
		bMoveToLast(h1, kwd_db);
	}
}

function
getAnodeCheckbox(m)
{
	var n;

/*
 ignore "node anode"
	n = gn(m, "node anode", 0, "anode-hw", 0, "checkbox");
	if (n) return n;
*/

	n = gn(m, "node mnode", 0, "anode-hw", 0, "checkbox");
	if (n) return n;

	n = gn(m, "node anode anode-armed", 0, "anode-hw", 0, "checkbox");
	if (n) return n;

	n = gn(m, "node knode", 0, "knode-hw", 0, "checkbox");
	if (n) return n;

	return null;
}

function
jik(je, lock_button, more_button, kwdp)
{
	var d, j, id;
	var t, ar;
	d = document;
	id = (kwdp ? "KWDS/" : "") + je.db.key;

	j = newDiv(d, kwdp ? "jik kwd" : "jik art");
	j.setAttribute("id", id);

	t = newDText(d, kwdp ? "jkt jkktw" : je.db.key == "kwd_db" ? "jkt jkkdtw" : "jkt jktw", je.db.title);
	nc(j, t);

	if (je.db.meta && lock_button) {
		var c0, d1, c1, c2, c3, t;

		c1 = newLCbox(d, "cblb", "cb", "", "lock", false, null);
		c2 = newDiv(d, "m");

		c3 = newAnode(d, "gather", null, "gath(this);");
		c3.entity = je.db;
		t = d.createTextNode("gather");
		nc(j, nc(newDiv(d, "inode"), c1, c2, nc(c3, t)));
	}

	ar = kwdp ? je.kwds : je.arts;
	if (ar) {
		var i;

		if (je.db.key != "kwd_db" && !je.db.meta && !kwdp) {
			var a;
			a = newDText(d, "inode", "total: " + je.atotal);
			nc(j, a);
		}

		for (i = 0; i < ar.length; i++) {
			var a;
			nc(j, a = create_anode(d, ar[i], kwdp, je.db.key, je.db.meta, ar[i].vec ? true : false));
			if (ar[i].vec) {
				setClass(a, "node anode anode-armed");
				if (xIE) {
					var v;
					v = getAnodeCheckbox(a);
					v.checked = true;
				}
			}
		}

		if (more_button && !kwdp && je.curn < je.atotal) {
			var d1, d0;
			d1 = newDiv(d, "morenodediv");
			d0 = newAnode(d, "morenode", null, "xren(this);");
			d0.entity = je.db;
			t = d.createTextNode("more...");
			nc(j, nc(newDiv(d, "jnode"), nc(d1, nc(d0, t))));
		}
	}
	if (je.db.key != "kwd_db") {
		nc(j, newDiv(d, "sentinel"));
	}
	j.entity = je;
	return j;
}

function
show_dbl(checked)
{
	var je, j;

	if (!(j = pickElementById(h1, "root_db"))) { /* not initilalized ! */
		return;
	}

	if (checked) {
		setClass(j, "jik art");
	}
	else {
		setClass(j, "jik-invisible");
	}
}

function
update_kwd_jik(j)
{
	var je = j.entity;
	if (je.db.key != "kwd_db" && !je.db.meta) {
		var kwd_db;
		kwd_db = pickElementById(h1, "kwd_db");
		if (kwd_db) {
			var kj;
			kj = jik(je, true, false, true);
			update_jik(kj, kwd_db);
		}
	}
}

function
show_kwl(checked)
{
	var je, j;
	var kwd_db = {type: 0, key: "kwd_db", title: "Keyword List", url: null, handle: null, props: null, meta: true};

	if (checked) {
		je = {db: kwd_db, arts: null, kwds: null, curn: 0};
		j = jik(je, false, false, false);
		update_jik(j, h1);
		mapc(update_kwd_jik, h1.childNodes);
	}
	else {
		delete_from(h1, pickElementById(h1, "kwd_db"));
	}
	reset_h1_size();
}

function
generateTopPage()
{
	var d, i1, ta1, btn1, btn2, ckdb, ckkw, ckg, c, h, b, top;

	d = document;

	i1 = ea(d, "img", "id", "i1",
		"src", "img/top_logo.gif",
		"alt", "想　-IMAGINE- [LIMITED ED.]");
	i1 = nc(newDiv(d, "logo"), i1);

	t1 = ea(d, "textarea", "id", "t1",
		"style", "width: 380px; height: 36px;",
		"name", "freetext");
	t1.value = sample_text;

	ta1 = nc(newDiv(d, "tatd"), t1);

	btn1 = ea(d, "input", "id", "b1",
		"type", "image", "src", "img/btn_top_imagine.gif",
		"alt", "IMAGINE");
	setOnClick(btn1, "ren(); return false;");
	btn1 = nc(newDiv(d, "btn1td"), btn1);

	btn2 = ea(d, "input", "id", "b2",
		"type", "image", "src", "img/btn_clear.gif",
		"alt", "Clear");
	setOnClick(btn2, "clear_check(); return false;");
	btn2 = nc(newDiv(d, "btn2td"), btn2);

	ckdb = newLCbox(d, "cblb", "cb", "show_dbl(this.checked);", "DB list", true, xIE ? "ckdb1" : null);
	ckkw = newLCbox(d, "cblb", "cb", "show_kwl(this.checked);", "Keywords", false, null);

	ckg = newDiv(d, "ckg");
	nc(ckg, ckdb, ckkw);

	c = newDiv(d, "c");
	nc(c, ta1, btn1, btn2, ckg);

	h = newDiv(d, "h");
	nc(h, i1, c);

	b = newDiv(d, "b");
	b.id = "h1";
	h1 = b;

	top = newDiv(d, "top");
	nc(top, h, b);

	return top;
}

function
mouseDown(e, n)
{
	var d, form;

	d = document;
	if (!(e = eenv(e))) return;
	n.o = oi(n, e);
	d.onmousemove = function (e) { mouseMove(e, n); return false; };
	d.onmouseup = function (e) { mouseUp(e, n); return false; };
	setClass(div0, "move");
}

function
oi(n, e)
{
	var l, i;
	var py, fy, ny, st, cg, fc, et;
	l = n.parentNode.childNodes;
	for (i = 0; i < l.length && l[i] != n; i++) ;
	i > 1 ? py = l[i - 1].offsetTop : -1;
	fy = l[i + 1].offsetTop;
	i + 2 < l.length ? ny = l[i + 2].offsetTop : -1;
	if (e) {
		st = n.offsetTop - e.clientY;
		et = st + (fy - n.offsetTop);
		cg = false;
		fc = true;
	}
	else {
		st = n.o.st;
		et = n.o.et;
		cg = true;	/* cg is true, when mouseMove is called. */
		fc = n.o.fc;	/* fc is true, until first call to mouseDown. */
	}
	return { i: i, py: py, ny: ny, st: st, et: et, cg: cg, fc: fc};
}

function
mouseUp(e, n)
{
	var d, cg;

	d = document;
	cg = n.o.cg;
	setClass(n, "node mnode");
	n.o = null;
	d.onmousemove = null;
	d.onmouseup = null;
	setClass(div0, "");

	if (cg) {
		gathj(pickElementById(h1, "root_db"));
	}
}

function
mouseMove(e, n)
{
	var o, dy, fy;
	if (!(o = n.o) || !(e = eenv(e))) return;
	dy = e.clientY + o.st;
	fy = e.clientY + o.et;
	if (o.fc) {	/* this is the first call to mouseMove */
		setClass(n, "node mnode mnode-moving");
		o.fc = false;
	}
	if (o.py != -1 && dy < o.py) {
		var p, prev;
		p = n.parentNode;
		prev = p.childNodes[o.i - 1];
		p.removeChild(n);
		p.insertBefore(n, prev);
		n.o = oi(n, null);
	}
	else if (o.ny != -1 && fy > o.ny) {
		var p, nnext;
		p = n.parentNode;
		next = p.childNodes[o.i + 2];
		p.removeChild(n);
		p.insertBefore(n, next);
		n.o = oi(n, null);
	}
}
