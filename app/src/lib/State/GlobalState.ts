import type { Session } from '$lib/Services/UserService';
import { defaultConfig, type Config } from '$lib/Util/ConfigUtils';
import { writable } from 'svelte/store';

export const GlobalState = {
	configHasPendingChanges: false,
	config: writable<Config>(defaultConfig),
	sessions: writable<Session[]>([])
};
