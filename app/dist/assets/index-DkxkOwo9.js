var zt = Object.defineProperty;
var Ft = (t, e, n) =>
	e in t ? zt(t, e, { enumerable: !0, configurable: !0, writable: !0, value: n }) : (t[e] = n);
var Ze = (t, e, n) => (Ft(t, typeof e != 'symbol' ? e + '' : e, n), n);
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
function I() {}
function Vt(t) {
	return !!t && (typeof t == 'object' || typeof t == 'function') && typeof t.then == 'function';
}
function Nt(t) {
	return t();
}
function ft() {
	return Object.create(null);
}
function re(t) {
	t.forEach(Nt);
}
function It(t) {
	return typeof t == 'function';
}
function oe(t, e) {
	return t != t ? e == e : t !== e || (t && typeof t == 'object') || typeof t == 'function';
}
let qe;
function Wt(t, e) {
	return t === e ? !0 : (qe || (qe = document.createElement('a')), (qe.href = e), t === qe.href);
}
function Zt(t) {
	return Object.keys(t).length === 0;
}
function jt(t, ...e) {
	if (t == null) {
		for (const r of e) r(void 0);
		return I;
	}
	const n = t.subscribe(...e);
	return n.unsubscribe ? () => n.unsubscribe() : n;
}
function Q(t) {
	let e;
	return jt(t, (n) => (e = n))(), e;
}
function V(t, e, n) {
	t.$$.on_destroy.push(jt(e, n));
}
function j(t, e, n) {
	return t.set(n), e;
}
function h(t, e) {
	t.appendChild(e);
}
function v(t, e, n) {
	t.insertBefore(e, n || null);
}
function y(t) {
	t.parentNode && t.parentNode.removeChild(t);
}
function Oe(t, e) {
	for (let n = 0; n < t.length; n += 1) t[n] && t[n].d(e);
}
function b(t) {
	return document.createElement(t);
}
function z(t) {
	return document.createTextNode(t);
}
function N() {
	return z(' ');
}
function _e() {
	return z('');
}
function H(t, e, n, r) {
	return t.addEventListener(e, n, r), () => t.removeEventListener(e, n, r);
}
function _(t, e, n) {
	n == null ? t.removeAttribute(e) : t.getAttribute(e) !== n && t.setAttribute(e, n);
}
function Yt(t) {
	return Array.from(t.childNodes);
}
function ie(t, e) {
	(e = '' + e), t.data !== e && (t.data = e);
}
function me(t, e) {
	t.value = e ?? '';
}
function pt(t, e, n) {
	for (let r = 0; r < t.options.length; r += 1) {
		const o = t.options[r];
		if (o.__value === e) {
			o.selected = !0;
			return;
		}
	}
	(!n || e !== void 0) && (t.selectedIndex = -1);
}
function $t(t) {
	const e = t.querySelector(':checked');
	return e && e.__value;
}
function Kt(t, e, { bubbles: n = !1, cancelable: r = !1 } = {}) {
	return new CustomEvent(t, { detail: e, bubbles: n, cancelable: r });
}
let Ne;
function he(t) {
	Ne = t;
}
function Ae() {
	if (!Ne) throw new Error('Function called outside component initialization');
	return Ne;
}
function De(t) {
	Ae().$$.on_mount.push(t);
}
function Qt(t) {
	Ae().$$.after_update.push(t);
}
function rt(t) {
	Ae().$$.on_destroy.push(t);
}
function en() {
	const t = Ae();
	return (e, n, { cancelable: r = !1 } = {}) => {
		const o = t.$$.callbacks[e];
		if (o) {
			const l = Kt(e, n, { cancelable: r });
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
const Re = [],
	J = [];
let Te = [];
const Ke = [],
	tn = Promise.resolve();
let Qe = !1;
function nn() {
	Qe || ((Qe = !0), tn.then(ot));
}
function Je(t) {
	Te.push(t);
}
function Ie(t) {
	Ke.push(t);
}
const Ye = new Set();
let xe = 0;
function ot() {
	if (xe !== 0) return;
	const t = Ne;
	do {
		try {
			for (; xe < Re.length; ) {
				const e = Re[xe];
				xe++, he(e), sn(e.$$);
			}
		} catch (e) {
			throw ((Re.length = 0), (xe = 0), e);
		}
		for (he(null), Re.length = 0, xe = 0; J.length; ) J.pop()();
		for (let e = 0; e < Te.length; e += 1) {
			const n = Te[e];
			Ye.has(n) || (Ye.add(n), n());
		}
		Te.length = 0;
	} while (Re.length);
	for (; Ke.length; ) Ke.pop()();
	(Qe = !1), Ye.clear(), he(t);
}
function sn(t) {
	if (t.fragment !== null) {
		t.update(), re(t.before_update);
		const e = t.dirty;
		(t.dirty = [-1]), t.fragment && t.fragment.p(t.ctx, e), t.after_update.forEach(Je);
	}
}
function rn(t) {
	const e = [],
		n = [];
	Te.forEach((r) => (t.indexOf(r) === -1 ? e.push(r) : n.push(r))),
		n.forEach((r) => r()),
		(Te = e);
}
const Ue = new Set();
let we;
function ve() {
	we = { r: 0, c: [], p: we };
}
function Se() {
	we.r || re(we.c), (we = we.p);
}
function D(t, e) {
	t && t.i && (Ue.delete(t), t.i(e));
}
function U(t, e, n, r) {
	if (t && t.o) {
		if (Ue.has(t)) return;
		Ue.add(t),
			we.c.push(() => {
				Ue.delete(t), r && (n && t.d(1), r());
			}),
			t.o(e);
	} else r && r();
}
function Me(t, e) {
	const n = (e.token = {});
	function r(o, l, c, i) {
		if (e.token !== n) return;
		e.resolved = i;
		let a = e.ctx;
		c !== void 0 && ((a = a.slice()), (a[c] = i));
		const u = o && (e.current = o)(a);
		let d = !1;
		e.block &&
			(e.blocks
				? e.blocks.forEach((f, p) => {
						p !== l &&
							f &&
							(ve(),
							U(f, 1, 1, () => {
								e.blocks[p] === f && (e.blocks[p] = null);
							}),
							Se());
					})
				: e.block.d(1),
			u.c(),
			D(u, 1),
			u.m(e.mount(), e.anchor),
			(d = !0)),
			(e.block = u),
			e.blocks && (e.blocks[l] = u),
			d && ot();
	}
	if (Vt(t)) {
		const o = Ae();
		if (
			(t.then(
				(l) => {
					he(o), r(e.then, 1, e.value, l), he(null);
				},
				(l) => {
					if ((he(o), r(e.catch, 2, e.error, l), he(null), !e.hasCatch)) throw l;
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
function lt(t, e, n) {
	const r = e.slice(),
		{ resolved: o } = t;
	t.current === t.then && (r[t.value] = o),
		t.current === t.catch && (r[t.error] = o),
		t.block.p(r, n);
}
function pe(t) {
	return (t == null ? void 0 : t.length) !== void 0 ? t : Array.from(t);
}
function je(t, e, n) {
	const r = t.$$.props[e];
	r !== void 0 && ((t.$$.bound[r] = n), n(t.$$.ctx[r]));
}
function ne(t) {
	t && t.c();
}
function ee(t, e, n) {
	const { fragment: r, after_update: o } = t.$$;
	r && r.m(e, n),
		Je(() => {
			const l = t.$$.on_mount.map(Nt).filter(It);
			t.$$.on_destroy ? t.$$.on_destroy.push(...l) : re(l), (t.$$.on_mount = []);
		}),
		o.forEach(Je);
}
function te(t, e) {
	const n = t.$$;
	n.fragment !== null &&
		(rn(n.after_update),
		re(n.on_destroy),
		n.fragment && n.fragment.d(e),
		(n.on_destroy = n.fragment = null),
		(n.ctx = []));
}
function on(t, e) {
	t.$$.dirty[0] === -1 && (Re.push(t), nn(), t.$$.dirty.fill(0)),
		(t.$$.dirty[(e / 31) | 0] |= 1 << e % 31);
}
function le(t, e, n, r, o, l, c = null, i = [-1]) {
	const a = Ne;
	he(t);
	const u = (t.$$ = {
		fragment: null,
		ctx: [],
		props: l,
		update: I,
		not_equal: o,
		bound: ft(),
		on_mount: [],
		on_destroy: [],
		on_disconnect: [],
		before_update: [],
		after_update: [],
		context: new Map(e.context || (a ? a.$$.context : [])),
		callbacks: ft(),
		dirty: i,
		skip_bound: !1,
		root: e.target || a.$$.root
	});
	c && c(u.root);
	let d = !1;
	if (
		((u.ctx = n
			? n(t, e.props || {}, (f, p, ...m) => {
					const g = m.length ? m[0] : p;
					return (
						u.ctx &&
							o(u.ctx[f], (u.ctx[f] = g)) &&
							(!u.skip_bound && u.bound[f] && u.bound[f](g), d && on(t, f)),
						p
					);
				})
			: []),
		u.update(),
		(d = !0),
		re(u.before_update),
		(u.fragment = r ? r(u.ctx) : !1),
		e.target)
	) {
		if (e.hydrate) {
			const f = Yt(e.target);
			u.fragment && u.fragment.l(f), f.forEach(y);
		} else u.fragment && u.fragment.c();
		e.intro && D(t.$$.fragment), ee(t, e.target, e.anchor), ot();
	}
	he(a);
}
class ce {
	constructor() {
		Ze(this, '$$');
		Ze(this, '$$set');
	}
	$destroy() {
		te(this, 1), (this.$destroy = I);
	}
	$on(e, n) {
		if (!It(n)) return I;
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
			!Zt(e) &&
			((this.$$.skip_bound = !0), this.$$set(e), (this.$$.skip_bound = !1));
	}
}
const ln = '4';
typeof window < 'u' && (window.__svelte || (window.__svelte = { v: new Set() })).v.add(ln);
const Pe = [];
function Ht(t, e) {
	return { subscribe: $(t, e).subscribe };
}
function $(t, e = I) {
	let n;
	const r = new Set();
	function o(i) {
		if (oe(t, i) && ((t = i), n)) {
			const a = !Pe.length;
			for (const u of r) u[1](), Pe.push(u, t);
			if (a) {
				for (let u = 0; u < Pe.length; u += 2) Pe[u][0](Pe[u + 1]);
				Pe.length = 0;
			}
		}
	}
	function l(i) {
		o(i(t));
	}
	function c(i, a = I) {
		const u = [i, a];
		return (
			r.add(u),
			r.size === 1 && (n = e(o, l) || I),
			i(t),
			() => {
				r.delete(u), r.size === 0 && n && (n(), (n = null));
			}
		);
	}
	return { set: o, update: l, subscribe: c };
}
function Ot(t) {
	if (t.ok) return t.value;
	throw t.error;
}
var K = ((t) => ((t[(t.rs3 = 0)] = 'rs3'), (t[(t.osrs = 1)] = 'osrs'), t))(K || {}),
	de = ((t) => (
		(t[(t.runeLite = 0)] = 'runeLite'), (t[(t.hdos = 1)] = 'hdos'), (t[(t.rs3 = 2)] = 'rs3'), t
	))(de || {});
const cn = {
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
	At = Ht('https://bolt-internal'),
	an = Ht('1fddee4e-b100-4f4e-b2b0-097f9088f9d2'),
	ct = $(),
	Fe = $(''),
	B = $({ ...cn }),
	ye = $(new Map()),
	ke = $(!1),
	fe = $(),
	et = $(),
	Le = $([]),
	Ve = $({}),
	it = $([]),
	tt = $(''),
	nt = $(''),
	st = $(''),
	W = $(!1),
	Ge = $(new Map()),
	Y = $({ game: K.osrs, client: de.runeLite }),
	Xe = $(!1);
function un() {
	se.use_dark_theme == !1 && document.documentElement.classList.remove('dark');
}
function dn(t, e) {
	e && t.win.close(), it.update((n) => (n.splice(ut.indexOf(t), 1), n));
}
function Dt() {
	if (ue && ue.win && !ue.win.closed) ue.win.focus();
	else if ((ue && ue.win && ue.win.closed) || ue) {
		const t = Bt(),
			e = _n();
		pn({
			origin: atob(Z.origin),
			redirect: atob(Z.redirect),
			authMethod: '',
			loginType: '',
			clientid: atob(Z.clientid),
			flow: 'launcher',
			pkceState: t,
			pkceCodeVerifier: e
		}).then((n) => {
			const r = window.open(n, '', 'width=480,height=720');
			Ve.set({ state: t, verifier: e, win: r });
		});
	}
}
function fn() {
	const t = new URLSearchParams(window.location.search);
	Fe.set(t.get('platform')),
		t.get('flathub'),
		tt.set(t.get('rs3_linux_installed_hash')),
		nt.set(t.get('runelite_installed_id')),
		st.set(t.get('hdos_installed_version'));
	const e = t.get('plugins');
	e !== null ? (ke.set(!0), fe.set(JSON.parse(e))) : ke.set(!1);
	const n = t.get('credentials');
	if (n)
		try {
			JSON.parse(n).forEach((l) => {
				ye.update((c) => (c.set(l.sub, l), c));
			});
		} catch (o) {
			F(`Couldn't parse credentials file: ${o}`, !1);
		}
	const r = t.get('config');
	if (r)
		try {
			const o = JSON.parse(r);
			B.set(o),
				B.update(
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
			F(`Couldn't parse config file: ${o}`, !1);
		}
}
async function qt(t, e, n) {
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
							i = Ot(c);
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
		return F(l, !1), { ok: !1, error: new Error(l) };
	}
	const r = JSON.parse(atob(n[0]));
	if (r.typ !== 'JWT') {
		const l = `Bad id_token header: typ ${r.typ}, expected JWT`;
		return F(l, !1), { ok: !1, error: new Error(l) };
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
async function pn(t) {
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
function _n() {
	const t = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~',
		e = new Uint32Array(43);
	return (
		crypto.getRandomValues(e),
		Array.from(e, function (n) {
			return t[n % t.length];
		}).join('')
	);
}
function Bt() {
	const e = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz',
		n = e.length - 1,
		r = crypto.getRandomValues(new Uint8Array(12));
	return Array.from(r)
		.map((o) => Math.round((o * (n - 0)) / 255 + 0))
		.map((o) => e[o])
		.join('');
}
async function Jt(t, e, n) {
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
								X(`Successfully added login for ${c.displayName}`),
									JSON.parse(o.response).forEach((i) => {
										c.characters.set(i.accountId, {
											accountId: i.accountId,
											displayName: i.displayName,
											userHash: i.userHash
										});
									}),
									gn(c),
									r(!0);
							} else F(`Error getting account info: ${l}`, !1), r(!1);
						})
					: (F(`Error: from ${e}: ${o.status}: ${o.response}`, !1), r(!1)));
		}),
			o.open('GET', e, !0),
			o.setRequestHeader('Accept', 'application/json'),
			o.setRequestHeader('Authorization', 'Bearer '.concat(t.session_id)),
			o.send();
	});
}
async function _t(t, e) {
	return await hn(t, e);
}
async function hn(t, e) {
	const n = Bt(),
		r = crypto.randomUUID(),
		o = atob(Z.origin)
			.concat('/oauth2/auth?')
			.concat(
				new URLSearchParams({
					id_token_hint: e.id_token,
					nonce: btoa(r),
					prompt: 'consent',
					redirect_uri: 'http://localhost',
					response_type: 'id_token code',
					state: n,
					client_id: Q(an),
					scope: 'openid offline'
				}).toString()
			),
		l = bn(e);
	return t
		? ((t.location.href = o),
			it.update(
				(c) => (
					c.push({ state: n, nonce: r, creds: e, win: t, account_info_promise: l }), c
				)
			),
			!1)
		: e.session_id
			? await Jt(e, atob(Z.auth_api).concat('/accounts'), l)
			: (F('Rejecting stored credentials with missing session_id', !1), !1);
}
function bn(t) {
	return new Promise((e) => {
		const n = `${atob(Z.api)}/users/${t.sub}/displayName`,
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
		Y.update((n) => {
			n.account = t;
			const [r] = t.characters.keys();
			return (
				(n.character = t.characters.get(r)),
				ge.size > 0 && (n.credentials = ge.get(t.userId)),
				n
			);
		});
	};
	Ge.update((n) => (n.set(t.userId, t), n)),
		ze.account && se.selected_account
			? t.userId == se.selected_account && e()
			: ze.account || e(),
		Ve.set({});
}
function mn(t, e, n) {
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
async function Ee() {
	const t = new XMLHttpRequest();
	t.open('POST', '/save-credentials', !0),
		t.setRequestHeader('Content-Type', 'application/json'),
		(t.onreadystatechange = () => {
			t.readyState == 4 && X(`Save-credentials status: ${t.responseText.trim()}`);
		}),
		Y.update((n) => {
			var r;
			return (n.credentials = ge.get((r = ze.account) == null ? void 0 : r.userId)), n;
		});
	const e = [];
	ge.forEach((n) => {
		e.push(n);
	}),
		t.send(JSON.stringify(e));
}
function kn(t, e, n) {
	We();
	const r = (i, a) => {
			const u = new XMLHttpRequest(),
				d = {};
			i && (d.hash = i),
				t && (d.jx_session_id = t),
				e && (d.jx_character_id = e),
				n && (d.jx_display_name = n),
				se.rs_plugin_loader && (d.plugin_loader = '1'),
				se.rs_config_uri
					? (d.config_uri = se.rs_config_uri)
					: (d.config_uri = atob(Z.default_config_uri)),
				u.open('POST', '/launch-rs3-deb?'.concat(new URLSearchParams(d).toString()), !0),
				(u.onreadystatechange = () => {
					u.readyState == 4 &&
						(X(`Game launch status: '${u.responseText.trim()}'`),
						u.status == 200 && i && tt.set(i));
				}),
				u.send(a);
		},
		o = new XMLHttpRequest(),
		l = atob(Z.content_url),
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
						.map((a) => a.split(': '))
				);
				if (!i.Filename || !i.Size) {
					F(`Could not parse package data from URL: ${c}`, !1), r();
					return;
				}
				if (i.SHA256 !== Q(tt)) {
					X('Downloading RS3 client...');
					const a = new XMLHttpRequest();
					a.open('GET', l.concat(i.Filename), !0),
						(a.responseType = 'arraybuffer'),
						(a.onprogress = (u) => {
							u.loaded &&
								Le.update(
									(d) => (
										(d[0].text = `Downloading RS3 client... ${(Math.round((1e3 * u.loaded) / u.total) / 10).toFixed(1)}%`),
										d
									)
								);
						}),
						(a.onreadystatechange = () => {
							a.readyState == 4 && a.status == 200 && r(i.SHA256, a.response);
						}),
						(a.onerror = () => {
							F(`Error downloading game client: from ${c}: non-http error`, !1), r();
						}),
						a.send();
				} else X('Latest client is already installed'), r();
			}
		}),
		(o.onerror = () => {
			F(`Error: from ${c}: non-http error`, !1), r();
		}),
		o.send();
}
function Gt(t, e, n, r) {
	We();
	const o = r ? '/launch-runelite-jar-configure?' : '/launch-runelite-jar?',
		l = (a, u, d) => {
			const f = new XMLHttpRequest(),
				p = {};
			a && (p.id = a),
				d && (p.jar_path = d),
				t && (p.jx_session_id = t),
				e && (p.jx_character_id = e),
				n && (p.jx_display_name = n),
				se.flatpak_rich_presence && (p.flatpak_rich_presence = ''),
				f.open(u ? 'POST' : 'GET', o.concat(new URLSearchParams(p).toString()), !0),
				(f.onreadystatechange = () => {
					f.readyState == 4 &&
						(X(`Game launch status: '${f.responseText.trim()}'`),
						f.status == 200 && a && nt.set(a));
				}),
				f.send(u);
		};
	if (se.runelite_use_custom_jar) {
		l(null, null, se.runelite_custom_jar);
		return;
	}
	const c = new XMLHttpRequest(),
		i = 'https://api.github.com/repos/runelite/launcher/releases';
	c.open('GET', i, !0),
		(c.onreadystatechange = () => {
			if (c.readyState == 4)
				if (c.status == 200) {
					const a = JSON.parse(c.responseText)
						.map((u) => u.assets)
						.flat()
						.find((u) => u.name.toLowerCase() == 'runelite.jar');
					if (a.id != Q(nt)) {
						X('Downloading RuneLite...');
						const u = new XMLHttpRequest();
						u.open('GET', a.browser_download_url, !0),
							(u.responseType = 'arraybuffer'),
							(u.onreadystatechange = () => {
								u.readyState == 4 &&
									(u.status == 200
										? l(a.id, u.response)
										: F(
												`Error downloading from ${a.url}: ${u.status}: ${u.responseText}`,
												!1
											));
							}),
							(u.onprogress = (d) => {
								d.loaded &&
									d.lengthComputable &&
									Le.update(
										(f) => (
											(f[0].text = `Downloading RuneLite... ${(Math.round((1e3 * d.loaded) / d.total) / 10).toFixed(1)}%`),
											f
										)
									);
							}),
							u.send();
					} else X('Latest JAR is already installed'), l();
				} else F(`Error from ${i}: ${c.status}: ${c.responseText}`, !1);
		}),
		c.send();
}
function wn(t, e, n) {
	return Gt(t, e, n, !1);
}
function yn(t, e, n) {
	return Gt(t, e, n, !0);
}
function vn(t, e, n) {
	We();
	const r = (c, i) => {
			const a = new XMLHttpRequest(),
				u = {};
			c && (u.version = c),
				t && (u.jx_session_id = t),
				e && (u.jx_character_id = e),
				n && (u.jx_display_name = n),
				a.open('POST', '/launch-hdos-jar?'.concat(new URLSearchParams(u).toString()), !0),
				(a.onreadystatechange = () => {
					a.readyState == 4 &&
						(X(`Game launch status: '${a.responseText.trim()}'`),
						a.status == 200 && c && st.set(c));
				}),
				a.send(i);
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
						if (i !== Q(st)) {
							const a = `https://cdn.hdos.dev/launcher/v${i}/hdos-launcher.jar`;
							X('Downloading HDOS...');
							const u = new XMLHttpRequest();
							u.open('GET', a, !0),
								(u.responseType = 'arraybuffer'),
								(u.onreadystatechange = () => {
									if (u.readyState == 4)
										if (u.status == 200) r(i, u.response);
										else {
											const d = JSON.parse(o.responseText)
												.map((f) => f.assets)
												.flat()
												.find(
													(f) => f.name.toLowerCase() == 'runelite.jar'
												);
											F(
												`Error downloading from ${d.url}: ${u.status}: ${u.responseText}`,
												!1
											);
										}
								}),
								(u.onprogress = (d) => {
									d.loaded &&
										d.lengthComputable &&
										Le.update(
											(f) => (
												(f[0].text = `Downloading HDOS... ${(Math.round((1e3 * d.loaded) / d.total) / 10).toFixed(1)}%`),
												f
											)
										);
								}),
								u.send();
						} else X('Latest JAR is already installed'), r();
					} else X("Couldn't parse latest launcher version"), r();
				} else F(`Error from ${l}: ${o.status}: ${o.responseText}`, !1);
		}),
		o.send();
}
let $e = !1;
function We() {
	var t;
	if (Q(W) && !$e) {
		$e = !0;
		const e = new XMLHttpRequest();
		e.open('POST', '/save-config', !0),
			(e.onreadystatechange = () => {
				e.readyState == 4 &&
					(X(`Save config status: '${e.responseText.trim()}'`),
					e.status == 200 && W.set(!1),
					($e = !1));
			}),
			e.setRequestHeader('Content-Type', 'application/json');
		const n = {};
		(t = se.selected_characters) == null ||
			t.forEach((l, c) => {
				n[c] = l;
			});
		const r = {};
		Object.assign(r, se), (r.selected_characters = n);
		const o = JSON.stringify(r, null, 4);
		e.send(o);
	}
}
function Xt() {
	return new Promise((t, e) => {
		const n = new XMLHttpRequest(),
			r = Q(At).concat('/list-game-clients');
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
function Sn() {
	const t = new XMLHttpRequest();
	t.open('POST', '/save-plugin-config', !0),
		t.setRequestHeader('Content-Type', 'application/json'),
		(t.onreadystatechange = () => {
			t.readyState == 4 && X(`Save-plugin-config status: ${t.responseText.trim()}`);
		}),
		t.send(JSON.stringify(Q(fe)));
}
function ht(t, e, n) {
	const r = t.slice();
	return (r[22] = e[n]), r;
}
function bt(t) {
	let e,
		n = t[22][1].displayName + '',
		r,
		o,
		l;
	return {
		c() {
			(e = b('option')),
				(r = z(n)),
				_(e, 'data-id', (o = t[22][1].userId)),
				_(e, 'class', 'dark:bg-slate-900'),
				(e.__value = l = t[22][1].displayName),
				me(e, e.__value);
		},
		m(c, i) {
			v(c, e, i), h(e, r);
		},
		p(c, i) {
			i & 4 && n !== (n = c[22][1].displayName + '') && ie(r, n),
				i & 4 && o !== (o = c[22][1].userId) && _(e, 'data-id', o),
				i & 4 && l !== (l = c[22][1].displayName) && ((e.__value = l), me(e, e.__value));
		},
		d(c) {
			c && y(e);
		}
	};
}
function Ln(t) {
	let e,
		n,
		r,
		o,
		l,
		c,
		i,
		a,
		u,
		d = pe(t[2]),
		f = [];
	for (let p = 0; p < d.length; p += 1) f[p] = bt(ht(t, d, p));
	return {
		c() {
			(e = b('div')), (n = b('select'));
			for (let p = 0; p < f.length; p += 1) f[p].c();
			(r = N()),
				(o = b('div')),
				(l = b('button')),
				(l.textContent = 'Log In'),
				(c = N()),
				(i = b('button')),
				(i.textContent = 'Log Out'),
				_(n, 'name', 'account_select'),
				_(n, 'id', 'account_select'),
				_(
					n,
					'class',
					'w-full cursor-pointer rounded-lg border-2 border-inherit bg-inherit p-2 text-center'
				),
				_(
					l,
					'class',
					'mx-auto mr-2 rounded-lg bg-blue-500 p-2 font-bold text-black duration-200 hover:opacity-75'
				),
				_(
					i,
					'class',
					'mx-auto rounded-lg border-2 border-blue-500 p-2 font-bold duration-200 hover:opacity-75'
				),
				_(o, 'class', 'mt-5 flex'),
				_(
					e,
					'class',
					'z-10 w-48 rounded-lg border-2 border-slate-300 bg-slate-100 p-3 shadow dark:border-slate-800 dark:bg-slate-900'
				),
				_(e, 'role', 'none');
		},
		m(p, m) {
			v(p, e, m), h(e, n);
			for (let g = 0; g < f.length; g += 1) f[g] && f[g].m(n, null);
			t[7](n),
				h(e, r),
				h(e, o),
				h(o, l),
				h(o, c),
				h(o, i),
				a ||
					((u = [
						H(n, 'change', t[8]),
						H(l, 'click', t[9]),
						H(i, 'click', t[10]),
						H(e, 'mouseenter', t[11]),
						H(e, 'mouseleave', t[12])
					]),
					(a = !0));
		},
		p(p, [m]) {
			if (m & 4) {
				d = pe(p[2]);
				let g;
				for (g = 0; g < d.length; g += 1) {
					const R = ht(p, d, g);
					f[g] ? f[g].p(R, m) : ((f[g] = bt(R)), f[g].c(), f[g].m(n, null));
				}
				for (; g < f.length; g += 1) f[g].d(1);
				f.length = d.length;
			}
		},
		i: I,
		o: I,
		d(p) {
			p && y(e), Oe(f, p), t[7](null), (a = !1), re(u);
		}
	};
}
function Cn(t, e, n) {
	let r, o, l, c;
	V(t, Y, (k) => n(13, (r = k))),
		V(t, ye, (k) => n(14, (o = k))),
		V(t, Ge, (k) => n(2, (l = k))),
		V(t, B, (k) => n(15, (c = k)));
	let { showAccountDropdown: i } = e,
		{ hoverAccountButton: a } = e;
	const u = atob(Z.origin),
		d = atob(Z.clientid),
		f = u.concat('/oauth2/token'),
		p = u.concat('/oauth2/revoke');
	let m = !1,
		g;
	function R(k) {
		a || (k.button === 0 && i && !m && n(5, (i = !1)));
	}
	function T() {
		var q;
		if (g.options.length == 0) {
			X('Logout unsuccessful: no account selected');
			return;
		}
		let k = r.credentials;
		r.account && (l.delete((q = r.account) == null ? void 0 : q.userId), Ge.set(l));
		const P = g.selectedIndex;
		P > 0
			? n(1, (g.selectedIndex = P - 1), g)
			: P == 0 && l.size > 0
				? n(1, (g.selectedIndex = P + 1), g)
				: (delete r.account, delete r.character, delete r.credentials),
			L(),
			k &&
				qt(k, f, d).then((A) => {
					A === null
						? mn(k.access_token, p, d).then((E) => {
								E === 200
									? (X('Successful logout'), C(k))
									: F(`Logout unsuccessful: status ${E}`, !1);
							})
						: A === 400 || A === 401
							? (X(
									'Logout unsuccessful: credentials are invalid, so discarding them anyway'
								),
								k && C(k))
							: F(
									'Logout unsuccessful: unable to verify credentials due to a network error',
									!1
								);
				});
	}
	function C(k) {
		o.delete(k.sub), Ee();
	}
	function L() {
		var P;
		W.set(!0);
		const k = g[g.selectedIndex].getAttribute('data-id');
		if (
			(j(Y, (r.account = l.get(k)), r),
			j(B, (c.selected_account = k), c),
			j(Y, (r.credentials = o.get((P = r.account) == null ? void 0 : P.userId)), r),
			r.account && r.account.characters)
		) {
			const q = document.getElementById('character_select');
			q.selectedIndex = 0;
			const [A] = r.account.characters.keys();
			j(Y, (r.character = r.account.characters.get(A)), r);
		}
	}
	addEventListener('mousedown', R),
		De(() => {
			var P;
			let k = 0;
			l.forEach((q, A) => {
				var E;
				q.displayName == ((E = r.account) == null ? void 0 : E.displayName) &&
					n(1, (g.selectedIndex = k), g),
					k++;
			}),
				j(Y, (r.credentials = o.get((P = r.account) == null ? void 0 : P.userId)), r);
		}),
		rt(() => {
			removeEventListener('mousedown', R);
		});
	function w(k) {
		J[k ? 'unshift' : 'push'](() => {
			(g = k), n(1, g);
		});
	}
	const S = () => {
			L();
		},
		M = () => {
			Dt();
		},
		O = () => {
			T();
		},
		G = () => {
			n(0, (m = !0));
		},
		x = () => {
			n(0, (m = !1));
		};
	return (
		(t.$$set = (k) => {
			'showAccountDropdown' in k && n(5, (i = k.showAccountDropdown)),
				'hoverAccountButton' in k && n(6, (a = k.hoverAccountButton));
		}),
		[m, g, l, T, L, i, a, w, S, M, O, G, x]
	);
}
class xn extends ce {
	constructor(e) {
		super(), le(this, e, Cn, Ln, oe, { showAccountDropdown: 5, hoverAccountButton: 6 });
	}
}
function Pn(t) {
	let e;
	return {
		c() {
			e = z('Log In');
		},
		m(n, r) {
			v(n, e, r);
		},
		p: I,
		d(n) {
			n && y(e);
		}
	};
}
function Rn(t) {
	var r;
	let e = ((r = t[6].account) == null ? void 0 : r.displayName) + '',
		n;
	return {
		c() {
			n = z(e);
		},
		m(o, l) {
			v(o, n, l);
		},
		p(o, l) {
			var c;
			l & 64 &&
				e !== (e = ((c = o[6].account) == null ? void 0 : c.displayName) + '') &&
				ie(n, e);
		},
		d(o) {
			o && y(n);
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
		(n = new xn({ props: c })),
		J.push(() => je(n, 'showAccountDropdown', l)),
		{
			c() {
				(e = b('div')), ne(n.$$.fragment), _(e, 'class', 'absolute right-2 top-[72px]');
			},
			m(i, a) {
				v(i, e, a), ee(n, e, null), (o = !0);
			},
			p(i, a) {
				const u = {};
				a & 4 && (u.hoverAccountButton = i[2]),
					!r && a & 2 && ((r = !0), (u.showAccountDropdown = i[1]), Ie(() => (r = !1))),
					n.$set(u);
			},
			i(i) {
				o || (D(n.$$.fragment, i), (o = !0));
			},
			o(i) {
				U(n.$$.fragment, i), (o = !1);
			},
			d(i) {
				i && y(e), te(n);
			}
		}
	);
}
function Tn(t) {
	let e, n, r, o, l, c, i, a, u, d, f, p, m, g, R, T;
	function C(M, O) {
		return M[6].account ? Rn : Pn;
	}
	let L = C(t),
		w = L(t),
		S = t[1] && gt(t);
	return {
		c() {
			(e = b('div')),
				(n = b('div')),
				(r = b('button')),
				(r.textContent = 'RS3'),
				(o = N()),
				(l = b('button')),
				(l.textContent = 'OSRS'),
				(c = N()),
				(i = b('div')),
				(a = b('button')),
				(a.innerHTML =
					'<img src="svgs/lightbulb-solid.svg" class="h-6 w-6" alt="Change Theme"/>'),
				(u = N()),
				(d = b('button')),
				(d.innerHTML = '<img src="svgs/gear-solid.svg" class="h-6 w-6" alt="Settings"/>'),
				(f = N()),
				(p = b('button')),
				w.c(),
				(m = N()),
				S && S.c(),
				_(
					r,
					'class',
					'mx-1 w-20 rounded-lg border-2 border-blue-500 p-2 duration-200 hover:opacity-75'
				),
				_(
					l,
					'class',
					'mx-1 w-20 rounded-lg border-2 border-blue-500 bg-blue-500 p-2 text-black duration-200 hover:opacity-75'
				),
				_(n, 'class', 'm-3 ml-9 font-bold'),
				_(
					a,
					'class',
					'my-3 h-10 w-10 rounded-full bg-blue-500 p-2 duration-200 hover:rotate-45 hover:opacity-75'
				),
				_(
					d,
					'class',
					'm-3 h-10 w-10 rounded-full bg-blue-500 p-2 duration-200 hover:rotate-45 hover:opacity-75'
				),
				_(
					p,
					'class',
					'm-2 w-48 rounded-lg border-2 border-slate-300 bg-inherit p-2 text-center font-bold text-black duration-200 hover:opacity-75 dark:border-slate-800 dark:text-slate-50'
				),
				_(i, 'class', 'ml-auto flex'),
				_(
					e,
					'class',
					'fixed top-0 flex h-16 w-screen border-b-2 border-slate-300 bg-slate-100 duration-200 dark:border-slate-800 dark:bg-slate-900'
				);
		},
		m(M, O) {
			v(M, e, O),
				h(e, n),
				h(n, r),
				t[10](r),
				h(n, o),
				h(n, l),
				t[12](l),
				h(e, c),
				h(e, i),
				h(i, a),
				h(i, u),
				h(i, d),
				h(i, f),
				h(i, p),
				w.m(p, null),
				t[16](p),
				h(e, m),
				S && S.m(e, null),
				(g = !0),
				R ||
					((T = [
						H(r, 'click', t[11]),
						H(l, 'click', t[13]),
						H(a, 'click', t[14]),
						H(d, 'click', t[15]),
						H(p, 'mouseenter', t[17]),
						H(p, 'mouseleave', t[18]),
						H(p, 'click', t[19])
					]),
					(R = !0));
		},
		p(M, [O]) {
			L === (L = C(M)) && w ? w.p(M, O) : (w.d(1), (w = L(M)), w && (w.c(), w.m(p, null))),
				M[1]
					? S
						? (S.p(M, O), O & 2 && D(S, 1))
						: ((S = gt(M)), S.c(), D(S, 1), S.m(e, null))
					: S &&
						(ve(),
						U(S, 1, 1, () => {
							S = null;
						}),
						Se());
		},
		i(M) {
			g || (D(S), (g = !0));
		},
		o(M) {
			U(S), (g = !1);
		},
		d(M) {
			M && y(e), t[10](null), t[12](null), w.d(), t[16](null), S && S.d(), (R = !1), re(T);
		}
	};
}
function Mn(t, e, n) {
	let r, o, l;
	V(t, B, (P) => n(21, (r = P))), V(t, W, (P) => n(22, (o = P))), V(t, Y, (P) => n(6, (l = P)));
	let { showSettings: c } = e,
		i = !1,
		a = !1,
		u,
		d,
		f;
	function p() {
		let P = document.documentElement;
		P.classList.contains('dark') ? P.classList.remove('dark') : P.classList.add('dark'),
			j(B, (r.use_dark_theme = !r.use_dark_theme), r),
			j(W, (o = !0), o);
	}
	function m(P) {
		switch (P) {
			case K.osrs:
				j(Y, (l.game = K.osrs), l),
					j(Y, (l.client = r.selected_client_index), l),
					j(B, (r.selected_game_index = K.osrs), r),
					j(W, (o = !0), o),
					d.classList.add('bg-blue-500', 'text-black'),
					u.classList.remove('bg-blue-500', 'text-black');
				break;
			case K.rs3:
				j(Y, (l.game = K.rs3), l),
					j(B, (r.selected_game_index = K.rs3), r),
					j(W, (o = !0), o),
					d.classList.remove('bg-blue-500', 'text-black'),
					u.classList.add('bg-blue-500', 'text-black');
				break;
		}
	}
	function g() {
		if (f.innerHTML == 'Log In') {
			Dt();
			return;
		}
		n(1, (i = !i));
	}
	De(() => {
		m(r.selected_game_index);
	});
	function R(P) {
		J[P ? 'unshift' : 'push'](() => {
			(u = P), n(3, u);
		});
	}
	const T = () => {
		m(K.rs3);
	};
	function C(P) {
		J[P ? 'unshift' : 'push'](() => {
			(d = P), n(4, d);
		});
	}
	const L = () => {
			m(K.osrs);
		},
		w = () => p(),
		S = () => {
			n(0, (c = !0));
		};
	function M(P) {
		J[P ? 'unshift' : 'push'](() => {
			(f = P), n(5, f);
		});
	}
	const O = () => {
			n(2, (a = !0));
		},
		G = () => {
			n(2, (a = !1));
		},
		x = () => g();
	function k(P) {
		(i = P), n(1, i);
	}
	return (
		(t.$$set = (P) => {
			'showSettings' in P && n(0, (c = P.showSettings));
		}),
		[c, i, a, u, d, f, l, p, m, g, R, T, C, L, w, S, M, O, G, x, k]
	);
}
class En extends ce {
	constructor(e) {
		super(), le(this, e, Mn, Tn, oe, { showSettings: 0 });
	}
}
function Nn(t) {
	let e, n, r, o, l, c;
	return {
		c() {
			(e = b('div')),
				(n = b('div')),
				(n.innerHTML = ''),
				(r = N()),
				(o = b('div')),
				(o.innerHTML = ''),
				_(
					n,
					'class',
					'absolute left-0 top-0 z-10 h-screen w-screen backdrop-blur-sm backdrop-filter'
				),
				_(o, 'class', 'absolute left-0 top-0 z-10 h-screen w-screen bg-black opacity-75'),
				_(o, 'role', 'none');
		},
		m(i, a) {
			v(i, e, a), h(e, n), h(e, r), h(e, o), l || ((c = H(o, 'click', t[1])), (l = !0));
		},
		p: I,
		i: I,
		o: I,
		d(i) {
			i && y(e), (l = !1), c();
		}
	};
}
function In(t) {
	const e = en();
	return [
		e,
		() => {
			e('click');
		}
	];
}
class at extends ce {
	constructor(e) {
		super(), le(this, e, In, Nn, oe, {});
	}
}
function jn(t) {
	let e, n, r, o, l, c, i, a, u, d, f, p;
	return (
		(n = new at({})),
		{
			c() {
				(e = b('div')),
					ne(n.$$.fragment),
					(r = N()),
					(o = b('div')),
					(l = b('p')),
					(l.textContent = `${t[1]}`),
					(c = N()),
					(i = b('p')),
					(i.textContent = `${t[2]}`),
					(a = N()),
					(u = b('button')),
					(u.textContent = 'I Understand'),
					_(l, 'class', 'p-2'),
					_(i, 'class', 'p-2'),
					_(
						u,
						'class',
						'm-5 rounded-lg border-2 border-blue-500 p-2 duration-200 hover:opacity-75'
					),
					_(
						o,
						'class',
						'absolute left-1/4 top-1/4 z-20 w-1/2 rounded-lg bg-slate-100 p-5 text-center shadow-lg dark:bg-slate-900'
					),
					_(e, 'id', 'disclaimer');
			},
			m(m, g) {
				v(m, e, g),
					ee(n, e, null),
					h(e, r),
					h(e, o),
					h(o, l),
					h(o, c),
					h(o, i),
					h(o, a),
					h(o, u),
					(d = !0),
					f || ((p = H(u, 'click', t[3])), (f = !0));
			},
			p: I,
			i(m) {
				d || (D(n.$$.fragment, m), (d = !0));
			},
			o(m) {
				U(n.$$.fragment, m), (d = !1);
			},
			d(m) {
				m && y(e), te(n), (f = !1), p();
			}
		}
	);
}
function Hn(t, e, n) {
	let r;
	V(t, Xe, (i) => n(0, (r = i)));
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
			j(Xe, (r = !1), r);
		}
	];
}
class On extends ce {
	constructor(e) {
		super(), le(this, e, Hn, jn, oe, {});
	}
}
function An(t) {
	let e, n, r, o;
	return {
		c() {
			(e = b('div')),
				(n = b('button')),
				(n.innerHTML = `<div class="flex"><img src="svgs/database-solid.svg" alt="Browse app data" class="mr-2 h-7 w-7 rounded-lg bg-violet-500 p-1"/>
			Browse App Data</div>`),
				_(n, 'id', 'data_dir_button'),
				_(n, 'class', 'p-2 hover:opacity-75'),
				_(e, 'id', 'general_options'),
				_(e, 'class', 'col-span-3 p-5 pt-10');
		},
		m(l, c) {
			v(l, e, c), h(e, n), r || ((o = H(n, 'click', t[1])), (r = !0));
		},
		p: I,
		i: I,
		o: I,
		d(l) {
			l && y(e), (r = !1), o();
		}
	};
}
function Dn(t) {
	function e() {
		var r = new XMLHttpRequest();
		r.open('GET', '/browse-data'),
			(r.onreadystatechange = () => {
				r.readyState == 4 && X(`Browse status: '${r.responseText.trim()}'`);
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
class qn extends ce {
	constructor(e) {
		super(), le(this, e, Dn, An, oe, {});
	}
}
function Un(t) {
	let e, n, r, o, l, c, i, a, u, d, f, p, m, g, R, T, C, L, w, S, M;
	return {
		c() {
			(e = b('div')),
				(n = b('button')),
				(n.innerHTML = `<div class="flex"><img src="svgs/wrench-solid.svg" alt="Configure RuneLite" class="mr-2 h-7 w-7 rounded-lg bg-pink-500 p-1"/>
			Configure RuneLite</div>`),
				(r = N()),
				(o = b('div')),
				(l = b('label')),
				(l.textContent = 'Expose rich presence to Flatpak Discord:'),
				(c = N()),
				(i = b('input')),
				(a = N()),
				(u = b('div')),
				(d = b('label')),
				(d.textContent = 'Use custom RuneLite JAR:'),
				(f = N()),
				(p = b('input')),
				(m = N()),
				(g = b('div')),
				(R = b('textarea')),
				(T = N()),
				(C = b('br')),
				(L = N()),
				(w = b('button')),
				(w.textContent = 'Select File'),
				_(n, 'id', 'rl_configure'),
				_(n, 'class', 'p-2 pb-5 hover:opacity-75'),
				_(l, 'for', 'flatpak_rich_presence'),
				_(i, 'type', 'checkbox'),
				_(i, 'name', 'flatpak_rich_presence'),
				_(i, 'id', 'flatpak_rich_presence'),
				_(i, 'class', 'ml-2'),
				_(o, 'id', 'flatpak_div'),
				_(o, 'class', 'mx-auto border-t-2 border-slate-300 p-2 py-5 dark:border-slate-800'),
				_(d, 'for', 'use_custom_jar'),
				_(p, 'type', 'checkbox'),
				_(p, 'name', 'use_custom_jar'),
				_(p, 'id', 'use_custom_jar'),
				_(p, 'class', 'ml-2'),
				_(u, 'class', 'mx-auto border-t-2 border-slate-300 p-2 pt-5 dark:border-slate-800'),
				(R.disabled = !0),
				_(R, 'name', 'custom_jar_file'),
				_(R, 'id', 'custom_jar_file'),
				_(
					R,
					'class',
					'h-10 rounded border-2 border-slate-300 bg-slate-100 text-slate-950 dark:border-slate-800 dark:bg-slate-900 dark:text-slate-50'
				),
				(w.disabled = !0),
				_(w, 'id', 'custom_jar_file_button'),
				_(
					w,
					'class',
					'mt-1 rounded-lg border-2 border-blue-500 p-1 duration-200 hover:opacity-75'
				),
				_(g, 'id', 'custom_jar_div'),
				_(g, 'class', 'mx-auto p-2 opacity-25'),
				_(e, 'id', 'osrs_options'),
				_(e, 'class', 'col-span-3 p-5 pt-10');
		},
		m(O, G) {
			v(O, e, G),
				h(e, n),
				h(e, r),
				h(e, o),
				h(o, l),
				h(o, c),
				h(o, i),
				t[12](i),
				t[14](o),
				h(e, a),
				h(e, u),
				h(u, d),
				h(u, f),
				h(u, p),
				t[15](p),
				h(e, m),
				h(e, g),
				h(g, R),
				t[17](R),
				h(g, T),
				h(g, C),
				h(g, L),
				h(g, w),
				t[19](w),
				t[21](g),
				S ||
					((M = [
						H(n, 'click', t[11]),
						H(i, 'change', t[13]),
						H(p, 'change', t[16]),
						H(R, 'change', t[18]),
						H(w, 'click', t[20])
					]),
					(S = !0));
		},
		p: I,
		i: I,
		o: I,
		d(O) {
			O && y(e),
				t[12](null),
				t[14](null),
				t[15](null),
				t[17](null),
				t[19](null),
				t[21](null),
				(S = !1),
				re(M);
		}
	};
}
function Bn(t, e, n) {
	let r, o, l, c;
	V(t, Fe, (E) => n(22, (r = E))),
		V(t, B, (E) => n(23, (o = E))),
		V(t, Y, (E) => n(24, (l = E))),
		V(t, W, (E) => n(25, (c = E)));
	let i, a, u, d, f, p;
	function m() {
		i.classList.toggle('opacity-25'),
			n(1, (a.disabled = !a.disabled), a),
			n(2, (u.disabled = !u.disabled), u),
			j(B, (o.runelite_use_custom_jar = d.checked), o),
			o.runelite_use_custom_jar
				? a.value && j(B, (o.runelite_custom_jar = a.value), o)
				: (n(1, (a.value = ''), a),
					j(B, (o.runelite_custom_jar = ''), o),
					j(B, (o.runelite_use_custom_jar = !1), o)),
			j(W, (c = !0), c);
	}
	function g() {
		j(B, (o.flatpak_rich_presence = f.checked), o), j(W, (c = !0), c);
	}
	function R() {
		j(B, (o.runelite_custom_jar = a.value), o), j(W, (c = !0), c);
	}
	function T() {
		n(3, (d.disabled = !0), d), n(2, (u.disabled = !0), u);
		var E = new XMLHttpRequest();
		(E.onreadystatechange = () => {
			E.readyState == 4 &&
				(E.status == 200 &&
					(n(1, (a.value = E.responseText), a),
					j(B, (o.runelite_custom_jar = E.responseText), o),
					j(W, (c = !0), c)),
				n(2, (u.disabled = !1), u),
				n(3, (d.disabled = !1), d));
		}),
			E.open('GET', '/jar-file-picker', !0),
			E.send();
	}
	function C() {
		var E, Ce, dt;
		if (!l.account || !l.character) {
			X('Please log in to configure RuneLite');
			return;
		}
		yn(
			(E = l.credentials) == null ? void 0 : E.session_id,
			(Ce = l.character) == null ? void 0 : Ce.accountId,
			(dt = l.character) == null ? void 0 : dt.displayName
		);
	}
	De(() => {
		n(4, (f.checked = o.flatpak_rich_presence), f),
			n(3, (d.checked = o.runelite_use_custom_jar), d),
			d.checked && o.runelite_custom_jar
				? (i.classList.remove('opacity-25'),
					n(1, (a.disabled = !1), a),
					n(2, (u.disabled = !1), u),
					n(1, (a.value = o.runelite_custom_jar), a))
				: (n(3, (d.checked = !1), d), j(B, (o.runelite_use_custom_jar = !1), o)),
			r !== 'linux' && p.remove();
	});
	const L = () => C();
	function w(E) {
		J[E ? 'unshift' : 'push'](() => {
			(f = E), n(4, f);
		});
	}
	const S = () => {
		g();
	};
	function M(E) {
		J[E ? 'unshift' : 'push'](() => {
			(p = E), n(5, p);
		});
	}
	function O(E) {
		J[E ? 'unshift' : 'push'](() => {
			(d = E), n(3, d);
		});
	}
	const G = () => {
		m();
	};
	function x(E) {
		J[E ? 'unshift' : 'push'](() => {
			(a = E), n(1, a);
		});
	}
	const k = () => {
		R();
	};
	function P(E) {
		J[E ? 'unshift' : 'push'](() => {
			(u = E), n(2, u);
		});
	}
	const q = () => {
		T();
	};
	function A(E) {
		J[E ? 'unshift' : 'push'](() => {
			(i = E), n(0, i);
		});
	}
	return [i, a, u, d, f, p, m, g, R, T, C, L, w, S, M, O, G, x, k, P, q, A];
}
class Jn extends ce {
	constructor(e) {
		super(), le(this, e, Bn, Un, oe, {});
	}
}
function Gn(t) {
	let e, n, r, o, l, c;
	return {
		c() {
			(e = b('div')),
				(n = b('label')),
				(n.textContent = 'Enable Bolt plugin loader:'),
				(r = N()),
				(o = b('input')),
				_(n, 'for', 'enable_plugins'),
				_(o, 'type', 'checkbox'),
				_(o, 'name', 'enable_plugins'),
				_(o, 'id', 'enable_plugins'),
				_(o, 'class', 'ml-2'),
				_(e, 'class', 'mx-auto p-2');
		},
		m(i, a) {
			v(i, e, a),
				h(e, n),
				h(e, r),
				h(e, o),
				(o.checked = t[3].rs_plugin_loader),
				l || ((c = [H(o, 'change', t[6]), H(o, 'change', t[7])]), (l = !0));
		},
		p(i, a) {
			a & 8 && (o.checked = i[3].rs_plugin_loader);
		},
		d(i) {
			i && y(e), (l = !1), re(c);
		}
	};
}
function Xn(t) {
	let e,
		n,
		r,
		o,
		l,
		c,
		i,
		a,
		u,
		d,
		f,
		p = ke && Gn(t);
	return {
		c() {
			(e = b('div')),
				p && p.c(),
				(n = N()),
				(r = b('div')),
				(o = b('label')),
				(o.textContent = 'Use custom config URI:'),
				(l = N()),
				(c = b('input')),
				(i = N()),
				(a = b('div')),
				(u = b('textarea')),
				_(o, 'for', 'use_custom_uri'),
				_(c, 'type', 'checkbox'),
				_(c, 'name', 'use_custom_uri'),
				_(c, 'id', 'use_custom_uri'),
				_(c, 'class', 'ml-2'),
				_(r, 'class', 'mx-auto p-2'),
				(u.disabled = !0),
				_(u, 'name', 'config_uri_address'),
				_(u, 'id', 'config_uri_address'),
				_(u, 'rows', '4'),
				_(
					u,
					'class',
					'rounded border-2 border-slate-300 bg-slate-100 text-slate-950 dark:border-slate-800 dark:bg-slate-900 dark:text-slate-50'
				),
				(u.value = '		'),
				_(a, 'id', 'config_uri_div'),
				_(a, 'class', 'mx-auto p-2 opacity-25'),
				_(e, 'id', 'rs3_options'),
				_(e, 'class', 'col-span-3 p-5 pt-10');
		},
		m(m, g) {
			v(m, e, g),
				p && p.m(e, null),
				h(e, n),
				h(e, r),
				h(r, o),
				h(r, l),
				h(r, c),
				t[8](c),
				h(e, i),
				h(e, a),
				h(a, u),
				t[11](u),
				t[12](a),
				d || ((f = [H(c, 'change', t[9]), H(u, 'change', t[10])]), (d = !0));
		},
		p(m, [g]) {
			ke && p.p(m, g);
		},
		i: I,
		o: I,
		d(m) {
			m && y(e), p && p.d(), t[8](null), t[11](null), t[12](null), (d = !1), re(f);
		}
	};
}
function zn(t, e, n) {
	let r, o, l;
	V(t, ct, (L) => n(13, (r = L))), V(t, B, (L) => n(3, (o = L))), V(t, W, (L) => n(14, (l = L)));
	let c, i, a;
	function u() {
		c.classList.toggle('opacity-25'),
			n(1, (i.disabled = !i.disabled), i),
			j(W, (l = !0), l),
			a.checked ||
				(n(1, (i.value = atob(r.default_config_uri)), i), j(B, (o.rs_config_uri = ''), o));
	}
	function d() {
		j(B, (o.rs_config_uri = i.value), o), j(W, (l = !0), l);
	}
	De(() => {
		o.rs_config_uri
			? (n(1, (i.value = o.rs_config_uri), i), n(2, (a.checked = !0), a), u())
			: n(1, (i.value = atob(r.default_config_uri)), i);
	});
	function f() {
		(o.rs_plugin_loader = this.checked), B.set(o);
	}
	const p = () => W.set(!0);
	function m(L) {
		J[L ? 'unshift' : 'push'](() => {
			(a = L), n(2, a);
		});
	}
	const g = () => {
			u();
		},
		R = () => {
			d();
		};
	function T(L) {
		J[L ? 'unshift' : 'push'](() => {
			(i = L), n(1, i);
		});
	}
	function C(L) {
		J[L ? 'unshift' : 'push'](() => {
			(c = L), n(0, c);
		});
	}
	return [c, i, a, o, u, d, f, p, m, g, R, T, C];
}
class Fn extends ce {
	constructor(e) {
		super(), le(this, e, zn, Xn, oe, {});
	}
}
function Vn(t) {
	let e, n;
	return (
		(e = new Fn({})),
		{
			c() {
				ne(e.$$.fragment);
			},
			m(r, o) {
				ee(e, r, o), (n = !0);
			},
			i(r) {
				n || (D(e.$$.fragment, r), (n = !0));
			},
			o(r) {
				U(e.$$.fragment, r), (n = !1);
			},
			d(r) {
				te(e, r);
			}
		}
	);
}
function Wn(t) {
	let e, n;
	return (
		(e = new Jn({})),
		{
			c() {
				ne(e.$$.fragment);
			},
			m(r, o) {
				ee(e, r, o), (n = !0);
			},
			i(r) {
				n || (D(e.$$.fragment, r), (n = !0));
			},
			o(r) {
				U(e.$$.fragment, r), (n = !1);
			},
			d(r) {
				te(e, r);
			}
		}
	);
}
function Zn(t) {
	let e, n;
	return (
		(e = new qn({})),
		{
			c() {
				ne(e.$$.fragment);
			},
			m(r, o) {
				ee(e, r, o), (n = !0);
			},
			i(r) {
				n || (D(e.$$.fragment, r), (n = !0));
			},
			o(r) {
				U(e.$$.fragment, r), (n = !1);
			},
			d(r) {
				te(e, r);
			}
		}
	);
}
function Yn(t) {
	let e, n, r, o, l, c, i, a, u, d, f, p, m, g, R, T, C, L, w, S, M, O, G, x;
	(n = new at({})), n.$on('click', t[7]);
	const k = [Zn, Wn, Vn],
		P = [];
	function q(A, E) {
		return A[1] == A[5].general ? 0 : A[1] == A[5].osrs ? 1 : A[1] == A[5].rs3 ? 2 : -1;
	}
	return (
		~(S = q(t)) && (M = P[S] = k[S](t)),
		{
			c() {
				(e = b('div')),
					ne(n.$$.fragment),
					(r = N()),
					(o = b('div')),
					(l = b('button')),
					(l.innerHTML = '<img src="svgs/xmark-solid.svg" class="h-5 w-5" alt="Close"/>'),
					(c = N()),
					(i = b('div')),
					(a = b('div')),
					(u = b('button')),
					(d = z('General')),
					(f = b('br')),
					(p = N()),
					(m = b('button')),
					(g = z('OSRS')),
					(R = b('br')),
					(T = N()),
					(C = b('button')),
					(L = z('RS3')),
					(w = N()),
					M && M.c(),
					_(
						l,
						'class',
						'absolute right-3 top-3 rounded-full bg-rose-500 p-[2px] shadow-lg duration-200 hover:rotate-90 hover:opacity-75'
					),
					_(u, 'id', 'general_button'),
					_(u, 'class', be),
					_(m, 'id', 'osrs_button'),
					_(m, 'class', Be),
					_(C, 'id', 'rs3_button'),
					_(C, 'class', be),
					_(
						a,
						'class',
						'relative h-full border-r-2 border-slate-300 pt-10 dark:border-slate-800'
					),
					_(i, 'class', 'grid h-full grid-cols-4'),
					_(
						o,
						'class',
						'absolute left-[13%] top-[13%] z-20 h-3/4 w-3/4 rounded-lg bg-slate-100 text-center shadow-lg dark:bg-slate-900'
					),
					_(e, 'id', 'settings');
			},
			m(A, E) {
				v(A, e, E),
					ee(n, e, null),
					h(e, r),
					h(e, o),
					h(o, l),
					h(o, c),
					h(o, i),
					h(i, a),
					h(a, u),
					h(u, d),
					t[9](u),
					h(a, f),
					h(a, p),
					h(a, m),
					h(m, g),
					t[11](m),
					h(a, R),
					h(a, T),
					h(a, C),
					h(C, L),
					t[13](C),
					h(i, w),
					~S && P[S].m(i, null),
					(O = !0),
					G ||
						((x = [
							H(l, 'click', t[8]),
							H(u, 'click', t[10]),
							H(m, 'click', t[12]),
							H(C, 'click', t[14])
						]),
						(G = !0));
			},
			p(A, [E]) {
				let Ce = S;
				(S = q(A)),
					S !== Ce &&
						(M &&
							(ve(),
							U(P[Ce], 1, 1, () => {
								P[Ce] = null;
							}),
							Se()),
						~S
							? ((M = P[S]),
								M || ((M = P[S] = k[S](A)), M.c()),
								D(M, 1),
								M.m(i, null))
							: (M = null));
			},
			i(A) {
				O || (D(n.$$.fragment, A), D(M), (O = !0));
			},
			o(A) {
				U(n.$$.fragment, A), U(M), (O = !1);
			},
			d(A) {
				A && y(e),
					te(n),
					t[9](null),
					t[11](null),
					t[13](null),
					~S && P[S].d(),
					(G = !1),
					re(x);
			}
		}
	);
}
let Be =
		'border-2 border-blue-500 bg-blue-500 hover:opacity-75 font-bold text-black duration-200 rounded-lg p-1 mx-auto my-1 w-3/4',
	be = 'border-2 border-blue-500 hover:opacity-75 duration-200 rounded-lg p-1 mx-auto my-1 w-3/4';
function $n(t, e, n) {
	let { showSettings: r } = e;
	var o = ((w) => (
		(w[(w.general = 0)] = 'general'), (w[(w.osrs = 1)] = 'osrs'), (w[(w.rs3 = 2)] = 'rs3'), w
	))(o || {});
	let l = 1,
		c,
		i,
		a;
	function u(w) {
		switch (w) {
			case 0:
				n(1, (l = 0)),
					n(2, (c.classList.value = Be), c),
					n(3, (i.classList.value = be), i),
					n(4, (a.classList.value = be), a);
				break;
			case 1:
				n(1, (l = 1)),
					n(2, (c.classList.value = be), c),
					n(3, (i.classList.value = Be), i),
					n(4, (a.classList.value = be), a);
				break;
			case 2:
				n(1, (l = 2)),
					n(2, (c.classList.value = be), c),
					n(3, (i.classList.value = be), i),
					n(4, (a.classList.value = Be), a);
				break;
		}
	}
	function d(w) {
		w.key === 'Escape' && n(0, (r = !1));
	}
	addEventListener('keydown', d),
		rt(() => {
			removeEventListener('keydown', d);
		});
	const f = () => {
			n(0, (r = !1));
		},
		p = () => {
			n(0, (r = !1));
		};
	function m(w) {
		J[w ? 'unshift' : 'push'](() => {
			(c = w), n(2, c);
		});
	}
	const g = () => {
		u(o.general);
	};
	function R(w) {
		J[w ? 'unshift' : 'push'](() => {
			(i = w), n(3, i);
		});
	}
	const T = () => {
		u(o.osrs);
	};
	function C(w) {
		J[w ? 'unshift' : 'push'](() => {
			(a = w), n(4, a);
		});
	}
	const L = () => {
		u(o.rs3);
	};
	return (
		(t.$$set = (w) => {
			'showSettings' in w && n(0, (r = w.showSettings));
		}),
		[r, l, c, i, a, o, u, f, p, m, g, R, T, C, L]
	);
}
class Kn extends ce {
	constructor(e) {
		super(), le(this, e, $n, Yn, oe, { showSettings: 0 });
	}
}
function mt(t, e, n) {
	const r = t.slice();
	return (r[1] = e[n]), r;
}
function Qn(t) {
	var a;
	let e,
		n = ((a = t[1].time) == null ? void 0 : a.toLocaleTimeString()) + '',
		r,
		o,
		l = t[1].text + '',
		c,
		i;
	return {
		c() {
			(e = b('li')),
				(r = z(n)),
				(o = z(`
					- `)),
				(c = z(l)),
				(i = N());
		},
		m(u, d) {
			v(u, e, d), h(e, r), h(e, o), h(e, c), h(e, i);
		},
		p(u, d) {
			var f;
			d & 1 &&
				n !== (n = ((f = u[1].time) == null ? void 0 : f.toLocaleTimeString()) + '') &&
				ie(r, n),
				d & 1 && l !== (l = u[1].text + '') && ie(c, l);
		},
		d(u) {
			u && y(e);
		}
	};
}
function es(t) {
	var a;
	let e,
		n = ((a = t[1].time) == null ? void 0 : a.toLocaleTimeString()) + '',
		r,
		o,
		l = t[1].text + '',
		c,
		i;
	return {
		c() {
			(e = b('li')),
				(r = z(n)),
				(o = z(`
					- `)),
				(c = z(l)),
				(i = N()),
				_(e, 'class', 'text-rose-500');
		},
		m(u, d) {
			v(u, e, d), h(e, r), h(e, o), h(e, c), h(e, i);
		},
		p(u, d) {
			var f;
			d & 1 &&
				n !== (n = ((f = u[1].time) == null ? void 0 : f.toLocaleTimeString()) + '') &&
				ie(r, n),
				d & 1 && l !== (l = u[1].text + '') && ie(c, l);
		},
		d(u) {
			u && y(e);
		}
	};
}
function kt(t) {
	let e;
	function n(l, c) {
		return l[1].isError ? es : Qn;
	}
	let r = n(t),
		o = r(t);
	return {
		c() {
			o.c(), (e = _e());
		},
		m(l, c) {
			o.m(l, c), v(l, e, c);
		},
		p(l, c) {
			r === (r = n(l)) && o
				? o.p(l, c)
				: (o.d(1), (o = r(l)), o && (o.c(), o.m(e.parentNode, e)));
		},
		d(l) {
			l && y(e), o.d(l);
		}
	};
}
function ts(t) {
	let e,
		n,
		r,
		o,
		l = pe(t[0]),
		c = [];
	for (let i = 0; i < l.length; i += 1) c[i] = kt(mt(t, l, i));
	return {
		c() {
			(e = b('div')),
				(n = b('div')),
				(n.innerHTML =
					'<img src="svgs/circle-info-solid.svg" alt="Message list icon" class="h-7 w-7 rounded-full bg-blue-500 p-[3px] duration-200"/>'),
				(r = N()),
				(o = b('ol'));
			for (let i = 0; i < c.length; i += 1) c[i].c();
			_(n, 'class', 'absolute right-2 top-2'),
				_(o, 'id', 'message_list'),
				_(
					o,
					'class',
					'h-full w-[105%] list-inside list-disc overflow-y-auto pl-5 pt-1 marker:text-blue-500'
				),
				_(
					e,
					'class',
					'fixed bottom-0 h-1/4 w-screen border-t-2 border-t-slate-300 bg-slate-100 duration-200 dark:border-t-slate-800 dark:bg-slate-900'
				);
		},
		m(i, a) {
			v(i, e, a), h(e, n), h(e, r), h(e, o);
			for (let u = 0; u < c.length; u += 1) c[u] && c[u].m(o, null);
		},
		p(i, [a]) {
			if (a & 1) {
				l = pe(i[0]);
				let u;
				for (u = 0; u < l.length; u += 1) {
					const d = mt(i, l, u);
					c[u] ? c[u].p(d, a) : ((c[u] = kt(d)), c[u].c(), c[u].m(o, null));
				}
				for (; u < c.length; u += 1) c[u].d(1);
				c.length = l.length;
			}
		},
		i: I,
		o: I,
		d(i) {
			i && y(e), Oe(c, i);
		}
	};
}
function ns(t, e, n) {
	let r;
	return V(t, Le, (o) => n(0, (r = o))), [r];
}
class ss extends ce {
	constructor(e) {
		super(), le(this, e, ns, ts, oe, {});
	}
}
function wt(t, e, n) {
	const r = t.slice();
	return (r[13] = e[n]), r;
}
function rs(t) {
	let e, n, r;
	return {
		c() {
			(e = b('button')),
				(e.textContent = 'Plugin menu'),
				(e.disabled = !Q(ke)),
				_(
					e,
					'class',
					'mx-auto mb-2 w-52 rounded-lg p-2 font-bold text-black duration-200 enabled:bg-blue-500 enabled:hover:opacity-75 disabled:bg-gray-500'
				);
		},
		m(o, l) {
			v(o, e, l), n || ((r = H(e, 'click', t[8])), (n = !0));
		},
		p: I,
		d(o) {
			o && y(e), (n = !1), r();
		}
	};
}
function os(t) {
	let e, n, r, o, l, c, i, a, u;
	return {
		c() {
			(e = b('label')),
				(e.textContent = 'Game Client'),
				(n = N()),
				(r = b('br')),
				(o = N()),
				(l = b('select')),
				(c = b('option')),
				(c.textContent = 'RuneLite'),
				(i = b('option')),
				(i.textContent = 'HDOS'),
				_(e, 'for', 'game_client_select'),
				_(e, 'class', 'text-sm'),
				_(c, 'data-id', de.runeLite),
				_(c, 'class', 'dark:bg-slate-900'),
				(c.__value = 'RuneLite'),
				me(c, c.__value),
				_(i, 'data-id', de.hdos),
				_(i, 'class', 'dark:bg-slate-900'),
				(i.__value = 'HDOS'),
				me(i, i.__value),
				_(l, 'id', 'game_client_select'),
				_(
					l,
					'class',
					'mx-auto w-52 cursor-pointer rounded-lg border-2 border-slate-300 bg-inherit p-2 text-inherit duration-200 hover:opacity-75 dark:border-slate-800'
				);
		},
		m(d, f) {
			v(d, e, f),
				v(d, n, f),
				v(d, r, f),
				v(d, o, f),
				v(d, l, f),
				h(l, c),
				h(l, i),
				t[7](l),
				a || ((u = H(l, 'change', t[5])), (a = !0));
		},
		p: I,
		d(d) {
			d && (y(e), y(n), y(r), y(o), y(l)), t[7](null), (a = !1), u();
		}
	};
}
function yt(t) {
	let e,
		n = pe(t[4].account.characters),
		r = [];
	for (let o = 0; o < n.length; o += 1) r[o] = vt(wt(t, n, o));
	return {
		c() {
			for (let o = 0; o < r.length; o += 1) r[o].c();
			e = _e();
		},
		m(o, l) {
			for (let c = 0; c < r.length; c += 1) r[c] && r[c].m(o, l);
			v(o, e, l);
		},
		p(o, l) {
			if (l & 16) {
				n = pe(o[4].account.characters);
				let c;
				for (c = 0; c < n.length; c += 1) {
					const i = wt(o, n, c);
					r[c] ? r[c].p(i, l) : ((r[c] = vt(i)), r[c].c(), r[c].m(e.parentNode, e));
				}
				for (; c < r.length; c += 1) r[c].d(1);
				r.length = n.length;
			}
		},
		d(o) {
			o && y(e), Oe(r, o);
		}
	};
}
function ls(t) {
	let e;
	return {
		c() {
			e = z('New Character');
		},
		m(n, r) {
			v(n, e, r);
		},
		p: I,
		d(n) {
			n && y(e);
		}
	};
}
function cs(t) {
	let e = t[13][1].displayName + '',
		n;
	return {
		c() {
			n = z(e);
		},
		m(r, o) {
			v(r, n, o);
		},
		p(r, o) {
			o & 16 && e !== (e = r[13][1].displayName + '') && ie(n, e);
		},
		d(r) {
			r && y(n);
		}
	};
}
function vt(t) {
	let e, n, r, o;
	function l(a, u) {
		return a[13][1].displayName ? cs : ls;
	}
	let c = l(t),
		i = c(t);
	return {
		c() {
			(e = b('option')),
				i.c(),
				(n = N()),
				_(e, 'data-id', (r = t[13][1].accountId)),
				_(e, 'class', 'dark:bg-slate-900'),
				(e.__value = o =
					`
						` +
					t[13][1].displayName +
					`
					`),
				me(e, e.__value);
		},
		m(a, u) {
			v(a, e, u), i.m(e, null), h(e, n);
		},
		p(a, u) {
			c === (c = l(a)) && i ? i.p(a, u) : (i.d(1), (i = c(a)), i && (i.c(), i.m(e, n))),
				u & 16 && r !== (r = a[13][1].accountId) && _(e, 'data-id', r),
				u & 16 &&
					o !==
						(o =
							`
						` +
							a[13][1].displayName +
							`
					`) &&
					((e.__value = o), me(e, e.__value));
		},
		d(a) {
			a && y(e), i.d();
		}
	};
}
function is(t) {
	let e, n, r, o, l, c, i, a, u, d, f, p, m, g, R, T;
	function C(M, O) {
		if (M[4].game == K.osrs) return os;
		if (M[4].game == K.rs3) return rs;
	}
	let L = C(t),
		w = L && L(t),
		S = t[4].account && yt(t);
	return {
		c() {
			(e = b('div')),
				(n = b('img')),
				(o = N()),
				(l = b('button')),
				(l.textContent = 'Play'),
				(c = N()),
				(i = b('div')),
				w && w.c(),
				(a = N()),
				(u = b('div')),
				(d = b('label')),
				(d.textContent = 'Character'),
				(f = N()),
				(p = b('br')),
				(m = N()),
				(g = b('select')),
				S && S.c(),
				Wt(n.src, (r = 'svgs/rocket-solid.svg')) || _(n, 'src', r),
				_(n, 'alt', 'Launch icon'),
				_(
					n,
					'class',
					'mx-auto mb-5 w-24 rounded-3xl bg-gradient-to-br from-rose-500 to-violet-500 p-5'
				),
				_(
					l,
					'class',
					'mx-auto mb-2 w-52 rounded-lg bg-emerald-500 p-2 font-bold text-black duration-200 hover:opacity-75'
				),
				_(i, 'class', 'mx-auto my-2'),
				_(d, 'for', 'character_select'),
				_(d, 'class', 'text-sm'),
				_(g, 'id', 'character_select'),
				_(
					g,
					'class',
					'mx-auto w-52 cursor-pointer rounded-lg border-2 border-slate-300 bg-inherit p-2 text-inherit duration-200 hover:opacity-75 dark:border-slate-800'
				),
				_(u, 'class', 'mx-auto my-2'),
				_(
					e,
					'class',
					'bg-grad flex h-full flex-col border-slate-300 p-5 duration-200 dark:border-slate-800'
				);
		},
		m(M, O) {
			v(M, e, O),
				h(e, n),
				h(e, o),
				h(e, l),
				h(e, c),
				h(e, i),
				w && w.m(i, null),
				h(e, a),
				h(e, u),
				h(u, d),
				h(u, f),
				h(u, p),
				h(u, m),
				h(u, g),
				S && S.m(g, null),
				t[9](g),
				R || ((T = [H(l, 'click', t[6]), H(g, 'change', t[10])]), (R = !0));
		},
		p(M, [O]) {
			L === (L = C(M)) && w
				? w.p(M, O)
				: (w && w.d(1), (w = L && L(M)), w && (w.c(), w.m(i, null))),
				M[4].account
					? S
						? S.p(M, O)
						: ((S = yt(M)), S.c(), S.m(g, null))
					: S && (S.d(1), (S = null));
		},
		i: I,
		o: I,
		d(M) {
			M && y(e), w && w.d(), S && S.d(), t[9](null), (R = !1), re(T);
		}
	};
}
function as(t, e, n) {
	let r, o, l;
	V(t, Y, (T) => n(4, (r = T))), V(t, B, (T) => n(11, (o = T))), V(t, W, (T) => n(12, (l = T)));
	let { showPluginMenu: c = !1 } = e,
		i,
		a;
	function u() {
		var C, L;
		if (!r.account) return;
		const T = i[i.selectedIndex].getAttribute('data-id');
		j(Y, (r.character = r.account.characters.get(T)), r),
			r.character &&
				((L = o.selected_characters) == null ||
					L.set(r.account.userId, (C = r.character) == null ? void 0 : C.accountId)),
			j(W, (l = !0), l);
	}
	function d() {
		a.value == 'RuneLite'
			? (j(Y, (r.client = de.runeLite), r), j(B, (o.selected_client_index = de.runeLite), o))
			: a.value == 'HDOS' &&
				(j(Y, (r.client = de.hdos), r), j(B, (o.selected_client_index = de.hdos), o)),
			j(W, (l = !0), l);
	}
	function f() {
		var T, C, L, w, S, M, O, G, x;
		if (!r.account || !r.character) {
			X('Please log in to launch a client');
			return;
		}
		switch (r.game) {
			case K.osrs:
				r.client == de.runeLite
					? wn(
							(T = r.credentials) == null ? void 0 : T.session_id,
							(C = r.character) == null ? void 0 : C.accountId,
							(L = r.character) == null ? void 0 : L.displayName
						)
					: r.client == de.hdos &&
						vn(
							(w = r.credentials) == null ? void 0 : w.session_id,
							(S = r.character) == null ? void 0 : S.accountId,
							(M = r.character) == null ? void 0 : M.displayName
						);
				break;
			case K.rs3:
				kn(
					(O = r.credentials) == null ? void 0 : O.session_id,
					(G = r.character) == null ? void 0 : G.accountId,
					(x = r.character) == null ? void 0 : x.displayName
				);
				break;
		}
	}
	Qt(() => {
		var T;
		if (
			(r.game == K.osrs && r.client && n(3, (a.selectedIndex = r.client), a),
			r.account && (T = o.selected_characters) != null && T.has(r.account.userId))
		)
			for (let C = 0; C < i.options.length; C++)
				i[C].getAttribute('data-id') == o.selected_characters.get(r.account.userId) &&
					n(2, (i.selectedIndex = C), i);
	}),
		De(() => {
			o.selected_game_index == K.osrs &&
				(n(3, (a.selectedIndex = o.selected_client_index), a),
				j(Y, (r.client = a.selectedIndex), r));
		});
	function p(T) {
		J[T ? 'unshift' : 'push'](() => {
			(a = T), n(3, a);
		});
	}
	const m = () => {
		n(0, (c = Q(ke) ?? !1));
	};
	function g(T) {
		J[T ? 'unshift' : 'push'](() => {
			(i = T), n(2, i);
		});
	}
	const R = () => u();
	return (
		(t.$$set = (T) => {
			'showPluginMenu' in T && n(0, (c = T.showPluginMenu));
		}),
		[c, u, i, a, r, d, f, p, m, g, R]
	);
}
class us extends ce {
	constructor(e) {
		super(), le(this, e, as, is, oe, { showPluginMenu: 0, characterChanged: 1 });
	}
	get characterChanged() {
		return this.$$.ctx[1];
	}
}
function ds(t) {
	let e;
	return {
		c() {
			(e = b('div')),
				(e.innerHTML =
					'<img src="svgs/circle-notch-solid.svg" alt="" class="mx-auto h-1/6 w-1/6 animate-spin"/>'),
				_(
					e,
					'class',
					'container mx-auto bg-slate-100 p-5 text-center text-slate-900 dark:bg-slate-900 dark:text-slate-50'
				);
		},
		m(n, r) {
			v(n, e, r);
		},
		p: I,
		i: I,
		o: I,
		d(n) {
			n && y(e);
		}
	};
}
class fs extends ce {
	constructor(e) {
		super(), le(this, e, null, ds, oe, {});
	}
}
function St(t, e, n) {
	const r = t.slice();
	return (r[23] = e[n][0]), (r[22] = e[n][1]), r;
}
function Lt(t, e, n) {
	const r = t.slice();
	return (r[27] = e[n]), r;
}
function ps(t) {
	let e;
	return {
		c() {
			(e = b('p')), (e.textContent = 'error');
		},
		m(n, r) {
			v(n, e, r);
		},
		p: I,
		d(n) {
			n && y(e);
		}
	};
}
function _s(t) {
	let e;
	function n(l, c) {
		return l[26].length == 0 ? bs : hs;
	}
	let r = n(t),
		o = r(t);
	return {
		c() {
			o.c(), (e = _e());
		},
		m(l, c) {
			o.m(l, c), v(l, e, c);
		},
		p(l, c) {
			r === (r = n(l)) && o
				? o.p(l, c)
				: (o.d(1), (o = r(l)), o && (o.c(), o.m(e.parentNode, e)));
		},
		d(l) {
			l && y(e), o.d(l);
		}
	};
}
function hs(t) {
	let e,
		n = pe(t[26]),
		r = [];
	for (let o = 0; o < n.length; o += 1) r[o] = Ct(Lt(t, n, o));
	return {
		c() {
			for (let o = 0; o < r.length; o += 1) r[o].c();
			e = _e();
		},
		m(o, l) {
			for (let c = 0; c < r.length; c += 1) r[c] && r[c].m(o, l);
			v(o, e, l);
		},
		p(o, l) {
			if (l & 22) {
				n = pe(o[26]);
				let c;
				for (c = 0; c < n.length; c += 1) {
					const i = Lt(o, n, c);
					r[c] ? r[c].p(i, l) : ((r[c] = Ct(i)), r[c].c(), r[c].m(e.parentNode, e));
				}
				for (; c < r.length; c += 1) r[c].d(1);
				r.length = n.length;
			}
		},
		d(o) {
			o && y(e), Oe(r, o);
		}
	};
}
function bs(t) {
	let e;
	return {
		c() {
			(e = b('p')),
				(e.textContent =
					'(start an RS3 game client with plugins enabled and it will be listed here.)');
		},
		m(n, r) {
			v(n, e, r);
		},
		p: I,
		d(n) {
			n && y(e);
		}
	};
}
function Ct(t) {
	let e,
		n = (t[27].identity || Rt) + '',
		r,
		o,
		l,
		c,
		i,
		a;
	function u() {
		return t[14](t[27]);
	}
	return {
		c() {
			(e = b('button')),
				(r = z(n)),
				(l = N()),
				(c = b('br')),
				_(
					e,
					'class',
					(o =
						'm-1 h-[28px] w-[95%] rounded-lg border-2 ' +
						(t[4] && t[1] === t[27].uid
							? 'border-black bg-blue-500 text-black'
							: 'border-blue-500 text-black dark:text-white') +
						' hover:opacity-75')
				);
		},
		m(d, f) {
			v(d, e, f), h(e, r), v(d, l, f), v(d, c, f), i || ((a = H(e, 'click', u)), (i = !0));
		},
		p(d, f) {
			(t = d),
				f & 4 && n !== (n = (t[27].identity || Rt) + '') && ie(r, n),
				f & 22 &&
					o !==
						(o =
							'm-1 h-[28px] w-[95%] rounded-lg border-2 ' +
							(t[4] && t[1] === t[27].uid
								? 'border-black bg-blue-500 text-black'
								: 'border-blue-500 text-black dark:text-white') +
							' hover:opacity-75') &&
					_(e, 'class', o);
		},
		d(d) {
			d && (y(e), y(l), y(c)), (i = !1), a();
		}
	};
}
function gs(t) {
	let e;
	return {
		c() {
			(e = b('p')), (e.textContent = 'loading...');
		},
		m(n, r) {
			v(n, e, r);
		},
		p: I,
		d(n) {
			n && y(e);
		}
	};
}
function ms(t) {
	let e;
	return {
		c() {
			(e = b('p')), (e.textContent = 'error');
		},
		m(n, r) {
			v(n, e, r);
		},
		p: I,
		d(n) {
			n && y(e);
		}
	};
}
function ks(t) {
	let e,
		n,
		r,
		o,
		l,
		c = pe(Object.entries(t[7])),
		i = [];
	for (let f = 0; f < c.length; f += 1) i[f] = xt(St(t, c, f));
	function a(f, p) {
		return f[4] ? ws : ys;
	}
	let u = a(t),
		d = u(t);
	return {
		c() {
			e = b('select');
			for (let f = 0; f < i.length; f += 1) i[f].c();
			(n = N()),
				d.c(),
				(r = _e()),
				_(
					e,
					'class',
					'mx-auto mb-4 w-[min(280px,_45%)] cursor-pointer rounded-lg border-2 border-slate-300 bg-inherit p-2 text-inherit duration-200 hover:opacity-75 dark:border-slate-800'
				),
				t[0] === void 0 && Je(() => t[15].call(e));
		},
		m(f, p) {
			v(f, e, p);
			for (let m = 0; m < i.length; m += 1) i[m] && i[m].m(e, null);
			pt(e, t[0], !0),
				v(f, n, p),
				d.m(f, p),
				v(f, r, p),
				o || ((l = H(e, 'change', t[15])), (o = !0));
		},
		p(f, p) {
			if (p & 128) {
				c = pe(Object.entries(f[7]));
				let m;
				for (m = 0; m < c.length; m += 1) {
					const g = St(f, c, m);
					i[m] ? i[m].p(g, p) : ((i[m] = xt(g)), i[m].c(), i[m].m(e, null));
				}
				for (; m < i.length; m += 1) i[m].d(1);
				i.length = c.length;
			}
			p & 129 && pt(e, f[0]),
				u === (u = a(f)) && d
					? d.p(f, p)
					: (d.d(1), (d = u(f)), d && (d.c(), d.m(r.parentNode, r)));
		},
		d(f) {
			f && (y(e), y(n), y(r)), Oe(i, f), d.d(f), (o = !1), l();
		}
	};
}
function xt(t) {
	let e,
		n = (t[22].name ?? He) + '',
		r,
		o;
	return {
		c() {
			(e = b('option')),
				(r = z(n)),
				_(e, 'class', 'dark:bg-slate-900'),
				(e.__value = o = t[23]),
				me(e, e.__value);
		},
		m(l, c) {
			v(l, e, c), h(e, r);
		},
		p(l, c) {
			c & 128 && n !== (n = (l[22].name ?? He) + '') && ie(r, n),
				c & 128 && o !== (o = l[23]) && ((e.__value = o), me(e, e.__value));
		},
		d(l) {
			l && y(e);
		}
	};
}
function ws(t) {
	let e,
		n,
		r,
		o,
		l = {
			ctx: t,
			current: null,
			token: null,
			hasCatch: !0,
			pending: Rs,
			then: Ss,
			catch: vs,
			value: 22
		};
	return (
		Me((o = t[6]), l),
		{
			c() {
				(e = b('br')), (n = N()), (r = _e()), l.block.c();
			},
			m(c, i) {
				v(c, e, i),
					v(c, n, i),
					v(c, r, i),
					l.block.m(c, (l.anchor = i)),
					(l.mount = () => r.parentNode),
					(l.anchor = r);
			},
			p(c, i) {
				(t = c), (l.ctx = t), (i & 64 && o !== (o = t[6]) && Me(o, l)) || lt(l, t, i);
			},
			d(c) {
				c && (y(e), y(n), y(r)), l.block.d(c), (l.token = null), (l = null);
			}
		}
	);
}
function ys(t) {
	let e, n, r, o, l, c, i, a, u;
	function d(m, g) {
		return (
			g & 128 && (c = null), c == null && (c = Object.entries(m[7]).length !== 0), c ? Ms : Ts
		);
	}
	let f = d(t, -1),
		p = f(t);
	return {
		c() {
			(e = b('button')),
				(n = z('+')),
				(r = N()),
				(o = b('br')),
				(l = N()),
				p.c(),
				(i = _e()),
				_(
					e,
					'class',
					'aspect-square w-8 rounded-lg border-2 border-blue-500 text-[20px] font-bold duration-200 enabled:hover:opacity-75 disabled:border-gray-500'
				),
				(e.disabled = t[3]);
		},
		m(m, g) {
			v(m, e, g),
				h(e, n),
				v(m, r, g),
				v(m, o, g),
				v(m, l, g),
				p.m(m, g),
				v(m, i, g),
				a || ((u = H(e, 'click', t[9])), (a = !0));
		},
		p(m, g) {
			g & 8 && (e.disabled = m[3]),
				f === (f = d(m, g)) && p
					? p.p(m, g)
					: (p.d(1), (p = f(m)), p && (p.c(), p.m(i.parentNode, i)));
		},
		d(m) {
			m && (y(e), y(r), y(o), y(l), y(i)), p.d(m), (a = !1), u();
		}
	};
}
function vs(t) {
	let e;
	return {
		c() {
			(e = b('p')), (e.textContent = 'error');
		},
		m(n, r) {
			v(n, e, r);
		},
		p: I,
		d(n) {
			n && y(e);
		}
	};
}
function Ss(t) {
	let e, n;
	function r(c, i) {
		return (
			i & 193 && (e = null),
			e == null && (e = !!(c[22] && c[22].main && Object.keys(c[7]).includes(c[0]))),
			e ? Cs : Ls
		);
	}
	let o = r(t, -1),
		l = o(t);
	return {
		c() {
			l.c(), (n = _e());
		},
		m(c, i) {
			l.m(c, i), v(c, n, i);
		},
		p(c, i) {
			o === (o = r(c, i)) && l
				? l.p(c, i)
				: (l.d(1), (l = o(c)), l && (l.c(), l.m(n.parentNode, n)));
		},
		d(c) {
			c && y(n), l.d(c);
		}
	};
}
function Ls(t) {
	let e;
	return {
		c() {
			(e = b('p')), (e.textContent = "can't start plugin: does not appear to be configured");
		},
		m(n, r) {
			v(n, e, r);
		},
		p: I,
		d(n) {
			n && y(e);
		}
	};
}
function Cs(t) {
	let e;
	function n(l, c) {
		return l[7][l[0]].path ? Ps : xs;
	}
	let r = n(t),
		o = r(t);
	return {
		c() {
			o.c(), (e = _e());
		},
		m(l, c) {
			o.m(l, c), v(l, e, c);
		},
		p(l, c) {
			r === (r = n(l)) && o
				? o.p(l, c)
				: (o.d(1), (o = r(l)), o && (o.c(), o.m(e.parentNode, e)));
		},
		d(l) {
			l && y(e), o.d(l);
		}
	};
}
function xs(t) {
	let e;
	return {
		c() {
			(e = b('p')), (e.textContent = "can't start plugin: no path is configured");
		},
		m(n, r) {
			v(n, e, r);
		},
		p: I,
		d(n) {
			n && y(e);
		}
	};
}
function Ps(t) {
	let e,
		n,
		r = t[22].name + '',
		o,
		l,
		c;
	function i() {
		return t[18](t[22]);
	}
	return {
		c() {
			(e = b('button')),
				(n = z('Start ')),
				(o = z(r)),
				_(
					e,
					'class',
					'mx-auto mb-1 w-auto rounded-lg bg-emerald-500 p-2 font-bold text-black duration-200 hover:opacity-75'
				);
		},
		m(a, u) {
			v(a, e, u), h(e, n), h(e, o), l || ((c = H(e, 'click', i)), (l = !0));
		},
		p(a, u) {
			(t = a), u & 64 && r !== (r = t[22].name + '') && ie(o, r);
		},
		d(a) {
			a && y(e), (l = !1), c();
		}
	};
}
function Rs(t) {
	let e;
	return {
		c() {
			(e = b('p')), (e.textContent = 'loading...');
		},
		m(n, r) {
			v(n, e, r);
		},
		p: I,
		d(n) {
			n && y(e);
		}
	};
}
function Ts(t) {
	let e;
	return {
		c() {
			(e = b('p')),
				(e.textContent = `You have no plugins installed. Click the + button and select a plugin's
							bolt.json file to add it.`);
		},
		m(n, r) {
			v(n, e, r);
		},
		p: I,
		d(n) {
			n && y(e);
		}
	};
}
function Ms(t) {
	let e = Object.keys(Q(fe)).includes(t[0]) && t[6] !== null,
		n,
		r = e && Pt(t);
	return {
		c() {
			r && r.c(), (n = _e());
		},
		m(o, l) {
			r && r.m(o, l), v(o, n, l);
		},
		p(o, l) {
			l & 65 && (e = Object.keys(Q(fe)).includes(o[0]) && o[6] !== null),
				e
					? r
						? r.p(o, l)
						: ((r = Pt(o)), r.c(), r.m(n.parentNode, n))
					: r && (r.d(1), (r = null));
		},
		d(o) {
			o && y(n), r && r.d(o);
		}
	};
}
function Pt(t) {
	let e,
		n,
		r,
		o,
		l,
		c,
		i,
		a = {
			ctx: t,
			current: null,
			token: null,
			hasCatch: !0,
			pending: Is,
			then: Ns,
			catch: Es,
			value: 22
		};
	return (
		Me((e = t[6]), a),
		{
			c() {
				a.block.c(),
					(n = N()),
					(r = b('button')),
					(r.textContent = 'Remove'),
					(o = N()),
					(l = b('button')),
					(l.textContent = 'Reload'),
					_(
						r,
						'class',
						'mx-auto mb-1 w-[min(144px,_25%)] rounded-lg p-2 font-bold text-black duration-200 enabled:bg-rose-500 enabled:hover:opacity-75 disabled:bg-gray-500'
					),
					_(
						l,
						'class',
						'mx-auto mb-1 w-[min(144px,_25%)] rounded-lg p-2 font-bold text-black duration-200 enabled:bg-blue-500 enabled:hover:opacity-75 disabled:bg-gray-500'
					);
			},
			m(u, d) {
				a.block.m(u, (a.anchor = d)),
					(a.mount = () => n.parentNode),
					(a.anchor = n),
					v(u, n, d),
					v(u, r, d),
					v(u, o, d),
					v(u, l, d),
					c || ((i = [H(r, 'click', t[16]), H(l, 'click', t[17])]), (c = !0));
			},
			p(u, d) {
				(t = u), (a.ctx = t), (d & 64 && e !== (e = t[6]) && Me(e, a)) || lt(a, t, d);
			},
			d(u) {
				u && (y(n), y(r), y(o), y(l)),
					a.block.d(u),
					(a.token = null),
					(a = null),
					(c = !1),
					re(i);
			}
		}
	);
}
function Es(t) {
	let e, n, r;
	return {
		c() {
			(e = b('p')), (e.textContent = 'error'), (n = N()), (r = b('br'));
		},
		m(o, l) {
			v(o, e, l), v(o, n, l), v(o, r, l);
		},
		p: I,
		d(o) {
			o && (y(e), y(n), y(r));
		}
	};
}
function Ns(t) {
	let e,
		n = (t[22].name ?? He) + '',
		r,
		o,
		l,
		c = (t[22].description ?? 'no description') + '',
		i,
		a,
		u,
		d;
	return {
		c() {
			(e = b('p')),
				(r = z(n)),
				(o = N()),
				(l = b('p')),
				(i = z(c)),
				(u = N()),
				(d = b('br')),
				_(e, 'class', 'pb-4 text-xl font-bold'),
				_(l, 'class', (a = t[22].description ? null : 'italic'));
		},
		m(f, p) {
			v(f, e, p), h(e, r), v(f, o, p), v(f, l, p), h(l, i), v(f, u, p), v(f, d, p);
		},
		p(f, p) {
			p & 64 && n !== (n = (f[22].name ?? He) + '') && ie(r, n),
				p & 64 && c !== (c = (f[22].description ?? 'no description') + '') && ie(i, c),
				p & 64 && a !== (a = f[22].description ? null : 'italic') && _(l, 'class', a);
		},
		d(f) {
			f && (y(e), y(o), y(l), y(u), y(d));
		}
	};
}
function Is(t) {
	let e;
	return {
		c() {
			(e = b('p')), (e.textContent = 'loading...');
		},
		m(n, r) {
			v(n, e, r);
		},
		p: I,
		d(n) {
			n && y(e);
		}
	};
}
function js(t) {
	let e, n, r, o, l, c, i, a, u, d, f, p, m, g, R, T, C, L, w;
	(n = new at({})), n.$on('click', t[11]);
	let S = {
		ctx: t,
		current: null,
		token: null,
		hasCatch: !0,
		pending: gs,
		then: _s,
		catch: ps,
		value: 26
	};
	Me((g = t[2]), S);
	function M(x, k) {
		return ke ? ks : ms;
	}
	let G = M()(t);
	return {
		c() {
			(e = b('div')),
				ne(n.$$.fragment),
				(r = N()),
				(o = b('div')),
				(l = b('button')),
				(l.innerHTML = '<img src="svgs/xmark-solid.svg" class="h-5 w-5" alt="Close"/>'),
				(c = N()),
				(i = b('div')),
				(a = b('button')),
				(u = z('Manage Plugins')),
				(f = N()),
				(p = b('hr')),
				(m = N()),
				S.block.c(),
				(R = N()),
				(T = b('div')),
				G.c(),
				_(
					l,
					'class',
					'absolute right-3 top-3 rounded-full bg-rose-500 p-[2px] shadow-lg duration-200 hover:rotate-90 hover:opacity-75'
				),
				_(
					a,
					'class',
					(d =
						'mx-auto mb-2 w-[95%] rounded-lg border-2 ' +
						(t[4]
							? 'border-blue-500 text-black dark:text-white'
							: 'border-black bg-blue-500 text-black') +
						' p-2 font-bold hover:opacity-75')
				),
				_(p, 'class', 'p-1 dark:border-slate-700'),
				_(
					i,
					'class',
					'left-0 float-left h-full w-[min(180px,_50%)] overflow-hidden border-r-2 border-slate-300 pt-2 dark:border-slate-800'
				),
				_(T, 'class', 'h-full pt-10'),
				_(
					o,
					'class',
					'absolute left-[5%] top-[5%] z-20 h-[90%] w-[90%] rounded-lg bg-slate-100 text-center shadow-lg dark:bg-slate-900'
				);
		},
		m(x, k) {
			v(x, e, k),
				ee(n, e, null),
				h(e, r),
				h(e, o),
				h(o, l),
				h(o, c),
				h(o, i),
				h(i, a),
				h(a, u),
				h(i, f),
				h(i, p),
				h(i, m),
				S.block.m(i, (S.anchor = null)),
				(S.mount = () => i),
				(S.anchor = null),
				h(o, R),
				h(o, T),
				G.m(T, null),
				(C = !0),
				L || ((w = [H(l, 'click', t[11]), H(a, 'click', t[13])]), (L = !0));
		},
		p(x, [k]) {
			(t = x),
				(!C ||
					(k & 16 &&
						d !==
							(d =
								'mx-auto mb-2 w-[95%] rounded-lg border-2 ' +
								(t[4]
									? 'border-blue-500 text-black dark:text-white'
									: 'border-black bg-blue-500 text-black') +
								' p-2 font-bold hover:opacity-75'))) &&
					_(a, 'class', d),
				(S.ctx = t),
				(k & 4 && g !== (g = t[2]) && Me(g, S)) || lt(S, t, k),
				G.p(t, k);
		},
		i(x) {
			C || (D(n.$$.fragment, x), (C = !0));
		},
		o(x) {
			U(n.$$.fragment, x), (C = !1);
		},
		d(x) {
			x && y(e), te(n), S.block.d(), (S.token = null), (S = null), G.d(), (L = !1), re(w);
		}
	};
}
const He = '(unnamed)',
	Rt = '(new character)';
function Hs(t, e, n) {
	let r, o, l;
	V(t, et, (k) => n(2, (o = k))), V(t, fe, (k) => n(7, (l = k)));
	let { showPluginMenu: c } = e;
	const i = (k) =>
			new Promise((P, q) => {
				const A = k.concat(k.endsWith('/') ? 'bolt.json' : '/bolt.json');
				var E = new XMLHttpRequest();
				(E.onreadystatechange = () => {
					E.readyState == 4 &&
						(E.status == 200 ? P(JSON.parse(E.responseText)) : q(E.responseText));
				}),
					E.open(
						'GET',
						'/read-json-file?'.concat(new URLSearchParams({ path: A }).toString()),
						!0
					),
					E.send();
			}),
		a = (k) => {
			const q = Q(fe)[k];
			if (!q) return null;
			const A = q.path;
			return A ? i(A) : null;
		},
		u = (k, P) => {
			i(k)
				.then((q) => {
					do n(0, (R = crypto.randomUUID()));
					while (Object.keys(Q(fe)).includes(R));
					j(fe, (l[R] = { name: q.name ?? He, path: k }), l), n(5, (L = !0));
				})
				.catch((q) => {
					console.error(`Config file '${P}' couldn't be fetched, reason: ${q}`);
				});
		};
	let d = !1;
	const f = () => {
		n(3, (d = !0));
		var k = new XMLHttpRequest();
		(k.onreadystatechange = () => {
			if (k.readyState == 4 && (n(3, (d = !1)), k.status == 200)) {
				const P =
					Q(Fe) === 'windows' ? k.responseText.replaceAll('\\', '/') : k.responseText;
				if (P.endsWith('/bolt.json')) {
					const q = P.substring(0, P.length - 9);
					u(q, P);
				} else console.log(`Selection '${P}' is not named bolt.json; ignored`);
			}
		}),
			k.open('GET', '/json-file-picker', !0),
			k.send();
	};
	et.set(Xt());
	const p = (k, P, q, A) => {
			var E = new XMLHttpRequest();
			(E.onreadystatechange = () => {
				E.readyState == 4 && X(`Start-plugin status: ${E.statusText.trim()}`);
			}),
				E.open(
					'GET',
					'/start-plugin?'.concat(
						new URLSearchParams({ client: k, id: P, path: q, main: A }).toString()
					),
					!0
				),
				E.send();
		},
		m = () => {
			d || n(12, (c = !1));
		};
	function g(k) {
		k.key === 'Escape' && m();
	}
	addEventListener('keydown', g);
	var R,
		T = !1,
		C;
	let L = !1;
	rt(() => {
		L && Sn(), removeEventListener('keydown', g);
	});
	const w = () => n(4, (T = !1)),
		S = (k) => {
			n(1, (C = k.uid)), n(4, (T = !0));
		};
	function M() {
		(R = $t(this)), n(0, R);
	}
	const O = () => {
			n(6, (r = null)), n(5, (L = !0));
			let k = Q(fe);
			delete k[R], fe.set(k);
		},
		G = () => n(6, (r = a(R))),
		x = (k) => p(C, R, l[R].path ?? '', k.main ?? '');
	return (
		(t.$$set = (k) => {
			'showPluginMenu' in k && n(12, (c = k.showPluginMenu));
		}),
		(t.$$.update = () => {
			t.$$.dirty & 6 &&
				o.then((k) => {
					k.some((P) => P.uid === C) || n(4, (T = !1));
				}),
				t.$$.dirty & 1 && n(6, (r = a(R)));
		}),
		[R, C, o, d, T, L, r, l, a, f, p, m, c, w, S, M, O, G, x]
	);
}
class Os extends ce {
	constructor(e) {
		super(), le(this, e, Hs, js, oe, { showPluginMenu: 12 });
	}
}
function As(t) {
	let e,
		n,
		r,
		o,
		l,
		c,
		i,
		a,
		u,
		d,
		f,
		p,
		m,
		g,
		R,
		T,
		C = t[1] && Tt(t),
		L = t[0] && Mt(t),
		w = t[3] && Et();
	function S(x) {
		t[6](x);
	}
	let M = {};
	t[0] !== void 0 && (M.showSettings = t[0]),
		(o = new En({ props: M })),
		J.push(() => je(o, 'showSettings', S));
	function O(x) {
		t[7](x);
	}
	let G = {};
	return (
		t[1] !== void 0 && (G.showPluginMenu = t[1]),
		(d = new us({ props: G })),
		J.push(() => je(d, 'showPluginMenu', O)),
		(R = new ss({})),
		{
			c() {
				C && C.c(),
					(e = N()),
					L && L.c(),
					(n = N()),
					w && w.c(),
					(r = N()),
					ne(o.$$.fragment),
					(c = N()),
					(i = b('div')),
					(a = b('div')),
					(u = N()),
					ne(d.$$.fragment),
					(p = N()),
					(m = b('div')),
					(g = N()),
					ne(R.$$.fragment),
					_(i, 'class', 'mt-16 grid h-full grid-flow-col grid-cols-3');
			},
			m(x, k) {
				C && C.m(x, k),
					v(x, e, k),
					L && L.m(x, k),
					v(x, n, k),
					w && w.m(x, k),
					v(x, r, k),
					ee(o, x, k),
					v(x, c, k),
					v(x, i, k),
					h(i, a),
					h(i, u),
					ee(d, i, null),
					h(i, p),
					h(i, m),
					v(x, g, k),
					ee(R, x, k),
					(T = !0);
			},
			p(x, k) {
				x[1]
					? C
						? (C.p(x, k), k & 2 && D(C, 1))
						: ((C = Tt(x)), C.c(), D(C, 1), C.m(e.parentNode, e))
					: C &&
						(ve(),
						U(C, 1, 1, () => {
							C = null;
						}),
						Se()),
					x[0]
						? L
							? (L.p(x, k), k & 1 && D(L, 1))
							: ((L = Mt(x)), L.c(), D(L, 1), L.m(n.parentNode, n))
						: L &&
							(ve(),
							U(L, 1, 1, () => {
								L = null;
							}),
							Se()),
					x[3]
						? w
							? k & 8 && D(w, 1)
							: ((w = Et()), w.c(), D(w, 1), w.m(r.parentNode, r))
						: w &&
							(ve(),
							U(w, 1, 1, () => {
								w = null;
							}),
							Se());
				const P = {};
				!l && k & 1 && ((l = !0), (P.showSettings = x[0]), Ie(() => (l = !1))), o.$set(P);
				const q = {};
				!f && k & 2 && ((f = !0), (q.showPluginMenu = x[1]), Ie(() => (f = !1))), d.$set(q);
			},
			i(x) {
				T ||
					(D(C),
					D(L),
					D(w),
					D(o.$$.fragment, x),
					D(d.$$.fragment, x),
					D(R.$$.fragment, x),
					(T = !0));
			},
			o(x) {
				U(C),
					U(L),
					U(w),
					U(o.$$.fragment, x),
					U(d.$$.fragment, x),
					U(R.$$.fragment, x),
					(T = !1);
			},
			d(x) {
				x && (y(e), y(n), y(r), y(c), y(i), y(g)),
					C && C.d(x),
					L && L.d(x),
					w && w.d(x),
					te(o, x),
					te(d),
					te(R, x);
			}
		}
	);
}
function Ds(t) {
	let e, n;
	return (
		(e = new fs({})),
		{
			c() {
				ne(e.$$.fragment);
			},
			m(r, o) {
				ee(e, r, o), (n = !0);
			},
			p: I,
			i(r) {
				n || (D(e.$$.fragment, r), (n = !0));
			},
			o(r) {
				U(e.$$.fragment, r), (n = !1);
			},
			d(r) {
				te(e, r);
			}
		}
	);
}
function Tt(t) {
	let e, n, r;
	function o(c) {
		t[4](c);
	}
	let l = {};
	return (
		t[1] !== void 0 && (l.showPluginMenu = t[1]),
		(e = new Os({ props: l })),
		J.push(() => je(e, 'showPluginMenu', o)),
		{
			c() {
				ne(e.$$.fragment);
			},
			m(c, i) {
				ee(e, c, i), (r = !0);
			},
			p(c, i) {
				const a = {};
				!n && i & 2 && ((n = !0), (a.showPluginMenu = c[1]), Ie(() => (n = !1))), e.$set(a);
			},
			i(c) {
				r || (D(e.$$.fragment, c), (r = !0));
			},
			o(c) {
				U(e.$$.fragment, c), (r = !1);
			},
			d(c) {
				te(e, c);
			}
		}
	);
}
function Mt(t) {
	let e, n, r;
	function o(c) {
		t[5](c);
	}
	let l = {};
	return (
		t[0] !== void 0 && (l.showSettings = t[0]),
		(e = new Kn({ props: l })),
		J.push(() => je(e, 'showSettings', o)),
		{
			c() {
				ne(e.$$.fragment);
			},
			m(c, i) {
				ee(e, c, i), (r = !0);
			},
			p(c, i) {
				const a = {};
				!n && i & 1 && ((n = !0), (a.showSettings = c[0]), Ie(() => (n = !1))), e.$set(a);
			},
			i(c) {
				r || (D(e.$$.fragment, c), (r = !0));
			},
			o(c) {
				U(e.$$.fragment, c), (r = !1);
			},
			d(c) {
				te(e, c);
			}
		}
	);
}
function Et(t) {
	let e, n;
	return (
		(e = new On({})),
		{
			c() {
				ne(e.$$.fragment);
			},
			m(r, o) {
				ee(e, r, o), (n = !0);
			},
			i(r) {
				n || (D(e.$$.fragment, r), (n = !0));
			},
			o(r) {
				U(e.$$.fragment, r), (n = !1);
			},
			d(r) {
				te(e, r);
			}
		}
	);
}
function qs(t) {
	let e, n, r, o;
	const l = [Ds, As],
		c = [];
	function i(a, u) {
		return a[2] ? 0 : 1;
	}
	return (
		(n = i(t)),
		(r = c[n] = l[n](t)),
		{
			c() {
				(e = b('main')), r.c(), _(e, 'class', 'h-full');
			},
			m(a, u) {
				v(a, e, u), c[n].m(e, null), (o = !0);
			},
			p(a, [u]) {
				let d = n;
				(n = i(a)),
					n === d
						? c[n].p(a, u)
						: (ve(),
							U(c[d], 1, 1, () => {
								c[d] = null;
							}),
							Se(),
							(r = c[n]),
							r ? r.p(a, u) : ((r = c[n] = l[n](a)), r.c()),
							D(r, 1),
							r.m(e, null));
			},
			i(a) {
				o || (D(r), (o = !0));
			},
			o(a) {
				U(r), (o = !1);
			},
			d(a) {
				a && y(e), c[n].d();
			}
		}
	);
}
function Us(t, e, n) {
	let r;
	V(t, Xe, (p) => n(3, (r = p)));
	let o = !1,
		l = !1,
		c = !1;
	fn();
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
	function a(p) {
		(l = p), n(1, l);
	}
	function u(p) {
		(o = p), n(0, o);
	}
	function d(p) {
		(o = p), n(0, o);
	}
	function f(p) {
		(l = p), n(1, l);
	}
	return [o, l, c, r, a, u, d, f];
}
class Bs extends ce {
	constructor(e) {
		super(), le(this, e, Us, qs, oe, {});
	}
}
new Bs({ target: document.getElementById('app') });
const ae = [],
	Js = Q(At);
let Z;
ae.push(ct.subscribe((t) => (Z = t)));
let se;
ae.push(B.subscribe((t) => (se = t)));
ae.push(Fe.subscribe((t) => t));
let ge;
ae.push(ye.subscribe((t) => (ge = t)));
ae.push(ke.subscribe((t) => t ?? !1));
let ue;
ae.push(Ve.subscribe((t) => (ue = t)));
let ut;
ae.push(it.subscribe((t) => (ut = t)));
ae.push(Le.subscribe((t) => t));
ae.push(Ge.subscribe((t) => t));
let ze;
ae.push(Y.subscribe((t) => (ze = t)));
function Gs() {
	var o;
	const t = atob(Z.origin),
		e = atob(Z.clientid),
		n = t.concat('/oauth2/token');
	ge.size == 0 && Xe.set(!0),
		un(),
		se.selected_game_accounts &&
			((o = se.selected_game_accounts) == null ? void 0 : o.size) > 0 &&
			B.update((l) => {
				var c;
				return (
					(l.selected_characters = l.selected_game_accounts),
					(c = l.selected_game_accounts) == null || c.clear(),
					l
				);
			});
	const r = [Js, t, atob(Z.origin_2fa)];
	window.addEventListener('message', (l) => {
		if (!r.includes(l.origin)) {
			X(`discarding window message from origin ${l.origin}`);
			return;
		}
		let c = ue;
		const i = new XMLHttpRequest();
		switch (l.data.type) {
			case 'authCode':
				if (c) {
					Ve.set({});
					const a = new URLSearchParams({
						grant_type: 'authorization_code',
						client_id: atob(Z.clientid),
						code: l.data.code,
						code_verifier: c.verifier,
						redirect_uri: atob(Z.redirect)
					});
					(i.onreadystatechange = () => {
						if (i.readyState == 4)
							if (i.status == 200) {
								const u = Ut(i.response),
									d = Ot(u);
								d
									? _t(c == null ? void 0 : c.win, d).then((f) => {
											f && (ye.update((p) => (p.set(d.sub, d), p)), Ee());
										})
									: (F('Error: invalid credentials received', !1), c.win.close());
							} else
								F(`Error: from ${n}: ${i.status}: ${i.response}`, !1),
									c.win.close();
					}),
						i.open('POST', n, !0),
						i.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded'),
						i.setRequestHeader('Accept', 'application/json'),
						i.send(a);
				}
				break;
			case 'externalUrl':
				(i.onreadystatechange = () => {
					i.readyState == 4 && X(`External URL status: '${i.responseText.trim()}'`);
				}),
					i.open('POST', '/open-external-url', !0),
					i.send(l.data.url);
				break;
			case 'gameSessionServerAuth':
				if (((c = ut.find((a) => l.data.state == a.state)), c)) {
					dn(c, !0);
					const a = l.data.id_token.split('.');
					if (a.length !== 3) {
						F(`Malformed id_token: ${a.length} sections, expected 3`, !1);
						break;
					}
					const u = JSON.parse(atob(a[0]));
					if (u.typ !== 'JWT') {
						F(`Bad id_token header: typ ${u.typ}, expected JWT`, !1);
						break;
					}
					const d = JSON.parse(atob(a[1]));
					if (atob(d.nonce) !== c.nonce) {
						F('Incorrect nonce in id_token', !1);
						break;
					}
					const f = atob(Z.auth_api).concat('/sessions');
					(i.onreadystatechange = () => {
						if (i.readyState == 4)
							if (i.status == 200) {
								const p = atob(Z.auth_api).concat('/accounts');
								(c.creds.session_id = JSON.parse(i.response).sessionId),
									Jt(c.creds, p, c.account_info_promise).then((m) => {
										m &&
											(ye.update((g) => {
												var R;
												return (
													g.set(
														(R = c == null ? void 0 : c.creds) == null
															? void 0
															: R.sub,
														c.creds
													),
													g
												);
											}),
											Ee());
									});
							} else F(`Error: from ${f}: ${i.status}: ${i.response}`, !1);
					}),
						i.open('POST', f, !0),
						i.setRequestHeader('Content-Type', 'application/json'),
						i.setRequestHeader('Accept', 'application/json'),
						i.send(`{"idToken": "${l.data.id_token}"}`);
				}
				break;
			case 'gameClientListUpdate':
				et.set(Xt());
				break;
			default:
				X('Unknown message type: '.concat(l.data.type));
				break;
		}
	}),
		(async () => (
			ge.size > 0 &&
				ge.forEach(async (l) => {
					const c = await qt(l, n, e);
					c !== null &&
						c !== 0 &&
						(F(`Discarding expired login for #${l.sub}`, !1),
						ye.update((a) => (a.delete(l.sub), a)),
						Ee());
					let i;
					if (
						(c === null && (await _t(null, l))
							? (i = { creds: l, valid: !0 })
							: (i = { creds: l, valid: c === 0 }),
						i.valid)
					) {
						const a = l;
						ye.update((u) => (u.set(a.sub, a), u)), Ee();
					}
				}),
			W.set(!1)
		))();
}
function X(t) {
	console.log(t);
	const e = { isError: !1, text: t, time: new Date(Date.now()) };
	Le.update((n) => (n.unshift(e), n));
}
function F(t, e) {
	const n = { isError: !0, text: t, time: new Date(Date.now()) };
	if ((Le.update((r) => (r.unshift(n), r)), !e)) console.error(t);
	else throw new Error(t);
}
ct.set(s());
onload = () => Gs();
onunload = () => {
	for (const t in ae) delete ae[t];
	We();
};
