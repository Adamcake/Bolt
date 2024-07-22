<script lang="ts">
	import { createEventDispatcher } from 'svelte';

	let className = '';
	export { className as class };
	export let canSelfClose = true;

	let dialog: HTMLDialogElement;
	let isOpen = false;
	const dispatch = createEventDispatcher<{ close: undefined }>();

	export function open() {
		dialog.showModal();
		isOpen = true;
	}

	export function close() {
		dispatch('close');
		dialog.close();
		isOpen = false;
	}
</script>

<!-- svelte-ignore a11y-no-noninteractive-element-interactions -->
<dialog
	bind:this={dialog}
	on:keydown={(e) => {
		if (e.key === 'Escape' && !canSelfClose) e.preventDefault();
	}}
	on:mousedown|self={() => {
		if (canSelfClose) dialog.close();
	}}
	class:backdrop:cursor-pointer={canSelfClose}
	class="{className} backdrop max-h-[90%] max-w-[90%] overflow-auto rounded-xl text-inherit focus-visible:outline-none"
>
	{#if canSelfClose}
		<button
			class="absolute right-3 top-3 rounded-full bg-rose-500 p-[2px] shadow-lg duration-200 hover:rotate-90 hover:opacity-75"
			on:click={() => dialog.close()}
		>
			<img src="svgs/xmark-solid.svg" class="h-5 w-5" alt="Close" />
		</button>
	{/if}
	{#if isOpen}
		<slot />
	{/if}
</dialog>

<style lang="postcss">
	.backdrop {
		@apply backdrop:bg-black/75 backdrop:backdrop-blur-sm dark:bg-slate-900;
	}
</style>
