<script lang="ts">
	import { onMount } from 'svelte';
	import { bolt, config, is_config_dirty } from '../../store';

	let config_uri_div: HTMLDivElement;
	let config_uri_address: HTMLTextAreaElement;
	let use_uri: HTMLInputElement;

	// enables the ability to use a custom uri
	// decided by config load as well
	function toggle_uri_div(): void {
		config_uri_div.classList.toggle('opacity-25');
		config_uri_address.disabled = !config_uri_address.disabled;
		$is_config_dirty = true;

		if (!use_uri.checked) {
			config_uri_address.value = atob($bolt.default_config_uri);
			$config.rs_config_uri = '';
		}
	}

	function uri_address_changed(): void {
		$config.rs_config_uri = config_uri_address.value;
		$is_config_dirty = true;
	}

	// loads configs for menu
	onMount(() => {
		if ($config.rs_config_uri) {
			config_uri_address.value = $config.rs_config_uri;
			use_uri.checked = true;
			toggle_uri_div();
		} else {
			config_uri_address.value = atob($bolt.default_config_uri);
		}
	});
</script>

<div id="rs3_options" class="col-span-3 p-5 pt-10">
	<div class="mx-auto p-2">
		<label for="use_custom_uri">Use custom config URI: </label>
		<input
			type="checkbox"
			name="use_custom_uri"
			id="use_custom_uri"
			bind:this={use_uri}
			on:change={() => {
				toggle_uri_div();
			}}
			class="ml-2" />
	</div>
	<div id="config_uri_div" class="mx-auto p-2 opacity-25" bind:this={config_uri_div}>
		<textarea
			disabled
			name="config_uri_address"
			id="config_uri_address"
			rows="4"
			on:change={() => {
				uri_address_changed();
			}}
			bind:this={config_uri_address}
			class="rounded border-2 border-slate-300 bg-slate-100 text-slate-950 dark:border-slate-800 dark:bg-slate-900 dark:text-slate-50">
		</textarea>
	</div>
</div>
