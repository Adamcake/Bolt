import type { Writable } from 'svelte/store';

export function onWritableChange<T>(store: Writable<T>, fn: (newValue: T) => void) {
	let initialized = false;
	return store.subscribe((value) => {
		if (initialized) {
			fn(value);
		} else {
			initialized = true;
		}
	});
}
