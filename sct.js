function scaw(k) {
	if (!document.longquery.xref.checked) {
		return;
	}
	for (i = 0; i < nw; i++) {
		if (k == 0) {
			v = "#000000";
		}
		else {
			v = aw[k][i];
		}
		document.getElementById(aw[0][i]).style.color = v;
	}
}

function scwa(k) {
	if (!document.longquery.xref.checked) {
		return;
	}
	for (i = 0; i < na; i++) {
		if (k == 0) {
			v = "#a52a2a";
		}
		else {
			v = wa[k][i];
		}
		document.getElementById(wa[0][i]).style.color = v;
	}
}
