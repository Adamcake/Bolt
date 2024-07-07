import { logger } from '$lib/Util/Logger';
import { AuthService, type AuthTokens, type Session } from '$lib/Services/AuthService';
import { error, ok, type Result } from '$lib/Util/interfaces';
import { UserService, type Profile } from '$lib/Services/UserService';
import { GlobalState } from '$lib/State/GlobalState';
import { get } from 'svelte/store';
import { selectFirstProfile } from '$lib/Util/ConfigUtils';

let saveInProgress: boolean = false;

export class BoltService {
	static async login(session_id: string, authTokens: AuthTokens): Promise<Result<Profile, string>> {
		const profileResult = await UserService.buildProfile(
			authTokens.sub,
			authTokens.access_token,
			session_id
		);
		if (!profileResult.ok) {
			return error(`Unable to build user profile: ${profileResult.error}`);
		}
		const newProfile = profileResult.value;
		GlobalState.profiles.update((profiles) => {
			profiles.push(newProfile);
			return profiles;
		});
		const session = { session_id, tokens: authTokens };
		const existingSession = BoltService.findSession(authTokens.sub);
		if (!existingSession) GlobalState.sessions.push(session);
		BoltService.saveCredentials();
		return ok(profileResult.value);
	}

	static async logout(sub: string): Promise<Session[]> {
		const { sessions, profiles } = GlobalState;
		const sessionIndex = sessions.findIndex((session) => session.tokens.sub === sub);
		if (sessionIndex > -1) {
			const { access_token } = GlobalState.sessions[sessionIndex].tokens;
			AuthService.revokeOauthCreds(access_token);
			sessions.splice(sessionIndex, 1);
		}
		const profileIndex = get(profiles).findIndex((profile) => profile.user.userId === sub);
		if (profileIndex > -1) {
			profiles.update((_profiles) => {
				_profiles.splice(profileIndex, 1);
				return _profiles;
			});
		}
		selectFirstProfile();
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

			// TODO: figure out why this was here, and how to re-implement it
			// selectedPlay.update((data) => {
			// 	data.credentials = credentialsSub.get(<string>selectedPlaySub.account?.userId);
			// 	return data;
			// });

			const sessions = GlobalState.sessions;
			xml.send(JSON.stringify(sessions));
		});
	}

	static findProfile(userId: string): Profile | undefined {
		const profiles = get(GlobalState.profiles);
		return profiles.find((profile) => profile.user.userId === userId);
	}

	static findSession(sub: string): Session | undefined {
		return GlobalState.sessions.find((session) => session.tokens.sub === sub);
	}
}
