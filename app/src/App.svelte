<script lang="ts">
	import TopBar from '$lib/Components/TopBar.svelte';
	import Launch from '$lib/Components/Launch.svelte';
	import Auth from '$lib/Components/Auth.svelte';
	import PluginMenu from '$lib/Components/PluginMenu.svelte';
	import LogView from '$lib/Components/LogView.svelte';
	import { logger } from '$lib/Util/Logger';
	import { config, showDisclaimer } from '$lib/Util/store';
	import DisclaimerModal from '$lib/Components/DisclaimerModal.svelte';
	import { BoltService } from '$lib/Services/BoltService';

	let showPluginMenu: boolean = false;
	let authorizing: boolean = false;
	$: darkTheme = $config.use_dark_theme;

	const logs = logger.logs;

	// called from cxx code in the url, to send message from auth window to main window
	const parentWindow = window.opener || window.parent;
	if (parentWindow) {
		const searchParams = new URLSearchParams(window.location.search);

		if (searchParams.get('id_token')) {
			authorizing = true;
			parentWindow.postMessage(
				{
					type: 'gameSessionServerAuth',
					code: searchParams.get('code'),
					id_token: searchParams.get('id_token'),
					state: searchParams.get('state')
				},
				'*'
			);
		} else if (searchParams.get('code')) {
			authorizing = true;
			parentWindow.postMessage(
				{
					type: 'authCode',
					code: searchParams.get('code'),
					state: searchParams.get('state')
				},
				'*'
			);
		}
	}
</script>

<svelte:window on:beforeunload={() => BoltService.saveConfig($config)} />

<main
	class:dark={darkTheme}
	class="fixed top-0 h-screen w-screen bg-slate-100 text-xs text-slate-900 duration-200 sm:text-sm md:text-base dark:bg-slate-900 dark:text-slate-50"
>
	{#if authorizing}
		<Auth></Auth>
	{:else}
		{#if showPluginMenu}
			<PluginMenu bind:showPluginMenu></PluginMenu>
		{/if}
		{#if showDisclaimer}
			<!-- TODO: remove showDisclaimer -->
			<DisclaimerModal></DisclaimerModal>
		{/if}
		<TopBar></TopBar>
		<div class="mt-16 grid h-full grid-flow-col grid-cols-3">
			<div></div>
			<Launch bind:showPluginMenu></Launch>
			<div></div>
		</div>
		<LogView logs={$logs} />
	{/if}
</main>
