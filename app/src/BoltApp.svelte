<script lang="ts">
	import DisclaimerModal from '$lib/Components/Modals/DisclaimerModal.svelte';
	import Launch from '$lib/Components/Launch.svelte';
	import LogView from '$lib/Components/LogView.svelte';
	import MainLayout from '$lib/Components/MainLayout.svelte';
	import TopBar from '$lib/Components/TopBar.svelte';
	import { logger } from '$lib/Util/Logger';

	const logs = logger.logs;
	let psa: string | null = null;
</script>

<MainLayout>
	<DisclaimerModal />
	<TopBar {psa}></TopBar>
	<div class="mt-16 grid h-full grid-flow-col grid-cols-3">
		<div></div>
		<Launch bind:psa></Launch>
		<div></div>
	</div>
	<LogView logs={$logs} />
</MainLayout>
