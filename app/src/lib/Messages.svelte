<script lang="ts">
	import { message_list } from '../store';
</script>

<div
	class="border-t-2 fixed bottom-0 w-screen h-1/4 duration-200 border-t-slate-300 dark:border-t-slate-800 bg-slate-100 dark:bg-slate-900">
	<div class="absolute right-2 top-2">
		<img
			src="svgs/circle-info-solid.svg"
			alt="Message list icon"
			class="w-7 h-7 p-[3px] rounded-full bg-blue-500 duration-200" />
	</div>
	<ol
		id="message_list"
		class="overflow-y-auto pt-1 h-full w-[105%] list-disc pl-5 list-inside marker:text-blue-500">
		{#each $message_list as message}
			{#if message.is_error}
				<li class="text-rose-500">
					{message.time?.getHours()}:{message.time?.getMinutes()}:{message.time?.getSeconds()}
					- {message.text}
				</li>
			{:else}
				<li>
					{message.time?.getHours()}:{message.time?.getMinutes()}:{message.time?.getSeconds()}
					- {message.text}
				</li>
			{/if}
		{/each}
	</ol>
</div>
