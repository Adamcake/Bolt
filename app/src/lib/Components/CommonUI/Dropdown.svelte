<script lang="ts">
	import { GlobalState } from '$lib/State/GlobalState';
	import clickOutside from '$lib/Util/ClickOutside';

	const { config } = GlobalState;
	let darkTheme = $derived($config.use_dark_theme);

	interface Props {
		class?: string;
		position?: 'top' | 'right' | 'bottom' | 'left';
		align?: 'start' | 'center' | 'end';
		children?: import('svelte').Snippet;
		content?: import('svelte').Snippet;
	}
	let {
		class: className = '',
		position = 'bottom',
		align = 'start',
		children,
		content
	}: Props = $props();

	let isOpen = $state(false);
	let openButton: HTMLButtonElement | undefined = $state();

	export function open() {
		isOpen = true;
	}

	export function close() {
		isOpen = false;
	}

	function toggle() {
		isOpen ? close() : open();
	}
</script>

<div class:dark={darkTheme} class="relative h-fit w-fit {className}">
	<button bind:this={openButton} onclick={toggle}>{@render children?.()}</button>

	{#if isOpen}
		<div
			class="dropdown-color absolute z-20 rounded-lg border-2 {position} {align}"
			use:clickOutside={{ callback: close, ignore: [openButton] }}
		>
			{@render content?.()}
		</div>
	{/if}
</div>

<style lang="postcss">
	.dropdown-color {
		@apply border-slate-300 bg-slate-100 p-3 shadow dark:border-slate-800 dark:bg-slate-900;
	}

	.top {
		bottom: 100%;
		@apply mb-1;
	}

	.bottom {
		top: 100%;
		@apply mt-1;
	}

	.top.start,
	.bottom.start {
		left: 0;
	}

	.top.center,
	.bottom.center {
		left: 50%;
		transform: translateX(-50%);
	}

	.top.end,
	.bottom.end {
		right: 0;
	}

	.left {
		right: 100%;
		@apply mr-1;
	}

	.right {
		left: 100%;
		@apply ml-1;
	}

	.left.start,
	.right.start {
		top: 0;
	}

	.left.center,
	.right.center {
		top: 50%;
		transform: translateY(-50%);
	}

	.left.end,
	.right.end {
		bottom: 0;
	}
</style>
