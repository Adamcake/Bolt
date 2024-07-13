<script lang="ts">
	import PluginModal from '$lib/Components/PluginModal.svelte';
	import { BoltService } from '$lib/Services/BoltService';
	import { bolt, Platform } from '$lib/State/Bolt';
	import { GlobalState } from '$lib/State/GlobalState';
	import {
		launchHdos,
		launchOfficialClient,
		launchRS3Linux,
		launchRuneLite
	} from '$lib/Util/functions';
	import { Client, clientMap, Game } from '$lib/Util/interfaces';
	import { logger } from '$lib/Util/Logger';

	let pluginModal: PluginModal;
	let { config } = GlobalState;
	$: accounts = BoltService.findSession($config.selected_user_id ?? '')?.accounts ?? [];

	// when play is clicked, check the selected_play store for all relevant details
	// calls the appropriate launch functions
	function launch(game: Game, client: Client): void {
		if (!$config.selected_user_id || !$config.selected_account_id) {
			return logger.warn('Please log in to launch a client');
		}
		const session = BoltService.findSession($config.selected_user_id);
		if (!session) return logger.warn('Unable to launch game, session was not found.');
		const { session_id } = session;
		const account = BoltService.findAccount(session.accounts, $config.selected_account_id);
		if (!account) return logger.warn('Unable to launch game, account was not found.');
		const { accountId, displayName } = account;
		const isWindows = bolt.platform === Platform.Windows;
		const isLinux = bolt.platform === Platform.Linux;
		switch (game) {
			case Game.osrs:
				switch (client) {
					case Client.official:
						launchOfficialClient(isWindows, true, session_id, accountId, displayName);
						break;
					case Client.runelite:
						launchRuneLite(session_id, accountId, displayName, false);
						break;
					case Client.hdos:
						launchHdos(session_id, accountId, displayName);
						break;
				}
				break;
			case Game.rs3:
				if (isLinux) {
					launchRS3Linux(session_id, accountId, displayName);
				} else {
					launchOfficialClient(isWindows, false, session_id, accountId, displayName);
				}
				break;
		}
	}
</script>

{#if bolt.hasBoltPlugins}
	<PluginModal bind:this={pluginModal}></PluginModal>
{/if}

<div class="bg-grad flex h-full flex-col border-slate-300 p-5 duration-200 dark:border-slate-800">
	<img
		src="svgs/rocket-solid.svg"
		alt="Launch icon"
		class="mx-auto mb-5 w-24 rounded-3xl bg-gradient-to-br from-rose-500 to-violet-500 p-5"
	/>
	<button
		class="mx-auto mb-2 w-52 rounded-lg bg-emerald-500 p-2 font-bold text-black duration-200 hover:opacity-75"
		on:click={() => launch($config.selected_game, $config.selected_client)}
	>
		Play
	</button>
	<div class="mx-auto my-2">
		{#if $config.selected_game == Game.osrs}
			<label>
				<span class="text-sm">Game Client</span>
				<select
					id="game_client_select"
					class="mx-auto w-52 cursor-pointer rounded-lg border-2 border-slate-300 bg-inherit p-2 text-inherit duration-200 hover:opacity-75 dark:border-slate-800"
					bind:value={$config.selected_client}
				>
					{#each clientMap[$config.selected_game] as client}
						<option
							class="dark:bg-slate-900"
							disabled={client === Client.official && bolt.platform !== Platform.Linux}
							value={client}>{client}</option
						>
					{/each}
				</select>
			</label>
		{:else if $config.selected_game === Game.rs3}
			<button
				disabled={!bolt.hasBoltPlugins}
				title={bolt.hasBoltPlugins ? null : 'Coming soon...'}
				class="mx-auto mb-2 w-52 rounded-lg p-2 font-bold text-black duration-200 enabled:bg-blue-500 enabled:hover:opacity-75 disabled:bg-gray-500"
				on:click={() => {
					pluginModal.open();
				}}
			>
				Plugin menu
			</button>
		{/if}
	</div>
	<div class="mx-auto my-2">
		<label>
			<span class="text-sm">Character</span>

			<select
				id="character_select"
				class="mx-auto w-52 cursor-pointer rounded-lg border-2 border-slate-300 bg-inherit p-2 text-inherit duration-200 hover:opacity-75 dark:border-slate-800"
				bind:value={$config.selected_account_id}
			>
				{#each accounts as account}
					<option value={account.accountId} class="dark:bg-slate-900">
						{#if account.displayName}
							{account.displayName}
						{:else}
							New Character
						{/if}
					</option>
				{:else}
					<option class="dark:bg-slate-900" disabled selected>No characters</option>
				{/each}
			</select>
		</label>
	</div>
</div>
