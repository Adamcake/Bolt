<script lang="ts">
	import Backdrop from '$lib/Components/Backdrop.svelte';
	import { showDisclaimer } from '$lib/Util/store';

	// atob encodings created from btoa; for disclaimer text
	const firstText: string = atob(
		'Qm9sdCBpcyBhbiB1bm9mZmljaWFsIHRoaXJkLXBhcnR5IGxhdW5jaGVyLiBJdCdzIGZyZWUgYW5kIG9wZW4tc291cmNlIHNvZnR3YXJlIGxpY2Vuc2VkIHVuZGVyIEFHUEwgMy4wLg=='
	);
	const secondText: string = atob(
		'SmFnZXggaXMgbm90IHJlc3BvbnNpYmxlIGZvciBhbnkgcHJvYmxlbXMgb3IgZGFtYWdlIGNhdXNlZCBieSB1c2luZyB0aGlzIHByb2R1Y3Qu'
	);
</script>

<div id="disclaimer">
	<Backdrop></Backdrop>
	<div
		class="absolute left-1/4 top-1/4 z-20 w-1/2 rounded-lg bg-slate-100 p-5 text-center shadow-lg dark:bg-slate-900"
	>
		<p class="p-2">
			{firstText}
		</p>
		<p class="p-2">
			{secondText}
		</p>
		<button
			class="m-5 rounded-lg border-2 border-blue-500 p-2 duration-200 hover:opacity-75"
			on:click={() => {
				$showDisclaimer = false;
			}}>I Understand</button
		>
	</div>
</div>
