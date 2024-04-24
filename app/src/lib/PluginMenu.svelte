<script lang="ts">
	import { get } from 'svelte/store';
	import { onDestroy } from 'svelte';
	import Backdrop from './Backdrop.svelte';
	import { getNewClientListPromise, savePluginConfig } from '../functions';
	import { type PluginConfig } from '../interfaces';
	import { clientListPromise, hasBoltPlugins, pluginList, platform } from '../store';

	// props
	export let showPluginMenu: boolean;

	// new plugin handler
	const handleNewPlugin = (folderPath: string, configPath: string) => {
		var xml = new XMLHttpRequest();
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				const plugin: PluginConfig = JSON.parse(xml.responseText);
				if (!plugin.name) {
					console.error(`Config file '${configPath}' missing required field "name"`);
				} else {
					const id: string = plugin.name
						.replaceAll('@', '-')
						.replaceAll(' ', '-')
						.toLowerCase()
						.concat(plugin.version ? `-${plugin.version}` : '');
					if (Object.keys(get(pluginList)).includes(id)) {
						console.error(
							`Config file '${configPath}' gives id '${id}', which is already installed`
						);
					} else {
						$pluginList[id] = { name: plugin.name, path: folderPath };
						savePluginConfig();
					}
				}
			}
		};
		xml.open(
			'GET',
			'/read-json-file?'.concat(new URLSearchParams({ path: configPath }).toString()),
			true
		);
		xml.send();
	};

	// json file picker
	let disableButtons: boolean = false;
	const jsonFilePicker = () => {
		disableButtons = true;
		var xml = new XMLHttpRequest();
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				disableButtons = false;
				// if the user closes the file picker without selecting a file, status here is 204
				if (xml.status == 200) {
					const path: string =
						get(platform) === 'windows'
							? xml.responseText.replaceAll('\\', '/')
							: xml.responseText;
					if (path.endsWith('/bolt.json')) {
						const subpath: string = path.substring(0, path.length - 9);
						handleNewPlugin(subpath, path);
					} else {
						console.log(`Selection '${path}' is not named bolt.json; ignored`);
					}
				}
			}
		};
		xml.open('GET', '/json-file-picker', true);
		xml.send();
	};

	// get connected client list
	clientListPromise.set(getNewClientListPromise());

	// hide plugin menu on 'escape'
	const tryExit = () => {
		if (!disableButtons) {
			showPluginMenu = false;
		}
	};
	function keyPressed(evt: KeyboardEvent): void {
		if (evt.key === 'Escape') {
			tryExit();
		}
	}
	addEventListener('keydown', keyPressed);

	onDestroy(() => {
		// When the component is removed, delete the event listener also
		removeEventListener('keydown', keyPressed);
	});
</script>

<div>
	<Backdrop on:click={tryExit}></Backdrop>
	<div
		class="absolute left-[5%] top-[5%] z-20 h-[90%] w-[90%] rounded-lg bg-slate-100 text-center shadow-lg dark:bg-slate-900">
		<button
			class="absolute right-3 top-3 rounded-full bg-rose-500 p-[2px] shadow-lg duration-200 hover:rotate-90 hover:opacity-75"
			on:click={tryExit}>
			<img src="svgs/xmark-solid.svg" class="h-5 w-5" alt="Close" />
		</button>
		<div
			class="left-0 float-left h-full w-[min(180px,_50%)] overflow-hidden border-r-2 border-slate-300 pt-2 dark:border-slate-800">
			<button
				class="mx-auto mb-2 w-[95%] rounded-lg bg-blue-500 p-2 font-bold text-black duration-200 hover:opacity-75"
				on:click={() => {}}>
				Manage Plugins
			</button>
			<hr class="p-1 dark:border-slate-700" />
			{#await $clientListPromise}
				<p>loading...</p>
			{:then clients}
				{#if clients.length == 0}
					<p>
						(start an RS3 game client with plugins enabled and it will be listed here.)
					</p>
				{:else}
					{#each clients as client}
						<button
							class="m-1 h-[28px] w-[95%] rounded-lg border-2 border-blue-500 duration-200 hover:opacity-75">
							{client.identity || '(unnamed)'}
						</button>
						<br />
					{/each}
				{/if}
			{:catch}
				<p>error</p>
			{/await}
		</div>
		<div class="h-full pt-10">
			{#if hasBoltPlugins}
				<select
					class="mx-auto mb-4 w-[min(280px,_45%)] cursor-pointer rounded-lg border-2 border-slate-300 bg-inherit p-2 text-inherit duration-200 hover:opacity-75 dark:border-slate-800">
					{#each Object.entries($pluginList) as [id, plugin]}
						<option class="dark:bg-slate-900">{plugin.name ?? id}</option>
					{/each}
				</select>
				<button
					class="aspect-square w-8 rounded-lg border-2 border-blue-500 text-[20px] font-bold duration-200 enabled:hover:opacity-75 disabled:border-gray-500"
					on:click={jsonFilePicker}
					disabled={disableButtons}>
					+
				</button>
				{#if Object.entries($pluginList).length === 0}
					<p>
						You have no plugins installed. Click the + button and select a plugin's
						bolt.json file to add it.
					</p>
				{/if}
			{:else}
				<p>error</p>
			{/if}
		</div>
	</div>
</div>
