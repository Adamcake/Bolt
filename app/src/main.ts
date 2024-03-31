import './assets/output.css';
import App from './App.svelte';
import {
	message_list,
	config,
	platform,
	internal_url,
	credentials,
	is_config_dirty,
	pending_oauth,
	pending_game_auth,
	bolt,
	account_list,
	selected_play,
	show_disclaimer
} from './store';
import { get, type Unsubscriber } from 'svelte/store';
import {
	parseCredentials,
	handleLogin,
	saveAllCreds,
	handleNewSessionId,
	checkRenewCreds,
	load_theme,
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

const internal_url_sub = get(internal_url);

export let bolt_sub: Bolt;
unsubscribers.push(bolt.subscribe((data) => (bolt_sub = data)));
export let config_sub: Config;
unsubscribers.push(config.subscribe((data) => (config_sub = data)));
export let platform_sub: string;
unsubscribers.push(platform.subscribe((data) => (platform_sub = data as string)));
export let credentials_sub: Map<string, Credentials>;
unsubscribers.push(credentials.subscribe((data) => (credentials_sub = data)));
export let pending_oauth_sub: Auth;
unsubscribers.push(pending_oauth.subscribe((data) => (pending_oauth_sub = <Auth>data)));
export let pending_game_auth_sub: Array<Auth>;
unsubscribers.push(pending_game_auth.subscribe((data) => (pending_game_auth_sub = data)));
export let message_list_sub: Array<Message>;
unsubscribers.push(message_list.subscribe((data) => (message_list_sub = data)));
export let account_list_sub: Map<string, Account>;
unsubscribers.push(account_list.subscribe((data) => (account_list_sub = data)));
export let selected_play_sub: SelectedPlay;
unsubscribers.push(selected_play.subscribe((data) => (selected_play_sub = data)));

// variables
export let credentials_are_dirty: boolean = false;

// body's onload function
function start(): void {
	const s_origin = atob(bolt_sub.origin);
	const client_id = atob(bolt_sub.clientid);
	const exchange_url = s_origin.concat('/oauth2/token');

	if (credentials_sub.size == 0) {
		show_disclaimer.set(true);
	}

	load_theme();

	const allowed_origins = [internal_url_sub, s_origin, atob(bolt_sub.origin_2fa)];
	window.addEventListener('message', (event: MessageEvent) => {
		if (!allowed_origins.includes(event.origin)) {
			msg(`discarding window message from origin ${event.origin}`);
			return;
		}
		let pending: any = pending_oauth_sub;
		const xml = new XMLHttpRequest();
		switch (event.data.type) {
			case 'authCode':
				if (pending) {
					pending_oauth.set({});
					const post_data = new URLSearchParams({
						grant_type: 'authorization_code',
						client_id: atob(bolt_sub.clientid),
						code: event.data.code,
						code_verifier: <string>pending.verifier,
						redirect_uri: atob(bolt_sub.redirect)
					});
					xml.onreadystatechange = () => {
						if (xml.readyState == 4) {
							if (xml.status == 200) {
								const result = parseCredentials(xml.response);
								const creds = unwrap(result);
								if (creds) {
									handleLogin(pending?.win, creds).then((x) => {
										if (x) {
											credentials.update((data) => {
												data.set(creds.sub, creds);
												return data;
											});
											credentials_are_dirty = true;
											saveAllCreds();
										}
									});
								} else {
									err(`Error: invalid credentials received`, false);
									pending!.win!.close();
								}
							} else {
								err(
									`Error: from ${exchange_url}: ${xml.status}: ${xml.response}`,
									false
								);
								pending!.win!.close();
							}
						}
					};
					xml.open('POST', exchange_url, true);
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
				pending = pending_game_auth_sub.find((x: Auth) => {
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
					const sessions_url = atob(bolt_sub.auth_api).concat('/sessions');
					xml.onreadystatechange = () => {
						if (xml.readyState == 4) {
							if (xml.status == 200) {
								const accounts_url = atob(bolt_sub.auth_api).concat('/accounts');
								pending!.creds!.session_id = JSON.parse(xml.response).sessionId;
								handleNewSessionId(
									<Credentials>pending!.creds,
									accounts_url,
									<Promise<Account>>pending!.account_info_promise
								).then((x) => {
									if (x) {
										credentials.update((data) => {
											data.set(
												<string>pending!.creds.sub,
												<Credentials>pending!.creds
											);
											return data;
										});
										credentials_are_dirty = true;
										saveAllCreds();
									}
								});
							} else {
								err(
									`Error: from ${sessions_url}: ${xml.status}: ${xml.response}`,
									false
								);
							}
						}
					};
					xml.open('POST', sessions_url, true);
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
		if (credentials_sub.size > 0) {
			const old_credentials_size = credentials_sub.size;
			const promises: Array<any> = [];
			credentials_sub.forEach(async (value, _key) => {
				const result = await checkRenewCreds(value, exchange_url, client_id);
				if (result !== null && result !== 0) {
					err(`Discarding expired login for #${value.sub}`, false);
					credentials.update((data) => {
						data.delete(value.sub);
						return data;
					});
					credentials_are_dirty = true;
					saveAllCreds();
				}
				if (result === null && (await handleLogin(null, value))) {
					promises.push({ creds: value, valid: true });
				} else {
					promises.push({ creds: value, valid: result === 0 });
				}
			});
			const responses = await Promise.all(promises);
			const valid_creds = responses.filter((x) => x.valid).map((x) => x.creds);
			valid_creds.forEach((value: Credentials) => {
				credentials.update((data) => {
					data.set(value.sub, value);
					return data;
				});
			});
			credentials_are_dirty = credentials_sub.size != old_credentials_size;
			saveAllCreds();
		}
		is_config_dirty.set(false); // overrides all cases where this gets set to "true" due to loading existing config values
	})();
}

// adds a message to the message list
export function msg(str: string) {
	console.log(str);
	const message: Message = {
		is_error: false,
		text: str,
		time: new Date(Date.now())
	};
	message_list.update((list: Array<Message>) => {
		list.unshift(message);
		return list;
	});
}

// adds an error message to the message list
// if do_throw is true, throws the error message
export function err(str: string, do_throw: boolean) {
	const message: Message = {
		is_error: true,
		text: str,
		time: new Date(Date.now())
	};
	message_list.update((list: Array<Message>) => {
		list.unshift(message);
		return list;
	});

	if (!do_throw) {
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
