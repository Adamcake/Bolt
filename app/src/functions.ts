import { get } from 'svelte/store';
import {
	type Result,
	type Credentials,
	type Account,
	type Auth,
	type Character,
	unwrap,
	type Message
} from './interfaces';
import {
	bolt_sub,
	config_sub,
	credentials_are_dirty,
	credentials_sub,
	err,
	msg,
	pending_game_auth_sub,
	pending_oauth_sub,
	selected_play_sub
} from './main';
import {
	account_list,
	config,
	credentials,
	hdos_installed_version,
	is_config_dirty,
	message_list,
	pending_game_auth,
	pending_oauth,
	platform,
	production_client_id,
	rs3_installed_hash,
	runelite_installed_id,
	selected_play
} from './store';

// deprecated?
// const rs3_basic_auth = 'Basic Y29tX2phZ2V4X2F1dGhfZGVza3RvcF9yczpwdWJsaWM=';
// const osrs_basic_auth = 'Basic Y29tX2phZ2V4X2F1dGhfZGVza3RvcF9vc3JzOnB1YmxpYw==';
let is_flathub: boolean = false;

// after config is loaded, check which theme (light/dark) the user prefers
export function load_theme() {
	if (config_sub.use_dark_theme == false) {
		document.documentElement.classList.remove('dark');
	}
}

// remove an entry by its reference from the pending_game_auth list
// this function assumes this entry is in the list
export function removePendingGameAuth(pending: Auth, close_window: boolean) {
	if (close_window) {
		pending.win!.close();
	}
	pending_game_auth.update((data) => {
		data.splice(pending_game_auth_sub.indexOf(pending), 1);
		return data;
	});
}

// checks if a login window is already open or not, then launches the window with the necessary parameters
export function loginClicked() {
	if (pending_oauth_sub && pending_oauth_sub.win && !pending_oauth_sub.win.closed) {
		pending_oauth_sub.win.focus();
	} else if (
		(pending_oauth_sub && pending_oauth_sub.win && pending_oauth_sub.win.closed) ||
		pending_oauth_sub
	) {
		const state = makeRandomState();
		const verifier = makeRandomVerifier();
		makeLoginUrl({
			origin: atob(bolt_sub.origin),
			redirect: atob(bolt_sub.redirect),
			authMethod: '',
			loginType: '',
			clientid: atob(bolt_sub.clientid),
			flow: 'launcher',
			pkceState: state,
			pkceCodeVerifier: verifier
		}).then((url: URL) => {
			const win = window.open(url, '', 'width=480,height=720');
			pending_oauth.set({ state: state, verifier: verifier, win: win });
		});
	}
}

// queries the url for relevant information, including credentials and config
export function url_search_params(): void {
	const query = new URLSearchParams(window.location.search);
	platform.set(query.get('platform'));
	is_flathub = query.get('flathub') != '0';
	rs3_installed_hash.set(query.get('rs3_linux_installed_hash'));
	runelite_installed_id.set(query.get('runelite_installed_id'));
	hdos_installed_version.set(query.get('hdos_installed_version'));
	const creds = query.get('credentials');
	if (creds) {
		try {
			// no need to set credentials_are_dirty here because the contents came directly from the file
			const creds_array: Array<Credentials> = JSON.parse(creds);
			creds_array.forEach((value, _index) => {
				credentials.update((data) => {
					data.set(value.sub, value);
					return data;
				});
			});
		} catch (error: unknown) {
			err(`Couldn't parse credentials file: ${error}`, false);
		}
	}
	const conf = query.get('config');
	if (conf) {
		try {
			// as above, no need to set configIsDirty
			config.set(JSON.parse(conf));
		} catch (error: unknown) {
			err(`Couldn't parse config file: ${error}`, false);
		}
	}
}

