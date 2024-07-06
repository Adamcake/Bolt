import { configHasPendingChanges, type Config } from '$lib/State/Config';
import { logger } from '$lib/Util/Logger';
import type { Session } from '$lib/Services/AuthService';

let saveInProgress: boolean = false;

export class BoltService {
	// sends an asynchronous request to save the current user config to disk, if it has changed
	static saveConfig(configToSave: Config, checkForPendingChanges = true) {
		if (saveInProgress) return;
		if (checkForPendingChanges && !configHasPendingChanges) return;

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

		// converting map into something that is compatible for JSON.stringify
		// maybe the map should be converted into a Record<string, string>
		// but this has other problems and implications
		const characters: Record<string, string> = {};
		configToSave.selected_characters?.forEach((value, key) => {
			characters[key] = value;
		});
		const object: Record<string, unknown> = {};
		Object.assign(object, configToSave);
		object.selected_characters = characters;
		const json = JSON.stringify(object, null, 4);
		xml.send(json);
	}

	// sends a request to save all credentials to their config file,
	// overwriting the previous file, if any
	static async saveCredentials(sessions: Session[]): Promise<undefined> {
		new Promise((resolve) => {
			const xml = new XMLHttpRequest();
			xml.open('POST', '/save-credentials', true);
			xml.setRequestHeader('Content-Type', 'application/json');
			xml.onreadystatechange = () => {
				if (xml.readyState == 4) {
					resolve(undefined);
				}
			};

			// TODO: figure out why this was here, and how to re-implement it
			// selectedPlay.update((data) => {
			// 	data.credentials = credentialsSub.get(<string>selectedPlaySub.account?.userId);
			// 	return data;
			// });

			xml.send(JSON.stringify(sessions));
		});
	}
}
