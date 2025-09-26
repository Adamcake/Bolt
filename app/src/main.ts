import { AuthService, type AuthTokens } from '$lib/Services/AuthService';
import { BoltService } from '$lib/Services/BoltService';
import { type Session } from '$lib/Services/UserService';
import { Platform, bolt } from '$lib/State/Bolt';
import { GlobalState } from '$lib/State/GlobalState';
import { initConfig } from '$lib/Util/Config';
import { type BoltMessage } from '$lib/Util/Interfaces';
import { logger } from '$lib/Util/Logger';
import { clientList } from '$lib/Util/Store';
import AuthApp from '@/AuthApp.svelte';
import BoltApp from '@/BoltApp.svelte';
import { get } from 'svelte/store';
import { mount } from 'svelte';

let app: BoltApp | AuthApp;
const appConfig = {
	target: document.getElementById('app') as HTMLElement
};

// TODO: instead of rendering different apps here, we should have separate .html files.
// For eg, index.html, and authenticate.html
// window.opener is set when the current window is a popup (the auth window)
// id_token is set after sending the consent request, aka the user is still authenticating
if (window.opener || window.location.search.includes('&id_token')) {
	app = mount(AuthApp, appConfig);
} else {
	initBolt();
	initConfig();
	addMessageListeners();
	refreshStoredSessions().finally(() => {
		GlobalState.initialized.set(true);
	});
	app = mount(BoltApp, appConfig);
}

export default app;

function initBolt() {
	const params = new URLSearchParams(window.location.search);
	bolt.platform = params.get('platform') as Platform | null;
	bolt.isFlathub = params.get('flathub') === '1';
	bolt.hasLibArchive = params.get('has_libarchive') === '1';
	bolt.rs3debInstalledHash = params.get('rs3_deb_installed_hash');
	bolt.rs3exeInstalledHash = params.get('rs3_exe_installed_hash');
	bolt.rs3appInstalledHash = params.get('rs3_app_installed_hash');
	bolt.osrsexeInstalledHash = params.get('osrs_exe_installed_hash');
	bolt.osrsappInstalledHash = params.get('osrs_app_installed_hash');
	bolt.runeLiteInstalledId = params.get('runelite_installed_id');
	bolt.hdosInstalledVersion = params.get('hdos_installed_version');

	const plugins = params.get('plugins');
	bolt.hasBoltPlugins = plugins !== null;
	if (plugins !== null) {
		try {
			bolt.pluginConfig = JSON.parse(plugins);
		} catch (e) {
			logger.error('Unable to parse plugin list');
		}
	}

	const sessionsParam = params.get('credentials');
	if (sessionsParam) {
		try {
			const sessions = JSON.parse(sessionsParam) as Session[];
			const validConfig = sessions.every((session) => {
				return (
					typeof session.session_id === 'string' &&
					typeof session.tokens === 'object' &&
					typeof session.accounts === 'object' &&
					typeof session.user === 'object'
				);
			});

			if (validConfig) {
				GlobalState.sessions.set(sessions);
			} else {
				GlobalState.sessions.set([]);
				BoltService.saveCredentials();
				logger.warn('Credentials saved on disk are out of date. Please sign in again.');
			}
			GlobalState.sessions.set(validConfig ? sessions : []);
		} catch (e) {
			GlobalState.sessions.set([]);
			BoltService.saveCredentials();
			logger.error('Unable to parse saved credentials. Please sign in again.');
		}
	}
}

function addMessageListeners(): void {
	const { origin, origin_2fa } = bolt.env;

	const allowedOrigins = [window.location.origin, origin, origin_2fa];
	let tokens: AuthTokens | null = null;
	window.addEventListener('message', async (event: MessageEvent<BoltMessage>) => {
		if (!allowedOrigins.includes(event.origin)) {
			logger.info(`discarding window message from origin ${event.origin}`);
			return;
		}
		switch (event.data.type) {
			case 'authTokenUpdate': {
				tokens = event.data.tokens;
				break;
			}
			case 'authSessionUpdate': {
				if (tokens === null) {
					return logger.error('auth is null. Please try again.');
				}
				const session_id = event.data.sessionId;
				const loginResult = await BoltService.login(tokens, session_id);
				if (!loginResult.ok) {
					logger.error(`Unable to add new user. Please try again. ${loginResult.error}`);
				} else {
					logger.info(`Added new user '${loginResult.value.user.displayName}'`);
				}
				BoltService.saveCredentials();
				AuthService.pendingLoginWindow = null;
				tokens = null;
				break;
			}
			case 'authFailed': {
				logger.error(`Unable to authenticate: ${event.data.reason}`);
				AuthService.pendingLoginWindow = null;
				tokens = null;
				break;
			}
			case 'externalUrl': {
				const xml = new XMLHttpRequest();
				xml.onreadystatechange = () => {
					if (xml.readyState == 4) {
						logger.info(`External URL status: '${xml.responseText.trim()}'`);
					}
				};
				xml.open('POST', '/open-external-url', true);
				xml.send(event.data.url);
				break;
			}
			case 'gameClientList':
				if (
					get(GlobalState.config).close_after_launch &&
					event.data.clients.length > get(clientList).length
				) {
					fetch('/close');
				} else {
					clientList.set(event.data.clients);
				}
				break;
			default: {
				const type = (event.data as { type: string | undefined })?.type ?? 'no type provided';
				logger.info(`Unknown message type: ${type}`);
				break;
			}
		}
	});
}

async function refreshStoredSessions() {
	const discardExpired = get(GlobalState.config).discard_expired_sessions;
	const sessions = get(GlobalState.sessions);
	const expiredTokens: string[] = [];

	if (sessions.length > 0) logger.info(`Logging in...`);

	const sessionResults = sessions.map((x) => {
		return { promise: AuthService.refreshOAuthToken(x.tokens), session: x };
	});
	const loginResults = [];

	for (const result of sessionResults) {
		const session = result.session;
		const tokensResult = await result.promise;
		if (!tokensResult.ok) {
			if (tokensResult.error === 0) {
				logger.error(
					`Unable to verify saved login, status: ${tokensResult.error}. Do you have an internet connection? Please relaunch Bolt to try again.`
				);
			} else if (tokensResult.error >= 400 && tokensResult.error < 500 && discardExpired) {
				logger.error(
					`Discarding expired login, status: ${tokensResult.error}. Please sign in again.`
				);
				expiredTokens.push(session.tokens.sub);
			} else {
				logger.error(`Unable to verify saved login due to HTTP error ${tokensResult.error}`);
			}
			continue;
		}

		const tokens = tokensResult.value;
		session.tokens = tokens;
		loginResults.push({ promise: BoltService.login(tokens, session.session_id), session: session });
	}

	for (const result of loginResults) {
		const session = result.session;
		const loginResult = await result.promise;
		if (!loginResult.ok) {
			logger.error(
				`Unable to sign into saved user '${session.user.displayName}' due to an error: ${loginResult.error}`
			);
			if (discardExpired) {
				expiredTokens.push(session.tokens.sub);
			}
		} else {
			logger.info(`Signed into saved user '${loginResult.value.user.displayName}'`);
		}
	}

	const expiredResults = expiredTokens.map(BoltService.logout);
	for (const result of expiredResults) {
		await result;
	}

	GlobalState.sessions.set(sessions);
	await BoltService.saveCredentials();
}
