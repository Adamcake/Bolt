<script lang="ts">
	import { bolt } from '$lib/State/Bolt';
	import { GlobalState } from '$lib/State/GlobalState';

	const { config } = GlobalState;
</script>

<div id="rs3_options" class="col-span-3 p-5 pt-10">
	{#if bolt.hasBoltPlugins}
		<div class="mx-auto p-2">
			<label for="enable_plugins">Enable Bolt plugin loader: </label>
			<input
				type="checkbox"
				name="enable_plugins"
				id="enable_plugins"
				bind:checked={$config.rs_plugin_loader}
				class="ml-2"
			/>
		</div>
	{/if}
	<div class="mx-auto p-2">
		<label for="use_custom_uri">Use custom config URI: </label>
		<input type="checkbox" bind:checked={$config.use_custom_rs_config_uri} class="ml-2" />
	</div>
	<div class="p-2">
		<textarea
			class="rounded border-2 border-slate-300 bg-slate-100 text-slate-950 disabled:opacity-50 dark:border-slate-800 dark:bg-slate-900 dark:text-slate-50"
			disabled={!$config.use_custom_rs_config_uri}
			rows="4"
			placeholder={bolt.env.default_config_uri}
			bind:value={$config.rs_config_uri}
		/>
	</div>
</div>
