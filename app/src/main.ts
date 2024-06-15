import App from '@/App.svelte';
import {
	clientListPromise,
	config,
	platform,
	internalUrl,
	credentials,
	hasBoltPlugins,
	isConfigDirty,
	pendingOauth,
	pendingGameAuth,
	bolt,
	accountList,
	selectedPlay,
	showDisclaimer
} from '$lib/Util/store';
import { get, type Unsubscriber } from 'svelte/store';
import {
	getNewClientListPromise,
	parseCredentials,
	handleLogin,
	saveAllCreds,
	handleNewSessionId,
	checkRenewCreds,
	loadTheme as loadTheme,
	saveConfig,
	removePendingGameAuth
} from '$lib/Util/functions';
import type { Bolt, Account, Auth, Config, Credentials, SelectedPlay } from '$lib/Util/interfaces';
import { unwrap } from '$lib/Util/interfaces';
import { logger } from '$lib/Util/Logger';

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
export let hasBoltPluginsSub: boolean;
unsubscribers.push(hasBoltPlugins.subscribe((data) => (hasBoltPluginsSub = data ?? false)));
export let pendingOauthSub: Auth;
unsubscribers.push(pendingOauth.subscribe((data) => (pendingOauthSub = <Auth>data)));
export let pendingGameAuthSub: Array<Auth>;
unsubscribers.push(pendingGameAuth.subscribe((data) => (pendingGameAuthSub = data)));
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
			logger.info(`discarding window message from origin ${event.origin}`);
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
									logger.error(`Error: invalid credentials received`);
									pending!.win!.close();
								}
							} else {
								logger.error(`Error: from ${exchangeUrl}: ${xml.status}: ${xml.response}`);
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
			case 'externalUrl':
				xml.onreadystatechange = () => {
					if (xml.readyState == 4) {
						logger.info(`External URL status: '${xml.responseText.trim()}'`);
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
						logger.error(`Malformed id_token: ${sections.length} sections, expected 3`);
						break;
					}
					const header = JSON.parse(atob(sections[0]));
					if (header.typ !== 'JWT') {
						logger.error(`Bad id_token header: typ ${header.typ}, expected JWT`);
						break;
					}
					const payload = JSON.parse(atob(sections[1]));
					if (atob(payload.nonce) !== pending.nonce) {
						logger.error('Incorrect nonce in id_token');
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
											data.set(<string>pending?.creds?.sub, <Credentials>pending!.creds);
											return data;
										});
										saveAllCreds();
									}
								});
							} else {
								logger.error(`Error: from ${sessionsUrl}: ${xml.status}: ${xml.response}`);
							}
						}
					};
					xml.open('POST', sessionsUrl, true);
					xml.setRequestHeader('Content-Type', 'application/json');
					xml.setRequestHeader('Accept', 'application/json');
					xml.send(`{"idToken": "${event.data.id_token}"}`);
				}
				break;
			case 'gameClientListUpdate':
				clientListPromise.set(getNewClientListPromise());
				break;
			default:
				logger.info('Unknown message type: '.concat(event.data.type));
				break;
		}
	});

	(async () => {
		if (credentialsSub.size > 0) {
			credentialsSub.forEach(async (value) => {
				const result = await checkRenewCreds(value, exchangeUrl, clientId);
				if (result !== null && result !== 0) {
					logger.error(`Discarding expired login for #${value.sub}`);
					credentials.update((data) => {
						data.delete(value.sub);
						return data;
					});
					saveAllCreds();
				}
				let checkedCred: Record<string, Credentials | boolean>;
				if (result === null && (await handleLogin(null, value))) {
					checkedCred = { creds: value, valid: true };
				} else {
					checkedCred = { creds: value, valid: result === 0 };
				}
				if (checkedCred.valid) {
					const creds = <Credentials>value;
					credentials.update((data) => {
						data.set(creds.sub, creds);
						return data;
					});
					saveAllCreds();
				}
			});
		}
		isConfigDirty.set(false); // overrides all cases where this gets set to "true" due to loading existing config values
	})();
}

declare const s: () => Bolt;
bolt.set(s());

onload = () => start();
onunload = () => {
	for (const i in unsubscribers) {
		delete unsubscribers[i];
	}
	saveConfig();
};
