import { Time } from '$lib/Enums/Time';
import { CookieService } from '$lib/Services/CookieService';
import { bolt } from '$lib/State/Bolt';
import { ParseUtils } from '$lib/Util/ParseUtils';
import { StringUtils } from '$lib/Util/StringUtils';
import { error, ok, type Result } from '$lib/Util/interfaces';

export interface AuthTokens {
	access_token: string;
	id_token: string;
	refresh_token: string;
	sub: string; // equivalent to userId on the User object
	expiry: number;
}

export class AuthService {
	static pendingLoginWindow: Window | null = null;

	static async openLoginWindow(origin: string, redirect: string, clientid: string): Promise<void> {
		if (AuthService.pendingLoginWindow !== null) {
			return AuthService.pendingLoginWindow.window.focus();
		}

		const state = StringUtils.makeRandomState();
		const verifier = StringUtils.makeRandomVerifier();
		const verifierData = new TextEncoder().encode(verifier);
		const digested = await crypto.subtle.digest('SHA-256', verifierData);
		let raw = '';
		const bytes = new Uint8Array(digested);
		for (let i = 0; i < bytes.byteLength; i++) {
			raw += String.fromCharCode(bytes[i]);
		}
		const codeChallenge = btoa(raw).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
		const params = new URLSearchParams({
			auth_method: '',
			login_type: '',
			flow: 'launcher',
			response_type: 'code',
			client_id: clientid,
			redirect_uri: redirect,
			code_challenge: codeChallenge,
			code_challenge_method: 'S256',
			prompt: 'login',
			scope: 'openid offline gamesso.token.create user.profile.read',
			state: state
		});

		const loginUrl = `${origin}/oauth2/auth?${params.toString()}`;
		// Give popup window access to the verifier code
		CookieService.set('auth_verifier', verifier, 10, Time.MINUTE);
		AuthService.pendingLoginWindow = window.open(loginUrl, '', 'width=480,height=720');
	}

	static navigateToAuthConsent(origin: string, id_token: string, nonce: string) {
		const state = StringUtils.makeRandomState();
		const params = new URLSearchParams({
			id_token_hint: id_token,
			nonce: nonce,
			prompt: 'consent',
			redirect_uri: 'http://localhost',
			response_type: 'id_token code',
			state: state,
			client_id: '1fddee4e-b100-4f4e-b2b0-097f9088f9d2',
			scope: 'openid offline'
		});
		const consentUrl = `${origin}/oauth2/auth?${params.toString()}`;
		CookieService.set('auth_nonce', nonce, 10, Time.MINUTE);
		window.location.href = consentUrl;
	}

	/**
	 * Checks if `authTokens` is about to expire or has already expired.
	 * If expired, return the new set of AuthTokens.
	 * If expired, but fails to refresh, return the HTTP status code of the request
	 * 0 means the request failed for an unknown reason (likely bad internet connection)
	 * Any other code means it should be removed from disk, as it is no longer valid.
	 */
	static async refreshOAuthToken(authTokens: AuthTokens): Promise<Result<AuthTokens, number>> {
		return new Promise((resolve) => {
			// only renew if less than 30 seconds left
			if (authTokens.expiry - Date.now() < 30000) {
				const postData = new URLSearchParams({
					grant_type: 'refresh_token',
					client_id: bolt.env.clientid,
					refresh_token: authTokens.refresh_token
				});
				const xml = new XMLHttpRequest();
				xml.onreadystatechange = () => {
					if (xml.readyState == 4) {
						if (xml.status == 200) {
							const credentialResult = ParseUtils.parseTokenResponse(xml.response);
							if (credentialResult.ok) {
								resolve(ok(credentialResult.value));
							} else {
								resolve(error(0));
							}
						} else {
							resolve(error(xml.status));
						}
					}
				};
				xml.onerror = () => {
					resolve(error(0));
				};
				const url = bolt.env.origin.concat('/oauth2/token');
				xml.open('POST', url, true);
				xml.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
				xml.setRequestHeader('Accept', 'application/json');
				xml.send(postData);
			} else {
				resolve(ok(authTokens));
			}
		});
	}

	static async getOAuthToken(
		origin: string,
		client_id: string,
		code_verifier: string,
		redirect_uri: string,
		authCode: string
	): Promise<Result<AuthTokens, string>> {
		const tokenUrl = `${origin}/oauth2/token`;
		return new Promise((resolve) => {
			const xml = new XMLHttpRequest();
			xml.onreadystatechange = () => {
				if (xml.readyState == 4) {
					if (xml.status == 200) {
						const parseResult = ParseUtils.parseTokenResponse(xml.response);
						if (parseResult.ok) {
							return resolve(ok(parseResult.value));
						} else {
							return resolve(error(parseResult.error));
						}
					} else {
						return resolve(error(`Error: from ${tokenUrl}: ${xml.status}: ${xml.response}`));
					}
				}
			};
			xml.open('POST', tokenUrl, true);
			xml.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
			xml.setRequestHeader('Accept', 'application/json');
			const post_data = new URLSearchParams({
				grant_type: 'authorization_code',
				client_id,
				code: authCode,
				code_verifier,
				redirect_uri
			});
			xml.send(post_data);
		});
	}

	static revokeOauthCreds(access_token: string) {
		const revokeUrl = `${bolt.env.origin}/oauth2/revoke`;
		return new Promise((resolve) => {
			const xml = new XMLHttpRequest();
			xml.open('POST', revokeUrl, true);
			xml.onreadystatechange = () => {
				if (xml.readyState == 4) {
					resolve(xml.status);
				}
			};
			xml.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
			const post_data = new URLSearchParams({ token: access_token, client_id: bolt.env.clientid });
			xml.send(post_data);
		});
	}

	static async getSessionId(auth_api: string, idToken: string): Promise<Result<string, string>> {
		const sessionsUrl = `${auth_api}/sessions`;
		return new Promise((resolve) => {
			const xml = new XMLHttpRequest();
			xml.onreadystatechange = () => {
				if (xml.readyState == 4) {
					if (xml.status == 200) {
						const parseResult = ParseUtils.parseSessionResponse(xml.response);
						if (parseResult.ok) {
							return resolve(ok(parseResult.value));
						} else {
							return resolve(error(parseResult.error));
						}
					} else {
						error(`Error: from ${sessionsUrl}: ${xml.status}: ${xml.response}`);
					}
				}
			};
			xml.open('POST', sessionsUrl, true);
			xml.setRequestHeader('Content-Type', 'application/json');
			xml.setRequestHeader('Accept', 'application/json');
			const params = {
				idToken: idToken
			};
			xml.send(JSON.stringify(params));
		});
	}

	static validateIdToken(token: string, expectedNonce: string): Result<undefined, string> {
		const sections = token.split('.');
		if (sections.length !== 3) {
			return error(`Malformed id_token: ${sections.length} sections, expected 3`);
		}
		const header = JSON.parse(atob(sections[0]));
		if (header.typ !== 'JWT') {
			return error(`Bad id_token header: typ ${header.typ}, expected JWT`);
		}
		const payload = JSON.parse(atob(sections[1]));
		if (payload.nonce !== expectedNonce) {
			return error('Incorrect nonce in id_token');
		}

		return ok(undefined);
	}
}
