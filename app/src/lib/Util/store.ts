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

export const clientListPromise: Writable<Promise<GameClient[]>> = writable();
export const accountList: Writable<Map<string, Account>> = writable(new Map());
export const selectedPlay: Writable<SelectedPlay> = writable({
	game: Game.osrs,
	client: Client.runeLite
});
