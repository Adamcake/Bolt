<script lang="ts">
	import { AuthService } from '$lib/Services/AuthService';
	import { BoltService } from '$lib/Services/BoltService';
	import { bolt } from '$lib/State/Bolt';
	import { GlobalState } from '$lib/State/GlobalState';

	const { sessions, config } = GlobalState;
</script>

<select
	class="w-full cursor-pointer rounded-lg border-2 border-inherit bg-inherit p-2 text-center"
	bind:value={$config.selected.user_id}
>
	{#each $sessions as session (session.user.userId)}
		<option
			selected={session.user.userId === $config.selected.user_id}
			value={session.user.userId}
			class="dark:bg-slate-900">{session.user.displayName}</option
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
		disabled={!$config.selected.user_id}
		on:click={() => {
			if (!$config.selected.user_id) return;
			BoltService.logout($config.selected.user_id);
		}}
	>
		Log Out
	</button>
</div>