// Checks if `credentials` are about to expire or have already expired,
// and renews them using the oauth endpoint if so.
// Does not save credentials but sets credentials_are_dirty as appropriate.
// Returns null on success or an http status code on failure
export async function checkRenewCreds(creds: Credentials, url: string, client_id: string) {
	return new Promise((resolve, _reject) => {
		// only renew if less than 30 seconds left
		if (creds.expiry - Date.now() < 30000) {
			const post_data = new URLSearchParams({
				grant_type: 'refresh_token',
				client_id: client_id,
				refresh_token: creds.refresh_token
			});
			const xml = new XMLHttpRequest();
			xml.onreadystatechange = () => {
				if (xml.readyState == 4) {
					if (xml.status == 200) {
						const result = parseCredentials(xml.response);
						const creds = unwrap(result);
						if (creds) {
							Object.assign(creds, creds);
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
			xml.send(post_data);
		} else {
			resolve(null);
		}
	});
}

// parses a response from the oauth endpoint
// returns a Result, it may
export function parseCredentials(str: string): Result<Credentials> {
	const oauth_creds = JSON.parse(str);
	const sections = oauth_creds.id_token.split('.');
	if (sections.length !== 3) {
		const err_msg: string = `Malformed id_token: ${sections.length} sections, expected 3`;
		err(err_msg, false);
		return { ok: false, error: new Error(err_msg) };
	}
	const header = JSON.parse(atob(sections[0]));
	if (header.typ !== 'JWT') {
		const err_msg: string = `Bad id_token header: typ ${header.typ}, expected JWT`;
		err(err_msg, false);
		return { ok: false, error: new Error(err_msg) };
	}
	const payload = JSON.parse(atob(sections[1]));
	return {
		ok: true,
		value: {
			access_token: oauth_creds.access_token,
			id_token: oauth_creds.id_token,
			refresh_token: oauth_creds.refresh_token,
			sub: payload.sub,
			login_provider: payload.login_provider || null,
			expiry: Date.now() + oauth_creds.expires_in * 1000,
			session_id: oauth_creds.session_id
		}
	};
}

// builds the url to be opened in the login window
// async because crypto.subtle.digest is async for some reason, so remember to `await`
export async function makeLoginUrl(url: any) {
	const verifier_data = new TextEncoder().encode(url.pkceCodeVerifier);
	const digested = await crypto.subtle.digest('SHA-256', verifier_data);
	let raw = '';
	const bytes = new Uint8Array(digested);
	for (let i = 0; i < bytes.byteLength; i++) {
		raw += String.fromCharCode(bytes[i]);
	}
	const code_challenge = btoa(raw).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
	return url.origin.concat('/oauth2/auth?').concat(
		new URLSearchParams({
			auth_method: url.authMethod,
			login_type: url.loginType,
			flow: url.flow,
			response_type: 'code',
			client_id: url.clientid,
			redirect_uri: url.redirect,
			code_challenge: code_challenge,
			code_challenge_method: 'S256',
			prompt: 'login',
			scope: 'openid offline gamesso.token.create user.profile.read',
			state: url.pkceState
		}).toString()
	);
}

// builds a random PKCE verifier string using crypto.getRandomValues
export function makeRandomVerifier() {
	const t = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~';
	const n = new Uint32Array(43);
	crypto.getRandomValues(n);
	return Array.from(n, function (e) {
		return t[e % t.length];
	}).join('');
}

// builds a random PKCE state string using crypto.getRandomValues
export function makeRandomState() {
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

// Handles a new session id as part of the login flow. Can also be called on startup with a
// persisted session id.
export async function handleNewSessionId(
	creds: Credentials,
	accounts_url: string,
	account_info_promise: Promise<Account>
) {
	return new Promise((resolve, _reject) => {
		const xml = new XMLHttpRequest();
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				if (xml.status == 200) {
					account_info_promise.then((account_info) => {
						if (typeof account_info !== 'number') {
							const account: Account = {
								id: account_info.id,
								userId: account_info.userId,
								displayName: account_info.displayName,
								suffix: account_info.suffix,
								characters: new Map()
							};
							msg(`Successfully added login for ${account.displayName}`);
							JSON.parse(xml.response).forEach((acc: Character) => {
								account.characters.set(acc.accountId, {
									accountId: acc.accountId,
									displayName: acc.displayName,
									userHash: acc.userHash
								});
							});
							addNewAccount(account);
							resolve(true);
						} else {
							err(`Error getting account info: ${account_info}`, false);
							resolve(false);
						}
					});
				} else {
					err(`Error: from ${accounts_url}: ${xml.status}: ${xml.response}`, false);
					resolve(false);
				}
			}
		};
		xml.open('GET', accounts_url, true);
		xml.setRequestHeader('Accept', 'application/json');
		xml.setRequestHeader('Authorization', 'Bearer '.concat(creds.session_id));
		xml.send();
	});
}

// called on new successful login with credentials. Delegates to a specific handler based on login_provider value.
// however it does not save credentials. You should call saveAllCreds after calling this function any number of times.
// Returns true if the credentials should be treated as valid by the caller immediately after return, or false if not.
export async function handleLogin(win: Window | null, creds: Credentials) {
	return await handleStandardLogin(win, creds);
}

// called when login was successful, but with absent or unrecognised login_provider
async function handleStandardLogin(win: Window | null, creds: Credentials) {
	const state = makeRandomState();
	const nonce: string = crypto.randomUUID();
	const location = atob(bolt_sub.origin)
		.concat('/oauth2/auth?')
		.concat(
			new URLSearchParams({
				id_token_hint: creds.id_token,
				nonce: btoa(nonce),
				prompt: 'consent',
				redirect_uri: 'http://localhost',
				response_type: 'id_token code',
				state: state,
				client_id: get(production_client_id),
				scope: 'openid offline'
			}).toString()
		);
	const account_info_promise: Promise<Account | number> = getStandardAccountInfo(creds);

	if (win) {
		win.location.href = location;
		pending_game_auth.update((data) => {
			data.push({
				state: state,
				nonce: nonce,
				creds: creds,
				win: win,
				account_info_promise: <Promise<Account>>account_info_promise
			});
			return data;
		});
		return false;
	} else {
		if (!creds.session_id) {
			err('Rejecting stored credentials with missing session_id', false);
			return false;
		}

		return await handleNewSessionId(
			creds,
			atob(bolt_sub.auth_api).concat('/accounts'),
			<Promise<Account>>account_info_promise
		);
	}
}

// makes a request to the account_info endpoint and returns the promise
// the promise will return either a JSON object on success or a status code on failure
function getStandardAccountInfo(creds: Credentials): Promise<Account | number> {
	return new Promise((resolve, _reject) => {
		const url = `${atob(bolt_sub.api)}/users/${creds.sub}/displayName`;
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

// adds an account to the accounts_list store item
export function addNewAccount(account: Account) {
	account_list.update((data) => {
		data.set(account.userId, account);
		return data;
	});
	selected_play.update((data) => {
		data.account = account;
		const [first_key] = account.characters.keys();
		data.character = account.characters.get(first_key);
		if (credentials_sub.size > 0) data.credentials = credentials_sub.get(account.userId);
		return data;
	});
	pending_oauth.set({});
}

// revokes the given oauth tokens, returning an http status code.
// tokens were revoked only if response is 200
export function revokeOauthCreds(access_token: string, revoke_url: string, client_id: string) {
	return new Promise((resolve, _reject) => {
		const xml = new XMLHttpRequest();
		xml.open('POST', revoke_url, true);
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				resolve(xml.status);
			}
		};
		xml.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
		xml.send(new URLSearchParams({ token: access_token, client_id: client_id }));
	});
}

// sends a request to save all credentials to their config file,
// overwriting the previous file, if any
export async function saveAllCreds() {
	if (credentials_are_dirty) {
		const xml = new XMLHttpRequest();
		xml.open('POST', '/save-credentials', true);
		xml.setRequestHeader('Content-Type', 'application/json');
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				msg(`Save-credentials status: ${xml.responseText.trim()}`);
			}
		};

		selected_play.update((data) => {
			data.credentials = credentials_sub.get(<string>selected_play_sub.account?.userId);
			return data;
		});

		const creds_array: Array<Credentials> = [];
		credentials_sub.forEach((value, key) => {
			creds_array.push(value);
		});
		xml.send(JSON.stringify(creds_array));
	}
}

// asynchronously download and launch RS3's official .deb client using the given env variables
export function launchRS3Linux(
	jx_access_token: string,
	jx_refresh_token: string,
	jx_session_id: string,
	jx_character_id: string,
	jx_display_name: string
) {
	saveConfig();

	const launch = (hash?: any, deb?: any) => {
		const xml = new XMLHttpRequest();
		const params: any = {};
		if (hash) params.hash = hash;
		// if (jx_access_token) params.jx_access_token = jx_access_token; // setting these cause login to fail
		// if (jx_refresh_token) params.jx_refresh_token = jx_refresh_token; // setting these cause login to fail
		params.jx_access_token = '';
		params.jx_refresh_token = '';
		if (jx_session_id) params.jx_session_id = jx_session_id;
		if (jx_character_id) params.jx_character_id = jx_character_id;
		if (jx_display_name) params.jx_display_name = jx_display_name;
		if (config_sub.rs_config_uri) {
			params.config_uri = config_sub.rs_config_uri;
		} else {
			params.config_uri = atob(bolt_sub.default_config_uri);
		}
		xml.open('POST', '/launch-rs3-deb?'.concat(new URLSearchParams(params).toString()), true);
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				msg(`Game launch status: '${xml.responseText.trim()}'`);
				if (xml.status == 200 && hash) {
					rs3_installed_hash.set(hash);
				}
			}
		};
		xml.send(deb);
	};

	const xml = new XMLHttpRequest();
	const content_url = atob(bolt_sub.content_url);
	const url = content_url.concat('dists/trusty/non-free/binary-amd64/Packages');
	xml.open('GET', url, true);
	xml.onreadystatechange = () => {
		if (xml.readyState == 4 && xml.status == 200) {
			const lines = Object.fromEntries(
				xml.response.split('\n').map((x: any) => {
					return x.split(': ');
				})
			);
			if (!lines.Filename || !lines.Size) {
				err(`Could not parse package data from URL: ${url}`, false);
				launch();
				return;
			}
			if (lines.SHA256 !== get(rs3_installed_hash)) {
				message_list.update((data) => {
					data.unshift({ is_error: false, text: 'Downloading RS3 client...' });
					return data;
				});
				const exe_xml = new XMLHttpRequest();
				exe_xml.open('GET', content_url.concat(lines.Filename), true);
				exe_xml.responseType = 'arraybuffer';
				exe_xml.onprogress = (e) => {
					if (e.loaded) {
						message_list.update((data) => {
							data[0] = {
								is_error: false,
								text: `Downloading RS3 client... ${(Math.round((1000.0 * e.loaded) / e.total) / 10.0).toFixed(1)}%`
							};
							return data;
						});
					}
				};
				exe_xml.onreadystatechange = () => {
					if (exe_xml.readyState == 4 && exe_xml.status == 200) {
						launch(lines.SHA256, exe_xml.response);
					}
				};
				exe_xml.onerror = () => {
					err(`Error downloading game client: from ${url}: non-http error`, false);
					launch();
				};
				exe_xml.send();
			} else {
				msg('Latest client is already installed');
				launch();
			}
		}
	};
	xml.onerror = () => {
		err(`Error: from ${url}: non-http error`, false);
		launch();
	};
	xml.send();
}

