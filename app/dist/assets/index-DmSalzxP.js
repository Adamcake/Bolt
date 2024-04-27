var Xt = Object.defineProperty;
var zt = (t, e, n) =>
	e in t ? Xt(t, e, { enumerable: !0, configurable: !0, writable: !0, value: n }) : (t[e] = n);
var We = (t, e, n) => (zt(t, typeof e != 'symbol' ? e + '' : e, n), n);
(function () {
	const e = document.createElement('link').relList;
	if (e && e.supports && e.supports('modulepreload')) return;
	for (const o of document.querySelectorAll('link[rel="modulepreload"]')) r(o);
	new MutationObserver((o) => {
		for (const l of o)
			if (l.type === 'childList')
				for (const c of l.addedNodes)
					c.tagName === 'LINK' && c.rel === 'modulepreload' && r(c);
	}).observe(document, { childList: !0, subtree: !0 });
	function n(o) {
		const l = {};
		return (
			o.integrity && (l.integrity = o.integrity),
			o.referrerPolicy && (l.referrerPolicy = o.referrerPolicy),
			o.crossOrigin === 'use-credentials'
				? (l.credentials = 'include')
				: o.crossOrigin === 'anonymous'
					? (l.credentials = 'omit')
					: (l.credentials = 'same-origin'),
			l
		);
	}
	function r(o) {
		if (o.ep) return;
		o.ep = !0;
		const l = n(o);
		fetch(o.href, l);
	}
})();
function j() {}
function Ft(t) {
	return !!t && (typeof t == 'object' || typeof t == 'function') && typeof t.then == 'function';
}
function Mt(t) {
	return t();
}
function dt() {
	return Object.create(null);
}
function oe(t) {
	t.forEach(Mt);
}
function Et(t) {
	return typeof t == 'function';
}
function se(t, e) {
	return t != t ? e == e : t !== e || (t && typeof t == 'object') || typeof t == 'function';
}
let De;
function $t(t, e) {
	return t === e ? !0 : (De || (De = document.createElement('a')), (De.href = e), t === De.href);
}
function Vt(t) {
	return Object.keys(t).length === 0;
}
function It(t, ...e) {
	if (t == null) {
		for (const r of e) r(void 0);
		return j;
	}
	const n = t.subscribe(...e);
	return n.unsubscribe ? () => n.unsubscribe() : n;
}
function te(t) {
	let e;
	return It(t, (n) => (e = n))(), e;
}
function X(t, e, n) {
	t.$$.on_destroy.push(It(e, n));
}
function H(t, e, n) {
	return t.set(n), e;
}
function _(t, e) {
	t.appendChild(e);
}
function C(t, e, n) {
	t.insertBefore(e, n || null);
}
function x(t) {
	t.parentNode && t.parentNode.removeChild(t);
}
function He(t, e) {
	for (let n = 0; n < t.length; n += 1) t[n] && t[n].d(e);
}
function k(t) {
	return document.createElement(t);
}
function F(t) {
	return document.createTextNode(t);
}
function T() {
	return F(' ');
}
function ve() {
	return F('');
}
function O(t, e, n, r) {
	return t.addEventListener(e, n, r), () => t.removeEventListener(e, n, r);
}
function f(t, e, n) {
	n == null ? t.removeAttribute(e) : t.getAttribute(e) !== n && t.setAttribute(e, n);
}
function Wt(t) {
	return Array.from(t.childNodes);
}
function ae(t, e) {
	(e = '' + e), t.data !== e && (t.data = e);
}
function ge(t, e) {
	t.value = e ?? '';
}
function ft(t, e, n) {
	for (let r = 0; r < t.options.length; r += 1) {
		const o = t.options[r];
		if (o.__value === e) {
			o.selected = !0;
			return;
		}
	}
	(!n || e !== void 0) && (t.selectedIndex = -1);
}
function Zt(t) {
	const e = t.querySelector(':checked');
	return e && e.__value;
}
function Yt(t, e, { bubbles: n = !1, cancelable: r = !1 } = {}) {
	return new CustomEvent(t, { detail: e, bubbles: n, cancelable: r });
}
let Ee;
function pe(t) {
	Ee = t;
}
function Oe() {
	if (!Ee) throw new Error('Function called outside component initialization');
	return Ee;
}
function Ae(t) {
	Oe().$$.on_mount.push(t);
}
function Kt(t) {
	Oe().$$.after_update.push(t);
}
function rt(t) {
	Oe().$$.on_destroy.push(t);
}
function Qt() {
	const t = Oe();
	return (e, n, { cancelable: r = !1 } = {}) => {
		const o = t.$$.callbacks[e];
		if (o) {
			const l = Yt(e, n, { cancelable: r });
			return (
				o.slice().forEach((c) => {
					c.call(t, l);
				}),
				!l.defaultPrevented
			);
		}
		return !0;
	};
}
const Pe = [],
	B = [];
let Re = [];
const Ke = [],
	en = Promise.resolve();
let Qe = !1;
function tn() {
	Qe || ((Qe = !0), en.then(ot));
}
function Be(t) {
	Re.push(t);
}
function Ie(t) {
	Ke.push(t);
}
const Ze = new Set();
let xe = 0;
function ot() {
	if (xe !== 0) return;
	const t = Ee;
	do {
		try {
			for (; xe < Pe.length; ) {
				const e = Pe[xe];
				xe++, pe(e), nn(e.$$);
			}
		} catch (e) {
			throw ((Pe.length = 0), (xe = 0), e);
		}
		for (pe(null), Pe.length = 0, xe = 0; B.length; ) B.pop()();
		for (let e = 0; e < Re.length; e += 1) {
			const n = Re[e];
			Ze.has(n) || (Ze.add(n), n());
		}
		Re.length = 0;
	} while (Pe.length);
	for (; Ke.length; ) Ke.pop()();
	(Qe = !1), Ze.clear(), pe(t);
}
function nn(t) {
	if (t.fragment !== null) {
		t.update(), oe(t.before_update);
		const e = t.dirty;
		(t.dirty = [-1]), t.fragment && t.fragment.p(t.ctx, e), t.after_update.forEach(Be);
	}
}
function sn(t) {
	const e = [],
		n = [];
	Re.forEach((r) => (t.indexOf(r) === -1 ? e.push(r) : n.push(r))),
		n.forEach((r) => r()),
		(Re = e);
}
const Ue = new Set();
let be;
function we() {
	be = { r: 0, c: [], p: be };
}
function ye() {
	be.r || oe(be.c), (be = be.p);
}
function A(t, e) {
	t && t.i && (Ue.delete(t), t.i(e));
}
function D(t, e, n, r) {
	if (t && t.o) {
		if (Ue.has(t)) return;
		Ue.add(t),
			be.c.push(() => {
				Ue.delete(t), r && (n && t.d(1), r());
			}),
			t.o(e);
	} else r && r();
}
function Je(t, e) {
	const n = (e.token = {});
	function r(o, l, c, i) {
		if (e.token !== n) return;
		e.resolved = i;
		let u = e.ctx;
		c !== void 0 && ((u = u.slice()), (u[c] = i));
		const a = o && (e.current = o)(u);
		let d = !1;
		e.block &&
			(e.blocks
				? e.blocks.forEach((h, p) => {
						p !== l &&
							h &&
							(we(),
							D(h, 1, 1, () => {
								e.blocks[p] === h && (e.blocks[p] = null);
							}),
							ye());
					})
				: e.block.d(1),
			a.c(),
			A(a, 1),
			a.m(e.mount(), e.anchor),
			(d = !0)),
			(e.block = a),
			e.blocks && (e.blocks[l] = a),
			d && ot();
	}
	if (Ft(t)) {
		const o = Oe();
		if (
			(t.then(
				(l) => {
					pe(o), r(e.then, 1, e.value, l), pe(null);
				},
				(l) => {
					if ((pe(o), r(e.catch, 2, e.error, l), pe(null), !e.hasCatch)) throw l;
				}
			),
			e.current !== e.pending)
		)
			return r(e.pending, 0), !0;
	} else {
		if (e.current !== e.then) return r(e.then, 1, e.value, t), !0;
		e.resolved = t;
	}
}
function Nt(t, e, n) {
	const r = e.slice(),
		{ resolved: o } = t;
	t.current === t.then && (r[t.value] = o),
		t.current === t.catch && (r[t.error] = o),
		t.block.p(r, n);
}
function fe(t) {
	return (t == null ? void 0 : t.length) !== void 0 ? t : Array.from(t);
}
function Ne(t, e, n) {
	const r = t.$$.props[e];
	r !== void 0 && ((t.$$.bound[r] = n), n(t.$$.ctx[r]));
}
function ne(t) {
	t && t.c();
}
function K(t, e, n) {
	const { fragment: r, after_update: o } = t.$$;
	r && r.m(e, n),
		Be(() => {
			const l = t.$$.on_mount.map(Mt).filter(Et);
			t.$$.on_destroy ? t.$$.on_destroy.push(...l) : oe(l), (t.$$.on_mount = []);
		}),
		o.forEach(Be);
}
function Q(t, e) {
	const n = t.$$;
	n.fragment !== null &&
		(sn(n.after_update),
		oe(n.on_destroy),
		n.fragment && n.fragment.d(e),
		(n.on_destroy = n.fragment = null),
		(n.ctx = []));
}
function rn(t, e) {
	t.$$.dirty[0] === -1 && (Pe.push(t), tn(), t.$$.dirty.fill(0)),
		(t.$$.dirty[(e / 31) | 0] |= 1 << e % 31);
}
function le(t, e, n, r, o, l, c = null, i = [-1]) {
	const u = Ee;
	pe(t);
	const a = (t.$$ = {
		fragment: null,
		ctx: [],
		props: l,
		update: j,
		not_equal: o,
		bound: dt(),
		on_mount: [],
		on_destroy: [],
		on_disconnect: [],
		before_update: [],
		after_update: [],
		context: new Map(e.context || (u ? u.$$.context : [])),
		callbacks: dt(),
		dirty: i,
		skip_bound: !1,
		root: e.target || u.$$.root
	});
	c && c(a.root);
	let d = !1;
	if (
		((a.ctx = n
			? n(t, e.props || {}, (h, p, ...v) => {
					const w = v.length ? v[0] : p;
					return (
						a.ctx &&
							o(a.ctx[h], (a.ctx[h] = w)) &&
							(!a.skip_bound && a.bound[h] && a.bound[h](w), d && rn(t, h)),
						p
					);
				})
			: []),
		a.update(),
		(d = !0),
		oe(a.before_update),
		(a.fragment = r ? r(a.ctx) : !1),
		e.target)
	) {
		if (e.hydrate) {
			const h = Wt(e.target);
			a.fragment && a.fragment.l(h), h.forEach(x);
		} else a.fragment && a.fragment.c();
		e.intro && A(t.$$.fragment), K(t, e.target, e.anchor), ot();
	}
	pe(u);
}
class ce {
	constructor() {
		We(this, '$$');
		We(this, '$$set');
	}
	$destroy() {
		Q(this, 1), (this.$destroy = j);
	}
	$on(e, n) {
		if (!Et(n)) return j;
		const r = this.$$.callbacks[e] || (this.$$.callbacks[e] = []);
		return (
			r.push(n),
			() => {
				const o = r.indexOf(n);
				o !== -1 && r.splice(o, 1);
			}
		);
	}
	$set(e) {
		this.$$set &&
			!Vt(e) &&
			((this.$$.skip_bound = !0), this.$$set(e), (this.$$.skip_bound = !1));
	}
}
const on = '4';
typeof window < 'u' && (window.__svelte || (window.__svelte = { v: new Set() })).v.add(on);
const Ce = [];
function jt(t, e) {
	return { subscribe: Z(t, e).subscribe };
}
function Z(t, e = j) {
	let n;
	const r = new Set();
	function o(i) {
		if (se(t, i) && ((t = i), n)) {
			const u = !Ce.length;
			for (const a of r) a[1](), Ce.push(a, t);
			if (u) {
				for (let a = 0; a < Ce.length; a += 2) Ce[a][0](Ce[a + 1]);
				Ce.length = 0;
			}
		}
	}
	function l(i) {
		o(i(t));
	}
	function c(i, u = j) {
		const a = [i, u];
		return (
			r.add(a),
			r.size === 1 && (n = e(o, l) || j),
			i(t),
			() => {
				r.delete(a), r.size === 0 && n && (n(), (n = null));
			}
		);
	}
	return { set: o, update: l, subscribe: c };
}
function Ht(t) {
	if (t.ok) return t.value;
	throw t.error;
}
var Y = ((t) => ((t[(t.rs3 = 0)] = 'rs3'), (t[(t.osrs = 1)] = 'osrs'), t))(Y || {}),
	de = ((t) => (
		(t[(t.runeLite = 0)] = 'runeLite'), (t[(t.hdos = 1)] = 'hdos'), (t[(t.rs3 = 2)] = 'rs3'), t
	))(de || {});
const ln = {
		use_dark_theme: !0,
		flatpak_rich_presence: !1,
		rs_config_uri: '',
		runelite_custom_jar: '',
		runelite_use_custom_jar: !1,
		selected_account: '',
		selected_characters: new Map(),
		selected_game_accounts: new Map(),
		selected_game_index: 1,
		selected_client_index: 1
	},
	Ot = jt('https://bolt-internal'),
	cn = jt('1fddee4e-b100-4f4e-b2b0-097f9088f9d2'),
	lt = Z(),
	Fe = Z(''),
	q = Z({ ...ln }),
	me = Z(new Map()),
	Te = Z(!1),
	ke = Z(),
	et = Z(),
	Se = Z([]),
	$e = Z({}),
	ct = Z([]),
	tt = Z(''),
	nt = Z(''),
	st = Z(''),
	$ = Z(!1),
	Ge = Z(new Map()),
	W = Z({ game: Y.osrs, client: de.runeLite }),
	Xe = Z(!1);
