import { bolt } from '$lib/State/Bolt';
import { error, ok, type Result } from '$lib/Util/interfaces';

export interface User {
	id: string;
	userId: string;
	displayName: string;
	suffix: string;
}

// in-game account details
export interface Account {
	accountId: string;
	displayName: string;
}

export interface Profile {
	user: User;
	accounts: Account[];
}

export class UserService {
	static profiles: Profile[] = [];

	static async buildProfile(
		userId: string,
		access_token: string,
		session_id: string
	): Promise<Result<Profile, string>> {
		const userResponse = await UserService.getUser(userId, access_token);

		if (!userResponse.ok) {
			return error(`Failed to fetch user. Status: ${userResponse.error}`);
		}

		const accountResponse = await UserService.getUserAccounts(session_id);

		if (!accountResponse.ok) {
			return error(`Failed to fetch game accounts. Reason: ${accountResponse.error}`);
		}

		return ok({
			user: userResponse.value,
			accounts: accountResponse.value
		});
	}

	static getUser(userId: string, access_token: string): Promise<Result<User, number>> {
		return new Promise((resolve) => {
			const url = `${bolt.env.api}/users/${userId}/displayName`;
			const xml = new XMLHttpRequest();
			xml.onreadystatechange = () => {
				if (xml.readyState == 4) {
					if (xml.status == 200) {
						const user = JSON.parse(xml.response) as User;
						resolve(ok(user));
					} else {
						resolve(error(xml.status));
					}
				}
			};
			xml.open('GET', url, true);
			xml.setRequestHeader('Authorization', 'Bearer '.concat(access_token));
			xml.send();
		});
	}

	static getUserAccounts(session_id: string): Promise<Result<Account[], string>> {
		const accountsUrl = `${bolt.env.auth_api}/accounts`;

		return new Promise((resolve) => {
			const xml = new XMLHttpRequest();
			xml.onreadystatechange = async () => {
				if (xml.readyState !== 4) return;
				if (xml.readyState == 4 && xml.status !== 200) {
					return resolve(error(`Error: from ${accountsUrl}: ${xml.status}: ${xml.response}`));
				}
			};
			xml.open('GET', accountsUrl, true);
			xml.setRequestHeader('Accept', 'application/json');
			xml.setRequestHeader('Authorization', 'Bearer '.concat(session_id));
			xml.send();
		});
	}
}
