<script lang="ts">
	import { onMount } from 'svelte';
	import { loginClicked } from '../functions';
	import { Client, Game } from '../interfaces';
	import { config, is_config_dirty, selected_play } from '../store';
	import Account from './Account.svelte';

	// prop
	export let show_settings: boolean;

	let show_account_dropdown: boolean = false;
	let hover_account_button: boolean = false;
	let rs3_button: HTMLButtonElement;
	let osrs_button: HTMLButtonElement;
	let account_button: HTMLButtonElement;

	// tailwind can easily change theme by adding or removing 'dark' to the root 'html' element
	function change_theme(): void {
		let html: HTMLElement = document.documentElement;
		if (html.classList.contains('dark')) html.classList.remove('dark');
		else html.classList.add('dark');
		$config.use_dark_theme = !$config.use_dark_theme;
		$is_config_dirty = true;
	}

	// swaps game visually, will effect all relevant selects
	function toggle_game(game: Game): void {
		switch (game) {
			case Game.OSRS:
				$selected_play.game = Game.OSRS;
				$selected_play.client = Client.RuneLite;
				$config.selected_game_index = Game.OSRS;
				$config.selected_client_index = Client.RuneLite;
				$is_config_dirty = true;
				osrs_button.classList.add('bg-blue-500', 'text-black');
				rs3_button.classList.remove('bg-blue-500', 'text-black');
				break;
			case Game.RS3:
				$selected_play.game = Game.RS3;
				$config.selected_game_index = Game.RS3;
				$config.selected_client_index = Client.RS3;
				$is_config_dirty = true;
				osrs_button.classList.remove('bg-blue-500', 'text-black');
				rs3_button.classList.add('bg-blue-500', 'text-black');
				break;
		}
	}

	// if no account is signed in, open the Jagex login
	// else, toggle the account dropdown
	function toggle_account(): void {
		if (account_button.innerHTML == 'Log In') {
			loginClicked();
			return;
		}
		show_account_dropdown = !show_account_dropdown;
	}

	onMount(() => {
		toggle_game(<Game>$config.selected_game_index);
	});
</script>

<div
	class="h-16 w-screen fixed top-0 border-b-2 duration-200 bg-slate-100 border-slate-300 dark:bg-slate-900 dark:border-slate-800 flex">
	<div class="m-3 ml-9 font-bold">
		<button
			class="mx-1 p-2 rounded-lg border-2 border-blue-500 hover:opacity-75 duration-200 w-20"
			bind:this={rs3_button}
			on:click={() => {
				toggle_game(Game.RS3);
			}}>
			RS3
		</button>
		<button
			class="mx-1 p-2 rounded-lg border-2 border-blue-500 bg-blue-500 text-black hover:opacity-75 duration-200 w-20"
			bind:this={osrs_button}
			on:click={() => {
				toggle_game(Game.OSRS);
			}}>
			OSRS
		</button>
	</div>
	<div class="ml-auto flex">
		<button
			class="p-2 my-3 rounded-full bg-blue-500 hover:opacity-75 duration-200 hover:rotate-45 w-10 h-10"
			on:click={() => change_theme()}>
			<img src="svgs/lightbulb-solid.svg" class="w-6 h-6" alt="Change Theme" />
		</button>
		<button
			class="p-2 m-3 rounded-full bg-blue-500 hover:opacity-75 duration-200 hover:rotate-45 w-10 h-10"
			on:click={() => {
				show_settings = true;
			}}>
			<img src="svgs/gear-solid.svg" class="w-6 h-6" alt="Settings" />
		</button>
		<button
			class="text-center font-bold duration-200 w-48 m-2 rounded-lg text-black dark:text-slate-50 p-2 bg-inherit border-2 border-slate-300 dark:border-slate-800 hover:opacity-75"
			bind:this={account_button}
			on:mouseenter={() => {
				hover_account_button = true;
			}}
			on:mouseleave={() => {
				hover_account_button = false;
			}}
			on:click={() => toggle_account()}>
			{#if $selected_play.account}
				{$selected_play.account?.displayName}
			{:else}
				Log In
			{/if}
		</button>
	</div>

	{#if show_account_dropdown}
		<div class="absolute right-2 top-[72px]">
			<Account bind:show_account_dropdown {hover_account_button}></Account>
		</div>
	{/if}
</div>
