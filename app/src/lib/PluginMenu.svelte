<script lang="ts">
	import { onDestroy } from 'svelte';
	import Backdrop from './Backdrop.svelte';

	// props
	export let showPluginMenu: boolean;

	// hide settings on 'escape'
	function escapeKeyPressed(evt: KeyboardEvent): void {
		if (evt.key === 'Escape') {
			showPluginMenu = false;
		}
	}

	// check if user presses escape
	addEventListener('keydown', escapeKeyPressed);

	// When the component is removed, delete the event listener also
	onDestroy(() => {
		removeEventListener('keydown', escapeKeyPressed);
	});
</script>

<div id="plugins">
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
			{#each ['Client 1', 'Client 2', 'Client 3'] as username}
				<button
					class="m-1 h-[28px] w-[95%] rounded-lg border-2 border-blue-500 duration-200 hover:opacity-75">
					{username}
				</button>
				<br />
			{/each}
		</div>
		<div class="h-full pt-10">
			<p>menu</p>
		</div>
	</div>
</div>
