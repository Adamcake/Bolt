import type { GameClient } from '$lib/Util/Interfaces';
import { writable, type Writable } from 'svelte/store';

// TODO: remove clientList
export const clientList: Writable<GameClient[]> = writable([]);
