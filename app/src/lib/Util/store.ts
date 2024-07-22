import type { GameClient } from '$lib/Util/interfaces';
import { writable, type Writable } from 'svelte/store';

// TODO: remove clientListPromise
export const clientListPromise: Writable<Promise<GameClient[]>> = writable();
