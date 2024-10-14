import type { GameClient } from '$lib/Util/interfaces';
import { writable, type Writable } from 'svelte/store';

// TODO: remove clientList
export const clientList: Writable<GameClient[]> = writable([]);

// used by launchRunelite, should be moved to wherever that function goes if we get rid of functions.ts
export const runeliteLastUpdateCheck: Writable<number | null> = writable(null);
