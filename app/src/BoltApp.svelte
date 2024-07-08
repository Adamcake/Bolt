<script lang="ts">
	import TopBar from '$lib/Components/TopBar.svelte';
	import Auth from '@/AuthApp.svelte';
	import LogView from '$lib/Components/LogView.svelte';
	import { logger } from '$lib/Util/Logger';
	import DisclaimerModal from '$lib/Components/DisclaimerModal.svelte';
	import { BoltService } from '$lib/Services/BoltService';
	import { AuthService } from '$lib/Services/AuthService';
	import MainLayout from '$lib/Components/MainLayout.svelte';
	import Launch from '$lib/Components/Launch.svelte';

	const logs = logger.logs;
</script>

<svelte:window on:beforeunload={() => BoltService.saveConfig()} />

<MainLayout>
	{#if AuthService.authenticating}
		<Auth></Auth>
	{:else}
		<DisclaimerModal />
		<TopBar></TopBar>
		<div class="mt-16 grid h-full grid-flow-col grid-cols-3">
			<div></div>
			<Launch></Launch>
			<div></div>
		</div>
		<LogView logs={$logs} />
	{/if}
</MainLayout>
