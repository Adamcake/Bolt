<script lang="ts">
	import { Severity, type Log } from '$lib/Util/Logger';

	interface Props {
		logs: Log[];
	}

	let { logs }: Props = $props();
</script>

<div
	class="fixed bottom-0 h-1/4 w-screen border-t-2 border-t-slate-300 bg-slate-100 duration-200 dark:border-t-slate-800 dark:bg-slate-900"
>
	<div class="absolute right-6 top-2">
		<img
			src="svgs/circle-info-solid.svg"
			alt="Message list icon"
			class="h-7 w-7 rounded-full bg-blue-500 p-[3px] duration-200"
		/>
	</div>
	<ol
		id="message_list"
		class="h-full list-inside list-disc overflow-y-auto pl-5 pt-1 marker:text-blue-500"
	>
		{#each logs as { date, severity, content }}
			<li
				class:text-rose-500={severity === Severity.error}
				class:text-orange-700={severity === Severity.warn}
			>
				{date?.toLocaleTimeString()}
				- {content}
			</li>
		{/each}
	</ol>
</div>
