import { AuthService, type AuthTokens } from '$lib/Services/AuthService';
import { BoltService } from '$lib/Services/BoltService';
import { UserService, type Session } from '$lib/Services/UserService';
import { Platform, bolt } from '$lib/State/Bolt';
import { GlobalState } from '$lib/State/GlobalState';
import { initConfig, selectFirstSession } from '$lib/Util/ConfigUtils';
import { getNewClientListPromise } from '$lib/Util/functions';
import { type BoltMessage } from '$lib/Util/interfaces';
import { logger } from '$lib/Util/Logger';
import { clientListPromise } from '$lib/Util/store';
import AuthApp from '@/AuthApp.svelte';
import BoltApp from '@/BoltApp.svelte';
import { get } from 'svelte/store';

let app: BoltApp | AuthApp;
const appConfig = {
	target: document.getElementById('app') as HTMLElement
};

// TODO: instead of rendering different apps here, we should have separate .html files.
// For eg, index.html, and authenticate.html
// window.opener is set when the current window is a popup (the auth window)
// id_token is set after sending the consent request, aka the user is still authenticating
if (window.opener || window.location.search.includes('&id_token')) {
	app = new AuthApp(appConfig);
} else {
	initBolt();
	initConfig();
	addMessageListeners();
	refreshStoredSessions();
	app = new BoltApp(appConfig);
}

export default app;

function initBolt() {
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
				const sessionResult = await UserService.buildSession(tokens, session_id);
				const { config, sessions } = GlobalState;
				if (sessionResult.ok) {
					sessions.update((session) => {
						session.push(sessionResult.value);
						return session;
					});
					if (!get(config).selected_user_id) selectFirstSession();
					BoltService.saveCredentials();
					logger.info(`Successfully added account '${sessionResult.value.user.displayName}'`);
				} else {
					logger.error(`Unable to sign into account. ${sessionResult.error}`);
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

async function refreshStoredSessions() {
	const sessions = get(GlobalState.sessions);
	const expiredTokens: string[] = [];

	for (const session of sessions) {
		const tokensResult = await AuthService.refreshOAuthToken(session.tokens);
		if (!tokensResult.ok) {
			if (tokensResult.error === 0) {
				logger.error(
					`Unable to verify saved login, status: ${tokensResult.error}. Do you have an internet connection? Please relaunch Bolt to try again.`
				);
			} else {
				logger.error(
					`Discarding expired login, status: ${tokensResult.error}. Please sign in again.`
				);
				expiredTokens.push(session.tokens.sub);
			}
			continue;
		}

		const tokens = tokensResult.value;
		session.tokens = tokens;
		const sessionResult = await UserService.buildSession(tokens, session.session_id);
		if (sessionResult.ok) {
			const existingSession = BoltService.findSession(tokens.sub);
			if (!existingSession) {
				session.user = sessionResult.value.user;
				session.accounts = sessionResult.value.accounts;
			}
			logger.info(`Logged into account '${sessionResult.value.user.displayName}'`);
		} else {
			logger.error(
				`Unable to login to account '${session.user.displayName}'. ${sessionResult.error}`
			);
			expiredTokens.push(session.tokens.sub);
		}
	}

	expiredTokens.forEach((sub) => {
		BoltService.logout(sub);
	});

	GlobalState.sessions.set(sessions);
	BoltService.saveCredentials();

	// After refreshing the sessions, check if the saved session_user_id is valid in the config
	const config = get(GlobalState.config);
	const selected_user_id = config.selected_user_id;
	const savedSessionIsMissing = BoltService.findSession(selected_user_id ?? '') === undefined;
	if (savedSessionIsMissing) selectFirstSession();
}
