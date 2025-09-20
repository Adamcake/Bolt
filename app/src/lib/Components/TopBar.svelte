<script lang="ts">
	import AccountDropdownContent from '$lib/Components/AccountDropdownContent.svelte';
	import Dropdown from '$lib/Components/CommonUI/Dropdown.svelte';
	import SettingsModal from './Modals/SettingsModal.svelte';
	import { AuthService } from '$lib/Services/AuthService';
	import { BoltService } from '$lib/Services/BoltService';
	import { bolt } from '$lib/State/Bolt';
	import { GlobalState } from '$lib/State/GlobalState';
	import { Game } from '$lib/Util/interfaces';

	const { config, sessions } = GlobalState;
	let settingsModal: SettingsModal;
</script>

<SettingsModal bind:this={settingsModal}></SettingsModal>

<div
	class="fixed top-0 flex h-16 w-screen border-b-2 border-slate-300 bg-slate-100 duration-200 dark:border-slate-800 dark:bg-slate-900"
>
	<div class="m-3 ml-9 font-bold">
		<button
			class="mx-1 w-20 rounded-lg border-2 border-blue-500 p-2 duration-200 hover:opacity-75"
			class:text-black={$config.selected.game === Game.rs3}
			class:bg-blue-500={$config.selected.game === Game.rs3}
			onclick={() => {
				$config.selected.game = Game.rs3;
			}}
		>
			RS3
		</button>
		<button
			class="mx-1 w-20 rounded-lg border-2 border-blue-500 bg-blue-500 p-2 text-black duration-200 hover:opacity-75"
			class:text-black={$config.selected.game === Game.osrs}
			class:bg-blue-500={$config.selected.game === Game.osrs}
			onclick={() => {
				$config.selected.game = Game.osrs;
			}}
		>
			OSRS
		</button>
	</div>
	<div class="m-2 ml-auto flex gap-2">
		<button
			class="h-10 w-10 rounded-full bg-blue-500 text-center duration-200 hover:rotate-45 hover:opacity-75"
			onclick={() => ($config.use_dark_theme = !$config.use_dark_theme)}
		>
			<img src="svgs/lightbulb-solid.svg" class="m-auto h-6 w-6" alt="Change Theme" />
		</button>
		<button
			class="h-10 w-10 rounded-full bg-blue-500 text-center duration-200 hover:rotate-45 hover:opacity-75"
			onclick={() => settingsModal.open()}
		>
			<img src="svgs/gear-solid.svg" class="m-auto h-6 w-6" alt="Settings" />
		</button>
		{#if $sessions.length > 0}
			{@const selectedSession = BoltService.findSession($config.selected.user_id ?? '')}
			<Dropdown align="center">
				<button
					class="h-11 w-48 rounded-lg border-2 border-slate-300 bg-inherit text-center font-bold text-black duration-200 hover:opacity-75 dark:border-slate-800 dark:text-slate-50"
				>
					{selectedSession?.user.displayName ?? 'No user selected'}
				</button>

				{#snippet content()}
					<div class="w-40">
						<AccountDropdownContent />
					</div>
				{/snippet}
			</Dropdown>
		{:else}
			<button
				class="h-11 w-48 rounded-lg border-2 border-slate-300 bg-inherit p-2 text-center font-bold text-black duration-200 hover:opacity-75 dark:border-slate-800 dark:text-slate-50"
				onclick={() => {
					const { origin, redirect, clientid } = bolt.env;
					AuthService.openLoginWindow(origin, redirect, clientid);
				}}
			>
				Login
			</button>
		{/if}
	</div>
</div>
