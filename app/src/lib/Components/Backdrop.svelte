<script lang="ts">
	import { createEventDispatcher, type EventDispatcher } from 'svelte';

	// notifies parent that anywhere outside the parent div was clicked
	// useful to close windows when clicking outside
	const dispatch: EventDispatcher<{ click: MouseEvent }> = createEventDispatcher();
</script>

<div>
	<div class="absolute left-0 top-0 z-10 h-screen w-screen backdrop-blur-sm backdrop-filter"></div>
	<div
		class="absolute left-0 top-0 z-10 h-screen w-screen bg-black opacity-75"
		role="none"
		on:click={(e) => {
			dispatch('click', e);
		}}
	></div>
</div>
