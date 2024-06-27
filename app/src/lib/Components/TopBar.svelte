<script lang="ts">
	import { onMount } from 'svelte';
	import { Game } from '$lib/Util/interfaces';
	import { config, isConfigDirty, selectedPlay } from '$lib/Util/store';
	import SettingsModal from '$lib/Components/SettingsModal.svelte';
	import { loginClicked } from '$lib/Util/functions';
	import Dropdown from '$lib/Components/CommonUI/Dropdown.svelte';
	import Account from '$lib/Components/Account.svelte';

	let settingsModal: SettingsModal;
	let rs3Button: HTMLButtonElement;
	let osrsButton: HTMLButtonElement;

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
	<div class="m-2 ml-auto flex gap-2">
		<button
			class="h-10 w-10 rounded-full bg-blue-500 text-center duration-200 hover:rotate-45 hover:opacity-75"
			on:click={() => change_theme()}
		>
			<img src="svgs/lightbulb-solid.svg" class="m-auto h-6 w-6" alt="Change Theme" />
		</button>
		<button
			class="h-10 w-10 rounded-full bg-blue-500 text-center duration-200 hover:rotate-45 hover:opacity-75"
			on:click={() => settingsModal.open()}
		>
			<img src="svgs/gear-solid.svg" class="m-auto h-6 w-6" alt="Settings" />
		</button>
		{#if $selectedPlay.account}
			<Dropdown align="center">
				<button
					class="h-11 w-48 rounded-lg border-2 border-slate-300 bg-inherit text-center font-bold text-black duration-200 hover:opacity-75 dark:border-slate-800 dark:text-slate-50"
				>
					{$selectedPlay.account?.displayName}
				</button>

				<div slot="content" class="w-40">
					<Account />
				</div>
			</Dropdown>
		{:else}
			<button
				class="h-11 w-48 rounded-lg border-2 border-slate-300 bg-inherit p-2 text-center font-bold text-black duration-200 hover:opacity-75 dark:border-slate-800 dark:text-slate-50"
				on:click={loginClicked}
			>
				Login
			</button>
		{/if}
	</div>
</div>
