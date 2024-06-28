import { bolt } from '$lib/State/Bolt';
import { ParseUtils } from '$lib/Util/ParseUtils';
import { StringUtils } from '$lib/Util/StringUtils';
import { unwrap, type Account } from '$lib/Util/interfaces';

export interface Session {
	access_token: string;
	id_token: string;
	refresh_token: string;
	sub: string;
	login_provider: string; // null
	expiry: number;
	session_id: string;
}

interface PendingLogin {
	state: string;
	verifier: string;
	win: Window | null;
}

export interface Auth {
	state?: string;
	nonce?: string;
	creds?: Session;
	win?: Window | null;
	account_info_promise?: Promise<Account>;
	verifier?: string;
}

export class AuthService {
	static pendingLogin: PendingLogin | undefined;
	static pendingOauth: Auth | null = null;
	static pendingGameAuth: Auth[] = [];

	// checks if a login window is already open or not, then launches the window with the necessary parameters
	static async openLoginWindow(origin: string, redirect: string, clientid: string): Promise<void> {
		if (AuthService.pendingLogin?.win?.closed === false) {
			return AuthService.pendingLogin.win.focus();
		}

		const state = StringUtils.makeRandomState();
		const verifier = StringUtils.makeRandomVerifier();
		const url = await StringUtils.makeLoginUrl({
			origin,
			redirect,
			authMethod: '',
			loginType: '',
			clientid,
			flow: 'launcher',
			pkceState: state,
			pkceCodeVerifier: verifier
		});

		const win = window.open(url, '', 'width=480,height=720');
		AuthService.pendingOauth = { state: state, verifier: verifier, win: win };
	}

	// makes a request to the account_info endpoint and returns the promise
	// the promise will return either a JSON object on success or a status code on failure
	static getStandardAccountInfo(creds: Session): Promise<Account | number> {
		return new Promise((resolve) => {
			const url = `${bolt.env.api}/users/${creds.sub}/displayName`;
			const xml = new XMLHttpRequest();
			xml.onreadystatechange = () => {
				if (xml.readyState == 4) {
					if (xml.status == 200) {
						resolve(JSON.parse(xml.response));
					} else {
						resolve(xml.status);
					}
				}
			};
			xml.open('GET', url, true);
			xml.setRequestHeader('Authorization', 'Bearer '.concat(creds.access_token));
			xml.send();
		});
	}

	// Checks if `credentials` are about to expire or have already expired,
	// and renews them using the oauth endpoint if so.
	// Does not save credentials but sets credentials_are_dirty as appropriate.
	// Returns null on success or an http status code on failure
	static async checkRenewCreds(creds: Session, url: string, clientId: string) {
		return new Promise((resolve) => {
			// only renew if less than 30 seconds left
			if (creds.expiry - Date.now() < 30000) {
				const postData = new URLSearchParams({
					grant_type: 'refresh_token',
					client_id: clientId,
					refresh_token: creds.refresh_token
				});
				const xml = new XMLHttpRequest();
				xml.onreadystatechange = () => {
					if (xml.readyState == 4) {
						if (xml.status == 200) {
							const result = ParseUtils.parseCredentials(xml.response);
							const resultCreds = unwrap(result);
							if (resultCreds) {
								creds.access_token = resultCreds.access_token;
								creds.expiry = resultCreds.expiry;
								creds.id_token = resultCreds.id_token;
								creds.login_provider = resultCreds.login_provider;
								creds.refresh_token = resultCreds.refresh_token;
								if (resultCreds.session_id) creds.session_id = resultCreds.session_id;
								creds.sub = resultCreds.sub;
								resolve(null);
							} else {
								resolve(0);
							}
						} else {
							resolve(xml.status);
						}
					}
				};
				xml.onerror = () => {
					resolve(0);
				};
				xml.open('POST', url, true);
				xml.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
				xml.setRequestHeader('Accept', 'application/json');
				xml.send(postData);
			} else {
				resolve(null);
			}
		});
	}
}
