import { readable, writable, type Readable, type Writable } from 'svelte/store';
import {
	type Message,
	type Credentials,
	type Config,
	type Auth,
	type Bolt,
	type Account,
	type SelectedPlay,
	config_defaults,
	Game,
	Client
} from './interfaces';

// readable stores. known at starup
export const internal_url: Readable<string> = readable('https://bolt-internal');
export const production_client_id: Readable<string> = readable(
	'1fddee4e-b100-4f4e-b2b0-097f9088f9d2'
);

// writable stores
export const bolt: Writable<Bolt> = writable();
export const platform: Writable<string | null> = writable('');
export const config: Writable<Config> = writable({ ...config_defaults });
export const credentials: Writable<Map<string, Credentials>> = writable(new Map());
export const message_list: Writable<Array<Message>> = writable([]);
export const pending_oauth: Writable<Auth | null> = writable({});
export const pending_game_auth: Writable<Array<Auth>> = writable([]);
export const rs3_installed_hash: Writable<string | null> = writable('');
export const runelite_installed_id: Writable<string | null> = writable('');
export const hdos_installed_version: Writable<string | null> = writable('');
export const is_config_dirty: Writable<boolean> = writable(false);
export const account_list: Writable<Map<string, Account>> = writable(new Map());
export const selected_play: Writable<SelectedPlay> = writable({
	game: Game.OSRS,
	client: Client.RuneLite
});
export const show_disclaimer: Writable<boolean> = writable(false);
