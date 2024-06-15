<script lang="ts">
	import { onMount } from 'svelte';
	import { loginClicked } from '$lib/Util/functions';
	import { Game } from '$lib/Util/interfaces';
	import { config, isConfigDirty, selectedPlay } from '$lib/Util/store';
	import Account from '$lib/Components/Account.svelte';
	import SettingsModal from '$lib/Components/SettingsModal.svelte';

	let settingsModal: SettingsModal;
	let showAccountDropdown: boolean = false;
	let hoverAccountButton: boolean = false;
	let rs3Button: HTMLButtonElement;
	let osrsButton: HTMLButtonElement;
	let accountButton: HTMLButtonElement;

	// tailwind can easily change theme by adding or removing 'dark' to the root 'html' element
	function change_theme(): void {
		let html: HTMLElement = document.documentElement;
		if (html.classList.contains('dark')) html.classList.remove('dark');
		else html.classList.add('dark');
		$config.use_dark_theme = !$config.use_dark_theme;
		$isConfigDirty = true;
	}

	// swaps game visually, will effect all relevant selects
	function toggle_game(game: Game): void {
		switch (game) {
			case Game.osrs:
				$selectedPlay.game = Game.osrs;
				$selectedPlay.client = $config.selected_client_index;
				$config.selected_game_index = Game.osrs;
				$isConfigDirty = true;
				osrsButton.classList.add('bg-blue-500', 'text-black');
				rs3Button.classList.remove('bg-blue-500', 'text-black');
				break;
			case Game.rs3:
				$selectedPlay.game = Game.rs3;
				$config.selected_game_index = Game.rs3;
				$isConfigDirty = true;
				osrsButton.classList.remove('bg-blue-500', 'text-black');
				rs3Button.classList.add('bg-blue-500', 'text-black');
				break;
		}
	}

	// if no account is signed in, open auth window
	// else, toggle the account dropdown
	function toggle_account(): void {
		if (accountButton.innerHTML == 'Log In') {
			loginClicked();
			return;
		}
		showAccountDropdown = !showAccountDropdown;
	}

	onMount(() => {
		toggle_game(<Game>$config.selected_game_index);
	});
</script>

<SettingsModal bind:this={settingsModal}></SettingsModal>

<div
	class="fixed top-0 flex h-16 w-screen border-b-2 border-slate-300 bg-slate-100 duration-200 dark:border-slate-800 dark:bg-slate-900"
>
	<div class="m-3 ml-9 font-bold">
		<button
			class="mx-1 w-20 rounded-lg border-2 border-blue-500 p-2 duration-200 hover:opacity-75"
			bind:this={rs3Button}
			on:click={() => {
				toggle_game(Game.rs3);
			}}
		>
			RS3
		</button>
		<button
			class="mx-1 w-20 rounded-lg border-2 border-blue-500 bg-blue-500 p-2 text-black duration-200 hover:opacity-75"
			bind:this={osrsButton}
			on:click={() => {
				toggle_game(Game.osrs);
			}}
		>
			OSRS
		</button>
	</div>
	<div class="ml-auto flex">
		<button
			class="my-3 h-10 w-10 rounded-full bg-blue-500 p-2 duration-200 hover:rotate-45 hover:opacity-75"
			on:click={() => change_theme()}
		>
			<img src="svgs/lightbulb-solid.svg" class="h-6 w-6" alt="Change Theme" />
		</button>
		<button
			class="m-3 h-10 w-10 rounded-full bg-blue-500 p-2 duration-200 hover:rotate-45 hover:opacity-75"
			on:click={() => settingsModal.open()}
		>
			<img src="svgs/gear-solid.svg" class="h-6 w-6" alt="Settings" />
		</button>
		<button
			class="m-2 w-48 rounded-lg border-2 border-slate-300 bg-inherit p-2 text-center font-bold text-black duration-200 hover:opacity-75 dark:border-slate-800 dark:text-slate-50"
			bind:this={accountButton}
			on:mouseenter={() => {
				hoverAccountButton = true;
			}}
			on:mouseleave={() => {
				hoverAccountButton = false;
			}}
			on:click={() => toggle_account()}
		>
			{#if $selectedPlay.account}
				{$selectedPlay.account?.displayName}
			{:else}
				Log In
			{/if}
		</button>
	</div>

	{#if showAccountDropdown}
		<div class="absolute right-2 top-[72px]">
			<Account bind:showAccountDropdown {hoverAccountButton}></Account>
		</div>
	{/if}
</div>
