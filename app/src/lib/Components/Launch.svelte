<script lang="ts">
	import {
		launchHdos,
		launchOfficialClient,
		launchRS3Linux,
		launchRuneLite
	} from '$lib/Util/functions';
	import { Client, clientMap, Game } from '$lib/Util/interfaces';
	import { logger } from '$lib/Util/Logger';
	import { bolt, Platform } from '$lib/State/Bolt';
	import { GlobalState } from '$lib/State/GlobalState';
	import { BoltService } from '$lib/Services/BoltService';

	export let showPluginMenu = false;

	let { config } = GlobalState;

	// when play is clicked, check the selected_play store for all relevant details
	// calls the appropriate launch functions
	function launch(game: Game, client: Client): void {
		if (!$config.selected_user_id) {
			return logger.warn('Please log in to launch a client');
		}
		const session = BoltService.findSession($config.selected_user_id);
		if (!session) return logger.warn('Unable to launch game, session was not found.');
		const { session_id, tokens } = session;

		switch (game) {
			case Game.osrs:
				switch (client) {
					case Client.official:
						launchOfficialClient(
							bolt.platform === 'windows',
							true,
							<string>$selectedPlay.credentials?.session_id,
							<string>$selectedPlay.character?.accountId,
							<string>$selectedPlay.character?.displayName
						);
						break;
					case Client.runelite:
						launchRuneLite(
							<string>$selectedPlay.credentials?.session_id,
							<string>$selectedPlay.character?.accountId,
							<string>$selectedPlay.character?.displayName
						);
						break;
					case Client.hdos:
						launchHdos(
							<string>$selectedPlay.credentials?.session_id,
							<string>$selectedPlay.character?.accountId,
							<string>$selectedPlay.character?.displayName
						);
						break;
				}
				break;
			case Game.rs3:
				if (bolt.platform === 'linux') {
					launchRS3Linux(
						<string>$selectedPlay.credentials?.session_id,
						<string>$selectedPlay.character?.accountId,
						<string>$selectedPlay.character?.displayName
					);
				} else {
					launchOfficialClient(
						bolt.platform === 'windows',
						false,
						<string>$selectedPlay.credentials?.session_id,
						<string>$selectedPlay.character?.accountId,
						<string>$selectedPlay.character?.displayName
					);
				}
				break;
		}
	}
</script>

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
		<label>
			<span class="text-sm">Game Client</span>
			<select
				id="game_client_select"
				class="mx-auto w-52 cursor-pointer rounded-lg border-2 border-slate-300 bg-inherit p-2 text-inherit duration-200 hover:opacity-75 dark:border-slate-800"
				bind:value={$config.selected_client}
			>
				{#each clientMap[$config.selected_game] as client}
					<option
						disabled={client === Client.official && bolt.platform !== Platform.Linux}
						value={client}>{client}</option
					>
				{/each}
			</select>
		</label>
		<!-- {#if $selectedPlay.game == Game.osrs}
			<label for="game_client_select" class="text-sm">Game Client</label>
			<br />
			<select
				id="game_client_select"
				class="mx-auto w-52 cursor-pointer rounded-lg border-2 border-slate-300 bg-inherit p-2 text-inherit duration-200 hover:opacity-75 dark:border-slate-800"
				bind:this={clientSelect}
				on:change={clientChanged}
			>
				{#if bolt.platform !== 'linux'}
					<option data-id={Client.osrs} class="dark:bg-slate-900">OSRS Client</option>
				{/if}
				<option data-id={Client.runeLite} class="dark:bg-slate-900">RuneLite</option>
				<option data-id={Client.hdos} class="dark:bg-slate-900">HDOS</option>
			</select>
		{:else if $selectedPlay.game == Game.rs3}
			<button
				disabled={!bolt.hasBoltPlugins}
				title={bolt.hasBoltPlugins ? null : 'Coming soon...'}
				class="mx-auto mb-2 w-52 rounded-lg p-2 font-bold text-black duration-200 enabled:bg-blue-500 enabled:hover:opacity-75 disabled:bg-gray-500"
				on:click={() => {
					showPluginMenu = bolt.hasBoltPlugins ?? false;
				}}
			>
				Plugin menu
			</button>
		{/if} -->
	</div>
	<div class="mx-auto my-2">
		<label for="character_select" class="text-sm"> Character</label>
		<br />
		<select
			id="character_select"
			class="mx-auto w-52 cursor-pointer rounded-lg border-2 border-slate-300 bg-inherit p-2 text-inherit duration-200 hover:opacity-75 dark:border-slate-800"
			bind:this={characterSelect}
			on:change={() => characterChanged()}
		>
			{#if $selectedPlay.account}
				{#each $selectedPlay.account.characters as character}
					<option data-id={character[1].accountId} class="dark:bg-slate-900">
						{#if character[1].displayName}
							{character[1].displayName}
						{:else}
							New Character
						{/if}
					</option>
				{/each}
			{/if}
		</select>
	</div>
</div>