function an() {
	re.use_dark_theme == !1 && document.documentElement.classList.remove('dark');
}
function un(t, e) {
	e && t.win.close(), ct.update((n) => (n.splice(at.indexOf(t), 1), n));
}
function At() {
	if (ue && ue.win && !ue.win.closed) ue.win.focus();
	else if ((ue && ue.win && ue.win.closed) || ue) {
		const t = qt(),
			e = pn();
		fn({
			origin: atob(V.origin),
			redirect: atob(V.redirect),
			authMethod: '',
			loginType: '',
			clientid: atob(V.clientid),
			flow: 'launcher',
			pkceState: t,
			pkceCodeVerifier: e
		}).then((n) => {
			const r = window.open(n, '', 'width=480,height=720');
			$e.set({ state: t, verifier: e, win: r });
		});
	}
}
function dn() {
	const t = new URLSearchParams(window.location.search);
	Fe.set(t.get('platform')),
		t.get('flathub'),
		tt.set(t.get('rs3_linux_installed_hash')),
		nt.set(t.get('runelite_installed_id')),
		st.set(t.get('hdos_installed_version'));
	const e = t.get('plugins');
	e !== null ? (Te.set(!0), ke.set(JSON.parse(e))) : Te.set(!1);
	const n = t.get('credentials');
	if (n)
		try {
			JSON.parse(n).forEach((l) => {
				me.update((c) => (c.set(l.sub, l), c));
			});
		} catch (o) {
			G(`Couldn't parse credentials file: ${o}`, !1);
		}
	const r = t.get('config');
	if (r)
		try {
			const o = JSON.parse(r);
			q.set(o),
				q.update(
					(l) => (
						l.selected_game_accounts
							? ((l.selected_characters = new Map(
									Object.entries(l.selected_game_accounts)
								)),
								delete l.selected_game_accounts)
							: l.selected_characters &&
								(l.selected_characters = new Map(
									Object.entries(l.selected_characters)
								)),
						l
					)
				);
		} catch (o) {
			G(`Couldn't parse config file: ${o}`, !1);
		}
}
async function Dt(t, e, n) {
	return new Promise((r) => {
		if (t.expiry - Date.now() < 3e4) {
			const o = new URLSearchParams({
					grant_type: 'refresh_token',
					client_id: n,
					refresh_token: t.refresh_token
				}),
				l = new XMLHttpRequest();
			(l.onreadystatechange = () => {
				if (l.readyState == 4)
					if (l.status == 200) {
						const c = Ut(l.response),
							i = Ht(c);
						i
							? ((t.access_token = i.access_token),
								(t.expiry = i.expiry),
								(t.id_token = i.id_token),
								(t.login_provider = i.login_provider),
								(t.refresh_token = i.refresh_token),
								i.session_id && (t.session_id = i.session_id),
								(t.sub = i.sub),
								r(null))
							: r(0);
					} else r(l.status);
			}),
				(l.onerror = () => {
					r(0);
				}),
				l.open('POST', e, !0),
				l.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded'),
				l.setRequestHeader('Accept', 'application/json'),
				l.send(o);
		} else r(null);
	});
}
function Ut(t) {
	const e = JSON.parse(t),
		n = e.id_token.split('.');
	if (n.length !== 3) {
		const l = `Malformed id_token: ${n.length} sections, expected 3`;
		return G(l, !1), { ok: !1, error: new Error(l) };
	}
	const r = JSON.parse(atob(n[0]));
	if (r.typ !== 'JWT') {
		const l = `Bad id_token header: typ ${r.typ}, expected JWT`;
		return G(l, !1), { ok: !1, error: new Error(l) };
	}
	const o = JSON.parse(atob(n[1]));
	return {
		ok: !0,
		value: {
			access_token: e.access_token,
			id_token: e.id_token,
			refresh_token: e.refresh_token,
			sub: o.sub,
			login_provider: o.login_provider || null,
			expiry: Date.now() + e.expires_in * 1e3,
			session_id: e.session_id
		}
	};
}
async function fn(t) {
	const e = new TextEncoder().encode(t.pkceCodeVerifier),
		n = await crypto.subtle.digest('SHA-256', e);
	let r = '';
	const o = new Uint8Array(n);
	for (let c = 0; c < o.byteLength; c++) r += String.fromCharCode(o[c]);
	const l = btoa(r).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
	return t.origin.concat('/oauth2/auth?').concat(
		new URLSearchParams({
			auth_method: t.authMethod,
			login_type: t.loginType,
			flow: t.flow,
			response_type: 'code',
			client_id: t.clientid,
			redirect_uri: t.redirect,
			code_challenge: l,
			code_challenge_method: 'S256',
			prompt: 'login',
			scope: 'openid offline gamesso.token.create user.profile.read',
			state: t.pkceState
		}).toString()
	);
}
function pn() {
	const t = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~',
		e = new Uint32Array(43);
	return (
		crypto.getRandomValues(e),
		Array.from(e, function (n) {
			return t[n % t.length];
		}).join('')
	);
}
function qt() {
	const e = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz',
		n = e.length - 1,
		r = crypto.getRandomValues(new Uint8Array(12));
	return Array.from(r)
		.map((o) => Math.round((o * (n - 0)) / 255 + 0))
		.map((o) => e[o])
		.join('');
}
async function Bt(t, e, n) {
	return new Promise((r) => {
		const o = new XMLHttpRequest();
		(o.onreadystatechange = () => {
			o.readyState == 4 &&
				(o.status == 200
					? n.then((l) => {
							if (typeof l != 'number') {
								const c = {
									id: l.id,
									userId: l.userId,
									displayName: l.displayName,
									suffix: l.suffix,
									characters: new Map()
								};
								z(`Successfully added login for ${c.displayName}`),
									JSON.parse(o.response).forEach((i) => {
										c.characters.set(i.accountId, {
											accountId: i.accountId,
											displayName: i.displayName,
											userHash: i.userHash
										});
									}),
									gn(c),
									r(!0);
							} else G(`Error getting account info: ${l}`, !1), r(!1);
						})
					: (G(`Error: from ${e}: ${o.status}: ${o.response}`, !1), r(!1)));
		}),
			o.open('GET', e, !0),
			o.setRequestHeader('Accept', 'application/json'),
			o.setRequestHeader('Authorization', 'Bearer '.concat(t.session_id)),
			o.send();
	});
}
async function pt(t, e) {
	return await _n(t, e);
}
async function _n(t, e) {
	const n = qt(),
		r = crypto.randomUUID(),
		o = atob(V.origin)
			.concat('/oauth2/auth?')
			.concat(
				new URLSearchParams({
					id_token_hint: e.id_token,
					nonce: btoa(r),
					prompt: 'consent',
					redirect_uri: 'http://localhost',
					response_type: 'id_token code',
					state: n,
					client_id: te(cn),
					scope: 'openid offline'
				}).toString()
			),
		l = hn(e);
	return t
		? ((t.location.href = o),
			ct.update(
				(c) => (
					c.push({ state: n, nonce: r, creds: e, win: t, account_info_promise: l }), c
				)
			),
			!1)
		: e.session_id
			? await Bt(e, atob(V.auth_api).concat('/accounts'), l)
			: (G('Rejecting stored credentials with missing session_id', !1), !1);
}
function hn(t) {
	return new Promise((e) => {
		const n = `${atob(V.api)}/users/${t.sub}/displayName`,
			r = new XMLHttpRequest();
		(r.onreadystatechange = () => {
			r.readyState == 4 && (r.status == 200 ? e(JSON.parse(r.response)) : e(r.status));
		}),
			r.open('GET', n, !0),
			r.setRequestHeader('Authorization', 'Bearer '.concat(t.access_token)),
			r.send();
	});
}
function gn(t) {
	const e = () => {
		W.update((n) => {
			n.account = t;
			const [r] = t.characters.keys();
			return (
				(n.character = t.characters.get(r)),
				he.size > 0 && (n.credentials = he.get(t.userId)),
				n
			);
		});
	};
	Ge.update((n) => (n.set(t.userId, t), n)),
		ze.account && re.selected_account
			? t.userId == re.selected_account && e()
			: ze.account || e(),
		$e.set({});
}
function bn(t, e, n) {
	return new Promise((r) => {
		const o = new XMLHttpRequest();
		o.open('POST', e, !0),
			(o.onreadystatechange = () => {
				o.readyState == 4 && r(o.status);
			}),
			o.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded'),
			o.send(new URLSearchParams({ token: t, client_id: n }));
	});
}
async function Me() {
	const t = new XMLHttpRequest();
	t.open('POST', '/save-credentials', !0),
		t.setRequestHeader('Content-Type', 'application/json'),
		(t.onreadystatechange = () => {
			t.readyState == 4 && z(`Save-credentials status: ${t.responseText.trim()}`);
		}),
		W.update((n) => {
			var r;
			return (n.credentials = he.get((r = ze.account) == null ? void 0 : r.userId)), n;
		});
	const e = [];
	he.forEach((n) => {
		e.push(n);
	}),
		t.send(JSON.stringify(e));
}
function mn(t, e, n) {
	Ve();
	const r = (i, u) => {
			const a = new XMLHttpRequest(),
				d = {};
			i && (d.hash = i),
				t && (d.jx_session_id = t),
				e && (d.jx_character_id = e),
				n && (d.jx_display_name = n),
				re.rs_config_uri
					? (d.config_uri = re.rs_config_uri)
					: (d.config_uri = atob(V.default_config_uri)),
				a.open('POST', '/launch-rs3-deb?'.concat(new URLSearchParams(d).toString()), !0),
				(a.onreadystatechange = () => {
					a.readyState == 4 &&
						(z(`Game launch status: '${a.responseText.trim()}'`),
						a.status == 200 && i && tt.set(i));
				}),
				a.send(u);
		},
		o = new XMLHttpRequest(),
		l = atob(V.content_url),
		c = l.concat('dists/trusty/non-free/binary-amd64/Packages');
	o.open('GET', c, !0),
		(o.onreadystatechange = () => {
			if (o.readyState == 4 && o.status == 200) {
				const i = Object.fromEntries(
					o.response
						.split(
							`
`
						)
						.map((u) => u.split(': '))
				);
				if (!i.Filename || !i.Size) {
					G(`Could not parse package data from URL: ${c}`, !1), r();
					return;
				}
				if (i.SHA256 !== te(tt)) {
					z('Downloading RS3 client...');
					const u = new XMLHttpRequest();
					u.open('GET', l.concat(i.Filename), !0),
						(u.responseType = 'arraybuffer'),
						(u.onprogress = (a) => {
							a.loaded &&
								Se.update(
									(d) => (
										(d[0].text = `Downloading RS3 client... ${(Math.round((1e3 * a.loaded) / a.total) / 10).toFixed(1)}%`),
										d
									)
								);
						}),
						(u.onreadystatechange = () => {
							u.readyState == 4 && u.status == 200 && r(i.SHA256, u.response);
						}),
						(u.onerror = () => {
							G(`Error downloading game client: from ${c}: non-http error`, !1), r();
						}),
						u.send();
				} else z('Latest client is already installed'), r();
			}
		}),
		(o.onerror = () => {
			G(`Error: from ${c}: non-http error`, !1), r();
		}),
		o.send();
}
function Jt(t, e, n, r) {
	Ve();
	const o = r ? '/launch-runelite-jar-configure?' : '/launch-runelite-jar?',
		l = (u, a, d) => {
			const h = new XMLHttpRequest(),
				p = {};
			u && (p.id = u),
				d && (p.jar_path = d),
				t && (p.jx_session_id = t),
				e && (p.jx_character_id = e),
				n && (p.jx_display_name = n),
				re.flatpak_rich_presence && (p.flatpak_rich_presence = ''),
				h.open(a ? 'POST' : 'GET', o.concat(new URLSearchParams(p).toString()), !0),
				(h.onreadystatechange = () => {
					h.readyState == 4 &&
						(z(`Game launch status: '${h.responseText.trim()}'`),
						h.status == 200 && u && nt.set(u));
				}),
				h.send(a);
		};
	if (re.runelite_use_custom_jar) {
		l(null, null, re.runelite_custom_jar);
		return;
	}
	const c = new XMLHttpRequest(),
		i = 'https://api.github.com/repos/runelite/launcher/releases';
	c.open('GET', i, !0),
		(c.onreadystatechange = () => {
			if (c.readyState == 4)
				if (c.status == 200) {
					const u = JSON.parse(c.responseText)
						.map((a) => a.assets)
						.flat()
						.find((a) => a.name.toLowerCase() == 'runelite.jar');
					if (u.id != te(nt)) {
						z('Downloading RuneLite...');
						const a = new XMLHttpRequest();
						a.open('GET', u.browser_download_url, !0),
							(a.responseType = 'arraybuffer'),
							(a.onreadystatechange = () => {
								a.readyState == 4 &&
									(a.status == 200
										? l(u.id, a.response)
										: G(
												`Error downloading from ${u.url}: ${a.status}: ${a.responseText}`,
												!1
											));
							}),
							(a.onprogress = (d) => {
								d.loaded &&
									d.lengthComputable &&
									Se.update(
										(h) => (
											(h[0].text = `Downloading RuneLite... ${(Math.round((1e3 * d.loaded) / d.total) / 10).toFixed(1)}%`),
											h
										)
									);
							}),
							a.send();
					} else z('Latest JAR is already installed'), l();
				} else G(`Error from ${i}: ${c.status}: ${c.responseText}`, !1);
		}),
		c.send();
}
function kn(t, e, n) {
	return Jt(t, e, n, !1);
}
function wn(t, e, n) {
	return Jt(t, e, n, !0);
}
function yn(t, e, n) {
	Ve();
	const r = (c, i) => {
			const u = new XMLHttpRequest(),
				a = {};
			c && (a.version = c),
				t && (a.jx_session_id = t),
				e && (a.jx_character_id = e),
				n && (a.jx_display_name = n),
				u.open('POST', '/launch-hdos-jar?'.concat(new URLSearchParams(a).toString()), !0),
				(u.onreadystatechange = () => {
					u.readyState == 4 &&
						(z(`Game launch status: '${u.responseText.trim()}'`),
						u.status == 200 && c && st.set(c));
				}),
				u.send(i);
		},
		o = new XMLHttpRequest(),
		l = 'https://cdn.hdos.dev/client/getdown.txt';
	o.open('GET', l, !0),
		(o.onreadystatechange = () => {
			if (o.readyState == 4)
				if (o.status == 200) {
					const c = o.responseText.match(/^launcher\.version *= *(.*?)$/m);
					if (c && c.length >= 2) {
						const i = c[1];
						if (i !== te(st)) {
							const u = `https://cdn.hdos.dev/launcher/v${i}/hdos-launcher.jar`;
							z('Downloading HDOS...');
							const a = new XMLHttpRequest();
							a.open('GET', u, !0),
								(a.responseType = 'arraybuffer'),
								(a.onreadystatechange = () => {
									if (a.readyState == 4)
										if (a.status == 200) r(i, a.response);
										else {
											const d = JSON.parse(o.responseText)
												.map((h) => h.assets)
												.flat()
												.find(
													(h) => h.name.toLowerCase() == 'runelite.jar'
												);
											G(
												`Error downloading from ${d.url}: ${a.status}: ${a.responseText}`,
												!1
											);
										}
								}),
								(a.onprogress = (d) => {
									d.loaded &&
										d.lengthComputable &&
										Se.update(
											(h) => (
												(h[0].text = `Downloading HDOS... ${(Math.round((1e3 * d.loaded) / d.total) / 10).toFixed(1)}%`),
												h
											)
										);
								}),
								a.send();
						} else z('Latest JAR is already installed'), r();
					} else z("Couldn't parse latest launcher version"), r();
				} else G(`Error from ${l}: ${o.status}: ${o.responseText}`, !1);
		}),
		o.send();
}
let Ye = !1;
function Ve() {
	var t;
	if (te($) && !Ye) {
		Ye = !0;
		const e = new XMLHttpRequest();
		e.open('POST', '/save-config', !0),
			(e.onreadystatechange = () => {
				e.readyState == 4 &&
					(z(`Save config status: '${e.responseText.trim()}'`),
					e.status == 200 && $.set(!1),
					(Ye = !1));
			}),
			e.setRequestHeader('Content-Type', 'application/json');
		const n = {};
		(t = re.selected_characters) == null ||
			t.forEach((l, c) => {
				n[c] = l;
			});
		const r = {};
		Object.assign(r, re), (r.selected_characters = n);
		const o = JSON.stringify(r, null, 4);
		e.send(o);
	}
}
function Gt() {
	return new Promise((t, e) => {
		const n = new XMLHttpRequest(),
			r = te(Ot).concat('/list-game-clients');
		n.open('GET', r, !0),
			(n.onreadystatechange = () => {
				if (n.readyState == 4)
					if (
						n.status == 200 &&
						n.getResponseHeader('content-type') === 'application/json'
					) {
						const o = JSON.parse(n.responseText);
						t(Object.keys(o).map((l) => ({ uid: l, identity: o[l].identity || null })));
					} else e(`error (${n.responseText})`);
			}),
			n.send();
	});
}
function _t(t, e, n) {
	const r = t.slice();
	return (r[22] = e[n]), r;
}
function ht(t) {
	let e,
		n = t[22][1].displayName + '',
		r,
		o,
		l;
	return {
		c() {
			(e = k('option')),
				(r = F(n)),
				f(e, 'data-id', (o = t[22][1].userId)),
				f(e, 'class', 'dark:bg-slate-900'),
				(e.__value = l = t[22][1].displayName),
				ge(e, e.__value);
		},
		m(c, i) {
			C(c, e, i), _(e, r);
		},
		p(c, i) {
			i & 4 && n !== (n = c[22][1].displayName + '') && ae(r, n),
				i & 4 && o !== (o = c[22][1].userId) && f(e, 'data-id', o),
				i & 4 && l !== (l = c[22][1].displayName) && ((e.__value = l), ge(e, e.__value));
		},
		d(c) {
			c && x(e);
		}
	};
}
function vn(t) {
	let e,
		n,
		r,
		o,
		l,
		c,
		i,
		u,
		a,
		d = fe(t[2]),
		h = [];
	for (let p = 0; p < d.length; p += 1) h[p] = ht(_t(t, d, p));
	return {
		c() {
			(e = k('div')), (n = k('select'));
			for (let p = 0; p < h.length; p += 1) h[p].c();
			(r = T()),
				(o = k('div')),
				(l = k('button')),
				(l.textContent = 'Log In'),
				(c = T()),
				(i = k('button')),
				(i.textContent = 'Log Out'),
				f(n, 'name', 'account_select'),
				f(n, 'id', 'account_select'),
				f(
					n,
					'class',
					'w-full cursor-pointer rounded-lg border-2 border-inherit bg-inherit p-2 text-center'
				),
				f(
					l,
					'class',
					'mx-auto mr-2 rounded-lg bg-blue-500 p-2 font-bold text-black duration-200 hover:opacity-75'
				),
				f(
					i,
					'class',
					'mx-auto rounded-lg border-2 border-blue-500 p-2 font-bold duration-200 hover:opacity-75'
				),
				f(o, 'class', 'mt-5 flex'),
				f(
					e,
					'class',
					'z-10 w-48 rounded-lg border-2 border-slate-300 bg-slate-100 p-3 shadow dark:border-slate-800 dark:bg-slate-900'
				),
				f(e, 'role', 'none');
		},
		m(p, v) {
			C(p, e, v), _(e, n);
			for (let w = 0; w < h.length; w += 1) h[w] && h[w].m(n, null);
			t[7](n),
				_(e, r),
				_(e, o),
				_(o, l),
				_(o, c),
				_(o, i),
				u ||
					((a = [
						O(n, 'change', t[8]),
						O(l, 'click', t[9]),
						O(i, 'click', t[10]),
						O(e, 'mouseenter', t[11]),
						O(e, 'mouseleave', t[12])
					]),
					(u = !0));
		},
		p(p, [v]) {
			if (v & 4) {
				d = fe(p[2]);
				let w;
				for (w = 0; w < d.length; w += 1) {
					const R = _t(p, d, w);
					h[w] ? h[w].p(R, v) : ((h[w] = ht(R)), h[w].c(), h[w].m(n, null));
				}
				for (; w < h.length; w += 1) h[w].d(1);
				h.length = d.length;
			}
		},
		i: j,
		o: j,
		d(p) {
			p && x(e), He(h, p), t[7](null), (u = !1), oe(a);
		}
	};
}
function Sn(t, e, n) {
	let r, o, l, c;
	X(t, W, (P) => n(13, (r = P))),
		X(t, me, (P) => n(14, (o = P))),
		X(t, Ge, (P) => n(2, (l = P))),
		X(t, q, (P) => n(15, (c = P)));
	let { showAccountDropdown: i } = e,
		{ hoverAccountButton: u } = e;
	const a = atob(V.origin),
		d = atob(V.clientid),
		h = a.concat('/oauth2/token'),
		p = a.concat('/oauth2/revoke');
	let v = !1,
		w;
	function R(P) {
		u || (P.button === 0 && i && !v && n(5, (i = !1)));
	}
	function b() {
		var ee;
		if (w.options.length == 0) {
			z('Logout unsuccessful: no account selected');
			return;
		}
		let P = r.credentials;
		r.account && (l.delete((ee = r.account) == null ? void 0 : ee.userId), Ge.set(l));
		const E = w.selectedIndex;
		E > 0
			? n(1, (w.selectedIndex = E - 1), w)
			: E == 0 && l.size > 0
				? n(1, (w.selectedIndex = E + 1), w)
				: (delete r.account, delete r.character, delete r.credentials),
			y(),
			P &&
				Dt(P, h, d).then((U) => {
					U === null
						? bn(P.access_token, p, d).then((I) => {
								I === 200
									? (z('Successful logout'), m(P))
									: G(`Logout unsuccessful: status ${I}`, !1);
							})
						: U === 400 || U === 401
							? (z(
									'Logout unsuccessful: credentials are invalid, so discarding them anyway'
								),
								P && m(P))
							: G(
									'Logout unsuccessful: unable to verify credentials due to a network error',
									!1
								);
				});
	}
	function m(P) {
		o.delete(P.sub), Me();
	}
	function y() {
		var E;
		$.set(!0);
		const P = w[w.selectedIndex].getAttribute('data-id');
		if (
			(H(W, (r.account = l.get(P)), r),
			H(q, (c.selected_account = P), c),
			H(W, (r.credentials = o.get((E = r.account) == null ? void 0 : E.userId)), r),
			r.account && r.account.characters)
		) {
			const ee = document.getElementById('character_select');
			ee.selectedIndex = 0;
			const [U] = r.account.characters.keys();
			H(W, (r.character = r.account.characters.get(U)), r);
		}
	}
	addEventListener('mousedown', R),
		Ae(() => {
			var E;
			let P = 0;
			l.forEach((ee, U) => {
				var I;
				ee.displayName == ((I = r.account) == null ? void 0 : I.displayName) &&
					n(1, (w.selectedIndex = P), w),
					P++;
			}),
				H(W, (r.credentials = o.get((E = r.account) == null ? void 0 : E.userId)), r);
		}),
		rt(() => {
			removeEventListener('mousedown', R);
		});
	function g(P) {
		B[P ? 'unshift' : 'push'](() => {
			(w = P), n(1, w);
		});
	}
	const S = () => {
			y();
		},
		L = () => {
			At();
		},
		N = () => {
			b();
		},
		J = () => {
			n(0, (v = !0));
		},
		M = () => {
			n(0, (v = !1));
		};
	return (
		(t.$$set = (P) => {
			'showAccountDropdown' in P && n(5, (i = P.showAccountDropdown)),
				'hoverAccountButton' in P && n(6, (u = P.hoverAccountButton));
		}),
		[v, w, l, b, y, i, u, g, S, L, N, J, M]
	);
}
class Ln extends ce {
	constructor(e) {
		super(), le(this, e, Sn, vn, se, { showAccountDropdown: 5, hoverAccountButton: 6 });
	}
}
function xn(t) {
	let e;
	return {
		c() {
			e = F('Log In');
		},
		m(n, r) {
			C(n, e, r);
		},
		p: j,
		d(n) {
			n && x(e);
		}
	};
}
function Cn(t) {
	var r;
	let e = ((r = t[6].account) == null ? void 0 : r.displayName) + '',
		n;
	return {
		c() {
			n = F(e);
		},
		m(o, l) {
			C(o, n, l);
		},
		p(o, l) {
			var c;
			l & 64 &&
				e !== (e = ((c = o[6].account) == null ? void 0 : c.displayName) + '') &&
				ae(n, e);
		},
		d(o) {
			o && x(n);
		}
	};
}
function gt(t) {
	let e, n, r, o;
	function l(i) {
		t[20](i);
	}
	let c = { hoverAccountButton: t[2] };
	return (
		t[1] !== void 0 && (c.showAccountDropdown = t[1]),
		(n = new Ln({ props: c })),
		B.push(() => Ne(n, 'showAccountDropdown', l)),
		{
			c() {
				(e = k('div')), ne(n.$$.fragment), f(e, 'class', 'absolute right-2 top-[72px]');
			},
			m(i, u) {
				C(i, e, u), K(n, e, null), (o = !0);
			},
			p(i, u) {
				const a = {};
				u & 4 && (a.hoverAccountButton = i[2]),
					!r && u & 2 && ((r = !0), (a.showAccountDropdown = i[1]), Ie(() => (r = !1))),
					n.$set(a);
			},
			i(i) {
				o || (A(n.$$.fragment, i), (o = !0));
			},
			o(i) {
				D(n.$$.fragment, i), (o = !1);
			},
			d(i) {
				i && x(e), Q(n);
			}
		}
	);
}
function Pn(t) {
	let e, n, r, o, l, c, i, u, a, d, h, p, v, w, R, b;
	function m(L, N) {
		return L[6].account ? Cn : xn;
	}
	let y = m(t),
		g = y(t),
		S = t[1] && gt(t);
	return {
		c() {
			(e = k('div')),
				(n = k('div')),
				(r = k('button')),
				(r.textContent = 'RS3'),
				(o = T()),
				(l = k('button')),
				(l.textContent = 'OSRS'),
				(c = T()),
				(i = k('div')),
				(u = k('button')),
				(u.innerHTML =
					'<img src="svgs/lightbulb-solid.svg" class="h-6 w-6" alt="Change Theme"/>'),
				(a = T()),
				(d = k('button')),
				(d.innerHTML = '<img src="svgs/gear-solid.svg" class="h-6 w-6" alt="Settings"/>'),
				(h = T()),
				(p = k('button')),
				g.c(),
				(v = T()),
				S && S.c(),
				f(
					r,
					'class',
					'mx-1 w-20 rounded-lg border-2 border-blue-500 p-2 duration-200 hover:opacity-75'
				),
				f(
					l,
					'class',
					'mx-1 w-20 rounded-lg border-2 border-blue-500 bg-blue-500 p-2 text-black duration-200 hover:opacity-75'
				),
				f(n, 'class', 'm-3 ml-9 font-bold'),
				f(
					u,
					'class',
					'my-3 h-10 w-10 rounded-full bg-blue-500 p-2 duration-200 hover:rotate-45 hover:opacity-75'
				),
				f(
					d,
					'class',
					'm-3 h-10 w-10 rounded-full bg-blue-500 p-2 duration-200 hover:rotate-45 hover:opacity-75'
				),
				f(
					p,
					'class',
					'm-2 w-48 rounded-lg border-2 border-slate-300 bg-inherit p-2 text-center font-bold text-black duration-200 hover:opacity-75 dark:border-slate-800 dark:text-slate-50'
				),
				f(i, 'class', 'ml-auto flex'),
				f(
					e,
					'class',
					'fixed top-0 flex h-16 w-screen border-b-2 border-slate-300 bg-slate-100 duration-200 dark:border-slate-800 dark:bg-slate-900'
				);
		},
		m(L, N) {
			C(L, e, N),
				_(e, n),
				_(n, r),
				t[10](r),
				_(n, o),
				_(n, l),
				t[12](l),
				_(e, c),
				_(e, i),
				_(i, u),
				_(i, a),
				_(i, d),
				_(i, h),
				_(i, p),
				g.m(p, null),
				t[16](p),
				_(e, v),
				S && S.m(e, null),
				(w = !0),
				R ||
					((b = [
						O(r, 'click', t[11]),
						O(l, 'click', t[13]),
						O(u, 'click', t[14]),
						O(d, 'click', t[15]),
						O(p, 'mouseenter', t[17]),
						O(p, 'mouseleave', t[18]),
						O(p, 'click', t[19])
					]),
					(R = !0));
		},
		p(L, [N]) {
			y === (y = m(L)) && g ? g.p(L, N) : (g.d(1), (g = y(L)), g && (g.c(), g.m(p, null))),
				L[1]
					? S
						? (S.p(L, N), N & 2 && A(S, 1))
						: ((S = gt(L)), S.c(), A(S, 1), S.m(e, null))
					: S &&
						(we(),
						D(S, 1, 1, () => {
							S = null;
						}),
						ye());
		},
		i(L) {
			w || (A(S), (w = !0));
		},
		o(L) {
			D(S), (w = !1);
		},
		d(L) {
			L && x(e), t[10](null), t[12](null), g.d(), t[16](null), S && S.d(), (R = !1), oe(b);
		}
	};
}
function Rn(t, e, n) {
	let r, o, l;
	X(t, q, (E) => n(21, (r = E))), X(t, $, (E) => n(22, (o = E))), X(t, W, (E) => n(6, (l = E)));
	let { showSettings: c } = e,
		i = !1,
		u = !1,
		a,
		d,
		h;
	function p() {
		let E = document.documentElement;
		E.classList.contains('dark') ? E.classList.remove('dark') : E.classList.add('dark'),
			H(q, (r.use_dark_theme = !r.use_dark_theme), r),
			H($, (o = !0), o);
	}
	function v(E) {
		switch (E) {
			case Y.osrs:
				H(W, (l.game = Y.osrs), l),
					H(W, (l.client = r.selected_client_index), l),
					H(q, (r.selected_game_index = Y.osrs), r),
					H($, (o = !0), o),
					d.classList.add('bg-blue-500', 'text-black'),
					a.classList.remove('bg-blue-500', 'text-black');
				break;
			case Y.rs3:
				H(W, (l.game = Y.rs3), l),
					H(q, (r.selected_game_index = Y.rs3), r),
					H($, (o = !0), o),
					d.classList.remove('bg-blue-500', 'text-black'),
					a.classList.add('bg-blue-500', 'text-black');
				break;
		}
	}
	function w() {
		if (h.innerHTML == 'Log In') {
			At();
			return;
		}
		n(1, (i = !i));
	}
	Ae(() => {
		v(r.selected_game_index);
	});
	function R(E) {
		B[E ? 'unshift' : 'push'](() => {
			(a = E), n(3, a);
		});
	}
	const b = () => {
		v(Y.rs3);
	};
	function m(E) {
		B[E ? 'unshift' : 'push'](() => {
			(d = E), n(4, d);
		});
	}
	const y = () => {
			v(Y.osrs);
		},
		g = () => p(),
		S = () => {
			n(0, (c = !0));
		};
	function L(E) {
		B[E ? 'unshift' : 'push'](() => {
			(h = E), n(5, h);
		});
	}
	const N = () => {
			n(2, (u = !0));
		},
		J = () => {
			n(2, (u = !1));
		},
		M = () => w();
	function P(E) {
		(i = E), n(1, i);
	}
	return (
		(t.$$set = (E) => {
			'showSettings' in E && n(0, (c = E.showSettings));
		}),
		[c, i, u, a, d, h, l, p, v, w, R, b, m, y, g, S, L, N, J, M, P]
	);
}
class Tn extends ce {
	constructor(e) {
		super(), le(this, e, Rn, Pn, se, { showSettings: 0 });
	}
}
function Mn(t) {
	let e, n, r, o, l, c;
	return {
		c() {
			(e = k('div')),
				(n = k('div')),
				(n.innerHTML = ''),
				(r = T()),
				(o = k('div')),
				(o.innerHTML = ''),
				f(
					n,
					'class',
					'absolute left-0 top-0 z-10 h-screen w-screen backdrop-blur-sm backdrop-filter'
				),
				f(o, 'class', 'absolute left-0 top-0 z-10 h-screen w-screen bg-black opacity-75'),
				f(o, 'role', 'none');
		},
		m(i, u) {
			C(i, e, u), _(e, n), _(e, r), _(e, o), l || ((c = O(o, 'click', t[1])), (l = !0));
		},
		p: j,
		i: j,
		o: j,
		d(i) {
			i && x(e), (l = !1), c();
		}
	};
}
function En(t) {
	const e = Qt();
	return [
		e,
		() => {
			e('click');
		}
	];
}
class it extends ce {
	constructor(e) {
		super(), le(this, e, En, Mn, se, {});
	}
}
function In(t) {
	let e, n, r, o, l, c, i, u, a, d, h, p;
	return (
		(n = new it({})),
		{
			c() {
				(e = k('div')),
					ne(n.$$.fragment),
					(r = T()),
					(o = k('div')),
					(l = k('p')),
					(l.textContent = `${t[1]}`),
					(c = T()),
					(i = k('p')),
					(i.textContent = `${t[2]}`),
					(u = T()),
					(a = k('button')),
					(a.textContent = 'I Understand'),
					f(l, 'class', 'p-2'),
					f(i, 'class', 'p-2'),
					f(
						a,
						'class',
						'm-5 rounded-lg border-2 border-blue-500 p-2 duration-200 hover:opacity-75'
					),
					f(
						o,
						'class',
						'absolute left-1/4 top-1/4 z-20 w-1/2 rounded-lg bg-slate-100 p-5 text-center shadow-lg dark:bg-slate-900'
					),
					f(e, 'id', 'disclaimer');
			},
			m(v, w) {
				C(v, e, w),
					K(n, e, null),
					_(e, r),
					_(e, o),
					_(o, l),
					_(o, c),
					_(o, i),
					_(o, u),
					_(o, a),
					(d = !0),
					h || ((p = O(a, 'click', t[3])), (h = !0));
			},
			p: j,
			i(v) {
				d || (A(n.$$.fragment, v), (d = !0));
			},
			o(v) {
				D(n.$$.fragment, v), (d = !1);
			},
			d(v) {
				v && x(e), Q(n), (h = !1), p();
			}
		}
	);
}
function Nn(t, e, n) {
	let r;
	X(t, Xe, (i) => n(0, (r = i)));
	const o = atob(
			'Qm9sdCBpcyBhbiB1bm9mZmljaWFsIHRoaXJkLXBhcnR5IGxhdW5jaGVyLiBJdCdzIGZyZWUgYW5kIG9wZW4tc291cmNlIHNvZnR3YXJlIGxpY2Vuc2VkIHVuZGVyIEFHUEwgMy4wLg=='
		),
		l = atob(
			'SmFnZXggaXMgbm90IHJlc3BvbnNpYmxlIGZvciBhbnkgcHJvYmxlbXMgb3IgZGFtYWdlIGNhdXNlZCBieSB1c2luZyB0aGlzIHByb2R1Y3Qu'
		);
	return [
		r,
		o,
		l,
		() => {
			H(Xe, (r = !1), r);
		}
	];
}
class jn extends ce {
	constructor(e) {
		super(), le(this, e, Nn, In, se, {});
	}
}
function Hn(t) {
	let e, n, r, o;
	return {
		c() {
			(e = k('div')),
				(n = k('button')),
				(n.innerHTML = `<div class="flex"><img src="svgs/database-solid.svg" alt="Browse app data" class="mr-2 h-7 w-7 rounded-lg bg-violet-500 p-1"/>
			Browse App Data</div>`),
				f(n, 'id', 'data_dir_button'),
				f(n, 'class', 'p-2 hover:opacity-75'),
				f(e, 'id', 'general_options'),
				f(e, 'class', 'col-span-3 p-5 pt-10');
		},
		m(l, c) {
			C(l, e, c), _(e, n), r || ((o = O(n, 'click', t[1])), (r = !0));
		},
		p: j,
		i: j,
		o: j,
		d(l) {
			l && x(e), (r = !1), o();
		}
	};
}
function On(t) {
	function e() {
		var r = new XMLHttpRequest();
		r.open('GET', '/browse-data'),
			(r.onreadystatechange = () => {
				r.readyState == 4 && z(`Browse status: '${r.responseText.trim()}'`);
			}),
			r.send();
	}
	return [
		e,
		() => {
			e();
		}
	];
}
class An extends ce {
	constructor(e) {
		super(), le(this, e, On, Hn, se, {});
	}
}
function Dn(t) {
	let e, n, r, o, l, c, i, u, a, d, h, p, v, w, R, b, m, y, g, S, L;
	return {
		c() {
			(e = k('div')),
				(n = k('button')),
				(n.innerHTML = `<div class="flex"><img src="svgs/wrench-solid.svg" alt="Configure RuneLite" class="mr-2 h-7 w-7 rounded-lg bg-pink-500 p-1"/>
			Configure RuneLite</div>`),
				(r = T()),
				(o = k('div')),
				(l = k('label')),
				(l.textContent = 'Expose rich presence to Flatpak Discord:'),
				(c = T()),
				(i = k('input')),
				(u = T()),
				(a = k('div')),
				(d = k('label')),
				(d.textContent = 'Use custom RuneLite JAR:'),
				(h = T()),
				(p = k('input')),
				(v = T()),
				(w = k('div')),
				(R = k('textarea')),
				(b = T()),
				(m = k('br')),
				(y = T()),
				(g = k('button')),
				(g.textContent = 'Select File'),
				f(n, 'id', 'rl_configure'),
				f(n, 'class', 'p-2 pb-5 hover:opacity-75'),
				f(l, 'for', 'flatpak_rich_presence'),
				f(i, 'type', 'checkbox'),
				f(i, 'name', 'flatpak_rich_presence'),
				f(i, 'id', 'flatpak_rich_presence'),
				f(i, 'class', 'ml-2'),
				f(o, 'id', 'flatpak_div'),
				f(o, 'class', 'mx-auto border-t-2 border-slate-300 p-2 py-5 dark:border-slate-800'),
				f(d, 'for', 'use_custom_jar'),
				f(p, 'type', 'checkbox'),
				f(p, 'name', 'use_custom_jar'),
				f(p, 'id', 'use_custom_jar'),
				f(p, 'class', 'ml-2'),
				f(a, 'class', 'mx-auto border-t-2 border-slate-300 p-2 pt-5 dark:border-slate-800'),
				(R.disabled = !0),
				f(R, 'name', 'custom_jar_file'),
				f(R, 'id', 'custom_jar_file'),
				f(
					R,
					'class',
					'h-10 rounded border-2 border-slate-300 bg-slate-100 text-slate-950 dark:border-slate-800 dark:bg-slate-900 dark:text-slate-50'
				),
				(g.disabled = !0),
				f(g, 'id', 'custom_jar_file_button'),
				f(
					g,
					'class',
					'mt-1 rounded-lg border-2 border-blue-500 p-1 duration-200 hover:opacity-75'
				),
				f(w, 'id', 'custom_jar_div'),
				f(w, 'class', 'mx-auto p-2 opacity-25'),
				f(e, 'id', 'osrs_options'),
				f(e, 'class', 'col-span-3 p-5 pt-10');
		},
		m(N, J) {
			C(N, e, J),
				_(e, n),
				_(e, r),
				_(e, o),
				_(o, l),
				_(o, c),
				_(o, i),
				t[12](i),
				t[14](o),
				_(e, u),
				_(e, a),
				_(a, d),
				_(a, h),
				_(a, p),
				t[15](p),
				_(e, v),
				_(e, w),
				_(w, R),
				t[17](R),
				_(w, b),
				_(w, m),
				_(w, y),
				_(w, g),
				t[19](g),
				t[21](w),
				S ||
					((L = [
						O(n, 'click', t[11]),
						O(i, 'change', t[13]),
						O(p, 'change', t[16]),
						O(R, 'change', t[18]),
						O(g, 'click', t[20])
					]),
					(S = !0));
		},
		p: j,
		i: j,
		o: j,
		d(N) {
			N && x(e),
				t[12](null),
				t[14](null),
				t[15](null),
				t[17](null),
				t[19](null),
				t[21](null),
				(S = !1),
				oe(L);
		}
	};
}
function Un(t, e, n) {
	let r, o, l, c;
	X(t, Fe, (I) => n(22, (r = I))),
		X(t, q, (I) => n(23, (o = I))),
		X(t, W, (I) => n(24, (l = I))),
		X(t, $, (I) => n(25, (c = I)));
	let i, u, a, d, h, p;
	function v() {
		i.classList.toggle('opacity-25'),
			n(1, (u.disabled = !u.disabled), u),
			n(2, (a.disabled = !a.disabled), a),
			H(q, (o.runelite_use_custom_jar = d.checked), o),
			o.runelite_use_custom_jar
				? u.value && H(q, (o.runelite_custom_jar = u.value), o)
				: (n(1, (u.value = ''), u),
					H(q, (o.runelite_custom_jar = ''), o),
					H(q, (o.runelite_use_custom_jar = !1), o)),
			H($, (c = !0), c);
	}
	function w() {
		H(q, (o.flatpak_rich_presence = h.checked), o), H($, (c = !0), c);
	}
	function R() {
		H(q, (o.runelite_custom_jar = u.value), o), H($, (c = !0), c);
	}
	function b() {
		n(3, (d.disabled = !0), d), n(2, (a.disabled = !0), a);
		var I = new XMLHttpRequest();
		(I.onreadystatechange = () => {
			I.readyState == 4 &&
				(I.status == 200 &&
					(n(1, (u.value = I.responseText), u),
					H(q, (o.runelite_custom_jar = I.responseText), o),
					H($, (c = !0), c)),
				n(2, (a.disabled = !1), a),
				n(3, (d.disabled = !1), d));
		}),
			I.open('GET', '/jar-file-picker', !0),
			I.send();
	}
	function m() {
		var I, Le, ut;
		if (!l.account || !l.character) {
			z('Please log in to configure RuneLite');
			return;
		}
		wn(
			(I = l.credentials) == null ? void 0 : I.session_id,
			(Le = l.character) == null ? void 0 : Le.accountId,
			(ut = l.character) == null ? void 0 : ut.displayName
		);
	}
	Ae(() => {
		n(4, (h.checked = o.flatpak_rich_presence), h),
			n(3, (d.checked = o.runelite_use_custom_jar), d),
			d.checked && o.runelite_custom_jar
				? (i.classList.remove('opacity-25'),
					n(1, (u.disabled = !1), u),
					n(2, (a.disabled = !1), a),
					n(1, (u.value = o.runelite_custom_jar), u))
				: (n(3, (d.checked = !1), d), H(q, (o.runelite_use_custom_jar = !1), o)),
			r !== 'linux' && p.remove();
	});
	const y = () => m();
	function g(I) {
		B[I ? 'unshift' : 'push'](() => {
			(h = I), n(4, h);
		});
	}
	const S = () => {
		w();
	};
	function L(I) {
		B[I ? 'unshift' : 'push'](() => {
			(p = I), n(5, p);
		});
	}
	function N(I) {
		B[I ? 'unshift' : 'push'](() => {
			(d = I), n(3, d);
		});
	}
	const J = () => {
		v();
	};
	function M(I) {
		B[I ? 'unshift' : 'push'](() => {
			(u = I), n(1, u);
		});
	}
	const P = () => {
		R();
	};
	function E(I) {
		B[I ? 'unshift' : 'push'](() => {
			(a = I), n(2, a);
		});
	}
	const ee = () => {
		b();
	};
	function U(I) {
		B[I ? 'unshift' : 'push'](() => {
			(i = I), n(0, i);
		});
	}
	return [i, u, a, d, h, p, v, w, R, b, m, y, g, S, L, N, J, M, P, E, ee, U];
}
class qn extends ce {
	constructor(e) {
		super(), le(this, e, Un, Dn, se, {});
	}
}
function Bn(t) {
	let e, n, r, o, l, c, i, u, a, d;
	return {
		c() {
			(e = k('div')),
				(n = k('div')),
				(r = k('label')),
				(r.textContent = 'Use custom config URI:'),
				(o = T()),
				(l = k('input')),
				(c = T()),
				(i = k('div')),
				(u = k('textarea')),
				f(r, 'for', 'use_custom_uri'),
				f(l, 'type', 'checkbox'),
				f(l, 'name', 'use_custom_uri'),
				f(l, 'id', 'use_custom_uri'),
				f(l, 'class', 'ml-2'),
				f(n, 'class', 'mx-auto p-2'),
				(u.disabled = !0),
				f(u, 'name', 'config_uri_address'),
				f(u, 'id', 'config_uri_address'),
				f(u, 'rows', '4'),
				f(
					u,
					'class',
					'rounded border-2 border-slate-300 bg-slate-100 text-slate-950 dark:border-slate-800 dark:bg-slate-900 dark:text-slate-50'
				),
				(u.value = '		'),
				f(i, 'id', 'config_uri_div'),
				f(i, 'class', 'mx-auto p-2 opacity-25'),
				f(e, 'id', 'rs3_options'),
				f(e, 'class', 'col-span-3 p-5 pt-10');
		},
		m(h, p) {
			C(h, e, p),
				_(e, n),
				_(n, r),
				_(n, o),
				_(n, l),
				t[5](l),
				_(e, c),
				_(e, i),
				_(i, u),
				t[8](u),
				t[9](i),
				a || ((d = [O(l, 'change', t[6]), O(u, 'change', t[7])]), (a = !0));
		},
		p: j,
		i: j,
		o: j,
		d(h) {
			h && x(e), t[5](null), t[8](null), t[9](null), (a = !1), oe(d);
		}
	};
}
function Jn(t, e, n) {
	let r, o, l;
	X(t, lt, (b) => n(10, (r = b))), X(t, q, (b) => n(11, (o = b))), X(t, $, (b) => n(12, (l = b)));
	let c, i, u;
	function a() {
		c.classList.toggle('opacity-25'),
			n(1, (i.disabled = !i.disabled), i),
			H($, (l = !0), l),
			u.checked ||
				(n(1, (i.value = atob(r.default_config_uri)), i), H(q, (o.rs_config_uri = ''), o));
	}
	function d() {
		H(q, (o.rs_config_uri = i.value), o), H($, (l = !0), l);
	}
	Ae(() => {
		o.rs_config_uri
			? (n(1, (i.value = o.rs_config_uri), i), n(2, (u.checked = !0), u), a())
			: n(1, (i.value = atob(r.default_config_uri)), i);
	});
	function h(b) {
		B[b ? 'unshift' : 'push'](() => {
			(u = b), n(2, u);
		});
	}
	const p = () => {
			a();
		},
		v = () => {
			d();
		};
	function w(b) {
		B[b ? 'unshift' : 'push'](() => {
			(i = b), n(1, i);
		});
	}
	function R(b) {
		B[b ? 'unshift' : 'push'](() => {
			(c = b), n(0, c);
		});
	}
	return [c, i, u, a, d, h, p, v, w, R];
}
class Gn extends ce {
	constructor(e) {
		super(), le(this, e, Jn, Bn, se, {});
	}
}
function Xn(t) {
	let e, n;
	return (
		(e = new Gn({})),
		{
			c() {
				ne(e.$$.fragment);
			},
			m(r, o) {
				K(e, r, o), (n = !0);
			},
			i(r) {
				n || (A(e.$$.fragment, r), (n = !0));
			},
			o(r) {
				D(e.$$.fragment, r), (n = !1);
			},
			d(r) {
				Q(e, r);
			}
		}
	);
}
function zn(t) {
	let e, n;
	return (
		(e = new qn({})),
		{
			c() {
				ne(e.$$.fragment);
			},
			m(r, o) {
				K(e, r, o), (n = !0);
			},
			i(r) {
				n || (A(e.$$.fragment, r), (n = !0));
			},
			o(r) {
				D(e.$$.fragment, r), (n = !1);
			},
			d(r) {
				Q(e, r);
			}
		}
	);
}
function Fn(t) {
	let e, n;
	return (
		(e = new An({})),
		{
			c() {
				ne(e.$$.fragment);
			},
			m(r, o) {
				K(e, r, o), (n = !0);
			},
			i(r) {
				n || (A(e.$$.fragment, r), (n = !0));
			},
			o(r) {
				D(e.$$.fragment, r), (n = !1);
			},
			d(r) {
				Q(e, r);
			}
		}
	);
}
function $n(t) {
	let e, n, r, o, l, c, i, u, a, d, h, p, v, w, R, b, m, y, g, S, L, N, J, M;
	(n = new it({})), n.$on('click', t[7]);
	const P = [Fn, zn, Xn],
		E = [];
	function ee(U, I) {
		return U[1] == U[5].general ? 0 : U[1] == U[5].osrs ? 1 : U[1] == U[5].rs3 ? 2 : -1;
	}
	return (
		~(S = ee(t)) && (L = E[S] = P[S](t)),
		{
			c() {
				(e = k('div')),
					ne(n.$$.fragment),
					(r = T()),
					(o = k('div')),
					(l = k('button')),
					(l.innerHTML = '<img src="svgs/xmark-solid.svg" class="h-5 w-5" alt="Close"/>'),
					(c = T()),
					(i = k('div')),
					(u = k('div')),
					(a = k('button')),
					(d = F('General')),
					(h = k('br')),
					(p = T()),
					(v = k('button')),
					(w = F('OSRS')),
					(R = k('br')),
					(b = T()),
					(m = k('button')),
					(y = F('RS3')),
					(g = T()),
					L && L.c(),
					f(
						l,
						'class',
						'absolute right-3 top-3 rounded-full bg-rose-500 p-[2px] shadow-lg duration-200 hover:rotate-90 hover:opacity-75'
					),
					f(a, 'id', 'general_button'),
					f(a, 'class', _e),
					f(v, 'id', 'osrs_button'),
					f(v, 'class', qe),
					f(m, 'id', 'rs3_button'),
					f(m, 'class', _e),
					f(
						u,
						'class',
						'relative h-full border-r-2 border-slate-300 pt-10 dark:border-slate-800'
					),
					f(i, 'class', 'grid h-full grid-cols-4'),
					f(
						o,
						'class',
						'absolute left-[13%] top-[13%] z-20 h-3/4 w-3/4 rounded-lg bg-slate-100 text-center shadow-lg dark:bg-slate-900'
					),
					f(e, 'id', 'settings');
			},
			m(U, I) {
				C(U, e, I),
					K(n, e, null),
					_(e, r),
					_(e, o),
					_(o, l),
					_(o, c),
					_(o, i),
					_(i, u),
					_(u, a),
					_(a, d),
					t[9](a),
					_(u, h),
					_(u, p),
					_(u, v),
					_(v, w),
					t[11](v),
					_(u, R),
					_(u, b),
					_(u, m),
					_(m, y),
					t[13](m),
					_(i, g),
					~S && E[S].m(i, null),
					(N = !0),
					J ||
						((M = [
							O(l, 'click', t[8]),
							O(a, 'click', t[10]),
							O(v, 'click', t[12]),
							O(m, 'click', t[14])
						]),
						(J = !0));
			},
			p(U, [I]) {
				let Le = S;
				(S = ee(U)),
					S !== Le &&
						(L &&
							(we(),
							D(E[Le], 1, 1, () => {
								E[Le] = null;
							}),
							ye()),
						~S
							? ((L = E[S]),
								L || ((L = E[S] = P[S](U)), L.c()),
								A(L, 1),
								L.m(i, null))
							: (L = null));
			},
			i(U) {
				N || (A(n.$$.fragment, U), A(L), (N = !0));
			},
			o(U) {
				D(n.$$.fragment, U), D(L), (N = !1);
			},
			d(U) {
				U && x(e),
					Q(n),
					t[9](null),
					t[11](null),
					t[13](null),
					~S && E[S].d(),
					(J = !1),
					oe(M);
			}
		}
	);
}
let qe =
		'border-2 border-blue-500 bg-blue-500 hover:opacity-75 font-bold text-black duration-200 rounded-lg p-1 mx-auto my-1 w-3/4',
	_e = 'border-2 border-blue-500 hover:opacity-75 duration-200 rounded-lg p-1 mx-auto my-1 w-3/4';
