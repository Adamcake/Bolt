import type { AuthTokens } from '$lib/Services/AuthService';
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
	userHash: string;
}

export interface Session {
	user: User;
	accounts: Account[];
	tokens: AuthTokens;
	session_id: string;
}

export class UserService {
	static async buildSession(
		tokens: AuthTokens,
		session_id: string
	): Promise<Result<Session, string>> {
		const userResult = await UserService.getUser(tokens.sub, tokens.access_token);
		if (!userResult.ok) {
			return error(`Failed to fetch user. Status: ${userResult.error}`);
		}

		const accountResult = await UserService.getUserAccounts(session_id);
		if (!accountResult.ok) {
			return error(`Failed to fetch game accounts. Status: ${accountResult.error}`);
		}

		return ok({
			user: userResult.value,
			accounts: accountResult.value,
			tokens: tokens,
			session_id: session_id
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

	static getUserAccounts(session_id: string): Promise<Result<Account[], number>> {
		const accountsUrl = `${bolt.env.auth_api}/accounts`;

		return new Promise((resolve) => {
			const xml = new XMLHttpRequest();
			xml.onreadystatechange = async () => {
				if (xml.readyState == 4) {
					if (xml.status == 200) {
						const accounts = JSON.parse(xml.response) as Account[];
						resolve(ok(accounts));
					} else {
						resolve(error(xml.status));
					}
				}
			};
			xml.open('GET', accountsUrl, true);
			xml.setRequestHeader('Accept', 'application/json');
			xml.setRequestHeader('Authorization', 'Bearer '.concat(session_id));
			xml.send();
		});
	}
}