// locate runelite's .jar either from the user's config or by parsing github releases,
// then attempt to launch it with the given env variables
// last param indices whether --configure will be passed or not
function launchRuneLiteInner(
	jx_access_token: string,
	jx_refresh_token: string,
	jx_session_id: string,
	jx_character_id: string,
	jx_display_name: string,
	configure: boolean
) {
	saveConfig();
	const launch_path = configure ? '/launch-runelite-jar-configure?' : '/launch-runelite-jar?';

	const launch = (id?: any, jar?: any, jar_path?: any) => {
		const xml = new XMLHttpRequest();
		const params: any = {};
		if (id) params.id = id;
		if (jar_path) params.jar_path = jar_path;
		// if (jx_access_token) params.jx_access_token = jx_access_token; // setting these seem to cause login to fail
		// if (jx_refresh_token) params.jx_refresh_token = jx_refresh_token; // setting these seem to cause login to fail
		params.jx_access_token = '';
		params.jx_refresh_token = '';
		if (jx_session_id) params.jx_session_id = jx_session_id;
		if (jx_character_id) params.jx_character_id = jx_character_id;
		if (jx_display_name) params.jx_display_name = jx_display_name;
		if (config_sub.flatpak_rich_presence) params.flatpak_rich_presence = '';
		xml.open(
			jar ? 'POST' : 'GET',
			launch_path.concat(new URLSearchParams(params).toString()),
			true
		);
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				msg(`Game launch status: '${xml.responseText.trim()}'`);
				if (xml.status == 200 && id) {
					runelite_installed_id.set(id);
				}
			}
		};
		xml.send(jar);
	};

	if (config_sub.runelite_use_custom_jar) {
		launch(null, null, config_sub.runelite_custom_jar);
		return;
	}

	const xml = new XMLHttpRequest();
	const url = 'https://api.github.com/repos/runelite/launcher/releases';
	xml.open('GET', url, true);
	xml.onreadystatechange = () => {
		if (xml.readyState == 4) {
			if (xml.status == 200) {
				const runelite = JSON.parse(xml.responseText)
					.map((x: any) => x.assets)
					.flat()
					.find((x: any) => x.name.toLowerCase() == 'runelite.jar');
				if (runelite.id != get(runelite_installed_id)) {
					message_list.update((data: Array<Message>) => {
						data.unshift({ is_error: false, text: 'Downloading RuneLite...' });
						return data;
					});
					const xml_rl = new XMLHttpRequest();
					xml_rl.open('GET', runelite.browser_download_url, true);
					xml_rl.responseType = 'arraybuffer';
					xml_rl.onreadystatechange = () => {
						if (xml_rl.readyState == 4) {
							if (xml_rl.status == 200) {
								launch(runelite.id, xml_rl.response);
							} else {
								err(
									`Error downloading from ${runelite.url}: ${xml_rl.status}: ${xml_rl.responseText}`,
									false
								);
							}
						}
					};
					xml_rl.onprogress = (e) => {
						if (e.loaded && e.lengthComputable) {
							message_list.update((data) => {
								data[0] = {
									is_error: false,
									text: `Downloading RuneLite... ${(Math.round((1000.0 * e.loaded) / e.total) / 10.0).toFixed(1)}%`
								};
								return data;
							});
						}
					};
					xml_rl.send();
				} else {
					msg('Latest JAR is already installed');
					launch();
				}
			} else {
				err(`Error from ${url}: ${xml.status}: ${xml.responseText}`, false);
			}
		}
	};
	xml.send();
}

