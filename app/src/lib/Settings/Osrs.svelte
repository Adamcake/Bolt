<script lang="ts">
	import { onMount } from 'svelte';
	import { config, is_config_dirty, platform, selected_play } from '../../store';
	import { launchRuneLiteConfigure } from '../../functions';
	import { msg } from '../../main';

	let custom_jar_div: HTMLDivElement;
	let custom_jar_file: HTMLTextAreaElement;
	let custom_jar_file_button: HTMLButtonElement;
	let use_jar: HTMLInputElement;
	let flatpak_presence: HTMLInputElement;
	let flatpak_div: HTMLDivElement;

	// enables the ability to select a custom jar
	// decided by config load as well
	function toggle_jar_div(): void {
		custom_jar_div.classList.toggle('opacity-25');
		custom_jar_file.disabled = !custom_jar_file.disabled;
		custom_jar_file_button.disabled = !custom_jar_file_button.disabled;

		$config.runelite_use_custom_jar = use_jar.checked;
		if ($config.runelite_use_custom_jar) {
			if (custom_jar_file.value) $config.runelite_custom_jar = custom_jar_file.value;
		} else {
			custom_jar_file.value = '';
			$config.runelite_custom_jar = '';
		}
		$is_config_dirty = true;
	}

	function toggle_rich_presence(): void {
		$config.flatpak_rich_presence = flatpak_presence!.checked;
		$is_config_dirty = true;
	}

	function text_changed(): void {
		$config.runelite_custom_jar = custom_jar_file.value;
		$is_config_dirty = true;
	}

	function select_file(): void {
		use_jar.disabled = true;
		custom_jar_file_button!.disabled = true;

		// note: this would give only the file contents, whereas we need the path and don't want the contents:
		//window.showOpenFilePicker({"types": [{"description": "Java Archive (JAR)", "accept": {"application/java-archive": [".jar"]}}]}).then((x) => { });

		var xml = new XMLHttpRequest();
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				// if the user closes the file picker without selecting a file, status here is 204
				if (xml.status == 200) {
					custom_jar_file.value = xml.responseText;
					$config.runelite_custom_jar = xml.responseText;
					$is_config_dirty = true;
				}
				custom_jar_file_button.disabled = false;
				use_jar.disabled = false;
			}
		};
		xml.open('GET', '/jar-file-picker', true);
		xml.send();
	}

	// gets relevant data from selected_play store
	function launch_configure(): void {
		if (!$selected_play.account || !$selected_play.character) {
			msg('Please log in to configure RuneLite');
			return;
		}
		launchRuneLiteConfigure(
			<string>$selected_play.credentials?.access_token,
			<string>$selected_play.credentials?.refresh_token,
			<string>$selected_play.credentials?.session_id,
			<string>$selected_play.character?.accountId,
			<string>$selected_play.character?.displayName
		);
	}

	// load config into the menues
	onMount(() => {
		flatpak_presence.checked = <boolean>$config.flatpak_rich_presence;

		use_jar.checked = <boolean>$config.runelite_use_custom_jar;
		if (use_jar.checked && $config.runelite_custom_jar) {
			custom_jar_div.classList.remove('opacity-25');
			custom_jar_file.disabled = false;
			custom_jar_file_button.disabled = false;
			custom_jar_file.value = $config.runelite_custom_jar;
		} else {
			use_jar.checked = false;
		}

		if ($platform !== 'linux') {
			flatpak_div.remove();
		}
	});
</script>

<div id="osrs_options" class="col-span-3 p-5 pt-10">
	<button id="rl_configure" class="p-2 pb-5 hover:opacity-75" on:click={() => launch_configure()}>
		<div class="flex">
			<img
				src="svgs/wrench-solid.svg"
				alt="Configure RuneLite"
				class="w-7 h-7 p-1 mr-2 rounded-lg bg-pink-500" />
			Configure RuneLite
		</div>
	</button>
	<div
		id="flatpak_div"
		class="mx-auto p-2 py-5 border-t-2 border-slate-300 dark:border-slate-800"
		bind:this={flatpak_div}>
		<label for="flatpak_rich_presence">Expose rich presense to Flatpak Discord: </label>
		<input
			type="checkbox"
			name="flatpak_rich_presence"
			id="flatpak_rich_presence"
			class="ml-2"
			bind:this={flatpak_presence}
			on:change={() => {
				toggle_rich_presence();
			}} />
	</div>
	<div class="mx-auto p-2 pt-5 border-t-2 border-slate-300 dark:border-slate-800">
		<label for="use_custom_jar">Use custom RuneLite JAR: </label>
		<input
			type="checkbox"
			name="use_custom_jar"
			id="use_custom_jar"
			bind:this={use_jar}
			on:change={() => {
				toggle_jar_div();
			}}
			class="ml-2" />
	</div>
	<div id="custom_jar_div" class="mx-auto p-2 opacity-25" bind:this={custom_jar_div}>
		<textarea
			disabled
			name="custom_jar_file"
			id="custom_jar_file"
			bind:this={custom_jar_file}
			on:change={() => {
				text_changed();
			}}
			class="h-10 rounded border-2 border-slate-300 dark:border-slate-800 text-slate-950 bg-slate-100 dark:text-slate-50 dark:bg-slate-900"
		></textarea>
		<br />
		<button
			disabled
			id="custom_jar_file_button"
			bind:this={custom_jar_file_button}
			class="p-1 mt-1 rounded-lg border-2 border-blue-500 hover:opacity-75 duration-200"
			on:click={() => {
				select_file();
			}}>
			Select File</button>
	</div>
</div>
