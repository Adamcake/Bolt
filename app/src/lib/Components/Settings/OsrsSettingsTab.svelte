<script lang="ts">
	import { BoltService } from '$lib/Services/BoltService';
	import { GlobalState } from '$lib/State/GlobalState';
	import { launchRuneLite } from '$lib/Util/functions';

	const { config } = GlobalState;

	$: selectedSession = BoltService.findSession($config.selected_user_id ?? '');
	$: selectedAccount = BoltService.findAccount(
		selectedSession?.accounts ?? [],
		$config.selected_account_id ?? ''
	);
	$: configureRuneLiteDisabled = !(selectedSession?.session_id && selectedAccount?.accountId);

	let currentlyPickingFile = false;

	async function openFilePicker() {
		currentlyPickingFile = true;
		$config.runelite_custom_jar = await BoltService.openFilePicker();
		console.log('done picking');
		currentlyPickingFile = false;
	}

	function launchConfigure(): void {
		if (
			!selectedSession?.session_id ||
			!selectedAccount?.accountId ||
			!selectedAccount?.displayName
		) {
			return;
		}

		launchRuneLite(
			selectedSession.session_id,
			selectedAccount.accountId,
			selectedAccount.displayName,
			true
		);
	}
</script>

<div id="osrs_options" class="col-span-3 p-5 pt-10">
	<button
		disabled={configureRuneLiteDisabled}
		class="p-2 pb-5 hover:opacity-75"
		on:click={() => launchConfigure()}
	>
		<div class="flex">
			<img
				src="svgs/wrench-solid.svg"
				alt="Configure RuneLite"
				class="mr-2 h-7 w-7 rounded-lg bg-pink-500 p-1"
			/>
			Configure RuneLite
		</div>
	</button>
	<div class="mx-auto border-t-2 border-slate-300 p-2 pt-5 dark:border-slate-800">
		<label for="use_custom_jar">Use custom RuneLite JAR: </label>
		<input type="checkbox" bind:checked={$config.runelite_use_custom_jar} class="ml-2" />
	</div>
	<div class="mx-auto p-2" class:opacity-25={!$config.runelite_use_custom_jar}>
		<textarea
			class="h-10 rounded border-2 border-slate-300 bg-slate-100 text-slate-950 dark:border-slate-800 dark:bg-slate-900 dark:text-slate-50"
			disabled={!$config.runelite_use_custom_jar}
			bind:value={$config.runelite_custom_jar}
		></textarea>
		<br />
		<button
			class="mt-1 rounded-lg border-2 border-blue-500 p-1 duration-200 hover:opacity-75"
			disabled={currentlyPickingFile}
			on:click={() => {
				openFilePicker();
			}}
		>
			Select File</button
		>
	</div>
</div>