function Vn(t, e, n) {
	let { showSettings: r } = e;
	var o = ((g) => (
		(g[(g.general = 0)] = 'general'), (g[(g.osrs = 1)] = 'osrs'), (g[(g.rs3 = 2)] = 'rs3'), g
	))(o || {});
	let l = 1,
		c,
		i,
		u;
	function a(g) {
		switch (g) {
			case 0:
				n(1, (l = 0)),
					n(2, (c.classList.value = qe), c),
					n(3, (i.classList.value = _e), i),
					n(4, (u.classList.value = _e), u);
				break;
			case 1:
				n(1, (l = 1)),
					n(2, (c.classList.value = _e), c),
					n(3, (i.classList.value = qe), i),
					n(4, (u.classList.value = _e), u);
				break;
			case 2:
				n(1, (l = 2)),
					n(2, (c.classList.value = _e), c),
					n(3, (i.classList.value = _e), i),
					n(4, (u.classList.value = qe), u);
				break;
		}
	}
	function d(g) {
		g.key === 'Escape' && n(0, (r = !1));
	}
	addEventListener('keydown', d),
		rt(() => {
			removeEventListener('keydown', d);
		});
	const h = () => {
			n(0, (r = !1));
		},
		p = () => {
			n(0, (r = !1));
		};
	function v(g) {
		B[g ? 'unshift' : 'push'](() => {
			(c = g), n(2, c);
		});
	}
	const w = () => {
		a(o.general);
	};
	function R(g) {
		B[g ? 'unshift' : 'push'](() => {
			(i = g), n(3, i);
		});
	}
	const b = () => {
		a(o.osrs);
	};
	function m(g) {
		B[g ? 'unshift' : 'push'](() => {
			(u = g), n(4, u);
		});
	}
	const y = () => {
		a(o.rs3);
	};
	return (
		(t.$$set = (g) => {
			'showSettings' in g && n(0, (r = g.showSettings));
		}),
		[r, l, c, i, u, o, a, h, p, v, w, R, b, m, y]
	);
}
class Wn extends ce {
	constructor(e) {
		super(), le(this, e, Vn, $n, se, { showSettings: 0 });
	}
}
function bt(t, e, n) {
	const r = t.slice();
	return (r[1] = e[n]), r;
}
function Zn(t) {
	var u;
	let e,
		n = ((u = t[1].time) == null ? void 0 : u.toLocaleTimeString()) + '',
		r,
		o,
		l = t[1].text + '',
		c,
		i;
	return {
		c() {
			(e = k('li')),
				(r = F(n)),
				(o = F(`
					- `)),
				(c = F(l)),
				(i = T());
		},
		m(a, d) {
			C(a, e, d), _(e, r), _(e, o), _(e, c), _(e, i);
		},
		p(a, d) {
			var h;
			d & 1 &&
				n !== (n = ((h = a[1].time) == null ? void 0 : h.toLocaleTimeString()) + '') &&
				ae(r, n),
				d & 1 && l !== (l = a[1].text + '') && ae(c, l);
		},
		d(a) {
			a && x(e);
		}
	};
}
function Yn(t) {
	var u;
	let e,
		n = ((u = t[1].time) == null ? void 0 : u.toLocaleTimeString()) + '',
		r,
		o,
		l = t[1].text + '',
		c,
		i;
	return {
		c() {
			(e = k('li')),
				(r = F(n)),
				(o = F(`
					- `)),
				(c = F(l)),
				(i = T()),
				f(e, 'class', 'text-rose-500');
		},
		m(a, d) {
			C(a, e, d), _(e, r), _(e, o), _(e, c), _(e, i);
		},
		p(a, d) {
			var h;
			d & 1 &&
				n !== (n = ((h = a[1].time) == null ? void 0 : h.toLocaleTimeString()) + '') &&
				ae(r, n),
				d & 1 && l !== (l = a[1].text + '') && ae(c, l);
		},
		d(a) {
			a && x(e);
		}
	};
}
function mt(t) {
	let e;
	function n(l, c) {
		return l[1].isError ? Yn : Zn;
	}
	let r = n(t),
		o = r(t);
	return {
		c() {
			o.c(), (e = ve());
		},
		m(l, c) {
			o.m(l, c), C(l, e, c);
		},
		p(l, c) {
			r === (r = n(l)) && o
				? o.p(l, c)
				: (o.d(1), (o = r(l)), o && (o.c(), o.m(e.parentNode, e)));
		},
		d(l) {
			l && x(e), o.d(l);
		}
	};
}
function Kn(t) {
	let e,
		n,
		r,
		o,
		l = fe(t[0]),
		c = [];
	for (let i = 0; i < l.length; i += 1) c[i] = mt(bt(t, l, i));
	return {
		c() {
			(e = k('div')),
				(n = k('div')),
				(n.innerHTML =
					'<img src="svgs/circle-info-solid.svg" alt="Message list icon" class="h-7 w-7 rounded-full bg-blue-500 p-[3px] duration-200"/>'),
				(r = T()),
				(o = k('ol'));
			for (let i = 0; i < c.length; i += 1) c[i].c();
			f(n, 'class', 'absolute right-2 top-2'),
				f(o, 'id', 'message_list'),
				f(
					o,
					'class',
					'h-full w-[105%] list-inside list-disc overflow-y-auto pl-5 pt-1 marker:text-blue-500'
				),
				f(
					e,
					'class',
					'fixed bottom-0 h-1/4 w-screen border-t-2 border-t-slate-300 bg-slate-100 duration-200 dark:border-t-slate-800 dark:bg-slate-900'
				);
		},
		m(i, u) {
			C(i, e, u), _(e, n), _(e, r), _(e, o);
			for (let a = 0; a < c.length; a += 1) c[a] && c[a].m(o, null);
		},
		p(i, [u]) {
			if (u & 1) {
				l = fe(i[0]);
				let a;
				for (a = 0; a < l.length; a += 1) {
					const d = bt(i, l, a);
					c[a] ? c[a].p(d, u) : ((c[a] = mt(d)), c[a].c(), c[a].m(o, null));
				}
				for (; a < c.length; a += 1) c[a].d(1);
				c.length = l.length;
			}
		},
		i: j,
		o: j,
		d(i) {
			i && x(e), He(c, i);
		}
	};
}
function Qn(t, e, n) {
	let r;
	return X(t, Se, (o) => n(0, (r = o))), [r];
}
class es extends ce {
	constructor(e) {
		super(), le(this, e, Qn, Kn, se, {});
	}
}
function kt(t, e, n) {
	const r = t.slice();
	return (r[13] = e[n]), r;
}
function ts(t) {
	let e, n, r;
	return {
		c() {
			(e = k('button')),
				(e.textContent = 'Plugin menu'),
				(e.disabled = !te(Te)),
				f(
					e,
					'class',
					'mx-auto mb-2 w-52 rounded-lg p-2 font-bold text-black duration-200 enabled:bg-blue-500 enabled:hover:opacity-75 disabled:bg-gray-500'
				);
		},
		m(o, l) {
			C(o, e, l), n || ((r = O(e, 'click', t[8])), (n = !0));
		},
		p: j,
		d(o) {
			o && x(e), (n = !1), r();
		}
	};
}
function ns(t) {
	let e, n, r, o, l, c, i, u, a;
	return {
		c() {
			(e = k('label')),
				(e.textContent = 'Game Client'),
				(n = T()),
				(r = k('br')),
				(o = T()),
				(l = k('select')),
				(c = k('option')),
				(c.textContent = 'RuneLite'),
				(i = k('option')),
				(i.textContent = 'HDOS'),
				f(e, 'for', 'game_client_select'),
				f(e, 'class', 'text-sm'),
				f(c, 'data-id', de.runeLite),
				f(c, 'class', 'dark:bg-slate-900'),
				(c.__value = 'RuneLite'),
				ge(c, c.__value),
				f(i, 'data-id', de.hdos),
				f(i, 'class', 'dark:bg-slate-900'),
				(i.__value = 'HDOS'),
				ge(i, i.__value),
				f(l, 'id', 'game_client_select'),
				f(
					l,
					'class',
					'mx-auto w-52 cursor-pointer rounded-lg border-2 border-slate-300 bg-inherit p-2 text-inherit duration-200 hover:opacity-75 dark:border-slate-800'
				);
		},
		m(d, h) {
			C(d, e, h),
				C(d, n, h),
				C(d, r, h),
				C(d, o, h),
				C(d, l, h),
				_(l, c),
				_(l, i),
				t[7](l),
				u || ((a = O(l, 'change', t[5])), (u = !0));
		},
		p: j,
		d(d) {
			d && (x(e), x(n), x(r), x(o), x(l)), t[7](null), (u = !1), a();
		}
	};
}
function wt(t) {
	let e,
		n = fe(t[4].account.characters),
		r = [];
	for (let o = 0; o < n.length; o += 1) r[o] = yt(kt(t, n, o));
	return {
		c() {
			for (let o = 0; o < r.length; o += 1) r[o].c();
			e = ve();
		},
		m(o, l) {
			for (let c = 0; c < r.length; c += 1) r[c] && r[c].m(o, l);
			C(o, e, l);
		},
		p(o, l) {
			if (l & 16) {
				n = fe(o[4].account.characters);
				let c;
				for (c = 0; c < n.length; c += 1) {
					const i = kt(o, n, c);
					r[c] ? r[c].p(i, l) : ((r[c] = yt(i)), r[c].c(), r[c].m(e.parentNode, e));
				}
				for (; c < r.length; c += 1) r[c].d(1);
				r.length = n.length;
			}
		},
		d(o) {
			o && x(e), He(r, o);
		}
	};
}
function ss(t) {
	let e;
	return {
		c() {
			e = F('New Character');
		},
		m(n, r) {
			C(n, e, r);
		},
		p: j,
		d(n) {
			n && x(e);
		}
	};
}
function rs(t) {
	let e = t[13][1].displayName + '',
		n;
	return {
		c() {
			n = F(e);
		},
		m(r, o) {
			C(r, n, o);
		},
		p(r, o) {
			o & 16 && e !== (e = r[13][1].displayName + '') && ae(n, e);
		},
		d(r) {
			r && x(n);
		}
	};
}
function yt(t) {
	let e, n, r, o;
	function l(u, a) {
		return u[13][1].displayName ? rs : ss;
	}
	let c = l(t),
		i = c(t);
	return {
		c() {
			(e = k('option')),
				i.c(),
				(n = T()),
				f(e, 'data-id', (r = t[13][1].accountId)),
				f(e, 'class', 'dark:bg-slate-900'),
				(e.__value = o =
					`
						` +
					t[13][1].displayName +
					`
					`),
				ge(e, e.__value);
		},
		m(u, a) {
			C(u, e, a), i.m(e, null), _(e, n);
		},
		p(u, a) {
			c === (c = l(u)) && i ? i.p(u, a) : (i.d(1), (i = c(u)), i && (i.c(), i.m(e, n))),
				a & 16 && r !== (r = u[13][1].accountId) && f(e, 'data-id', r),
				a & 16 &&
					o !==
						(o =
							`
						` +
							u[13][1].displayName +
							`
					`) &&
					((e.__value = o), ge(e, e.__value));
		},
		d(u) {
			u && x(e), i.d();
		}
	};
}
function os(t) {
	let e, n, r, o, l, c, i, u, a, d, h, p, v, w, R, b;
	function m(L, N) {
		if (L[4].game == Y.osrs) return ns;
		if (L[4].game == Y.rs3) return ts;
	}
	let y = m(t),
		g = y && y(t),
		S = t[4].account && wt(t);
	return {
		c() {
			(e = k('div')),
				(n = k('img')),
				(o = T()),
				(l = k('button')),
				(l.textContent = 'Play'),
				(c = T()),
				(i = k('div')),
				g && g.c(),
				(u = T()),
				(a = k('div')),
				(d = k('label')),
				(d.textContent = 'Character'),
				(h = T()),
				(p = k('br')),
				(v = T()),
				(w = k('select')),
				S && S.c(),
				$t(n.src, (r = 'svgs/rocket-solid.svg')) || f(n, 'src', r),
				f(n, 'alt', 'Launch icon'),
				f(
					n,
					'class',
					'mx-auto mb-5 w-24 rounded-3xl bg-gradient-to-br from-rose-500 to-violet-500 p-5'
				),
				f(
					l,
					'class',
					'mx-auto mb-2 w-52 rounded-lg bg-emerald-500 p-2 font-bold text-black duration-200 hover:opacity-75'
				),
				f(i, 'class', 'mx-auto my-2'),
				f(d, 'for', 'character_select'),
				f(d, 'class', 'text-sm'),
				f(w, 'id', 'character_select'),
				f(
					w,
					'class',
					'mx-auto w-52 cursor-pointer rounded-lg border-2 border-slate-300 bg-inherit p-2 text-inherit duration-200 hover:opacity-75 dark:border-slate-800'
				),
				f(a, 'class', 'mx-auto my-2'),
				f(
					e,
					'class',
					'bg-grad flex h-full flex-col border-slate-300 p-5 duration-200 dark:border-slate-800'
				);
		},
		m(L, N) {
			C(L, e, N),
				_(e, n),
				_(e, o),
				_(e, l),
				_(e, c),
				_(e, i),
				g && g.m(i, null),
				_(e, u),
				_(e, a),
				_(a, d),
				_(a, h),
				_(a, p),
				_(a, v),
				_(a, w),
				S && S.m(w, null),
				t[9](w),
				R || ((b = [O(l, 'click', t[6]), O(w, 'change', t[10])]), (R = !0));
		},
		p(L, [N]) {
			y === (y = m(L)) && g
				? g.p(L, N)
				: (g && g.d(1), (g = y && y(L)), g && (g.c(), g.m(i, null))),
				L[4].account
					? S
						? S.p(L, N)
						: ((S = wt(L)), S.c(), S.m(w, null))
					: S && (S.d(1), (S = null));
		},
		i: j,
		o: j,
		d(L) {
			L && x(e), g && g.d(), S && S.d(), t[9](null), (R = !1), oe(b);
		}
	};
}
function ls(t, e, n) {
	let r, o, l;
	X(t, W, (b) => n(4, (r = b))), X(t, q, (b) => n(11, (o = b))), X(t, $, (b) => n(12, (l = b)));
	let { showPluginMenu: c = !1 } = e,
		i,
		u;
	function a() {
		var m, y;
		if (!r.account) return;
		const b = i[i.selectedIndex].getAttribute('data-id');
		H(W, (r.character = r.account.characters.get(b)), r),
			r.character &&
				((y = o.selected_characters) == null ||
					y.set(r.account.userId, (m = r.character) == null ? void 0 : m.accountId)),
			H($, (l = !0), l);
	}
	function d() {
		u.value == 'RuneLite'
			? (H(W, (r.client = de.runeLite), r), H(q, (o.selected_client_index = de.runeLite), o))
			: u.value == 'HDOS' &&
				(H(W, (r.client = de.hdos), r), H(q, (o.selected_client_index = de.hdos), o)),
			H($, (l = !0), l);
	}
	function h() {
		var b, m, y, g, S, L, N, J, M;
		if (!r.account || !r.character) {
			z('Please log in to launch a client');
			return;
		}
		switch (r.game) {
			case Y.osrs:
				r.client == de.runeLite
					? kn(
							(b = r.credentials) == null ? void 0 : b.session_id,
							(m = r.character) == null ? void 0 : m.accountId,
							(y = r.character) == null ? void 0 : y.displayName
						)
					: r.client == de.hdos &&
						yn(
							(g = r.credentials) == null ? void 0 : g.session_id,
							(S = r.character) == null ? void 0 : S.accountId,
							(L = r.character) == null ? void 0 : L.displayName
						);
				break;
			case Y.rs3:
				mn(
					(N = r.credentials) == null ? void 0 : N.session_id,
					(J = r.character) == null ? void 0 : J.accountId,
					(M = r.character) == null ? void 0 : M.displayName
				);
				break;
		}
	}
	Kt(() => {
		var b;
		if (
			(r.game == Y.osrs && r.client && n(3, (u.selectedIndex = r.client), u),
			r.account && (b = o.selected_characters) != null && b.has(r.account.userId))
		)
			for (let m = 0; m < i.options.length; m++)
				i[m].getAttribute('data-id') == o.selected_characters.get(r.account.userId) &&
					n(2, (i.selectedIndex = m), i);
	}),
		Ae(() => {
			o.selected_game_index == Y.osrs &&
				(n(3, (u.selectedIndex = o.selected_client_index), u),
				H(W, (r.client = u.selectedIndex), r));
		});
	function p(b) {
		B[b ? 'unshift' : 'push'](() => {
			(u = b), n(3, u);
		});
	}
	const v = () => {
		n(0, (c = te(Te) ?? !1));
	};
	function w(b) {
		B[b ? 'unshift' : 'push'](() => {
			(i = b), n(2, i);
		});
	}
	const R = () => a();
	return (
		(t.$$set = (b) => {
			'showPluginMenu' in b && n(0, (c = b.showPluginMenu));
		}),
		[c, a, i, u, r, d, h, p, v, w, R]
	);
}
class cs extends ce {
	constructor(e) {
		super(), le(this, e, ls, os, se, { showPluginMenu: 0, characterChanged: 1 });
	}
	get characterChanged() {
		return this.$$.ctx[1];
	}
}
function is(t) {
	let e;
	return {
		c() {
			(e = k('div')),
				(e.innerHTML =
					'<img src="svgs/circle-notch-solid.svg" alt="" class="mx-auto h-1/6 w-1/6 animate-spin"/>'),
				f(
					e,
					'class',
					'container mx-auto bg-slate-100 p-5 text-center text-slate-900 dark:bg-slate-900 dark:text-slate-50'
				);
		},
		m(n, r) {
			C(n, e, r);
		},
		p: j,
		i: j,
		o: j,
		d(n) {
			n && x(e);
		}
	};
}
class as extends ce {
	constructor(e) {
		super(), le(this, e, null, is, se, {});
	}
}
function vt(t, e, n) {
	const r = t.slice();
	return (r[14] = e[n][0]), (r[13] = e[n][1]), r;
}
function St(t, e, n) {
	const r = t.slice();
	return (r[18] = e[n]), r;
}
function us(t) {
	let e;
	return {
		c() {
			(e = k('p')), (e.textContent = 'error');
		},
		m(n, r) {
			C(n, e, r);
		},
		p: j,
		d(n) {
			n && x(e);
		}
	};
}
function ds(t) {
	let e;
	function n(l, c) {
		return l[17].length == 0 ? ps : fs;
	}
	let r = n(t),
		o = r(t);
	return {
		c() {
			o.c(), (e = ve());
		},
		m(l, c) {
			o.m(l, c), C(l, e, c);
		},
		p(l, c) {
			r === (r = n(l)) && o
				? o.p(l, c)
				: (o.d(1), (o = r(l)), o && (o.c(), o.m(e.parentNode, e)));
		},
		d(l) {
			l && x(e), o.d(l);
		}
	};
}
function fs(t) {
	let e,
		n = fe(t[17]),
		r = [];
	for (let o = 0; o < n.length; o += 1) r[o] = Lt(St(t, n, o));
	return {
		c() {
			for (let o = 0; o < r.length; o += 1) r[o].c();
			e = ve();
		},
		m(o, l) {
			for (let c = 0; c < r.length; c += 1) r[c] && r[c].m(o, l);
			C(o, e, l);
		},
		p(o, l) {
			if (l & 16) {
				n = fe(o[17]);
				let c;
				for (c = 0; c < n.length; c += 1) {
					const i = St(o, n, c);
					r[c] ? r[c].p(i, l) : ((r[c] = Lt(i)), r[c].c(), r[c].m(e.parentNode, e));
				}
				for (; c < r.length; c += 1) r[c].d(1);
				r.length = n.length;
			}
		},
		d(o) {
			o && x(e), He(r, o);
		}
	};
}
function ps(t) {
	let e;
	return {
		c() {
			(e = k('p')),
				(e.textContent =
					'(start an RS3 game client with plugins enabled and it will be listed here.)');
		},
		m(n, r) {
			C(n, e, r);
		},
		p: j,
		d(n) {
			n && x(e);
		}
	};
}
function Lt(t) {
	let e,
		n = (t[18].identity || '(unnamed)') + '',
		r,
		o,
		l;
	return {
		c() {
			(e = k('button')),
				(r = F(n)),
				(o = T()),
				(l = k('br')),
				f(
					e,
					'class',
					'm-1 h-[28px] w-[95%] rounded-lg border-2 border-blue-500 duration-200 hover:opacity-75'
				);
		},
		m(c, i) {
			C(c, e, i), _(e, r), C(c, o, i), C(c, l, i);
		},
		p(c, i) {
			i & 16 && n !== (n = (c[18].identity || '(unnamed)') + '') && ae(r, n);
		},
		d(c) {
			c && (x(e), x(o), x(l));
		}
	};
}
function _s(t) {
	let e;
	return {
		c() {
			(e = k('p')), (e.textContent = 'loading...');
		},
		m(n, r) {
			C(n, e, r);
		},
		p: j,
		d(n) {
			n && x(e);
		}
	};
}
function hs(t) {
	let e;
	return {
		c() {
			(e = k('p')), (e.textContent = 'error');
		},
		m(n, r) {
			C(n, e, r);
		},
		p: j,
		d(n) {
			n && x(e);
		}
	};
}
function gs(t) {
	let e,
		n,
		r,
		o,
		l,
		c,
		i,
		u,
		a,
		d,
		h,
		p = fe(Object.entries(t[3])),
		v = [];
	for (let m = 0; m < p.length; m += 1) v[m] = xt(vt(t, p, m));
	function w(m, y) {
		return (
			y & 8 && (u = null), u == null && (u = Object.entries(m[3]).length !== 0), u ? ms : bs
		);
	}
	let R = w(t, -1),
		b = R(t);
	return {
		c() {
			e = k('select');
			for (let m = 0; m < v.length; m += 1) v[m].c();
			(n = T()),
				(r = k('button')),
				(o = F('+')),
				(l = T()),
				(c = k('br')),
				(i = T()),
				b.c(),
				(a = ve()),
				f(
					e,
					'class',
					'mx-auto mb-4 w-[min(280px,_45%)] cursor-pointer rounded-lg border-2 border-slate-300 bg-inherit p-2 text-inherit duration-200 hover:opacity-75 dark:border-slate-800'
				),
				t[0] === void 0 && Be(() => t[8].call(e)),
				f(
					r,
					'class',
					'aspect-square w-8 rounded-lg border-2 border-blue-500 text-[20px] font-bold duration-200 enabled:hover:opacity-75 disabled:border-gray-500'
				),
				(r.disabled = t[1]);
		},
		m(m, y) {
			C(m, e, y);
			for (let g = 0; g < v.length; g += 1) v[g] && v[g].m(e, null);
			ft(e, t[0], !0),
				C(m, n, y),
				C(m, r, y),
				_(r, o),
				C(m, l, y),
				C(m, c, y),
				C(m, i, y),
				b.m(m, y),
				C(m, a, y),
				d || ((h = [O(e, 'change', t[8]), O(r, 'click', t[5])]), (d = !0));
		},
		p(m, y) {
			if (y & 8) {
				p = fe(Object.entries(m[3]));
				let g;
				for (g = 0; g < p.length; g += 1) {
					const S = vt(m, p, g);
					v[g] ? v[g].p(S, y) : ((v[g] = xt(S)), v[g].c(), v[g].m(e, null));
				}
				for (; g < v.length; g += 1) v[g].d(1);
				v.length = p.length;
			}
			y & 9 && ft(e, m[0]),
				y & 2 && (r.disabled = m[1]),
				R === (R = w(m, y)) && b
					? b.p(m, y)
					: (b.d(1), (b = R(m)), b && (b.c(), b.m(a.parentNode, a)));
		},
		d(m) {
			m && (x(e), x(n), x(r), x(l), x(c), x(i), x(a)), He(v, m), b.d(m), (d = !1), oe(h);
		}
	};
}
function xt(t) {
	let e,
		n = (t[13].name ?? je) + '',
		r,
		o;
	return {
		c() {
			(e = k('option')),
				(r = F(n)),
				f(e, 'class', 'dark:bg-slate-900'),
				(e.__value = o = t[14]),
				ge(e, e.__value);
		},
		m(l, c) {
			C(l, e, c), _(e, r);
		},
		p(l, c) {
			c & 8 && n !== (n = (l[13].name ?? je) + '') && ae(r, n),
				c & 8 && o !== (o = l[14]) && ((e.__value = o), ge(e, e.__value));
		},
		d(l) {
			l && x(e);
		}
	};
}
function bs(t) {
	let e;
	return {
		c() {
			(e = k('p')),
				(e.textContent = `You have no plugins installed. Click the + button and select a plugin's
						bolt.json file to add it.`);
		},
		m(n, r) {
			C(n, e, r);
		},
		p: j,
		d(n) {
			n && x(e);
		}
	};
}
function ms(t) {
	let e = Object.keys(te(ke)).includes(t[0]) && t[2] !== null,
		n,
		r = e && Ct(t);
	return {
		c() {
			r && r.c(), (n = ve());
		},
		m(o, l) {
			r && r.m(o, l), C(o, n, l);
		},
		p(o, l) {
			l & 5 && (e = Object.keys(te(ke)).includes(o[0]) && o[2] !== null),
				e
					? r
						? r.p(o, l)
						: ((r = Ct(o)), r.c(), r.m(n.parentNode, n))
					: r && (r.d(1), (r = null));
		},
		d(o) {
			o && x(n), r && r.d(o);
		}
	};
}
function Ct(t) {
	let e,
		n,
		r = {
			ctx: t,
			current: null,
			token: null,
			hasCatch: !0,
			pending: ys,
			then: ws,
			catch: ks,
			value: 13
		};
	return (
		Je((n = t[2]), r),
		{
			c() {
				(e = ve()), r.block.c();
			},
			m(o, l) {
				C(o, e, l),
					r.block.m(o, (r.anchor = l)),
					(r.mount = () => e.parentNode),
					(r.anchor = e);
			},
			p(o, l) {
				(t = o), (r.ctx = t), (l & 4 && n !== (n = t[2]) && Je(n, r)) || Nt(r, t, l);
			},
			d(o) {
				o && x(e), r.block.d(o), (r.token = null), (r = null);
			}
		}
	);
}
function ks(t) {
	let e;
	return {
		c() {
			(e = k('p')), (e.textContent = 'error');
		},
		m(n, r) {
			C(n, e, r);
		},
		p: j,
		d(n) {
			n && x(e);
		}
	};
}
function ws(t) {
	let e,
		n = (t[13].name ?? je) + '',
		r,
		o,
		l,
		c = (t[13].description ?? 'no description') + '',
		i,
		u;
	return {
		c() {
			(e = k('p')),
				(r = F(n)),
				(o = T()),
				(l = k('p')),
				(i = F(c)),
				f(e, 'class', 'text-xl font-bold pb-4'),
				f(l, 'class', (u = t[13].description ? 'not-italic' : 'italic'));
		},
		m(a, d) {
			C(a, e, d), _(e, r), C(a, o, d), C(a, l, d), _(l, i);
		},
		p(a, d) {
			d & 4 && n !== (n = (a[13].name ?? je) + '') && ae(r, n),
				d & 4 && c !== (c = (a[13].description ?? 'no description') + '') && ae(i, c),
				d & 4 &&
					u !== (u = a[13].description ? 'not-italic' : 'italic') &&
					f(l, 'class', u);
		},
		d(a) {
			a && (x(e), x(o), x(l));
		}
	};
}
function ys(t) {
	let e;
	return {
		c() {
			(e = k('p')), (e.textContent = 'loading...');
		},
		m(n, r) {
			C(n, e, r);
		},
		p: j,
		d(n) {
			n && x(e);
		}
	};
}
function vs(t) {
	let e, n, r, o, l, c, i, u, a, d, h, p, v, w, R, b, m;
	(n = new it({})), n.$on('click', t[6]);
	let y = {
		ctx: t,
		current: null,
		token: null,
		hasCatch: !0,
		pending: _s,
		then: ds,
		catch: us,
		value: 17
	};
	Je((p = t[4]), y);
	function g(N, J) {
		return Te ? gs : hs;
	}
	let L = g()(t);
	return {
		c() {
			(e = k('div')),
				ne(n.$$.fragment),
				(r = T()),
				(o = k('div')),
				(l = k('button')),
				(l.innerHTML = '<img src="svgs/xmark-solid.svg" class="h-5 w-5" alt="Close"/>'),
				(c = T()),
				(i = k('div')),
				(u = k('button')),
				(u.textContent = 'Manage Plugins'),
				(a = T()),
				(d = k('hr')),
				(h = T()),
				y.block.c(),
				(v = T()),
				(w = k('div')),
				L.c(),
				f(
					l,
					'class',
					'absolute right-3 top-3 rounded-full bg-rose-500 p-[2px] shadow-lg duration-200 hover:rotate-90 hover:opacity-75'
				),
				f(
					u,
					'class',
					'mx-auto mb-2 w-[95%] rounded-lg bg-blue-500 p-2 font-bold text-black duration-200 hover:opacity-75'
				),
				f(d, 'class', 'p-1 dark:border-slate-700'),
				f(
					i,
					'class',
					'left-0 float-left h-full w-[min(180px,_50%)] overflow-hidden border-r-2 border-slate-300 pt-2 dark:border-slate-800'
				),
				f(w, 'class', 'h-full pt-10'),
				f(
					o,
					'class',
					'absolute left-[5%] top-[5%] z-20 h-[90%] w-[90%] rounded-lg bg-slate-100 text-center shadow-lg dark:bg-slate-900'
				);
		},
		m(N, J) {
			C(N, e, J),
				K(n, e, null),
				_(e, r),
				_(e, o),
				_(o, l),
				_(o, c),
				_(o, i),
				_(i, u),
				_(i, a),
				_(i, d),
				_(i, h),
				y.block.m(i, (y.anchor = null)),
				(y.mount = () => i),
				(y.anchor = null),
				_(o, v),
				_(o, w),
				L.m(w, null),
				(R = !0),
				b || ((m = [O(l, 'click', t[6]), O(u, 'click', Ss)]), (b = !0));
		},
		p(N, [J]) {
			(t = N),
				(y.ctx = t),
				(J & 16 && p !== (p = t[4]) && Je(p, y)) || Nt(y, t, J),
				L.p(t, J);
		},
		i(N) {
			R || (A(n.$$.fragment, N), (R = !0));
		},
		o(N) {
			D(n.$$.fragment, N), (R = !1);
		},
		d(N) {
			N && x(e), Q(n), y.block.d(), (y.token = null), (y = null), L.d(), (b = !1), oe(m);
		}
	};
}
const je = '(unnamed)',
	Ss = () => {};
