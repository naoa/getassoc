/*	$Id: librn.js,v 1.3 2009/07/18 11:21:48 nis Exp $	*/

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

/* DOM-Tree Utility */

function
ea(d, name)
{
	var e, i;

	e = d.createElement(name);
	for (i = 2; i < arguments.length; i += 2) {
		if (i + 1 < arguments.length) {
			if (arguments[i] == "class") {
				setClass(e, arguments[i + 1]);
			}
			else {
				e.setAttribute(arguments[i], arguments[i + 1]);
			}
		}
	}
	return e;
}

function
nc(e)
{
	var i;

	for (i = 1; i < arguments.length; i++) {
		e.appendChild(arguments[i]);
	}
	return e;
}

function
classp(e, c)
{
	return !c || e && e.getAttribute && e.getAttribute("class") == c;
}

function
gn(e, c)
{
	var i;
	if (!classp(e, c)) {
		return null;
	}
	if (arguments.length % 2 != 0) {
		alert("invalid # of arguments to gn");
		return null;
	}
	for (i = 2; i < arguments.length; i += 2) {
		var x;
		x = arguments[i];
		c = arguments[i + 1];
		if (e.childNodes.length <= x) {
			return null;
		}
		e = e.childNodes[x];
		if (!classp(e, c)) {
			return null;
		}
	}
	return e;
}

function
delete_from(t, e)
{
	t.removeChild(e);
}

function
subst(t, e, o)
{
	t.replaceChild(e, o);
}

function
bMoveToLast(t, j)
{
	t.removeChild(j);
	t.appendChild(j);
}

function
pickElementById(t, key)
{
	return member_if(t.childNodes, function(e) { return e.id == key; });
}

function
newCheckbox(d, c, onClick, checked)
{
	var e;
	e = ea(d, "input", "type", "checkbox", "class", c);
	setOnClick(e, onClick);
	e.checked = checked;
	return e;
}

function
newLCbox(d, c, cc, onClick, l, checked, cid)
{
	var cb;
	cb = newCheckbox(d, cc, onClick, checked);
	if (cid) {
		cb.setAttribute("id", cid);
	}
	return nc(newDiv(d, c), cb, d.createTextNode(l));
}

function
newAnode(d, c, url, onClick)
{
	if (url) {
		return ea(d, "a", "class", c, "href", url);
	}
	else if (onClick) {
		return setOnClick(ea(d, "a", "class", c), onClick);
	}
	else {
		return ea(d, "a", "class", c);
	}
}

function
newDiv(d, c)
{
	return ea(d, "div", "class", c);
}

function
newImg(d, c, src)
{
	/* class => not set!!! */
	return ea(d, "img", "src", src);
}

function
newButton(d, c, title, onClick)
{
	return setOnClick(ea(d, "button", "class", c, "title", title), onClick);
}

function
newDText(d, c, t)
{
	return nc(ea(d, "div", "class", c), d.createTextNode(t));
}

/* List Manipulation Functions */

function
mapc(fn, g)
{
	var i;

	for (i = 0; i < g.length; i++) {
		fn(g[i]);
	}
}

function
mapcar(fn, g)
{
	var r, i;

	r = new Array();
	for (i = 0; i < g.length; i++) {
		r[r.length] = fn(g[i]);
	}
	return r;
}

function
mapcan(fn, g)
{
	var r, i;

	r = new Array();
	for (i = 0; i < g.length; i++) {
		nconc(r, fn(g[i]));
	}
	return r;
}

function
nconc(r, v)
{
	var i;
	for (i = 0; i < v.length; i++) {
		r[r.length] = v[i];
	}
	return r;
}

function
member_if(t, fn)
{
	var l = null;
	mapc(function(e) {
			if (fn(e)) {
				l = e;
			}
		}, t);
	return l;
}

/* XMLHttpRequest Utility */

function
xreqandcb(url, msg, fn)
{
	var req;

	req = xh();
	req.onreadystatechange = function () {
		if (req.readyState == 4 && req.status == 200) {
			fn(req.responseXML);
		}
	}
	xreqsend(req, url, msg);
}

var xIE = false;
var xIE8 = false;

function
xreqsend(req, url, msg)
{
	req.open("POST", url, true);
	setmodhdr(req);

	if (!xIE) {
		var serializer = new XMLSerializer();
		msg = serializer.serializeToString(msg);
	}

	req.send(msg);
}

function
check_platform()
{
        if (navigator.appName.indexOf('Microsoft') != -1) {
		xIE = true;
		if (navigator.userAgent.indexOf('MSIE 8') != -1) {
			xIE8 = true;
		}
        }
}

function
xh()
{
	var xmlhttp;
	xmlhttp = null;
	if(window.XMLHttpRequest) {
		xmlhttp = new XMLHttpRequest();
	}
	else if(window.ActiveXObject) {
		try {
			xmlhttp = new ActiveXObject("Msxml2.XMLHTTP");
		} catch(e) {
			xmlhttp = new ActiveXObject("Microsoft.XMLHTTP");
		}
	}
	return xmlhttp;
}

function
eenv(e)
{
	if (xIE) {
		return window.event;
	}
	return e;
}

function
createXMLDocument()
{
	if (document.implementation && document.implementation.createDocument) {
		return document.implementation.createDocument("", "", null);
	}
	else if (window.ActiveXObject) {
		return new ActiveXObject("MSXML2.DOMDocument");
	}
	return null;
}

function
setmodhdr(req)
{
	if (xIE) {
		req.setRequestHeader("If-Modified-Since", "Sat, 1 Jan 2000 00:00:00 GMT");
	}
}

function
setClass(e, c)
{
	if (xIE) {
		e.className = c;
	}
	e.setAttribute("class", c);
	return e;
}

function
setOnClick(e, c)
{
	if (!xIE || xIE8) {
		e.setAttribute("onclick", c);
	}
	else {
		e.setAttribute("onclick", new Function(c));
	}
	return e;
}
