import type { Time } from '$lib/Enums/Time';

export class CookieService {
	static set(name: string, value: string, duration: number, unit: Time): void {
		const date = new Date();
		date.setTime(date.getTime() + duration * unit);
		const expires = `expires=${date.toUTCString()}`;
		document.cookie = `${name}=${value};${expires};path=/`;
	}

	static get(name: string): string | null {
		const nameEQ = `${name}=`;
		const ca = document.cookie.split(';');
		for (let i = 0; i < ca.length; i++) {
			let c = ca[i];
			while (c.charAt(0) === ' ') c = c.substring(1, c.length);
			if (c.indexOf(nameEQ) === 0) return c.substring(nameEQ.length, c.length);
		}
		return null;
	}

	static remove(name: string): void {
		document.cookie = `${name}=; Max-Age=-99999999;`;
	}
}
