import App from '@/App.svelte';
import { clientListPromise, internalUrl, showDisclaimer } from '$lib/Util/store';
import { get } from 'svelte/store';
import { getNewClientListPromise, handleLogin, handleNewSessionId } from '$lib/Util/functions';
import type { Account } from '$lib/Util/interfaces';
import { unwrap } from '$lib/Util/interfaces';
import { logger } from '$lib/Util/Logger';
import { BoltService } from '$lib/Services/BoltService';
import { ParseUtils } from '$lib/Util/ParseUtils';
import { AuthService, type Auth, type Session } from '$lib/Services/AuthService';
import { Platform, bolt } from '$lib/State/Bolt';

initBolt();
addWindowListeners();

const app = new App({
	target: document.getElementById('app')!
});

export default app;

function initBolt() {
	const params = new URLSearchParams(window.location.search);
	bolt.platform = params.get('platform') as Platform | null;
	bolt.isFlathub = params.get('flathub') === '1';
	bolt.rs3InstalledHash = params.get('rs3_linux_installed_hash');
	bolt.runeLiteInstalledId = params.get('runelite_installed_id');
	bolt.hdosInstalledVersion = params.get('hdos_installed_version');

	const plugins = params.get('plugins');
	bolt.hasBoltPlugins = plugins !== null;
	if (plugins !== null) {
		try {
			bolt.pluginList = JSON.parse(plugins);
		} catch (e) {
			logger.error('Unable to parse plugin list');
		}
	}

	const sessionsParam = params.get('credentials');
	if (sessionsParam) {
		try {
			bolt.sessions = JSON.parse(sessionsParam) as Session[];
		} catch (e) {
			logger.error('Unable to parse saved credentials');
		}
	}
}

// TODO: refactor to not use listeners
function addWindowListeners(): void {
	const env = bolt.env;
	const origin = env.origin;
	const clientId = env.clientid;
	const origin_2fa = env.origin_2fa;
	const boltUrl = get(internalUrl);
	const exchangeUrl = origin.concat('/oauth2/token');

	if (bolt.sessions.length == 0) {
		showDisclaimer.set(true);
	}

	const allowedOrigins = [boltUrl, origin, origin_2fa];
	window.addEventListener('message', (event: MessageEvent) => {
		if (!allowedOrigins.includes(event.origin)) {
			logger.info(`discarding window message from origin ${event.origin}`);
			return;
		}
		let pending: Auth | undefined | null = AuthService.pendingOauth;
		const xml = new XMLHttpRequest();
		switch (event.data.type) {
			case 'authCode':
				if (pending) {
					AuthService.pendingOauth = null;
					const post_data = new URLSearchParams({
						grant_type: 'authorization_code',
						client_id: env.clientid,
						code: event.data.code,
						code_verifier: <string>pending.verifier,
						redirect_uri: env.redirect
					});
					xml.onreadystatechange = () => {
						if (xml.readyState == 4) {
							if (xml.status == 200) {
								const result = ParseUtils.parseCredentials(xml.response);
								const creds = unwrap(result);
								if (creds) {
									handleLogin(<Window>pending?.win, creds).then((x) => {
										if (x) {
											bolt.sessions.push(creds);
											BoltService.saveAllCreds();
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
				pending = AuthService.pendingGameAuth.find((x: Auth) => {
					return event.data.state == x.state;
				});
				if (pending) {
					pending.win?.close();
					AuthService.pendingGameAuth.splice(AuthService.pendingGameAuth.indexOf(pending), 1);
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
					const sessionsUrl = env.auth_api.concat('/sessions');
					xml.onreadystatechange = () => {
						if (xml.readyState == 4) {
							if (xml.status == 200) {
								const accountsUrl = env.auth_api.concat('/accounts');
								pending!.creds!.session_id = JSON.parse(xml.response).sessionId;
								handleNewSessionId(
									<Session>pending!.creds,
									accountsUrl,
									<Promise<Account>>pending!.account_info_promise
								).then((x) => {
									if (x) {
										bolt.sessions.push(<Session>pending!.creds);
										BoltService.saveAllCreds();
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
		if (bolt.sessions.length > 0) {
			bolt.sessions.forEach(async (value) => {
				const result = await AuthService.checkRenewCreds(value, exchangeUrl, clientId);
				if (result !== null && result !== 0) {
					logger.error(`Discarding expired login for #${value.sub}`);
					const index = bolt.sessions.findIndex((session) => session.sub === value.sub);
					if (index > -1) bolt.sessions.splice(index, 1);
					BoltService.saveAllCreds();
				}
				let checkedCred: Record<string, Session | boolean>;
				if (result === null && (await handleLogin(null, value))) {
					checkedCred = { creds: value, valid: true };
				} else {
					checkedCred = { creds: value, valid: result === 0 };
				}
				if (checkedCred.valid) {
					bolt.sessions.push(value);
					BoltService.saveAllCreds();
				}
			});
		}
	})();
}
