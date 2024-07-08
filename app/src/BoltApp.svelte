<script lang="ts">
	import TopBar from '$lib/Components/TopBar.svelte';
	import Auth from '@/AuthApp.svelte';
	import PluginMenu from '$lib/Components/PluginMenu.svelte';
	import LogView from '$lib/Components/LogView.svelte';
	import { logger } from '$lib/Util/Logger';
	import DisclaimerModal from '$lib/Components/DisclaimerModal.svelte';
	import { BoltService } from '$lib/Services/BoltService';
	import { AuthService } from '$lib/Services/AuthService';
	import MainLayout from '$lib/Components/MainLayout.svelte';

	let showPluginMenu: boolean = false;

	const logs = logger.logs;
</script>

<svelte:window on:beforeunload={() => BoltService.saveConfig()} />

<MainLayout>
	{#if AuthService.authenticating}
		<Auth></Auth>
	{:else}
		{#if showPluginMenu}
			<PluginMenu bind:showPluginMenu></PluginMenu>
		{/if}
		<DisclaimerModal />
		<TopBar></TopBar>
		<div class="mt-16 grid h-full grid-flow-col grid-cols-3">
			<div></div>
			<!-- <Launch bind:showPluginMenu></Launch> -->
			<div></div>
		</div>
		<LogView logs={$logs} />
	{/if}
</MainLayout>
