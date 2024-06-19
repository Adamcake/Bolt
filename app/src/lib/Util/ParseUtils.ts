import type { Credentials } from '$lib/Services/AuthService';
import { logger } from '$lib/Util/Logger';
import type { Bolt, Result } from '$lib/Util/interfaces';

export class ParseUtils {
	// parses a response from the oauth endpoint
	// returns a Result, it may
	static parseCredentials(str: string): Result<Credentials> {
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

	// The bolt object from the C++ app has its values base64 encoded.
	// This will decode each value, and return the original structure
	static decodeBolt(encoded: Bolt): Bolt {
		const decodedObject: Record<string, string | string[]> = {};
		for (const _key in encoded) {
			const key = _key as keyof Bolt;
			const value = encoded[key];
			if (typeof value === 'string') {
				decodedObject[key] = atob(value);
			} else if (Array.isArray(value) && value.every((item) => typeof item === 'string')) {
				decodedObject[key] = value.map((item) => atob(item));
			} else {
				decodedObject[key] = value;
			}
		}

		return decodedObject as unknown as Bolt;
	}
}
