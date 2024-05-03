<script lang="ts">
	import { afterUpdate, onMount } from 'svelte';
	import { get } from 'svelte/store';
	import { launchHdos, launchRS3Linux, launchRuneLite } from '../functions';
	import { Client, Game } from '../interfaces';
	import { msg } from '../main';
	import { config, hasBoltPlugins, isConfigDirty, selectedPlay } from '../store';

	export let showPluginMenu = false;

	let characterSelect: HTMLSelectElement;
	let clientSelect: HTMLSelectElement;

	// update selected_play store
	export function characterChanged(): void {
		if (!$selectedPlay.account) return;

		const key: string = <string>(
			characterSelect[characterSelect.selectedIndex].getAttribute('data-id')
		);
		$selectedPlay.character = $selectedPlay.account.characters.get(key);
		if ($selectedPlay.character) {
			$config.selected_characters?.set(
				$selectedPlay.account.userId,
				$selectedPlay.character?.accountId
			);
		}
		$isConfigDirty = true;
	}

	// update selected_play
	function clientChanged(): void {
		if (clientSelect.value == 'RuneLite') {
			$selectedPlay.client = Client.runeLite;
			$config.selected_client_index = Client.runeLite;
		} else if (clientSelect.value == 'HDOS') {
			$selectedPlay.client = Client.hdos;
			$config.selected_client_index = Client.hdos;
		}
		$isConfigDirty = true;
	}

	// when play is clicked, check the selected_play store for all relevant details
	// calls the appropriate launch functions
	function play_clicked(): void {
		if (!$selectedPlay.account || !$selectedPlay.character) {
			msg('Please log in to launch a client');
			return;
		}
		switch ($selectedPlay.game) {
			case Game.osrs:
				if ($selectedPlay.client == Client.runeLite) {
					launchRuneLite(
						<string>$selectedPlay.credentials?.session_id,
						<string>$selectedPlay.character?.accountId,
						<string>$selectedPlay.character?.displayName
					);
				} else if ($selectedPlay.client == Client.hdos) {
					launchHdos(
						<string>$selectedPlay.credentials?.session_id,
						<string>$selectedPlay.character?.accountId,
						<string>$selectedPlay.character?.displayName
					);
				}
				break;
			case Game.rs3:
				launchRS3Linux(
					<string>$selectedPlay.credentials?.session_id,
					<string>$selectedPlay.character?.accountId,
					<string>$selectedPlay.character?.displayName
				);
				break;
		}
	}

	afterUpdate(() => {
		if ($selectedPlay.game == Game.osrs && $selectedPlay.client) {
			clientSelect.selectedIndex = $selectedPlay.client;
		}
		if (
			$selectedPlay.account &&
			$config.selected_characters?.has($selectedPlay.account.userId)
		) {
			for (let i = 0; i < characterSelect.options.length; i++) {
				if (
					characterSelect[i].getAttribute('data-id') ==
					$config.selected_characters.get($selectedPlay.account.userId)
				) {
					characterSelect.selectedIndex = i;
				}
			}
		}
	});

	onMount(() => {
		if ($config.selected_game_index == Game.osrs) {
			clientSelect.selectedIndex = <number>$config.selected_client_index;
			$selectedPlay.client = clientSelect.selectedIndex;
		}
	});
</script>

<div class="bg-grad flex h-full flex-col border-slate-300 p-5 duration-200 dark:border-slate-800">
	<img
		src="svgs/rocket-solid.svg"
		alt="Launch icon"
		class="mx-auto mb-5 w-24 rounded-3xl bg-gradient-to-br from-rose-500 to-violet-500 p-5" />
	<button
		class="mx-auto mb-2 w-52 rounded-lg bg-emerald-500 p-2 font-bold text-black duration-200 hover:opacity-75"
		on:click={play_clicked}>
		Play
	</button>
	<div class="mx-auto my-2">
		{#if $selectedPlay.game == Game.osrs}
			<label for="game_client_select" class="text-sm">Game Client</label>
			<br />
			<select
				id="game_client_select"
				class="mx-auto w-52 cursor-pointer rounded-lg border-2 border-slate-300 bg-inherit p-2 text-inherit duration-200 hover:opacity-75 dark:border-slate-800"
				bind:this={clientSelect}
				on:change={clientChanged}>
				<option data-id={Client.runeLite} class="dark:bg-slate-900">RuneLite</option>
				<option data-id={Client.hdos} class="dark:bg-slate-900">HDOS</option>
			</select>
		{:else if $selectedPlay.game == Game.rs3}
			<button
				disabled={!get(hasBoltPlugins)}
				class="mx-auto mb-2 w-52 rounded-lg p-2 font-bold text-black duration-200 enabled:bg-blue-500 enabled:hover:opacity-75 disabled:bg-gray-500"
				on:click={() => {
					showPluginMenu = get(hasBoltPlugins) ?? false;
				}}>
				Plugin menu
			</button>
		{/if}
	</div>
	<div class="mx-auto my-2">
		<label for="character_select" class="text-sm"> Character</label>
		<br />
		<select
			id="character_select"
			class="mx-auto w-52 cursor-pointer rounded-lg border-2 border-slate-300 bg-inherit p-2 text-inherit duration-200 hover:opacity-75 dark:border-slate-800"
			bind:this={characterSelect}
			on:change={() => characterChanged()}>
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
