import type { AuthTokens } from '$lib/Services/AuthService';

export type Result<T, E = Error> = { ok: true; value: T } | { ok: false; error: E };

export function ok<T>(value: T): Result<T, never> {
	return { ok: true, value };
}

export function error<T>(error: T): Result<never, T> {
	return { ok: false, error };
}

export enum Game {
	rs3 = 'rs3',
	osrs = 'osrs'
}

export enum Client {
	official = 'Official',
	runelite = 'RuneLite',
	hdos = 'HDOS'
}

export const clientMap: Record<Game, Client[]> = {
	[Game.rs3]: [Client.official],
	[Game.osrs]: [Client.official, Client.runelite, Client.hdos]
};

// response token from the official "Direct6" URL
export interface Direct6Token {
	id?: string;
	version?: string;
	promoteTime?: number;
	scanTime?: number;
}

// a specific instance of a currently-running rs3 plugin, associated with a GameClient
// "id" is the UUID string of the installed plugin, uid uniquely identifies this specific instance of it
export interface GameClientPlugin {
	id: string;
	uid: number;
}

// connected game client for plugin management purposes
export interface GameClient {
	uid: number;
	identity?: string;
	plugins: GameClientPlugin[];
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

interface AuthTokenUpdateMessage {
	type: 'authTokenUpdate';
	tokens: AuthTokens;
}

interface AuthSessionUpdateMessage {
	type: 'authSessionUpdate';
	sessionId: string;
}

interface AuthFailedMessage {
	type: 'authFailed';
	reason: string;
}

interface ExternalUrlMessage {
	type: 'externalUrl';
	url: string;
}

interface GameClientListMessage {
	type: 'gameClientList';
	clients: GameClient[];
}

export type BoltMessage =
	| AuthTokenUpdateMessage
	| AuthSessionUpdateMessage
	| AuthFailedMessage
	| ExternalUrlMessage
	| GameClientListMessage;
