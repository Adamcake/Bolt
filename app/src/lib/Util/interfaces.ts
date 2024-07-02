// file for all interfaces, types, and their helper functions

import type { Session } from '$lib/Services/AuthService';

// result type, similar to rust's implementation
// useful if a function may succeed or fail
export type Result<T, E = Error> = { ok: true; value: T } | { ok: false; error: E };

// wraps a type in Result
export function wrap<T, E = Error>(value: T): Result<T, E> {
	return { value, ok: true };
}

// unwraps the value within a Result
export function unwrap<T, E = Error>(result: Result<T, E>): T {
	if (result.ok) {
		return result.value;
	} else {
		throw result.error;
	}
}

export function ok<T>(value: T): Result<T, never> {
	return { ok: true, value };
}

export function error<T>(error: T): Result<never, T> {
	return { ok: false, error };
}

// game enum
export enum Game {
	rs3,
	osrs
}

// client enum, maybe this list will grow as clients are added
export enum Client {
	osrs,
	runeLite,
	hdos,
	rs3
}

// account info
export interface Account {
	id: string;
	userId: string;
	displayName: string;
	suffix: string;
	characters: Map<string, Character>;
}

// character info, an account may have multiple characters
export interface Character {
	accountId: string;
	displayName: string;
	userHash: string;
}

// this is the referenced object when wanting to launch any client
// these values are changed across the components to ensure selected choices are used
export interface SelectedPlay {
	account?: Account;
	character?: Character;
	credentials?: Session;
	game?: Game;
	client?: Client;
}

// response token from the official "Direct6" URL
export interface Direct6Token {
	id?: string;
	version?: string;
	promoteTime?: number;
	scanTime?: number;
}

// connected game client for plugin management purposes
export interface GameClient {
	uid: string;
	identity?: string;
}

// rs3 plugin configured in plugins.json
export interface PluginMeta {
	path?: string;
	name?: string;
}

// rs3 plugin config, from the plugin's bolt.json
export interface PluginConfig {
	name?: string;
	version?: string;
	description?: string;
	main?: string;
}
