<script lang="ts">
	import { logger } from '$lib/Util/Logger';
	import { GlobalState } from '$lib/State/GlobalState';

	// opens local data on disk
	function openDataDir(): void {
		var xml = new XMLHttpRequest();
		xml.open('GET', '/browse-data');
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				logger.info(`Browse status: '${xml.responseText.trim()}'`);
			}
		};
		xml.send();
	}

	const { config } = GlobalState;
</script>

<button
	id="data_dir_button"
	class="p-2 hover:opacity-75"
	on:click={() => {
		openDataDir();
	}}
>
	<div class="flex">
		<img
			src="svgs/database-solid.svg"
			alt="Browse app data"
			class="mr-2 h-7 w-7 rounded-lg bg-violet-500 p-1"
		/>
		Browse App Data
	</div>
</button>

<div class="mx-auto p-2">
	<label for="check_announcements">Check game announcements: </label>
	<input
		id="check_announcements"
		type="checkbox"
		bind:checked={$config.check_announcements}
		class="ml-2"
	/>
</div>
