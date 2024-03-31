<script lang="ts">
	import { onMount } from 'svelte';
	import { launchHdos, launchRS3Linux, launchRuneLite } from '../functions';
	import { Client, Game } from '../interfaces';
	import { msg } from '../main';
	import { config, is_config_dirty, selected_play } from '../store';

	let character_select: HTMLSelectElement;
	let client_select: HTMLSelectElement;

	// update selected_play store
	export function character_changed(): void {
		if (!$selected_play.account) return;

		const key: string = <string>(
			character_select[character_select.selectedIndex].getAttribute('data-id')
		);
		$selected_play.character = $selected_play.account.characters.get(key);
	}

	// update selected_play
	function client_changed(): void {
		if (client_select.value == 'RuneLite') {
			$selected_play.client = Client.RuneLite;
			$config.selected_client_index = Client.RuneLite;
		} else if (client_select.value == 'HDOS') {
			$selected_play.client = Client.HDOS;
			$config.selected_client_index = Client.HDOS;
		} else if (client_select.value == 'RS3') {
			$selected_play.client = Client.RS3;
		}
		$is_config_dirty = true;
	}

	// when play is clicked, check the selected_play store for all relevant details
	// calls the appropriate launch functions
	function play_clicked(): void {
		if (!$selected_play.account || !$selected_play.character) {
			msg('Please log in to launch a client');
			return;
		}
		switch ($selected_play.game) {
			case Game.OSRS:
				if ($selected_play.client == Client.RuneLite) {
					launchRuneLite(
						<string>$selected_play.credentials?.access_token,
						<string>$selected_play.credentials?.refresh_token,
						<string>$selected_play.credentials?.session_id,
						<string>$selected_play.character?.accountId,
						<string>$selected_play.character?.displayName
					);
				} else if ($selected_play.client == Client.HDOS) {
					launchHdos(
						<string>$selected_play.credentials?.access_token,
						<string>$selected_play.credentials?.refresh_token,
						<string>$selected_play.credentials?.session_id,
						<string>$selected_play.character?.accountId,
						<string>$selected_play.character?.displayName
					);
				}
				break;
			case Game.RS3:
				launchRS3Linux(
					<string>$selected_play.credentials?.access_token,
					<string>$selected_play.credentials?.refresh_token,
					<string>$selected_play.credentials?.session_id,
					<string>$selected_play.character?.accountId,
					<string>$selected_play.character?.displayName
				);
				break;
		}
	}

	onMount(() => {
		if ($config.selected_game_index == Game.OSRS) {
			client_select.selectedIndex = <number>$config.selected_client_index;
			$selected_play.client = client_select.selectedIndex;
		} else {
			$selected_play.client = Client.RS3;
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
		on:click={() => {
			play_clicked();
		}}>
		Play
	</button>
	<div class="mx-auto my-2">
		<label for="game_client_select" class="text-sm">Game Client</label>
		<br />
		<select
			id="game_client_select"
			class="mx-auto w-52 cursor-pointer rounded-lg border-2 border-slate-300 bg-inherit p-2 text-inherit duration-200 hover:opacity-75 dark:border-slate-800"
			bind:this={client_select}
			on:change={() => {
				client_changed();
			}}>
			{#if $selected_play.game == Game.OSRS}
				<option class="dark:bg-slate-900">RuneLite</option>
				<option class="dark:bg-slate-900">HDOS</option>
			{:else if $selected_play.game == Game.RS3}
				<option class="dark:bg-slate-900">RS3</option>
			{/if}
		</select>
	</div>
	<div class="mx-auto my-2">
		<label for="character_select" class="text-sm"> Character</label>
		<br />
		<select
			id="character_select"
			class="mx-auto w-52 cursor-pointer rounded-lg border-2 border-slate-300 bg-inherit p-2 text-inherit duration-200 hover:opacity-75 dark:border-slate-800"
			bind:this={character_select}
			on:change={() => character_changed()}>
			{#if $selected_play.account}
				{#each $selected_play.account.characters as character}
					<option data-id={character[1].accountId} class="dark:bg-slate-900">
						{character[1].displayName}
					</option>
				{/each}
			{/if}
		</select>
	</div>
</div>
