<script lang="ts">
	import { messageList } from '../store';
</script>

<div
	class="fixed bottom-0 h-1/4 w-screen border-t-2 border-t-slate-300 bg-slate-100 duration-200 dark:border-t-slate-800 dark:bg-slate-900">
	<div class="absolute right-2 top-2">
		<img
			src="svgs/circle-info-solid.svg"
			alt="Message list icon"
			class="h-7 w-7 rounded-full bg-blue-500 p-[3px] duration-200" />
	</div>
	<ol
		id="message_list"
		class="h-full w-[105%] list-inside list-disc overflow-y-auto pl-5 pt-1 marker:text-blue-500">
		{#each $messageList as message}
			{#if message.isError}
				<li class="text-rose-500">
					{message.time?.toLocaleTimeString()}
					- {message.text}
				</li>
			{:else}
				<li>
					{message.time?.toLocaleTimeString()}
					- {message.text}
				</li>
			{/if}
		{/each}
	</ol>
</div>
