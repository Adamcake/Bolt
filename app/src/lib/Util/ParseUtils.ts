import type { TokenSet } from '$lib/Services/AuthService';
import type { BoltEnv } from '$lib/State/Bolt';
import { error, ok, type Result } from '$lib/Util/interfaces';

export class ParseUtils {
	/**
	 * Parses the response from /oauth2/token, and returns a Session object
	 */
	static parseTokenResponse(response: string): Result<TokenSet, string> {
		try {
			const oauthCreds = JSON.parse(response);
			const sections = oauthCreds.id_token.split('.');
			if (sections.length !== 3) {
				const msg: string = `Malformed id_token: ${sections.length} sections, expected 3`;
				return error(msg);
			}
			const header = JSON.parse(atob(sections[0]));
			if (header.typ !== 'JWT') {
				const msg: string = `Bad id_token header: typ ${header.typ}, expected JWT`;
				return error(msg);
			}
			const payload = JSON.parse(atob(sections[1]));
			return ok({
				access_token: oauthCreds.access_token,
				id_token: oauthCreds.id_token,
				refresh_token: oauthCreds.refresh_token,
				sub: payload.sub,
				expiry: Date.now() + oauthCreds.expires_in * 1000
			});
		} catch (e) {
			return error('Unable to parse token response');
		}
	}

	static parseSessionResponse(response: string): Result<string, string> {
		try {
			const parsed = JSON.parse(response);
			if (parsed.sessionId) {
				return ok(parsed.sessionId);
			} else {
				return error('sessionId does not exist on parsed object');
			}
		} catch (e) {
			return error('Unable to parse session response');
		}
	}

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
