<script lang="ts">
	import { onDestroy } from 'svelte';
	import Backdrop from './Backdrop.svelte';
	import General from './Settings/General.svelte';
	import Osrs from './Settings/Osrs.svelte';
	import Rs3 from './Settings/Rs3.svelte';

	// props
	export let show_settings: boolean;

	// settings categories
	enum Options {
		General,
		Osrs,
		Rs3
	}

	// variables for swapping options and styling
	let show_option: Options = Options.Osrs;
	let active_class =
		'border-2 border-blue-500 bg-blue-500 hover:opacity-75 font-bold text-black duration-200 rounded-lg p-1 mx-auto my-1 w-3/4';
	let inactive_class =
		'border-2 border-blue-500 hover:opacity-75 duration-200 rounded-lg p-1 mx-auto my-1 w-3/4';
	let general_button: HTMLButtonElement;
	let osrs_button: HTMLButtonElement;
	let rs3_button: HTMLButtonElement;

	// uses enum above to determine active option and styling
	function toggle_options(option: Options): void {
		switch (option) {
			case Options.General:
				show_option = Options.General;
				general_button!.classList.value = active_class;
				osrs_button!.classList.value = inactive_class;
				rs3_button!.classList.value = inactive_class;
				break;
			case Options.Osrs:
				show_option = Options.Osrs;
				general_button!.classList.value = inactive_class;
				osrs_button!.classList.value = active_class;
				rs3_button!.classList.value = inactive_class;
				break;
			case Options.Rs3:
				show_option = Options.Rs3;
				general_button!.classList.value = inactive_class;
				osrs_button!.classList.value = inactive_class;
				rs3_button!.classList.value = active_class;
				break;
		}
	}

	// hide settings on 'escape'
	function escape_key_pressed(evt: KeyboardEvent): void {
		if (evt.key === 'Escape') {
			show_settings = false;
		}
	}

	// check if user presses escape
	addEventListener('keydown', escape_key_pressed);

	// When the component is removed, delete the event listener also
	onDestroy(() => {
		removeEventListener('keydown', escape_key_pressed);
	});
</script>

<div id="settings">
	<Backdrop
		on:click={() => {
			show_settings = false;
		}}></Backdrop>
	<div
		class="absolute left-[13%] top-[13%] z-20 h-3/4 w-3/4 rounded-lg bg-slate-100 text-center shadow-lg dark:bg-slate-900">
		<button
			class="absolute right-3 top-3 rounded-full bg-rose-500 p-[2px] shadow-lg duration-200 hover:rotate-90 hover:opacity-75"
			on:click={() => {
				show_settings = false;
			}}>
			<img src="svgs/xmark-solid.svg" class="h-5 w-5" alt="Close" />
		</button>
		<div class="grid h-full grid-cols-4">
			<div class="relative h-full border-r-2 border-slate-300 pt-10 dark:border-slate-800">
				<button
					id="general_button"
					bind:this={general_button}
					class={inactive_class}
					on:click={() => {
						toggle_options(Options.General);
					}}>General</button
				><br />
				<button
					id="osrs_button"
					bind:this={osrs_button}
					class={active_class}
					on:click={() => {
						toggle_options(Options.Osrs);
					}}>OSRS</button
				><br />
				<button
					id="rs3_button"
					bind:this={rs3_button}
					class={inactive_class}
					on:click={() => {
						toggle_options(Options.Rs3);
					}}>RS3</button>
			</div>
			{#if show_option == Options.General}
				<General></General>
			{:else if show_option == Options.Osrs}
				<Osrs></Osrs>
			{:else if show_option == Options.Rs3}
				<Rs3></Rs3>
			{/if}
		</div>
	</div>
</div>
