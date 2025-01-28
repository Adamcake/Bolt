import type { Session } from '$lib/Services/UserService';
import { defaultConfig, type Config } from '$lib/Util/Config';
import { writable } from 'svelte/store';

export const GlobalState = {
	initialized: writable(false),
	configHasPendingChanges: false,
	pluginConfigHasPendingChanges: false,
	config: writable<Config>(defaultConfig),
	sessions: writable<Session[]>([])
};
