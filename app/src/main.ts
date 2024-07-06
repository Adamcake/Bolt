import App from '@/App.svelte';
import { clientListPromise, internalUrl } from '$lib/Util/store';
import { get } from 'svelte/store';
import { getNewClientListPromise } from '$lib/Util/functions';
import { type BoltMessage } from '$lib/Util/interfaces';
import { logger } from '$lib/Util/Logger';
import { AuthService, type Session, type AuthTokens } from '$lib/Services/AuthService';
import { Platform, bolt } from '$lib/State/Bolt';
import { initConfig } from '$lib/State/Config';
import { LocalStorageService } from '$lib/Services/LocalStorageService';

// TODO: make this less obscure
// window.opener is set when the current window is a popup (the auth window)
// id_token is set after sending the consent request, aka the user is still authenticating
if (window.opener || window.location.search.includes('&id_token')) {
	AuthService.authenticating = true;
} else {
	initBolt();
	initConfig();
	addMessageListeners();
	refreshStoredTokens();
}

// TODO: render a different app when authenticating, instead of using AuthService.authenticating
const app = new App({
	target: document.getElementById('app')!
});

export default app;

function initBolt() {
	// Store all of the bolt env variables for the auth window to use
	LocalStorageService.set('boltEnv', bolt.env);

	const params = new URLSearchParams(window.location.search);
	bolt.platform = params.get('platform') as Platform | null;
	bolt.isFlathub = params.get('flathub') === '1';
	bolt.rs3DebInstalledHash = params.get('rs3_deb_installed_hash');
	bolt.rs3ExeInstalledHash = params.get('rs3_exe_installed_hash');
	bolt.rs3AppInstalledHash = params.get('rs3_app_installed_hash');
	bolt.osrsExeInstalledHash = params.get('osrs_exe_installed_hash');
	bolt.osrsAppInstalledHash = params.get('osrs_app_installed_hash');
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
			const sessions = JSON.parse(sessionsParam) as Session[];
			const validConfig = sessions.every((session) => {
				return typeof session.session_id === 'string' && typeof session.tokens === 'object';
			});
			// If user has an old creds file, reset it to empty on load
			AuthService.sessions = validConfig ? sessions : [];
		} catch (e) {
			logger.error('Unable to parse saved credentials');
		}
	}
}

function addMessageListeners(): void {
	const { origin, origin_2fa } = bolt.env;
	const boltUrl = get(internalUrl);

	const allowedOrigins = [boltUrl, origin, origin_2fa];
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
				const loginResult = await AuthService.login(session_id, tokens);
				if (loginResult.ok) {
					logger.info(`Successfully added account ${loginResult.value.user.displayName}`);
				} else {
					logger.error(`Unable to sign into account. ${loginResult.error}`);
				}

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
			case 'gameClientListUpdate':
				clientListPromise.set(getNewClientListPromise());
				break;
			default: {
				const type = (event.data as { type: string | undefined })?.type ?? 'no type provided';
				logger.info(`Unknown message type: ${type}`);
				break;
			}
		}
	});
}

async function refreshStoredTokens() {
	const expiredTokens: string[] = [];

	for (const session of AuthService.sessions) {
		const tokensResult = await AuthService.refreshOAuthToken(session.tokens);
		if (!tokensResult.ok) {
			if (tokensResult.error === 0) {
				return logger.error(
					`Unable to verify saved login, status: ${tokensResult.error}. Do you have an internet connection? Please relaunch Bolt to try again.`
				);
			} else {
				logger.error(
					`Discarding expired login, status: ${tokensResult.error}. Please sign in again.`
				);
				expiredTokens.push(session.tokens.sub);
				continue;
			}
		}

		const tokens = tokensResult.value;
		session.tokens = tokens;
		const loginResult = await AuthService.login(session.session_id, tokens);
		if (loginResult.ok) {
			logger.info(`Logged into stored account ${loginResult.value.user.displayName}`);
		} else {
			logger.error(`Unable to login with stored account. ${loginResult.error}`);
		}
	}

	expiredTokens.forEach((sub) => {
		AuthService.logout(sub);
	});
}
