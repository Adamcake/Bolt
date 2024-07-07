<script lang="ts">
	import { AuthService } from '$lib/Services/AuthService';
	import { BoltService } from '$lib/Services/BoltService';
	import { bolt } from '$lib/State/Bolt';
	import { GlobalState } from '$lib/State/GlobalState';

	const { profiles, config } = GlobalState;
</script>

<select
	class="w-full cursor-pointer rounded-lg border-2 border-inherit bg-inherit p-2 text-center"
	bind:value={$config.selected_user_id}
>
	{#each $profiles as profile (profile.user.userId)}
		<option
			selected={profile.user.userId === $config.selected_user_id}
			value={profile.user.userId}
			class="dark:bg-slate-900">{profile.user.displayName}</option
		>
	{/each}
</select>

<div class="mt-5 flex">
	<button
		class="mx-auto mr-2 rounded-lg bg-blue-500 p-2 font-bold text-black duration-200 hover:opacity-75"
		on:click={() => {
			const { origin, redirect, clientid } = bolt.env;
			AuthService.openLoginWindow(origin, redirect, clientid);
		}}
	>
		Log In
	</button>
	<button
		class="mx-auto rounded-lg border-2 border-blue-500 p-2 font-bold duration-200 hover:opacity-75"
		disabled={!$config.selected_user_id}
		on:click={() => {
			if (!$config.selected_user_id) return;
			BoltService.logout($config.selected_user_id);
		}}
	>
		Log Out
	</button>
</div>