function Ls(t, e, n) {
	let r, o, l;
	X(t, ke, (b) => n(3, (o = b))), X(t, et, (b) => n(4, (l = b)));
	let { showPluginMenu: c } = e;
	const i = (b) =>
			new Promise((m, y) => {
				const g = b.concat(b.endsWith('/') ? 'bolt.json' : '/bolt.json');
				var S = new XMLHttpRequest();
				(S.onreadystatechange = () => {
					S.readyState == 4 &&
						(S.status == 200 ? m(JSON.parse(S.responseText)) : y(S.responseText));
				}),
					S.open(
						'GET',
						'/read-json-file?'.concat(new URLSearchParams({ path: g }).toString()),
						!0
					),
					S.send();
			}),
		u = (b) => {
			const y = te(ke)[b];
			if (!y) return null;
			const g = y.path;
			return g ? i(g) : null;
		},
		a = (b, m) => {
			i(b)
				.then((y) => {
					do n(0, (w = crypto.randomUUID()));
					while (Object.keys(te(ke)).includes(w));
					H(ke, (o[w] = { name: y.name ?? je, path: b }), o);
				})
				.catch((y) => {
					console.error(`Config file '${m}' couldn't be fetched, reason: ${y}`);
				});
		};
	let d = !1;
	const h = () => {
		n(1, (d = !0));
		var b = new XMLHttpRequest();
		(b.onreadystatechange = () => {
			if (b.readyState == 4 && (n(1, (d = !1)), b.status == 200)) {
				const m =
					te(Fe) === 'windows' ? b.responseText.replaceAll('\\', '/') : b.responseText;
				if (m.endsWith('/bolt.json')) {
					const y = m.substring(0, m.length - 9);
					a(y, m);
				} else console.log(`Selection '${m}' is not named bolt.json; ignored`);
			}
		}),
			b.open('GET', '/json-file-picker', !0),
			b.send();
	};
	et.set(Gt());
	const p = () => {
		d || n(7, (c = !1));
	};
	function v(b) {
		b.key === 'Escape' && p();
	}
	addEventListener('keydown', v);
	var w;
	rt(() => {
		removeEventListener('keydown', v);
	});
	function R() {
		(w = Zt(this)), n(0, w);
	}
	return (
		(t.$$set = (b) => {
			'showPluginMenu' in b && n(7, (c = b.showPluginMenu));
		}),
		(t.$$.update = () => {
			t.$$.dirty & 1 && n(2, (r = u(w)));
		}),
		[w, d, r, o, l, h, p, c, R]
	);
}
class xs extends ce {
	constructor(e) {
		super(), le(this, e, Ls, vs, se, { showPluginMenu: 7 });
	}
}
function Cs(t) {
	let e,
		n,
		r,
		o,
		l,
		c,
		i,
		u,
		a,
		d,
		h,
		p,
		v,
		w,
		R,
		b,
		m = t[1] && Pt(t),
		y = t[0] && Rt(t),
		g = t[3] && Tt();
	function S(M) {
		t[6](M);
	}
	let L = {};
	t[0] !== void 0 && (L.showSettings = t[0]),
		(o = new Tn({ props: L })),
		B.push(() => Ne(o, 'showSettings', S));
	function N(M) {
		t[7](M);
	}
	let J = {};
	return (
		t[1] !== void 0 && (J.showPluginMenu = t[1]),
		(d = new cs({ props: J })),
		B.push(() => Ne(d, 'showPluginMenu', N)),
		(R = new es({})),
		{
			c() {
				m && m.c(),
					(e = T()),
					y && y.c(),
					(n = T()),
					g && g.c(),
					(r = T()),
					ne(o.$$.fragment),
					(c = T()),
					(i = k('div')),
					(u = k('div')),
					(a = T()),
					ne(d.$$.fragment),
					(p = T()),
					(v = k('div')),
					(w = T()),
					ne(R.$$.fragment),
					f(i, 'class', 'mt-16 grid h-full grid-flow-col grid-cols-3');
			},
			m(M, P) {
				m && m.m(M, P),
					C(M, e, P),
					y && y.m(M, P),
					C(M, n, P),
					g && g.m(M, P),
					C(M, r, P),
					K(o, M, P),
					C(M, c, P),
					C(M, i, P),
					_(i, u),
					_(i, a),
					K(d, i, null),
					_(i, p),
					_(i, v),
					C(M, w, P),
					K(R, M, P),
					(b = !0);
			},
			p(M, P) {
				M[1]
					? m
						? (m.p(M, P), P & 2 && A(m, 1))
						: ((m = Pt(M)), m.c(), A(m, 1), m.m(e.parentNode, e))
					: m &&
						(we(),
						D(m, 1, 1, () => {
							m = null;
						}),
						ye()),
					M[0]
						? y
							? (y.p(M, P), P & 1 && A(y, 1))
							: ((y = Rt(M)), y.c(), A(y, 1), y.m(n.parentNode, n))
						: y &&
							(we(),
							D(y, 1, 1, () => {
								y = null;
							}),
							ye()),
					M[3]
						? g
							? P & 8 && A(g, 1)
							: ((g = Tt()), g.c(), A(g, 1), g.m(r.parentNode, r))
						: g &&
							(we(),
							D(g, 1, 1, () => {
								g = null;
							}),
							ye());
				const E = {};
				!l && P & 1 && ((l = !0), (E.showSettings = M[0]), Ie(() => (l = !1))), o.$set(E);
				const ee = {};
				!h && P & 2 && ((h = !0), (ee.showPluginMenu = M[1]), Ie(() => (h = !1))),
					d.$set(ee);
			},
			i(M) {
				b ||
					(A(m),
					A(y),
					A(g),
					A(o.$$.fragment, M),
					A(d.$$.fragment, M),
					A(R.$$.fragment, M),
					(b = !0));
			},
			o(M) {
				D(m),
					D(y),
					D(g),
					D(o.$$.fragment, M),
					D(d.$$.fragment, M),
					D(R.$$.fragment, M),
					(b = !1);
			},
			d(M) {
				M && (x(e), x(n), x(r), x(c), x(i), x(w)),
					m && m.d(M),
					y && y.d(M),
					g && g.d(M),
					Q(o, M),
					Q(d),
					Q(R, M);
			}
		}
	);
}
function Ps(t) {
	let e, n;
	return (
		(e = new as({})),
		{
			c() {
				ne(e.$$.fragment);
			},
			m(r, o) {
				K(e, r, o), (n = !0);
			},
			p: j,
			i(r) {
				n || (A(e.$$.fragment, r), (n = !0));
			},
			o(r) {
				D(e.$$.fragment, r), (n = !1);
			},
			d(r) {
				Q(e, r);
			}
		}
	);
}
function Pt(t) {
	let e, n, r;
	function o(c) {
		t[4](c);
	}
	let l = {};
	return (
		t[1] !== void 0 && (l.showPluginMenu = t[1]),
		(e = new xs({ props: l })),
		B.push(() => Ne(e, 'showPluginMenu', o)),
		{
			c() {
				ne(e.$$.fragment);
			},
			m(c, i) {
				K(e, c, i), (r = !0);
			},
			p(c, i) {
				const u = {};
				!n && i & 2 && ((n = !0), (u.showPluginMenu = c[1]), Ie(() => (n = !1))), e.$set(u);
			},
			i(c) {
				r || (A(e.$$.fragment, c), (r = !0));
			},
			o(c) {
				D(e.$$.fragment, c), (r = !1);
			},
			d(c) {
				Q(e, c);
			}
		}
	);
}
function Rt(t) {
	let e, n, r;
	function o(c) {
		t[5](c);
	}
	let l = {};
	return (
		t[0] !== void 0 && (l.showSettings = t[0]),
		(e = new Wn({ props: l })),
		B.push(() => Ne(e, 'showSettings', o)),
		{
			c() {
				ne(e.$$.fragment);
			},
			m(c, i) {
				K(e, c, i), (r = !0);
			},
			p(c, i) {
				const u = {};
				!n && i & 1 && ((n = !0), (u.showSettings = c[0]), Ie(() => (n = !1))), e.$set(u);
			},
			i(c) {
				r || (A(e.$$.fragment, c), (r = !0));
			},
			o(c) {
				D(e.$$.fragment, c), (r = !1);
			},
			d(c) {
				Q(e, c);
			}
		}
	);
}
function Tt(t) {
	let e, n;
	return (
		(e = new jn({})),
		{
			c() {
				ne(e.$$.fragment);
			},
			m(r, o) {
				K(e, r, o), (n = !0);
			},
			i(r) {
				n || (A(e.$$.fragment, r), (n = !0));
			},
			o(r) {
				D(e.$$.fragment, r), (n = !1);
			},
			d(r) {
				Q(e, r);
			}
		}
	);
}
function Rs(t) {
	let e, n, r, o;
	const l = [Ps, Cs],
		c = [];
	function i(u, a) {
		return u[2] ? 0 : 1;
	}
	return (
		(n = i(t)),
		(r = c[n] = l[n](t)),
		{
			c() {
				(e = k('main')), r.c(), f(e, 'class', 'h-full');
			},
			m(u, a) {
				C(u, e, a), c[n].m(e, null), (o = !0);
			},
			p(u, [a]) {
				let d = n;
				(n = i(u)),
					n === d
						? c[n].p(u, a)
						: (we(),
							D(c[d], 1, 1, () => {
								c[d] = null;
							}),
							ye(),
							(r = c[n]),
							r ? r.p(u, a) : ((r = c[n] = l[n](u)), r.c()),
							A(r, 1),
							r.m(e, null));
			},
			i(u) {
				o || (A(r), (o = !0));
			},
			o(u) {
				D(r), (o = !1);
			},
			d(u) {
				u && x(e), c[n].d();
			}
		}
	);
}
function Ts(t, e, n) {
	let r;
	X(t, Xe, (p) => n(3, (r = p)));
	let o = !1,
		l = !1,
		c = !1;
	dn();
	const i = window.opener || window.parent;
	if (i) {
		const p = new URLSearchParams(window.location.search);
		p.get('id_token')
			? ((c = !0),
				i.postMessage(
					{
						type: 'gameSessionServerAuth',
						code: p.get('code'),
						id_token: p.get('id_token'),
						state: p.get('state')
					},
					'*'
				))
			: p.get('code') &&
				((c = !0),
				i.postMessage(
					{ type: 'authCode', code: p.get('code'), state: p.get('state') },
					'*'
				));
	}
	function u(p) {
		(l = p), n(1, l);
	}
	function a(p) {
		(o = p), n(0, o);
	}
	function d(p) {
		(o = p), n(0, o);
	}
	function h(p) {
		(l = p), n(1, l);
	}
	return [o, l, c, r, u, a, d, h];
}
class Ms extends ce {
	constructor(e) {
		super(), le(this, e, Ts, Rs, se, {});
	}
}
new Ms({ target: document.getElementById('app') });
const ie = [],
	Es = te(Ot);
