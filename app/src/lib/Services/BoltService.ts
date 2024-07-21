import { AuthService } from '$lib/Services/AuthService';
import { type Account, type Session } from '$lib/Services/UserService';
import { GlobalState } from '$lib/State/GlobalState';
import { selectFirstSession } from '$lib/Util/Config';
import { logger } from '$lib/Util/Logger';
import { get } from 'svelte/store';

let saveInProgress: boolean = false;

export class BoltService {
	static async logout(sub: string): Promise<Session[]> {
		const { sessions: sessionsStore } = GlobalState;
		const sessions = get(sessionsStore);
		const sessionIndex = sessions.findIndex((session) => session.user.userId === sub);
		if (sessionIndex > -1) {
			AuthService.revokeOauthCreds(sessions[sessionIndex].tokens.access_token);
			sessions.splice(sessionIndex, 1);
			sessionsStore.set(sessions);
		}
		selectFirstSession();
		BoltService.saveCredentials();
		return sessions;
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

	static findSession(userId: string): Session | undefined {
		const sessions = get(GlobalState.sessions);
		return sessions.find((session) => session.user.userId === userId);
	}

	static findAccount(accounts: Account[], accountId: string): Account | undefined {
		return accounts.find((account) => account.accountId == accountId);
	}
}
