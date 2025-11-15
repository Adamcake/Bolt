<script lang="ts">
	import DisclaimerModal from '$lib/Components/Modals/DisclaimerModal.svelte';
	import Launch from '$lib/Components/Launch.svelte';
	import LogView from '$lib/Components/LogView.svelte';
	import MainLayout from '$lib/Components/MainLayout.svelte';
	import TopBar from '$lib/Components/TopBar.svelte';
	import { logger } from '$lib/Util/Logger';

	const logs = logger.logs;
</script>

<MainLayout>
	<DisclaimerModal />
	<TopBar></TopBar>
	<div class="mt-16 grid h-full grid-flow-col grid-cols-3">
		<div></div>
		<Launch></Launch>
		<div></div>
	</div>
	<LogView logs={$logs} />
</MainLayout>
