<script lang="ts">
	import { createEventDispatcher, type EventDispatcher } from 'svelte';

	// notifies parent that anywhere outside the parent div was clicked
	// useful to close windows when clicking outside
	const dispatch: EventDispatcher<any> = createEventDispatcher();
</script>

<div>
	<div class="w-screen h-screen absolute left-0 top-0 backdrop-filter backdrop-blur-sm z-10">
	</div>
	<div
		class="absolute left-0 top-0 w-screen h-screen z-10 bg-black opacity-75"
		role="none"
		on:click={() => {
			dispatch('click');
		}}>
	</div>
</div>
