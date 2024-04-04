import './assets/output.css';
import App from './App.svelte';
import {
	messageList,
	config,
	platform,
	internalUrl,
	credentials,
	isConfigDirty,
	pendingOauth,
	pendingGameAuth,
	bolt,
	accountList,
	selectedPlay,
	showDisclaimer
} from './store';
import { get, type Unsubscriber } from 'svelte/store';
import {
	parseCredentials,
	handleLogin,
	saveAllCreds,
	handleNewSessionId,
	checkRenewCreds,
	loadTheme as loadTheme,
	saveConfig,
	removePendingGameAuth
} from './functions';
import type { Bolt, Account, Auth, Config, Credentials, Message, SelectedPlay } from './interfaces';
import { unwrap } from './interfaces';

const app = new App({
	target: document.getElementById('app')!
});

export default app;

// store access, subscribers, and unsubscribers array
const unsubscribers: Array<Unsubscriber> = [];

const internalUrlSub = get(internalUrl);

export let boltSub: Bolt;
unsubscribers.push(bolt.subscribe((data) => (boltSub = data)));
export let configSub: Config;
unsubscribers.push(config.subscribe((data) => (configSub = data)));
export let platformSub: string;
unsubscribers.push(platform.subscribe((data) => (platformSub = data as string)));
export let credentialsSub: Map<string, Credentials>;
unsubscribers.push(credentials.subscribe((data) => (credentialsSub = data)));
export let pendingOauthSub: Auth;
unsubscribers.push(pendingOauth.subscribe((data) => (pendingOauthSub = <Auth>data)));
export let pendingGameAuthSub: Array<Auth>;
unsubscribers.push(pendingGameAuth.subscribe((data) => (pendingGameAuthSub = data)));
export let messageListSub: Array<Message>;
unsubscribers.push(messageList.subscribe((data) => (messageListSub = data)));
export let accountListSub: Map<string, Account>;
unsubscribers.push(accountList.subscribe((data) => (accountListSub = data)));
export let selectedPlaySub: SelectedPlay;
unsubscribers.push(selectedPlay.subscribe((data) => (selectedPlaySub = data)));

