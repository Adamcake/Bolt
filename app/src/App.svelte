<script lang="ts">
	import TopBar from '$lib/Components/TopBar.svelte';
	import Disclaimer from '$lib/Components/Disclaimer.svelte';
	import Settings from '$lib/Components/Settings.svelte';
	import Messages from '$lib/Components/Messages.svelte';
	import Launch from '$lib/Components/Launch.svelte';
	import Auth from '$lib/Components/Auth.svelte';
	import PluginMenu from '$lib/Components/PluginMenu.svelte';
	import { urlSearchParams } from '$lib/Util/functions';
	import { showDisclaimer } from '$lib/Util/store';

	let showSettings: boolean = false;
	let showPluginMenu: boolean = false;
	let authorizing: boolean = false;

	urlSearchParams();

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

<main class="h-full">
	{#if authorizing}
		<Auth></Auth>
	{:else}
		{#if showPluginMenu}
			<PluginMenu bind:showPluginMenu></PluginMenu>
		{/if}
		{#if showSettings}
			<Settings bind:showSettings></Settings>
		{/if}
		{#if $showDisclaimer}
			<Disclaimer></Disclaimer>
		{/if}
		<TopBar bind:showSettings></TopBar>
		<div class="mt-16 grid h-full grid-flow-col grid-cols-3">
			<div></div>
			<Launch bind:showPluginMenu></Launch>
			<div></div>
		</div>
		<Messages></Messages>
	{/if}
</main>
