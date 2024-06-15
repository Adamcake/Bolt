// file for all interfaces, types, and their helper functions

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

// game enum
export enum Game {
	rs3,
	osrs
}

// client enum, maybe this list will grow as clients are added
export enum Client {
	runeLite,
	hdos,
	rs3
}

// s()
export interface Bolt {
	provider: string;
	origin: string;
	origin_2fa: string;
	redirect: string;
	clientid: string;
	api: string;
	auth_api: string;
	profile_api: string;
	shield_url: string;
	content_url: string;
	default_config_uri: string;
	games: Array<string>;
}

// load on start and save on exit
export interface Config {
	use_dark_theme?: boolean;
	rs_plugin_loader?: boolean;
	rs_config_uri?: string;
	flatpak_rich_presence?: boolean;
	runelite_use_custom_jar?: boolean;
	runelite_custom_jar?: string;
	selected_account?: string;
	selected_characters?: Map<string, string>; // account userId, then character accountId
	selected_game_accounts?: Map<string, string>; // legacy version of selected_characters
	selected_game_index?: number;
	selected_client_index?: number;
}

// if no config is loaded, these defaults are set to ensure the app runs
export const configDefaults: Pick<
	Config,
	| 'use_dark_theme'
	| 'flatpak_rich_presence'
	| 'rs_config_uri'
	| 'runelite_custom_jar'
	| 'runelite_use_custom_jar'
	| 'selected_account'
	| 'selected_characters'
	| 'selected_game_accounts'
	| 'selected_game_index'
	| 'selected_client_index'
> = {
	use_dark_theme: true,
	flatpak_rich_presence: false,
	rs_config_uri: '',
	runelite_custom_jar: '',
	runelite_use_custom_jar: false,
	selected_account: '',
	selected_characters: new Map(),
	selected_game_accounts: new Map(),
	selected_game_index: 1,
	selected_client_index: 1
};

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

// credential type, passed around often
export interface Credentials {
	access_token: string;
	id_token: string;
	refresh_token: string;
	sub: string;
	login_provider: string; // null
	expiry: number;
	session_id: string;
}

// useful for knowing if auth is current happening
export interface Auth {
	state?: string;
	nonce?: string;
	creds?: Credentials;
	win?: Window | null;
	account_info_promise?: Promise<Account>;
	verifier?: string;
}

// this is the referenced object when wanting to launch any client
// these values are changed across the components to ensure selected choices are used
export interface SelectedPlay {
	account?: Account;
	character?: Character;
	credentials?: Credentials;
	game?: Game;
	client?: Client;
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
