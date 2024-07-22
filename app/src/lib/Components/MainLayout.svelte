<script lang="ts">
	import { GlobalState } from '$lib/State/GlobalState';

	const { config } = GlobalState;
	$: darkTheme = $config.use_dark_theme;
</script>

<main
	class:dark={darkTheme}
	class="fixed top-0 h-screen w-screen bg-slate-100 text-xs text-slate-900 duration-200 sm:text-sm md:text-base dark:bg-slate-900 dark:text-slate-50"
>
	<slot />
</main>
