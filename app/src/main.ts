import App from '@/App.svelte';
import { clientListPromise, internalUrl } from '$lib/Util/store';
import { get } from 'svelte/store';
import { getNewClientListPromise } from '$lib/Util/functions';
import { type BoltMessage } from '$lib/Util/interfaces';
import { logger } from '$lib/Util/Logger';
import { AuthService, type Session, type TokenSet } from '$lib/Services/AuthService';
import { Platform, bolt } from '$lib/State/Bolt';
import { initConfig } from '$lib/State/Config';
import { LocalStorageService } from '$lib/Services/LocalStorageService';
import { BoltService } from '$lib/Services/BoltService';

// TODO: make this less obscure
// window.opener is set when the current window is a popup (the auth window)
// id_token is set after sending the consent request, aka the user is still authenticating
if (window.opener || window.location.search.includes('id_token')) {
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
			bolt.sessions = JSON.parse(sessionsParam) as Session[];
		} catch (e) {
			logger.error('Unable to parse saved credentials');
		}
	}
}

function addMessageListeners(): void {
	const { origin, origin_2fa } = bolt.env;
	const boltUrl = get(internalUrl);

	const allowedOrigins = [boltUrl, origin, origin_2fa];
	let pendingAuthTokenSet: TokenSet | null = null;
	window.addEventListener('message', async (event: MessageEvent<BoltMessage>) => {
		if (!allowedOrigins.includes(event.origin)) {
			logger.info(`discarding window message from origin ${event.origin}`);
			return;
		}
		switch (event.data.type) {
			case 'authTokenUpdate': {
				pendingAuthTokenSet = event.data.tokenSet;
				break;
			}
			case 'authSessionUpdate': {
				if (pendingAuthTokenSet === null) {
					return logger.error('pendingAuthTokens is not defined. Please try again.');
				}
				const session: Session = {
					session_id: event.data.sessionId,
					tokenSet: pendingAuthTokenSet
				};
				bolt.sessions.push(session);
				// TODO: make a request to /accounts and /displayName to update the UI
				BoltService.saveAllCreds();
				pendingAuthTokenSet = null;
				AuthService.pendingLoginWindow = null;
				break;
			}
			case 'authFailed': {
				logger.error(`Unable to authenticate: ${event.data.reason}`);
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

function refreshStoredTokens() {
	bolt.sessions.forEach(async (session) => {
		const result = await AuthService.refreshOAuthToken(session.tokenSet);
		if (!result.ok) {
			if (result.error === 0) {
				return logger.error(
					`Unable to verify saved login, status: ${result.error}. Do you have an internet connection? Please relaunch Bolt to try again.`
				);
			} else {
				logger.error(`Discarding expired login, status: ${result.error}. Please sign in again.`);
				const index = bolt.sessions.findIndex((s) => s.tokenSet.sub === session.tokenSet.sub);
				if (index > -1) {
					bolt.sessions.splice(index, 1);
					BoltService.saveAllCreds();
				} else {
					// TODO: updated stored tokenSet to reflect newly refreshed tokenSet
					// make a request to /accounts and /displayName to update the UI
				}
			}
		}
	});
}
