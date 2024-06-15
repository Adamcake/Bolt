<script lang="ts">
	import { msg } from '@/main';

	// opens local data on disk
	function openDataDir(): void {
		var xml = new XMLHttpRequest();
		xml.open('GET', '/browse-data');
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				msg(`Browse status: '${xml.responseText.trim()}'`);
			}
		};
		xml.send();
	}
</script>

<div id="general_options" class="col-span-3 p-5 pt-10">
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
</div>
