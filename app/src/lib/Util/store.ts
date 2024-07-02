import { readable, writable, type Readable, type Writable } from 'svelte/store';
import {
	type Account,
	type SelectedPlay,
	type GameClient,
	Game,
	Client
} from '$lib/Util/interfaces';

// readable stores. known at starup
export const internalUrl: Readable<string> = readable('https://bolt-internal');
export const productionClientId: Readable<string> = readable(
	'1fddee4e-b100-4f4e-b2b0-097f9088f9d2'
);

export const clientListPromise: Writable<Promise<GameClient[]>> = writable();
export const accountList: Writable<Map<string, Account>> = writable(new Map());
export const selectedPlay: Writable<SelectedPlay> = writable({
	game: Game.osrs,
	client: Client.runeLite
});
