import { readable, writable, type Readable, type Writable } from 'svelte/store';
import type { GameClient } from '$lib/Util/interfaces';

// readable stores. known at starup
export const internalUrl: Readable<string> = readable('https://bolt-internal');

// TODO: remove clientListPromise
export const clientListPromise: Writable<Promise<GameClient[]>> = writable();
