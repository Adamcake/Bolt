export class StringUtils {
	// builds the url to be opened in the login window
	static async makeLoginUrl(url: Record<string, string>) {
		const verifierData = new TextEncoder().encode(url.pkceCodeVerifier);
		const digested = await crypto.subtle.digest('SHA-256', verifierData);
		let raw = '';
		const bytes = new Uint8Array(digested);
		for (let i = 0; i < bytes.byteLength; i++) {
			raw += String.fromCharCode(bytes[i]);
		}
		const codeChallenge = btoa(raw).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
		return url.origin.concat('/oauth2/auth?').concat(
			new URLSearchParams({
				auth_method: url.authMethod,
				login_type: url.loginType,
				flow: url.flow,
				response_type: 'code',
				client_id: url.clientid,
				redirect_uri: url.redirect,
				code_challenge: codeChallenge,
				code_challenge_method: 'S256',
				prompt: 'login',
				scope: 'openid offline gamesso.token.create user.profile.read',
				state: url.pkceState
			}).toString()
		);
	}

	// builds a random PKCE verifier string using crypto.getRandomValues
	static makeRandomVerifier() {
		const t = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~';
		const n = new Uint32Array(43);
		crypto.getRandomValues(n);
		return Array.from(n, function (e) {
			return t[e % t.length];
		}).join('');
	}

	// builds a random PKCE state string using crypto.getRandomValues
	static makeRandomState() {
		const t = 0;
		const r = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz';
		const n = r.length - 1;

		const o = crypto.getRandomValues(new Uint8Array(12));
		return Array.from(o)
			.map((e) => {
				return Math.round((e * (n - t)) / 255 + t);
			})
			.map((e) => {
				return r[e];
			})
			.join('');
	}
}
