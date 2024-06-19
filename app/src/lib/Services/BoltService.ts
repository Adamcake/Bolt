import type { Credentials } from '$lib/Services/AuthService';
import { logger } from '$lib/Util/Logger';
import type { Bolt, Config } from '$lib/Util/interfaces';
import { credentials, isConfigDirty } from '$lib/Util/store';
import { get } from 'svelte/store';

let saveConfigInProgress: boolean = false;

export class BoltService {
	static bolt: Bolt;

	// sends an asynchronous request to save the current user config to disk, if it has changed
	static saveConfig(configToSave: Config) {
		if (get(isConfigDirty) && !saveConfigInProgress) {
			saveConfigInProgress = true;
			const xml = new XMLHttpRequest();
			xml.open('POST', '/save-config', true);
			xml.onreadystatechange = () => {
				if (xml.readyState == 4) {
					logger.info(`Save config status: '${xml.responseText.trim()}'`);
					if (xml.status == 200) {
						isConfigDirty.set(false);
					}
					saveConfigInProgress = false;
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
	}

	// sends a request to save all credentials to their config file,
	// overwriting the previous file, if any
	static async saveAllCreds() {
		const xml = new XMLHttpRequest();
		xml.open('POST', '/save-credentials', true);
		xml.setRequestHeader('Content-Type', 'application/json');
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				logger.info(`Save-credentials status: ${xml.responseText.trim()}`);
			}
		};

		// TODO: figure out why this was here, and how to re-implement it
		// selectedPlay.update((data) => {
		// 	data.credentials = credentialsSub.get(<string>selectedPlaySub.account?.userId);
		// 	return data;
		// });

		const credsList: Array<Credentials> = [];
		get(credentials).forEach((value) => {
			credsList.push(value);
		});
		xml.send(JSON.stringify(credsList));
	}
}
