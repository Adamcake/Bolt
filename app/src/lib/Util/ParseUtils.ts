import type { Session } from '$lib/Services/AuthService';
import type { BoltEnv } from '$lib/State/Bolt';
import { logger } from '$lib/Util/Logger';
import type { Result } from '$lib/Util/interfaces';

export class ParseUtils {
	// parses a response from the oauth endpoint
	// returns a Result, it may
	static parseCredentials(str: string): Result<Session> {
		const oauthCreds = JSON.parse(str);
		const sections = oauthCreds.id_token.split('.');
		if (sections.length !== 3) {
			const errMsg: string = `Malformed id_token: ${sections.length} sections, expected 3`;
			logger.error(errMsg);
			return { ok: false, error: new Error(errMsg) };
		}
		const header = JSON.parse(atob(sections[0]));
		if (header.typ !== 'JWT') {
			const errMsg: string = `Bad id_token header: typ ${header.typ}, expected JWT`;
			logger.error(errMsg);
			return { ok: false, error: new Error(errMsg) };
		}
		const payload = JSON.parse(atob(sections[1]));
		return {
			ok: true,
			value: {
				access_token: oauthCreds.access_token,
				id_token: oauthCreds.id_token,
				refresh_token: oauthCreds.refresh_token,
				sub: payload.sub,
				login_provider: payload.login_provider || null,
				expiry: Date.now() + oauthCreds.expires_in * 1000,
				session_id: oauthCreds.session_id
			}
		};
	}

	// static parseUrlParams(url: string) {
	// 	const query = new URLSearchParams(url);
	// 	platform.set(query.get('platform'));
	// 	//isFlathub = query.get('flathub') === '1';
	// 	rs3InstalledHash.set(query.get('rs3_linux_installed_hash'));
	// 	runeLiteInstalledId.set(query.get('runelite_installed_id'));
	// 	hdosInstalledVersion.set(query.get('hdos_installed_version'));
	// 	const queryPlugins: string | null = query.get('plugins');
	// 	if (queryPlugins !== null) {
	// 		hasBoltPlugins.set(true);
	// 		pluginList.set(JSON.parse(queryPlugins));
	// 	} else {
	// 		hasBoltPlugins.set(false);
	// 	}

	// 	const creds = query.get('credentials');
	// 	if (creds) {
	// 		try {
	// 			// no need to set credentials_are_dirty here because the contents came directly from the file
	// 			const credsList: Array<Session> = JSON.parse(creds);
	// 			credsList.forEach((value) => {
	// 				credentials.update((data) => {
	// 					data.set(value.sub, value);
	// 					return data;
	// 				});
	// 			});
	// 		} catch (error: unknown) {
	// 			logger.error(`Couldn't parse credentials file: ${error}`);
	// 		}
	// 	}
	// 	const conf = query.get('config');
	// 	if (conf) {
	// 		try {
	// 			// as above, no need to set configIsDirty
	// 			const parsedConf = JSON.parse(conf);
	// 			config.set(parsedConf);
	// 			// convert parsed objects into Maps
	// 			config.update((data) => {
	// 				if (data.selected_game_accounts) {
	// 					data.selected_characters = new Map(Object.entries(data.selected_game_accounts));
	// 					delete data.selected_game_accounts;
	// 				} else if (data.selected_characters) {
	// 					data.selected_characters = new Map(Object.entries(data.selected_characters));
	// 				}
	// 				return data;
	// 			});
	// 		} catch (error: unknown) {
	// 			logger.error(`Couldn't parse config file: ${error}`);
	// 		}
	// 	}
	// }

	// The bolt object from the C++ app has its values base64 encoded.
	// This will decode each value, and return the original structure
	static decodeBolt(encoded: BoltEnv): BoltEnv {
		const decodedObject: Record<string, string | string[]> = {};
		for (const _key in encoded) {
			const key = _key as keyof BoltEnv;
			const value = encoded[key];
			if (typeof value === 'string') {
				decodedObject[key] = atob(value);
			} else if (Array.isArray(value) && value.every((item) => typeof item === 'string')) {
				decodedObject[key] = value.map((item) => atob(item));
			} else {
				decodedObject[key] = value;
			}
		}

		return decodedObject as unknown as BoltEnv;
	}
}
