<script lang="ts">
	import { BoltService } from '$lib/Services/BoltService';
	import { bolt, Platform } from '$lib/State/Bolt';
	import { GlobalState } from '$lib/State/GlobalState';
	import {
		launchHdos,
		launchOfficialClient,
		launchRS3Linux,
		launchRuneLite
	} from '$lib/Util/Functions';
	import { Client, clientMap, Game } from '$lib/Util/Interfaces';
	import { logger } from '$lib/Util/Logger';
	import { writable, type Writable } from 'svelte/store';
	import LaunchConfirmModal from './Modals/LaunchConfirmModal.svelte';
	import PluginModal from './Modals/PluginModal.svelte';

	let confirmModal: LaunchConfirmModal;
	let pluginModal: PluginModal;
	let { config, initialized } = GlobalState;
	let selectedUserId = $derived($config.selected.user_id);
	let selectedAccountId = $derived($config.userDetails[selectedUserId ?? '']?.account_id);
	let accounts = $derived(BoltService.findSession($config.selected.user_id)?.accounts ?? []);

	// messages about game downtime, retrieved from game server
	let psa: Writable<string | null> = writable(null);
	let gameEnabled: Writable<boolean> = writable(true);
	$effect(() => {
		if ($config.check_announcements) {
			const gameName = $config.selected.game == Game.osrs ? 'osrs' : bolt.env.provider;
			const url: string = `${bolt.env.psa_url}${gameName}/${gameName}.json`;
			// added no-store due to an issue where new messages are not shown until cache is cleared.
			// remote server appears to be using etags incorrectly?
			fetch(url, { method: 'GET', cache: 'no-store' })
				.then((response) => response.json())
				.then((response) => {
					$psa = response.psaEnabled && response.psaMessage ? response.psaMessage : null;
					$gameEnabled = !(response.playDisabled ?? false);
				});
		} else {
			$psa = null;
			$gameEnabled = true;
		}
	});

	// when play is clicked, check the selected_play store for all relevant details
	// calls the appropriate launch functions
	export function launch(game: Game, client: Client): void {
		if (!selectedUserId) {
			return logger.warn('Please log in or select a user to play.');
		}
		if (!selectedAccountId) {
			return logger.warn('Please select a character from the select menu.');
		}
		const session = BoltService.findSession($config.selected.user_id);
		if (!session) return logger.warn('Unable to launch game, session was not found.');
		const { session_id } = session;
		const account = BoltService.findAccount(session.accounts, selectedAccountId);
		if (!account) return logger.warn('Unable to launch game, account was not found.');
		const { accountId, displayName } = account;
		const isWindows = bolt.platform === Platform.Windows;
		const isLinux = bolt.platform === Platform.Linux;
		switch (game) {
			case Game.osrs:
				switch (client) {
					case Client.official:
						// when launching official osrs client on linux, set windows=true so that it will download the
						// windows exe instead of the mac app, as the exe is what we want to run through wine/proton
						launchOfficialClient(isWindows || isLinux, true, session_id, accountId, displayName);
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

	function handleAccountChange(e: Event) {
		const value = (e.target as HTMLSelectElement).value;
		if (!selectedUserId) return;
		const details = $config.userDetails[selectedUserId];
		if (details) {
			details.account_id = value;
			$config.userDetails[selectedUserId] = details;
		} else {
			$config.userDetails[selectedUserId] = {
				account_id: value
			};
		}
	}
</script>

<LaunchConfirmModal bind:this={confirmModal}></LaunchConfirmModal>
<PluginModal bind:this={pluginModal}></PluginModal>

<div class="bg-grad flex h-full flex-col border-slate-300 p-5 duration-200 dark:border-slate-800">
	{#if $psa}
		<div class="absolute left-[2%] w-[96%] rounded-lg bg-blue-400 px-2 text-black">
			{$psa}
		</div>
	{/if}
	<div class="flex flex-col items-center gap-4">
		<img
			src="svgs/rocket-solid.svg"
			alt="Launch icon"
			class="mb-3 w-24 rounded-3xl bg-gradient-to-br from-rose-500 to-violet-500 p-5"
		/>
		<button
			class="w-52 rounded-lg bg-emerald-500 p-2 font-bold text-black duration-200 enabled:hover:opacity-75 disabled:bg-gray-500"
			disabled={!$initialized}
			onclick={() => {
				if ($gameEnabled) {
					launch($config.selected.game, $config.selected.client);
				} else {
					confirmModal.open(launch, $config.selected.game, $config.selected.client);
				}
			}}
		>
			Play
		</button>
		{#if $config.selected.game == Game.osrs}
			<label class="flex flex-col">
				<span class="text-sm">Game Client</span>
				<select
					id="game_client_select"
					class="w-52 cursor-pointer rounded-lg border-2 border-slate-300 bg-inherit p-2 text-inherit duration-200 hover:opacity-75 dark:border-slate-800"
					bind:value={$config.selected.client}
				>
					{#each clientMap[$config.selected.game] as client}
						<option class="dark:bg-slate-900" value={client}>{client}</option>
					{/each}
				</select>
			</label>
		{:else if $config.selected.game === Game.rs3}
			<button
				disabled={!bolt.hasBoltPlugins}
				title={bolt.hasBoltPlugins ? null : 'This feature is disabled'}
				class="w-52 rounded-lg p-2 font-bold text-black duration-200 enabled:bg-blue-500 enabled:hover:opacity-75 disabled:bg-gray-500"
				onclick={() => {
					pluginModal.open();
				}}
			>
				Plugin menu
			</button>
		{/if}
		<label class="flex flex-col">
			<span class="text-sm">Character</span>
			<select
				id="character_select"
				class="mx-auto w-52 cursor-pointer rounded-lg border-2 border-slate-300 bg-inherit p-2 text-inherit duration-200 hover:opacity-75 dark:border-slate-800"
				disabled={!$initialized || $config.selected.user_id === null}
				value={selectedAccountId}
				onchange={handleAccountChange}
			>
				<option value={undefined} disabled class="dark:bg-slate-900">Select an account</option>
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
