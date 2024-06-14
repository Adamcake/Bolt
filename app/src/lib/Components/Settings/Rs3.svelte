<script lang="ts">
	import { onMount } from 'svelte';
	import { bolt, config, isConfigDirty, hasBoltPlugins } from '$lib/store';

	let configUriDiv: HTMLDivElement;
	let configUriAddress: HTMLTextAreaElement;
	let useUri: HTMLInputElement;

	// enables the ability to use a custom uri
	// decided by config load as well
	function toggleUriDiv(): void {
		configUriDiv.classList.toggle('opacity-25');
		configUriAddress.disabled = !configUriAddress.disabled;
		$isConfigDirty = true;

		if (!useUri.checked) {
			configUriAddress.value = atob($bolt.default_config_uri);
			$config.rs_config_uri = '';
		}
	}

	function uriAddressChanged(): void {
		$config.rs_config_uri = configUriAddress.value;
		$isConfigDirty = true;
	}

	// loads configs for menu
	onMount(() => {
		if ($config.rs_config_uri) {
			configUriAddress.value = $config.rs_config_uri;
			useUri.checked = true;
			toggleUriDiv();
		} else {
			configUriAddress.value = atob($bolt.default_config_uri);
		}
	});
</script>

<div id="rs3_options" class="col-span-3 p-5 pt-10">
	{#if hasBoltPlugins}
		<div class="mx-auto p-2">
			<label for="enable_plugins">Enable Bolt plugin loader: </label>
			<input
				type="checkbox"
				name="enable_plugins"
				id="enable_plugins"
				bind:checked={$config.rs_plugin_loader}
				on:change={() => isConfigDirty.set(true)}
				class="ml-2"
			/>
		</div>
	{/if}
	<div class="mx-auto p-2">
		<label for="use_custom_uri">Use custom config URI: </label>
		<input
			type="checkbox"
			name="use_custom_uri"
			id="use_custom_uri"
			bind:this={useUri}
			on:change={() => {
				toggleUriDiv();
			}}
			class="ml-2"
		/>
	</div>
	<div id="config_uri_div" class="mx-auto p-2 opacity-25" bind:this={configUriDiv}>
		<textarea
			disabled
			name="config_uri_address"
			id="config_uri_address"
			rows="4"
			on:change={() => {
				uriAddressChanged();
			}}
			bind:this={configUriAddress}
			class="rounded border-2 border-slate-300 bg-slate-100 text-slate-950 dark:border-slate-800 dark:bg-slate-900 dark:text-slate-50"
		>
		</textarea>
	</div>
</div>
