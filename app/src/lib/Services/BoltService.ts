import { AuthService, type AuthTokens } from '$lib/Services/AuthService';
import { UserService, type Account, type Session } from '$lib/Services/UserService';
import { GlobalState } from '$lib/State/GlobalState';
import { error, ok, type Result } from '$lib/Util/interfaces';
import { logger } from '$lib/Util/Logger';
import { get } from 'svelte/store';

let saveInProgress: boolean = false;

export class BoltService {
	// After refreshing or adding a new account, this function will need to be called.
	// It will initialize or update the Sessions array, and set the config when appropriate
	// Note: Does not call saveCredentials
	static async login(tokens: AuthTokens, session_id: string): Promise<Result<Session, string>> {
		const sessionResult = await UserService.buildSession(tokens, session_id);
		if (!sessionResult.ok) return error(sessionResult.error);
		const { config, sessions } = GlobalState;
		const newSession = sessionResult.value;
		const sessionExists = BoltService.findSession(tokens.sub);
		if (!sessionExists) {
			sessions.update((session) => {
				session.push(newSession);
				return session;
			});
			config.update((c) => {
				c.selected.user_id = newSession.user.userId;
				return c;
			});
		}
		if (!get(config).selected.user_id === null) newSession.user.userId;
		return ok(newSession);
	}

	// After the tokens are older than 30 days, or the user clicks logout, this function will need to be called.
	// It will remove the session from the list of Sessions, and update the saved config
	// Note: Does not call saveCredentials
	static async logout(sub: string): Promise<Session[]> {
		const { sessions, config } = GlobalState;
		sessions.update((s) => {
			const sessionIndex = s.findIndex((session) => session.user.userId === sub);
			if (sessionIndex > -1) {
				AuthService.revokeOauthCreds(s[sessionIndex].tokens.access_token);
				s.splice(sessionIndex, 1);
			}
			return s;
		});
		config.update((c) => {
			if (typeof c.userDetails[sub] !== 'undefined') {
				delete c.userDetails[sub];
			}
			if (c.selected.user_id === sub) c.selected.user_id = null;
			return c;
		});
		return get(sessions);
	}

	// sends an asynchronous request to save the current user config to disk, if it has changed
	static saveConfig(checkForPendingChanges = true) {
		if (saveInProgress) return;
		if (checkForPendingChanges && !GlobalState.configHasPendingChanges) return;

		saveInProgress = true;
		const xml = new XMLHttpRequest();
		xml.open('POST', '/save-config', true);
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				logger.info(`Save config status: '${xml.responseText.trim()}'`);
				saveInProgress = false;
			}
		};
		xml.setRequestHeader('Content-Type', 'application/json');

		const config = get(GlobalState.config);
		xml.send(JSON.stringify(config));
		return config;
	}

	// sends a request to save all credentials to their config file,
	// overwriting the previous file, if any
	static async saveCredentials(): Promise<undefined> {
		new Promise((resolve) => {
			const xml = new XMLHttpRequest();
			xml.open('POST', '/save-credentials', true);
			xml.setRequestHeader('Content-Type', 'application/json');
			xml.onreadystatechange = () => {
				if (xml.readyState == 4) {
					resolve(undefined);
				}
			};

			const sessions = get(GlobalState.sessions);
			xml.send(JSON.stringify(sessions));
		});
	}

	static async openFilePicker(): Promise<string | undefined> {
		return new Promise((resolve) => {
			const xml = new XMLHttpRequest();
			xml.onreadystatechange = () => {
				if (xml.readyState == 4) {
					// if the user closes the file picker without selecting a file, status here is 204
					if (xml.status == 200) {
						return resolve(xml.responseText);
					}
					return resolve(undefined);
				}
			};
			xml.open('GET', '/jar-file-picker', true);
			xml.send();
		});
	}

	static findSession(userId: string | null): Session | undefined {
		const sessions = get(GlobalState.sessions);
		return sessions.find((session) => session.user.userId === userId);
	}

	static findAccount(accounts: Account[], accountId?: string): Account | undefined {
		return accounts.find((account) => account.accountId == accountId);
	}
}
