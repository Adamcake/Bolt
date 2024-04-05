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
	boltSub,
	configSub,
	credentialsSub,
	err,
	msg,
	pendingGameAuthSub,
	pendingOauthSub,
	selectedPlaySub
} from './main';
import {
	accountList,
	config,
	credentials,
	hdosInstalledVersion,
	isConfigDirty,
	messageList,
	pendingGameAuth,
	pendingOauth,
	platform,
	productionClientId,
	rs3InstalledHash,
	runeLiteInstalledId,
	selectedPlay
} from './store';

// deprecated?
// const rs3_basic_auth = 'Basic Y29tX2phZ2V4X2F1dGhfZGVza3RvcF9yczpwdWJsaWM=';
// const osrs_basic_auth = 'Basic Y29tX2phZ2V4X2F1dGhfZGVza3RvcF9vc3JzOnB1YmxpYw==';
let isFlathub: boolean = false;

// after config is loaded, check which theme (light/dark) the user prefers
export function loadTheme() {
	if (configSub.use_dark_theme == false) {
		document.documentElement.classList.remove('dark');
	}
}

// remove an entry by its reference from the pending_game_auth list
// this function assumes this entry is in the list
export function removePendingGameAuth(pending: Auth, closeWindow: boolean) {
	if (closeWindow) {
		pending.win!.close();
	}
	pendingGameAuth.update((data) => {
		data.splice(pendingGameAuthSub.indexOf(pending), 1);
		return data;
	});
}

// checks if a login window is already open or not, then launches the window with the necessary parameters
export function loginClicked() {
	if (pendingOauthSub && pendingOauthSub.win && !pendingOauthSub.win.closed) {
		pendingOauthSub.win.focus();
	} else if (
		(pendingOauthSub && pendingOauthSub.win && pendingOauthSub.win.closed) ||
		pendingOauthSub
	) {
		const state = makeRandomState();
		const verifier = makeRandomVerifier();
		makeLoginUrl({
			origin: atob(boltSub.origin),
			redirect: atob(boltSub.redirect),
			authMethod: '',
			loginType: '',
			clientid: atob(boltSub.clientid),
			flow: 'launcher',
			pkceState: state,
			pkceCodeVerifier: verifier
		}).then((url: string) => {
			const win = window.open(url, '', 'width=480,height=720');
			pendingOauth.set({ state: state, verifier: verifier, win: win });
		});
	}
}