// body's onload function
function start(): void {
	const sOrigin = atob(boltSub.origin);
	const clientId = atob(boltSub.clientid);
	const exchangeUrl = sOrigin.concat('/oauth2/token');

	if (credentialsSub.size == 0) {
		showDisclaimer.set(true);
	}

	loadTheme();

	// support legacy config name, selected_game_accounts; load it into the new one
	if (configSub.selected_game_accounts && configSub.selected_game_accounts?.size > 0) {
		config.update((data) => {
			data.selected_characters = data.selected_game_accounts;
			data.selected_game_accounts?.clear();
			return data;
		});
	}

	const allowedOrigins = [internalUrlSub, sOrigin, atob(boltSub.origin_2fa)];
	window.addEventListener('message', (event: MessageEvent) => {
		if (!allowedOrigins.includes(event.origin)) {
			msg(`discarding window message from origin ${event.origin}`);
			return;
		}
		let pending: Auth | undefined = pendingOauthSub;
		const xml = new XMLHttpRequest();
		switch (event.data.type) {
			case 'authCode':
				if (pending) {
					pendingOauth.set({});
					const post_data = new URLSearchParams({
						grant_type: 'authorization_code',
						client_id: atob(boltSub.clientid),
						code: event.data.code,
						code_verifier: <string>pending.verifier,
						redirect_uri: atob(boltSub.redirect)
					});
					xml.onreadystatechange = () => {
						if (xml.readyState == 4) {
							if (xml.status == 200) {
								const result = parseCredentials(xml.response);
								const creds = unwrap(result);
								if (creds) {
									handleLogin(<Window>pending?.win, creds).then((x) => {
										if (x) {
											credentials.update((data) => {
												data.set(creds.sub, creds);
												return data;
											});
											saveAllCreds();
										}
									});
								} else {
									err(`Error: invalid credentials received`, false);
									pending!.win!.close();
								}
							} else {
								err(
									`Error: from ${exchangeUrl}: ${xml.status}: ${xml.response}`,
									false
								);
								pending!.win!.close();
							}
						}
					};
					xml.open('POST', exchangeUrl, true);
					xml.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
					xml.setRequestHeader('Accept', 'application/json');
					xml.send(post_data);
				}
				break;
			case 'initAuth':
				msg(`message: init auth: ${event.data.auth_method}`);
				break;
			case 'externalUrl':
				xml.onreadystatechange = () => {
					if (xml.readyState == 4) {
						msg(`External URL status: '${xml.responseText.trim()}'`);
					}
				};
				xml.open('POST', '/open-external-url', true);
				xml.send(event.data.url);
				break;
			case 'gameSessionServerAuth':
				pending = pendingGameAuthSub.find((x: Auth) => {
					return event.data.state == x.state;
				});
				if (pending) {
					removePendingGameAuth(pending, true);
					const sections = event.data.id_token.split('.');
					if (sections.length !== 3) {
						err(`Malformed id_token: ${sections.length} sections, expected 3`, false);
						break;
					}
					const header = JSON.parse(atob(sections[0]));
					if (header.typ !== 'JWT') {
						err(`Bad id_token header: typ ${header.typ}, expected JWT`, false);
						break;
					}
					const payload = JSON.parse(atob(sections[1]));
					if (atob(payload.nonce) !== pending.nonce) {
						err('Incorrect nonce in id_token', false);
						break;
					}
					const sessionsUrl = atob(boltSub.auth_api).concat('/sessions');
					xml.onreadystatechange = () => {
						if (xml.readyState == 4) {
							if (xml.status == 200) {
								const accountsUrl = atob(boltSub.auth_api).concat('/accounts');
								pending!.creds!.session_id = JSON.parse(xml.response).sessionId;
								handleNewSessionId(
									<Credentials>pending!.creds,
									accountsUrl,
									<Promise<Account>>pending!.account_info_promise
								).then((x) => {
									if (x) {
										credentials.update((data) => {
											data.set(
												<string>pending?.creds?.sub,
												<Credentials>pending!.creds
											);
											return data;
										});
										saveAllCreds();
									}
								});
							} else {
								err(
									`Error: from ${sessionsUrl}: ${xml.status}: ${xml.response}`,
									false
								);
							}
						}
					};
					xml.open('POST', sessionsUrl, true);
					xml.setRequestHeader('Content-Type', 'application/json');
					xml.setRequestHeader('Accept', 'application/json');
					xml.send(`{"idToken": "${event.data.id_token}"}`);
				}
				break;
			default:
				msg('Unknown message type: '.concat(event.data.type));
				break;
		}
	});

	(async () => {
		if (credentialsSub.size > 0) {
			const oldCredentialsSize = credentialsSub.size;
			const promises: Array<Record<string, Credentials | boolean>> = [];
			credentialsSub.forEach(async (value) => {
				const result = await checkRenewCreds(value, exchangeUrl, clientId);
				if (result !== null && result !== 0) {
					err(`Discarding expired login for #${value.sub}`, false);
					credentials.update((data) => {
						data.delete(value.sub);
						return data;
					});
					saveAllCreds();
				}
				if (result === null && (await handleLogin(null, value))) {
					promises.push({ creds: value, valid: true });
				} else {
					promises.push({ creds: value, valid: result === 0 });
				}
			});
			const responses = await Promise.all(promises);
			const validCreds = responses.filter((x) => x.valid).map((x) => x.creds);
			validCreds.forEach((value) => {
				const creds = <Credentials>value;
				credentials.update((data) => {
					data.set(creds.sub, creds);
					return data;
				});
			});
			if (credentialsSub.size != oldCredentialsSize) {
				saveAllCreds();
			}
		}
		isConfigDirty.set(false); // overrides all cases where this gets set to "true" due to loading existing config values
	})();
}

// adds a message to the message list
export function msg(str: string) {
	console.log(str);
	const message: Message = {
		isError: false,
		text: str,
		time: new Date(Date.now())
	};
	messageList.update((list: Array<Message>) => {
		list.unshift(message);
		return list;
	});
}

// adds an error message to the message list
// if do_throw is true, throws the error message
export function err(str: string, doThrow: boolean) {
	const message: Message = {
		isError: true,
		text: str,
		time: new Date(Date.now())
	};
	messageList.update((list: Array<Message>) => {
		list.unshift(message);
		return list;
	});

	if (!doThrow) {
		console.error(str);
	} else {
		throw new Error(str);
	}
}

declare const s: any;
bolt.set(<Bolt>s());

onload = () => start();
onunload = () => {
	for (const i in unsubscribers) {
		delete unsubscribers[i];
	}
	saveConfig();
};
