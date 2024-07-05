export class LocalStorageService {
	static set<T>(key: string, value: T): void {
		const jsonValue = JSON.stringify(value);
		localStorage.setItem(key, jsonValue);
	}

	static get<T>(key: string): T | null {
		const jsonValue = localStorage.getItem(key);
		if (jsonValue) {
			try {
				return JSON.parse(jsonValue) as T;
			} catch (e) {
				console.error(`Error parsing JSON from localStorage for key "${key}":`, e);
				return null;
			}
		}
		return null;
	}

	static remove(key: string): void {
		localStorage.removeItem(key);
	}
}
