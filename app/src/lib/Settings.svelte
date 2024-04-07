<script lang="ts">
	import { onDestroy } from 'svelte';
	import Backdrop from './Backdrop.svelte';
	import General from './Settings/General.svelte';
	import Osrs from './Settings/Osrs.svelte';
	import Rs3 from './Settings/Rs3.svelte';

	// props
	export let showSettings: boolean;

	// settings categories
	enum Options {
		general,
		osrs,
		rs3
	}

	// variables for swapping options and styling
	let showOption: Options = Options.osrs;
	let activeClass =
		'border-2 border-blue-500 bg-blue-500 hover:opacity-75 font-bold text-black duration-200 rounded-lg p-1 mx-auto my-1 w-3/4';
	let inactiveClass =
		'border-2 border-blue-500 hover:opacity-75 duration-200 rounded-lg p-1 mx-auto my-1 w-3/4';
	let generalButton: HTMLButtonElement;
	let osrsButton: HTMLButtonElement;
	let rs3Button: HTMLButtonElement;

	// uses enum above to determine active option and styling
	function toggleOptions(option: Options): void {
		switch (option) {
			case Options.general:
				showOption = Options.general;
				generalButton!.classList.value = activeClass;
				osrsButton!.classList.value = inactiveClass;
				rs3Button!.classList.value = inactiveClass;
				break;
			case Options.osrs:
				showOption = Options.osrs;
				generalButton!.classList.value = inactiveClass;
				osrsButton!.classList.value = activeClass;
				rs3Button!.classList.value = inactiveClass;
				break;
			case Options.rs3:
				showOption = Options.rs3;
				generalButton!.classList.value = inactiveClass;
				osrsButton!.classList.value = inactiveClass;
				rs3Button!.classList.value = activeClass;
				break;
		}
	}

	// hide settings on 'escape'
	function escapeKeyPressed(evt: KeyboardEvent): void {
		if (evt.key === 'Escape') {
			showSettings = false;
		}
	}

	// check if user presses escape
	addEventListener('keydown', escapeKeyPressed);

	// When the component is removed, delete the event listener also
	onDestroy(() => {
		removeEventListener('keydown', escapeKeyPressed);
	});
</script>

<div id="settings">
	<Backdrop
		on:click={() => {
			showSettings = false;
		}}></Backdrop>
	<div
		class="absolute left-[13%] top-[13%] z-20 h-3/4 w-3/4 rounded-lg bg-slate-100 text-center shadow-lg dark:bg-slate-900">
		<button
			class="absolute right-3 top-3 rounded-full bg-rose-500 p-[2px] shadow-lg duration-200 hover:rotate-90 hover:opacity-75"
			on:click={() => {
				showSettings = false;
			}}>
			<img src="svgs/xmark-solid.svg" class="h-5 w-5" alt="Close" />
		</button>
		<div class="grid h-full grid-cols-4">
			<div class="relative h-full border-r-2 border-slate-300 pt-10 dark:border-slate-800">
				<button
					id="general_button"
					bind:this={generalButton}
					class={inactiveClass}
					on:click={() => {
						toggleOptions(Options.general);
					}}>General</button
				><br />
				<button
					id="osrs_button"
					bind:this={osrsButton}
					class={activeClass}
					on:click={() => {
						toggleOptions(Options.osrs);
					}}>OSRS</button
				><br />
				<button
					id="rs3_button"
					bind:this={rs3Button}
					class={inactiveClass}
					on:click={() => {
						toggleOptions(Options.rs3);
					}}>RS3</button>
			</div>
			{#if showOption == Options.general}
				<General></General>
			{:else if showOption == Options.osrs}
				<Osrs></Osrs>
			{:else if showOption == Options.rs3}
				<Rs3></Rs3>
			{/if}
		</div>
	</div>
</div>
