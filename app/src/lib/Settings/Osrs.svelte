<script lang="ts">
	import { onMount } from 'svelte';
	import { config, isConfigDirty, platform, selectedPlay } from '../../store';
	import { launchRuneLiteConfigure } from '../../functions';
	import { msg } from '../../main';

	let customJarDiv: HTMLDivElement;
	let customJarFile: HTMLTextAreaElement;
	let cutomJarFileButton: HTMLButtonElement;
	let useJar: HTMLInputElement;
	let flatpakPresence: HTMLInputElement;
	let flatpakDiv: HTMLDivElement;

	// enables the ability to select a custom jar
	// decided by config load as well
	function toggleJarDiv(): void {
		customJarDiv.classList.toggle('opacity-25');
		customJarFile.disabled = !customJarFile.disabled;
		cutomJarFileButton.disabled = !cutomJarFileButton.disabled;

		$config.runelite_use_custom_jar = useJar.checked;
		if ($config.runelite_use_custom_jar) {
			if (customJarFile.value) $config.runelite_custom_jar = customJarFile.value;
		} else {
			customJarFile.value = '';
			$config.runelite_custom_jar = '';
		}
		$isConfigDirty = true;
	}

	function toggleRichPresence(): void {
		$config.flatpak_rich_presence = flatpakPresence!.checked;
		$isConfigDirty = true;
	}

	function textChanged(): void {
		$config.runelite_custom_jar = customJarFile.value;
		$isConfigDirty = true;
	}

	function selectFile(): void {
		useJar.disabled = true;
		cutomJarFileButton!.disabled = true;

		// note: this would give only the file contents, whereas we need the path and don't want the contents:
		//window.showOpenFilePicker({"types": [{"description": "Java Archive (JAR)", "accept": {"application/java-archive": [".jar"]}}]}).then((x) => { });

		var xml = new XMLHttpRequest();
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				// if the user closes the file picker without selecting a file, status here is 204
				if (xml.status == 200) {
					customJarFile.value = xml.responseText;
					$config.runelite_custom_jar = xml.responseText;
					$isConfigDirty = true;
				}
				cutomJarFileButton.disabled = false;
				useJar.disabled = false;
			}
		};
		xml.open('GET', '/jar-file-picker', true);
		xml.send();
	}

	// gets relevant data from selected_play store
	function launchConfigure(): void {
		if (!$selectedPlay.account || !$selectedPlay.character) {
			msg('Please log in to configure RuneLite');
			return;
		}
		launchRuneLiteConfigure(
			<string>$selectedPlay.credentials?.session_id,
			<string>$selectedPlay.character?.accountId,
			<string>$selectedPlay.character?.displayName
		);
	}

	// load config into the menues
	onMount(() => {
		flatpakPresence.checked = <boolean>$config.flatpak_rich_presence;

		useJar.checked = <boolean>$config.runelite_use_custom_jar;
		if (useJar.checked && $config.runelite_custom_jar) {
			customJarDiv.classList.remove('opacity-25');
			customJarFile.disabled = false;
			cutomJarFileButton.disabled = false;
			customJarFile.value = $config.runelite_custom_jar;
		} else {
			useJar.checked = false;
		}

		if ($platform !== 'linux') {
			flatpakDiv.remove();
		}
	});
</script>

<div id="osrs_options" class="col-span-3 p-5 pt-10">
	<button id="rl_configure" class="p-2 pb-5 hover:opacity-75" on:click={() => launchConfigure()}>
		<div class="flex">
			<img
				src="svgs/wrench-solid.svg"
				alt="Configure RuneLite"
				class="mr-2 h-7 w-7 rounded-lg bg-pink-500 p-1" />
			Configure RuneLite
		</div>
	</button>
	<div
		id="flatpak_div"
		class="mx-auto border-t-2 border-slate-300 p-2 py-5 dark:border-slate-800"
		bind:this={flatpakDiv}>
		<label for="flatpak_rich_presence">Expose rich presense to Flatpak Discord: </label>
		<input
			type="checkbox"
			name="flatpak_rich_presence"
			id="flatpak_rich_presence"
			class="ml-2"
			bind:this={flatpakPresence}
			on:change={() => {
				toggleRichPresence();
			}} />
	</div>
	<div class="mx-auto border-t-2 border-slate-300 p-2 pt-5 dark:border-slate-800">
		<label for="use_custom_jar">Use custom RuneLite JAR: </label>
		<input
			type="checkbox"
			name="use_custom_jar"
			id="use_custom_jar"
			bind:this={useJar}
			on:change={() => {
				toggleJarDiv();
			}}
			class="ml-2" />
	</div>
	<div id="custom_jar_div" class="mx-auto p-2 opacity-25" bind:this={customJarDiv}>
		<textarea
			disabled
			name="custom_jar_file"
			id="custom_jar_file"
			bind:this={customJarFile}
			on:change={() => {
				textChanged();
			}}
			class="h-10 rounded border-2 border-slate-300 bg-slate-100 text-slate-950 dark:border-slate-800 dark:bg-slate-900 dark:text-slate-50"
		></textarea>
		<br />
		<button
			disabled
			id="custom_jar_file_button"
			bind:this={cutomJarFileButton}
			class="mt-1 rounded-lg border-2 border-blue-500 p-1 duration-200 hover:opacity-75"
			on:click={() => {
				selectFile();
			}}>
			Select File</button>
	</div>
</div>
