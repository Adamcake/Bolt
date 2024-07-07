<script lang="ts">
	import TopBar from '$lib/Components/TopBar.svelte';
	import Auth from '$lib/Components/Auth.svelte';
	import PluginMenu from '$lib/Components/PluginMenu.svelte';
	import LogView from '$lib/Components/LogView.svelte';
	import { logger } from '$lib/Util/Logger';
	import DisclaimerModal from '$lib/Components/DisclaimerModal.svelte';
	import { BoltService } from '$lib/Services/BoltService';
	import { AuthService } from '$lib/Services/AuthService';
	import { GlobalState } from '$lib/State/GlobalState';

	let showPluginMenu: boolean = false;
	const { config } = GlobalState;
	$: darkTheme = $config.use_dark_theme;

	const logs = logger.logs;
</script>

<svelte:window on:beforeunload={() => BoltService.saveConfig()} />

<main
	class:dark={darkTheme}
	class="fixed top-0 h-screen w-screen bg-slate-100 text-xs text-slate-900 duration-200 sm:text-sm md:text-base dark:bg-slate-900 dark:text-slate-50"
>
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
</main>
