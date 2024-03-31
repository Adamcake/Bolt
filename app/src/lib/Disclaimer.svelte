<script lang="ts">
	import Backdrop from './Backdrop.svelte';

	// user must click 'I Understand' to close
	export let show_dislaimer: boolean;

	// atob encodings created from btoa; for disclaimer text
	const first_text: string = atob(
		'Qm9sdCBpcyBhbiB1bm9mZmljaWFsIHRoaXJkLXBhcnR5IGxhdW5jaGVyLiBJdCdzIGZyZWUgYW5kIG9wZW4tc291cmNlIHNvZnR3YXJlIGxpY2Vuc2VkIHVuZGVyIEFHUEwgMy4wLg=='
	);
	const second_text: string = atob(
		'SmFnZXggaXMgbm90IHJlc3BvbnNpYmxlIGZvciBhbnkgcHJvYmxlbXMgb3IgZGFtYWdlIGNhdXNlZCBieSB1c2luZyB0aGlzIHByb2R1Y3Qu'
	);
</script>

<div id="disclaimer">
	<Backdrop></Backdrop>
	<div
		class="absolute left-1/4 top-1/4 rounded-lg shadow-lg w-1/2 p-5 z-20 text-center bg-slate-100 dark:bg-slate-900">
		<p class="p-2">
			{first_text}
		</p>
		<p class="p-2">
			{second_text}
		</p>
		<button
			class="border-2 border-blue-500 hover:opacity-75 duration-200 rounded-lg m-5 p-2"
			on:click={() => {
				show_dislaimer = false;
			}}>I Understand</button>
	</div>
</div>
