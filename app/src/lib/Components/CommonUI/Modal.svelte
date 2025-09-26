<script lang="ts">
	import { GlobalState } from '$lib/State/GlobalState';

	const { config } = GlobalState;
	let darkTheme = $derived($config.use_dark_theme);

	interface Props {
		class?: string;
		canSelfClose?: boolean;
		onClose?: () => void;
		children?: import('svelte').Snippet;
	}

	let {
		class: className = '',
		canSelfClose = true,
		onClose = () => {},
		children
	}: Props = $props();

	let dialog: HTMLDialogElement;
	let isOpen = $state(false);

	export function open() {
		dialog.showModal();
		isOpen = true;
	}

	export function close() {
		onClose();
		dialog.close();
		isOpen = false;
	}
</script>

<dialog
	bind:this={dialog}
	onkeydown={(e) => {
		if (e.key !== 'Escape') return;
		e.preventDefault();
		if (canSelfClose) close();
	}}
	onmousedown={(e: MouseEvent) => {
		if (dialog === (e.target as Node)) close();
	}}
	class:dark={darkTheme}
	class="{className} backdrop max-h-[90%] max-w-[90%] overflow-auto rounded-xl text-inherit focus-visible:outline-none"
>
	{#if canSelfClose}
		<button
			class="absolute right-3 top-3 rounded-full bg-rose-500 p-[2px] shadow-lg duration-200 hover:rotate-90 hover:opacity-75"
			onclick={close}
		>
			<img src="svgs/xmark-solid.svg" class="h-5 w-5" alt="Close" />
		</button>
	{/if}
	{#if isOpen}
		{@render children?.()}
	{/if}
</dialog>

<style lang="postcss">
	.backdrop {
		@apply backdrop:bg-black/75 backdrop:backdrop-blur-sm dark:bg-slate-900;
	}
</style>