// queries the url for relevant information, including credentials and config
export function urlSearchParams(): void {
	const query = new URLSearchParams(window.location.search);
	platform.set(query.get('platform'));
	isFlathub = query.get('flathub') != '0';
	rs3InstalledHash.set(query.get('rs3_linux_installed_hash'));
	runeLiteInstalledId.set(query.get('runelite_installed_id'));
	hdosInstalledVersion.set(query.get('hdos_installed_version'));
	const creds = query.get('credentials');
	if (creds) {
		try {
			// no need to set credentials_are_dirty here because the contents came directly from the file
			const credsList: Array<Credentials> = JSON.parse(creds);
			credsList.forEach((value) => {
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
			const parsedConf = JSON.parse(conf);
			config.set(parsedConf);
			// convert parsed objects into Maps
			config.update((data) => {
				if (data.selected_game_accounts) {
					data.selected_characters = new Map(Object.entries(data.selected_game_accounts));
					delete data.selected_game_accounts;
				} else if (data.selected_characters) {
					data.selected_characters = new Map(Object.entries(data.selected_characters));
				}
				return data;
			});
		} catch (error: unknown) {
			err(`Couldn't parse config file: ${error}`, false);
		}
	}
}

// Checks if `credentials` are about to expire or have already expired,
// and renews them using the oauth endpoint if so.
// Does not save credentials but sets credentials_are_dirty as appropriate.
// Returns null on success or an http status code on failure
export async function checkRenewCreds(creds: Credentials, url: string, clientId: string) {
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
						const result = parseCredentials(xml.response);
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

// parses a response from the oauth endpoint
// returns a Result, it may
export function parseCredentials(str: string): Result<Credentials> {
	const oauthCreds = JSON.parse(str);
	const sections = oauthCreds.id_token.split('.');
	if (sections.length !== 3) {
		const errMsg: string = `Malformed id_token: ${sections.length} sections, expected 3`;
		err(errMsg, false);
		return { ok: false, error: new Error(errMsg) };
	}
	const header = JSON.parse(atob(sections[0]));
	if (header.typ !== 'JWT') {
		const errMsg: string = `Bad id_token header: typ ${header.typ}, expected JWT`;
		err(errMsg, false);
		return { ok: false, error: new Error(errMsg) };
	}
	const payload = JSON.parse(atob(sections[1]));
	return {
		ok: true,
		value: {
			access_token: oauthCreds.access_token,
			id_token: oauthCreds.id_token,
			refresh_token: oauthCreds.refresh_token,
			sub: payload.sub,
			login_provider: payload.login_provider || null,
			expiry: Date.now() + oauthCreds.expires_in * 1000,
			session_id: oauthCreds.session_id
		}
	};
}

// builds the url to be opened in the login window
// async because crypto.subtle.digest is async for some reason, so remember to `await`
export async function makeLoginUrl(url: Record<string, string>) {
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
	accountsUrl: string,
	accountsInfoPromise: Promise<Account>
) {
	return new Promise((resolve) => {
		const xml = new XMLHttpRequest();
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				if (xml.status == 200) {
					accountsInfoPromise.then((accountInfo) => {
						if (typeof accountInfo !== 'number') {
							const account: Account = {
								id: accountInfo.id,
								userId: accountInfo.userId,
								displayName: accountInfo.displayName,
								suffix: accountInfo.suffix,
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
							err(`Error getting account info: ${accountInfo}`, false);
							resolve(false);
						}
					});
				} else {
					err(`Error: from ${accountsUrl}: ${xml.status}: ${xml.response}`, false);
					resolve(false);
				}
			}
		};
		xml.open('GET', accountsUrl, true);
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
	const location = atob(boltSub.origin)
		.concat('/oauth2/auth?')
		.concat(
			new URLSearchParams({
				id_token_hint: creds.id_token,
				nonce: btoa(nonce),
				prompt: 'consent',
				redirect_uri: 'http://localhost',
				response_type: 'id_token code',
				state: state,
				client_id: get(productionClientId),
				scope: 'openid offline'
			}).toString()
		);
	const accountInfoPromise: Promise<Account | number> = getStandardAccountInfo(creds);

	if (win) {
		win.location.href = location;
		pendingGameAuth.update((data) => {
			data.push({
				state: state,
				nonce: nonce,
				creds: creds,
				win: win,
				account_info_promise: <Promise<Account>>accountInfoPromise
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
			atob(boltSub.auth_api).concat('/accounts'),
			<Promise<Account>>accountInfoPromise
		);
	}
}

// makes a request to the account_info endpoint and returns the promise
// the promise will return either a JSON object on success or a status code on failure
function getStandardAccountInfo(creds: Credentials): Promise<Account | number> {
	return new Promise((resolve) => {
		const url = `${atob(boltSub.api)}/users/${creds.sub}/displayName`;
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
	const updateSelectedPlay = () => {
		selectedPlay.update((data) => {
			data.account = account;
			const [firstKey] = account.characters.keys();
			data.character = account.characters.get(firstKey);
			if (credentialsSub.size > 0) data.credentials = credentialsSub.get(account.userId);
			return data;
		});
	};

	accountList.update((data) => {
		data.set(account.userId, account);
		return data;
	});

	if (selectedPlaySub.account && configSub.selected_account) {
		if (account.userId == configSub.selected_account) {
			updateSelectedPlay();
		}
	} else if (!selectedPlaySub.account) {
		updateSelectedPlay();
	}
	pendingOauth.set({});
}

// revokes the given oauth tokens, returning an http status code.
// tokens were revoked only if response is 200
export function revokeOauthCreds(accessToken: string, revokeUrl: string, clientId: string) {
	return new Promise((resolve) => {
		const xml = new XMLHttpRequest();
		xml.open('POST', revokeUrl, true);
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				resolve(xml.status);
			}
		};
		xml.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
		xml.send(new URLSearchParams({ token: accessToken, client_id: clientId }));
	});
}

// sends a request to save all credentials to their config file,
// overwriting the previous file, if any
export async function saveAllCreds() {
	const xml = new XMLHttpRequest();
	xml.open('POST', '/save-credentials', true);
	xml.setRequestHeader('Content-Type', 'application/json');
	xml.onreadystatechange = () => {
		if (xml.readyState == 4) {
			msg(`Save-credentials status: ${xml.responseText.trim()}`);
		}
	};

	selectedPlay.update((data) => {
		data.credentials = credentialsSub.get(<string>selectedPlaySub.account?.userId);
		return data;
	});

	const credsList: Array<Credentials> = [];
	credentialsSub.forEach((value) => {
		credsList.push(value);
	});
	xml.send(JSON.stringify(credsList));
}

// asynchronously download and launch RS3's official .deb client using the given env variables
export function launchRS3Linux(
	jx_session_id: string,
	jx_character_id: string,
	jx_display_name: string
) {
	saveConfig();

	const launch = (hash?: unknown, deb?: never) => {
		const xml = new XMLHttpRequest();
		const params: Record<string, string> = {};
		if (hash) params.hash = <string>hash;
		if (jx_session_id) params.jx_session_id = jx_session_id;
		if (jx_character_id) params.jx_character_id = jx_character_id;
		if (jx_display_name) params.jx_display_name = jx_display_name;
		if (configSub.rs_config_uri) {
			params.config_uri = configSub.rs_config_uri;
		} else {
			params.config_uri = atob(boltSub.default_config_uri);
		}
		xml.open('POST', '/launch-rs3-deb?'.concat(new URLSearchParams(params).toString()), true);
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				msg(`Game launch status: '${xml.responseText.trim()}'`);
				if (xml.status == 200 && hash) {
					rs3InstalledHash.set(<string>hash);
				}
			}
		};
		xml.send(deb);
	};

	const xml = new XMLHttpRequest();
	const contentUrl = atob(boltSub.content_url);
	const url = contentUrl.concat('dists/trusty/non-free/binary-amd64/Packages');
	xml.open('GET', url, true);
	xml.onreadystatechange = () => {
		if (xml.readyState == 4 && xml.status == 200) {
			const lines = Object.fromEntries(
				xml.response.split('\n').map((x: string) => {
					return x.split(': ');
				})
			);
			if (!lines.Filename || !lines.Size) {
				err(`Could not parse package data from URL: ${url}`, false);
				launch();
				return;
			}
			if (lines.SHA256 !== get(rs3InstalledHash)) {
				msg('Downloading RS3 client...');
				const exeXml = new XMLHttpRequest();
				exeXml.open('GET', contentUrl.concat(lines.Filename), true);
				exeXml.responseType = 'arraybuffer';
				exeXml.onprogress = (e) => {
					if (e.loaded) {
						messageList.update((data) => {
							data[0].text = `Downloading RS3 client... ${(Math.round((1000.0 * e.loaded) / e.total) / 10.0).toFixed(1)}%`;
							return data;
						});
					}
				};
				exeXml.onreadystatechange = () => {
					if (exeXml.readyState == 4 && exeXml.status == 200) {
						launch(lines.SHA256, exeXml.response);
					}
				};
				exeXml.onerror = () => {
					err(`Error downloading game client: from ${url}: non-http error`, false);
					launch();
				};
				exeXml.send();
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
	jx_session_id: string,
	jx_character_id: string,
	jx_display_name: string,
	configure: boolean
) {
	saveConfig();
	const launchPath = configure ? '/launch-runelite-jar-configure?' : '/launch-runelite-jar?';

	const launch = (id?: unknown, jar?: unknown, jarPath?: unknown) => {
		const xml = new XMLHttpRequest();
		const params: Record<string, string> = {};
		if (id) params.id = <string>id;
		if (jarPath) params.jar_path = <string>jarPath;
		if (jx_session_id) params.jx_session_id = jx_session_id;
		if (jx_character_id) params.jx_character_id = jx_character_id;
		if (jx_display_name) params.jx_display_name = jx_display_name;
		if (configSub.flatpak_rich_presence) params.flatpak_rich_presence = '';
		xml.open(
			jar ? 'POST' : 'GET',
			launchPath.concat(new URLSearchParams(params).toString()),
			true
		);
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				msg(`Game launch status: '${xml.responseText.trim()}'`);
				if (xml.status == 200 && id) {
					runeLiteInstalledId.set(<string>id);
				}
			}
		};
		xml.send(<string>jar);
	};

	if (configSub.runelite_use_custom_jar) {
		launch(null, null, configSub.runelite_custom_jar);
		return;
	}

	const xml = new XMLHttpRequest();
	const url = 'https://api.github.com/repos/runelite/launcher/releases';
	xml.open('GET', url, true);
	xml.onreadystatechange = () => {
		if (xml.readyState == 4) {
			if (xml.status == 200) {
				const runelite = JSON.parse(xml.responseText)
					.map((x: Record<string, string>) => x.assets)
					.flat()
					.find((x: Record<string, string>) => x.name.toLowerCase() == 'runelite.jar');
				if (runelite.id != get(runeLiteInstalledId)) {
					msg('Downloading RuneLite...');
					const xmlRl = new XMLHttpRequest();
					xmlRl.open('GET', runelite.browser_download_url, true);
					xmlRl.responseType = 'arraybuffer';
					xmlRl.onreadystatechange = () => {
						if (xmlRl.readyState == 4) {
							if (xmlRl.status == 200) {
								launch(runelite.id, xmlRl.response);
							} else {
								err(
									`Error downloading from ${runelite.url}: ${xmlRl.status}: ${xmlRl.responseText}`,
									false
								);
							}
						}
					};
					xmlRl.onprogress = (e) => {
						if (e.loaded && e.lengthComputable) {
							messageList.update((data) => {
								data[0].text = `Downloading RuneLite... ${(Math.round((1000.0 * e.loaded) / e.total) / 10.0).toFixed(1)}%`;
								return data;
							});
						}
					};
					xmlRl.send();
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
	jx_session_id: string,
	jx_character_id: string,
	jx_display_name: string
) {
	return launchRuneLiteInner(jx_session_id, jx_character_id, jx_display_name, false);
}

export function launchRuneLiteConfigure(
	jx_session_id: string,
	jx_character_id: string,
	jx_display_name: string
) {
	return launchRuneLiteInner(jx_session_id, jx_character_id, jx_display_name, true);
}

// locate hdos's .jar from their CDN, then attempt to launch it with the given env variables
export function launchHdos(
	jx_session_id: string,
	jx_character_id: string,
	jx_display_name: string
) {
	saveConfig();

	const launch = (version?: string, jar?: string) => {
		const xml = new XMLHttpRequest();
		const params: Record<string, string> = {};
		if (version) params.version = version;
		if (jx_session_id) params.jx_session_id = jx_session_id;
		if (jx_character_id) params.jx_character_id = jx_character_id;
		if (jx_display_name) params.jx_display_name = jx_display_name;
		xml.open('POST', '/launch-hdos-jar?'.concat(new URLSearchParams(params).toString()), true);
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				msg(`Game launch status: '${xml.responseText.trim()}'`);
				if (xml.status == 200 && version) {
					hdosInstalledVersion.set(version);
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
				const versionRegex: RegExpMatchArray | null = xml.responseText.match(
					/^launcher\.version *= *(.*?)$/m
				);
				if (versionRegex && versionRegex.length >= 2) {
					const latestVersion = versionRegex[1];
					if (latestVersion !== get(hdosInstalledVersion)) {
						const jarUrl = `https://cdn.hdos.dev/launcher/v${latestVersion}/hdos-launcher.jar`;
						msg('Downloading HDOS...');
						const xmlHdos = new XMLHttpRequest();
						xmlHdos.open('GET', jarUrl, true);
						xmlHdos.responseType = 'arraybuffer';
						xmlHdos.onreadystatechange = () => {
							if (xmlHdos.readyState == 4) {
								if (xmlHdos.status == 200) {
									launch(latestVersion, xmlHdos.response);
								} else {
									const runelite = JSON.parse(xml.responseText)
										.map((x: Record<string, string>) => x.assets)
										.flat()
										.find(
											(x: Record<string, string>) =>
												x.name.toLowerCase() == 'runelite.jar'
										);
									err(
										`Error downloading from ${runelite.url}: ${xmlHdos.status}: ${xmlHdos.responseText}`,
										false
									);
								}
							}
						};
						xmlHdos.onprogress = (e) => {
							if (e.loaded && e.lengthComputable) {
								messageList.update((data) => {
									data[0].text = `Downloading HDOS... ${(Math.round((1000.0 * e.loaded) / e.total) / 10.0).toFixed(1)}%`;
									return data;
								});
							}
						};
						xmlHdos.send();
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
let saveConfigInProgress: boolean = false;
export function saveConfig() {
	if (get(isConfigDirty) && !saveConfigInProgress) {
		saveConfigInProgress = true;
		const xml = new XMLHttpRequest();
		xml.open('POST', '/save-config', true);
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				msg(`Save config status: '${xml.responseText.trim()}'`);
				if (xml.status == 200) {
					isConfigDirty.set(false);
				}
				saveConfigInProgress = false;
			}
		};
		xml.setRequestHeader('Content-Type', 'application/json');

		// converting map into something that is compatible for JSON.stringify
		// maybe the map should be converted into a Record<string, string>
		// but this has other problems and implications
		const characters: Record<string, string> = {};
		configSub.selected_characters?.forEach((value, key) => {
			characters[key] = value;
		});
		const object: Record<string, unknown> = {};
		Object.assign(object, configSub);
		object.selected_characters = characters;
		const json = JSON.stringify(object, null, 4);
		xml.send(json);
	}
}
