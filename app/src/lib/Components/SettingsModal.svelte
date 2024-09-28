<script lang="ts">
	import Modal from '$lib/Components/CommonUI/Modal.svelte';
	import GeneralSettingsTab from '$lib/Components/Settings/GeneralSettingsTab.svelte';
	import OsrsSettingsTab from '$lib/Components/Settings/OsrsSettingsTab.svelte';
	import Rs3SettingsTab from '$lib/Components/Settings/Rs3SettingsTab.svelte';

	let modal: Modal;
	export function open() {
		modal.open();
	}

	// settings categories
	enum Tab {
		general,
		osrs,
		rs3
	}

	// variables for swapping options and styling
	let openTab: Tab = Tab.osrs;
	let activeClass =
		'border-2 border-blue-500 bg-blue-500 hover:opacity-75 font-bold text-black duration-200 rounded-lg p-1 w-3/4';
	let inactiveClass = 'border-2 border-blue-500 hover:opacity-75 duration-200 rounded-lg p-1 w-3/4';
</script>

<Modal bind:this={modal} class="h-3/4 w-3/4 select-none">
	<div class="grid h-full grid-cols-4">
		<div
			class="flex flex-col items-center gap-2 border-r-2 border-slate-300 pt-10 dark:border-slate-800"
		>
			<button
				class={openTab === Tab.general ? activeClass : inactiveClass}
				on:click={() => {
					openTab = Tab.general;
				}}>General</button
			>
			<button
				class={openTab === Tab.osrs ? activeClass : inactiveClass}
				on:click={() => {
					openTab = Tab.osrs;
				}}>OSRS</button
			>
			<button
				class={openTab === Tab.rs3 ? activeClass : inactiveClass}
				on:click={() => {
					openTab = Tab.rs3;
				}}>RS3</button
			>
		</div>

		<div class="col-span-3 p-5 pt-10 text-center">
			{#if openTab == Tab.general}
				<GeneralSettingsTab />
			{:else if openTab == Tab.osrs}
				<OsrsSettingsTab />
			{:else if openTab == Tab.rs3}
				<Rs3SettingsTab />
			{/if}
		</div>
	</div>
</Modal>
