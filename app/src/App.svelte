<script lang="ts">
	import TopBar from './lib/TopBar.svelte';
	import Disclaimer from './lib/Disclaimer.svelte';
	import Settings from './lib/Settings.svelte';
	import Messages from './lib/Messages.svelte';
	import Launch from './lib/Launch.svelte';
	import Auth from './lib/Auth.svelte';
	import { url_search_params } from './functions';
	import { show_disclaimer } from './store';

	let show_settings: boolean = false;
	let authorizing: boolean = false;

	url_search_params();

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
		{#if $show_disclaimer}
			<Disclaimer></Disclaimer>
		{/if}
		{#if show_settings}
			<Settings bind:show_settings></Settings>
		{/if}
		<TopBar bind:show_settings></TopBar>
		<div class="mt-16 grid h-full grid-flow-col grid-cols-3">
			<div></div>
			<Launch></Launch>
			<div></div>
		</div>
		<Messages></Messages>
	{/if}
</main>