export function launchRuneLite(
	jx_access_token: string,
	jx_refresh_token: string,
	jx_session_id: string,
	jx_character_id: string,
	jx_display_name: string
) {
	return launchRuneLiteInner(
		jx_access_token,
		jx_refresh_token,
		jx_session_id,
		jx_character_id,
		jx_display_name,
		false
	);
}

export function launchRuneLiteConfigure(
	jx_access_token: string,
	jx_refresh_token: string,
	jx_session_id: string,
	jx_character_id: string,
	jx_display_name: string
) {
	return launchRuneLiteInner(
		jx_access_token,
		jx_refresh_token,
		jx_session_id,
		jx_character_id,
		jx_display_name,
		true
	);
}

// locate hdos's .jar from their CDN, then attempt to launch it with the given env variables
export function launchHdos(
	jx_access_token: string,
	jx_refresh_token: string,
	jx_session_id: string,
	jx_character_id: string,
	jx_display_name: string
) {
	saveConfig();

	const launch = (version?: any, jar?: any) => {
		const xml = new XMLHttpRequest();
		const params: any = {};
		if (version) params.version = version;
		// if (jx_access_token) params.jx_access_token = jx_access_token; // setting these cause login to fail
		// if (jx_refresh_token) params.jx_refresh_token = jx_refresh_token; // setting these cause login to fail
		params.jx_access_token = '';
		params.jx_refresh_token = '';
		if (jx_session_id) params.jx_session_id = jx_session_id;
		if (jx_character_id) params.jx_character_id = jx_character_id;
		if (jx_display_name) params.jx_display_name = jx_display_name;
		xml.open('POST', '/launch-hdos-jar?'.concat(new URLSearchParams(params).toString()), true);
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				msg(`Game launch status: '${xml.responseText.trim()}'`);
				if (xml.status == 200 && version) {
					hdos_installed_version.set(version);
				}
			}
		};
		xml.send(jar);
	};

	const xml = new XMLHttpRequest();
	const url = 'https://cdn.hdos.dev/client/getdown.txt';
	xml.open('GET', url, true);
	xml.onreadystatechange = () => {
		if (xml.readyState == 4) {
			if (xml.status == 200) {
				const version_regex: RegExpMatchArray | null = xml.responseText.match(
					/^launcher\.version *= *(.*?)$/m
				);
				if (version_regex && version_regex.length >= 2) {
					const latest_version = version_regex[1];
					if (latest_version !== get(hdos_installed_version)) {
						const jar_url = `https://cdn.hdos.dev/launcher/v${latest_version}/hdos-launcher.jar`;
						message_list.update((data) => {
							data.unshift({ is_error: false, text: 'Downloading HDOS...' });
							return data;
						});
						const xml_hdos = new XMLHttpRequest();
						xml_hdos.open('GET', jar_url, true);
						xml_hdos.responseType = 'arraybuffer';
						xml_hdos.onreadystatechange = () => {
							if (xml_hdos.readyState == 4) {
								if (xml_hdos.status == 200) {
									launch(latest_version, xml_hdos.response);
								} else {
									const runelite = JSON.parse(xml.responseText)
										.map((x: any) => x.assets)
										.flat()
										.find((x: any) => x.name.toLowerCase() == 'runelite.jar');
									err(
										`Error downloading from ${runelite.url}: ${xml_hdos.status}: ${xml_hdos.responseText}`,
										false
									);
								}
							}
						};
						xml_hdos.onprogress = (e) => {
							if (e.loaded && e.lengthComputable) {
								message_list.update((data) => {
									data[0] = {
										is_error: false,
										text: `Downloading HDOS... ${(Math.round((1000.0 * e.loaded) / e.total) / 10.0).toFixed(1)}%`
									};
									return data;
								});
							}
						};
						xml_hdos.send();
					} else {
						msg('Latest JAR is already installed');
						launch();
					}
				} else {
					msg("Couldn't parse latest launcher version");
					launch();
				}
			} else {
				err(`Error from ${url}: ${xml.status}: ${xml.responseText}`, false);
			}
		}
	};
	xml.send();
}

// sends an asynchronous request to save the current user config to disk, if it has changed
let save_config_in_progess: boolean = false;
export function saveConfig() {
	if (get(is_config_dirty) && !save_config_in_progess) {
		save_config_in_progess = true;
		const xml = new XMLHttpRequest();
		xml.open('POST', '/save-config', true);
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				msg(`Save config status: '${xml.responseText.trim()}'`);
				if (xml.status == 200) {
					is_config_dirty.set(false);
				}
				save_config_in_progess = false;
			}
		};
		xml.setRequestHeader('Content-Type', 'application/json');
		xml.send(JSON.stringify(config_sub, null, 4));
	}
}
