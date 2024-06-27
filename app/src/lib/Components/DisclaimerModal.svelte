<script lang="ts">
	import Modal from '$lib/Components/CommonUI/Modal.svelte';
	import { credentials } from '$lib/Util/store';
	import { onMount } from 'svelte';

	let modal: Modal;

	onMount(() => {
		if ($credentials.size == 0) {
			modal.open();
		}
	});
</script>

<Modal bind:this={modal} canSelfClose={false} class="w-1/2 text-center">
	<p class="p-2">
		{atob(
			'Qm9sdCBpcyBhbiB1bm9mZmljaWFsIHRoaXJkLXBhcnR5IGxhdW5jaGVyLiBJdCdzIGZyZWUgYW5kIG9wZW4tc291cmNlIHNvZnR3YXJlIGxpY2Vuc2VkIHVuZGVyIEFHUEwgMy4wLg=='
		)}
	</p>
	<p class="p-2">
		{atob(
			'SmFnZXggaXMgbm90IHJlc3BvbnNpYmxlIGZvciBhbnkgcHJvYmxlbXMgb3IgZGFtYWdlIGNhdXNlZCBieSB1c2luZyB0aGlzIHByb2R1Y3Qu'
		)}
	</p>
	<button
		class="m-5 rounded-lg border-2 border-blue-500 p-2 duration-200 hover:opacity-75"
		on:click={() => modal.close()}>I Understand</button
	>
</Modal>
