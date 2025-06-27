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

	const checkAnnouncementsDesc =
		"Allow checking for official announcements about game status. These will be displayed above the 'play' button. Disabling will reduce the number of web requests made when opening the launcher.";
	const closeAfterLaunchDesc = 'Close the launcher immediately after launching a game';
	const discardExpiredLoginsDesc =
		"Discard login sessions if they're no longer valid, prompting the player to sign-in again. If unchecked, the only way to discard a login will be by manually pressing the 'log out' button, even if your login no longer works.";
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

<div class="mx-auto p-2" title={checkAnnouncementsDesc}>
	<label for="check_announcements">Check game announcements: </label>
	<input
		id="check_announcements"
		type="checkbox"
		bind:checked={$config.check_announcements}
		class="ml-2"
	/>
</div>
<div class="mx-auto p-2" title={closeAfterLaunchDesc}>
	<label for="close_after_launch">Close Bolt after launching a game: </label>
	<input
		id="close_after_launch"
		type="checkbox"
		bind:checked={$config.close_after_launch}
		class="ml-2"
	/>
</div>
<div class="mx-auto p-2" title={discardExpiredLoginsDesc}>
	<label for="discard_expired_logins">Discard expired login sessions: </label>
	<input
		id="discard_expired_logins"
		type="checkbox"
		bind:checked={$config.discard_expired_sessions}
		class="ml-2"
	/>
</div>