let V;
ie.push(lt.subscribe((t) => (V = t)));
let re;
ie.push(q.subscribe((t) => (re = t)));
ie.push(Fe.subscribe((t) => t));
let he;
ie.push(me.subscribe((t) => (he = t)));
ie.push(Te.subscribe((t) => t ?? !1));
let ue;
ie.push($e.subscribe((t) => (ue = t)));
let at;
ie.push(ct.subscribe((t) => (at = t)));
ie.push(Se.subscribe((t) => t));
ie.push(Ge.subscribe((t) => t));
let ze;
ie.push(W.subscribe((t) => (ze = t)));
function Is() {
	var o;
	const t = atob(V.origin),
		e = atob(V.clientid),
		n = t.concat('/oauth2/token');
	he.size == 0 && Xe.set(!0),
		an(),
		re.selected_game_accounts &&
			((o = re.selected_game_accounts) == null ? void 0 : o.size) > 0 &&
			q.update((l) => {
				var c;
				return (
					(l.selected_characters = l.selected_game_accounts),
					(c = l.selected_game_accounts) == null || c.clear(),
					l
				);
			});
	const r = [Es, t, atob(V.origin_2fa)];
	window.addEventListener('message', (l) => {
		if (!r.includes(l.origin)) {
			z(`discarding window message from origin ${l.origin}`);
			return;
		}
		let c = ue;
		const i = new XMLHttpRequest();
		switch (l.data.type) {
			case 'authCode':
				if (c) {
					$e.set({});
					const u = new URLSearchParams({
						grant_type: 'authorization_code',
						client_id: atob(V.clientid),
						code: l.data.code,
						code_verifier: c.verifier,
						redirect_uri: atob(V.redirect)
					});
					(i.onreadystatechange = () => {
						if (i.readyState == 4)
							if (i.status == 200) {
								const a = Ut(i.response),
									d = Ht(a);
								d
									? pt(c == null ? void 0 : c.win, d).then((h) => {
											h && (me.update((p) => (p.set(d.sub, d), p)), Me());
										})
									: (G('Error: invalid credentials received', !1), c.win.close());
							} else
								G(`Error: from ${n}: ${i.status}: ${i.response}`, !1),
									c.win.close();
					}),
						i.open('POST', n, !0),
						i.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded'),
						i.setRequestHeader('Accept', 'application/json'),
						i.send(u);
				}
				break;
			case 'externalUrl':
				(i.onreadystatechange = () => {
					i.readyState == 4 && z(`External URL status: '${i.responseText.trim()}'`);
				}),
					i.open('POST', '/open-external-url', !0),
					i.send(l.data.url);
				break;
			case 'gameSessionServerAuth':
				if (((c = at.find((u) => l.data.state == u.state)), c)) {
					un(c, !0);
					const u = l.data.id_token.split('.');
					if (u.length !== 3) {
						G(`Malformed id_token: ${u.length} sections, expected 3`, !1);
						break;
					}
					const a = JSON.parse(atob(u[0]));
					if (a.typ !== 'JWT') {
						G(`Bad id_token header: typ ${a.typ}, expected JWT`, !1);
						break;
					}
					const d = JSON.parse(atob(u[1]));
					if (atob(d.nonce) !== c.nonce) {
						G('Incorrect nonce in id_token', !1);
						break;
					}
					const h = atob(V.auth_api).concat('/sessions');
					(i.onreadystatechange = () => {
						if (i.readyState == 4)
							if (i.status == 200) {
								const p = atob(V.auth_api).concat('/accounts');
								(c.creds.session_id = JSON.parse(i.response).sessionId),
									Bt(c.creds, p, c.account_info_promise).then((v) => {
										v &&
											(me.update((w) => {
												var R;
												return (
													w.set(
														(R = c == null ? void 0 : c.creds) == null
															? void 0
															: R.sub,
														c.creds
													),
													w
												);
											}),
											Me());
									});
							} else G(`Error: from ${h}: ${i.status}: ${i.response}`, !1);
					}),
						i.open('POST', h, !0),
						i.setRequestHeader('Content-Type', 'application/json'),
						i.setRequestHeader('Accept', 'application/json'),
						i.send(`{"idToken": "${l.data.id_token}"}`);
				}
				break;
			case 'gameClientListUpdate':
				et.set(Gt());
				break;
			default:
				z('Unknown message type: '.concat(l.data.type));
				break;
		}
	}),
		(async () => (
			he.size > 0 &&
				he.forEach(async (l) => {
					const c = await Dt(l, n, e);
					c !== null &&
						c !== 0 &&
						(G(`Discarding expired login for #${l.sub}`, !1),
						me.update((u) => (u.delete(l.sub), u)),
						Me());
					let i;
					if (
						(c === null && (await pt(null, l))
							? (i = { creds: l, valid: !0 })
							: (i = { creds: l, valid: c === 0 }),
						i.valid)
					) {
						const u = l;
						me.update((a) => (a.set(u.sub, u), a)), Me();
					}
				}),
			$.set(!1)
		))();
}
function z(t) {
	console.log(t);
	const e = { isError: !1, text: t, time: new Date(Date.now()) };
	Se.update((n) => (n.unshift(e), n));
}
function G(t, e) {
	const n = { isError: !0, text: t, time: new Date(Date.now()) };
	if ((Se.update((r) => (r.unshift(n), r)), !e)) console.error(t);
	else throw new Error(t);
}
lt.set(s());
onload = () => Is();
onunload = () => {
	for (const t in ie) delete ie[t];
	Ve();
};
