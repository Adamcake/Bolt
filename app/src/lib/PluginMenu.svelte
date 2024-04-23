<script lang="ts">
	import { onDestroy } from 'svelte';
	import { get } from 'svelte/store';
	import Backdrop from './Backdrop.svelte';
	import { getNewClientListPromise } from '../functions';
	import { clientListPromise } from '../store';

	// props
	export let showPluginMenu: boolean;

	// get connected client list
	clientListPromise.set(getNewClientListPromise());

	// hide plugin menu on 'escape'
	function escapeKeyPressed(evt: KeyboardEvent): void {
		if (evt.key === 'Escape') {
			showPluginMenu = false;
		}
	}
	addEventListener('keydown', escapeKeyPressed);

	onDestroy(() => {
		// When the component is removed, delete the event listener also
		removeEventListener('keydown', escapeKeyPressed);
	});
</script>

<div>
	<Backdrop
		on:click={() => {
			showPluginMenu = false;
		}}></Backdrop>
	<div
		class="absolute left-[5%] top-[5%] z-20 h-[90%] w-[90%] rounded-lg bg-slate-100 text-center shadow-lg dark:bg-slate-900">
		<button
			class="absolute right-3 top-3 rounded-full bg-rose-500 p-[2px] shadow-lg duration-200 hover:rotate-90 hover:opacity-75"
			on:click={() => {
				showPluginMenu = false;
			}}>
			<img src="svgs/xmark-solid.svg" class="h-5 w-5" alt="Close" />
		</button>
		<div
			class="left-0 float-left h-full w-[min(180px,_50%)] overflow-hidden border-r-2 border-slate-300 pt-2 dark:border-slate-800">
			<button
				class="mx-auto mb-2 w-[95%] rounded-lg bg-blue-500 p-2 font-bold text-black duration-200 hover:opacity-75"
				on:click={() => {}}>
				Manage Plugins
			</button>
			<hr class="p-1 dark:border-slate-700" />
			{#await $clientListPromise}
				<p>loading...</p>
			{:then clients}
				{#if clients.length == 0}
					<p>(start an RS3 game client with plugin library enabled and it will be listed here.)</p>
				{:else}
					{#each clients as client}
						<button
							class="m-1 h-[28px] w-[95%] rounded-lg border-2 border-blue-500 duration-200 hover:opacity-75">
							{client.identity || "(unnamed)"}
						</button>
						<br />
					{/each}
				{/if}
			{:catch error}
					<p>error</p>
			{/await}
		</div>
		<div class="h-full pt-10">
			<p>menu</p>
		</div>
	</div>
</div>
