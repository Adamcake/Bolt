<script lang="ts">
	import Modal from '$lib/Components/CommonUI/Modal.svelte';
	import { bolt } from '$lib/State/Bolt';
	import { requestNewClientListPromise, savePluginConfig } from '$lib/Util/functions';
	import { type PluginConfig } from '$lib/Util/interfaces';
	import { logger } from '$lib/Util/Logger';
	import { clientList } from '$lib/Util/store';

	let modal: Modal;

	const platformFileSep: string = bolt.platform === 'windows' ? '\\' : '/';
	const configFileName: string = 'bolt.json';
	const sepConfigFileName: string = platformFileSep.concat(configFileName);
	const defaultMainLuaFilename: string = 'main.lua';

	export function open() {
		modal.open();
	}

	const getPluginConfigPromise = (dirpath: string): Promise<PluginConfig> => {
		return new Promise((resolve, reject) => {
			const path: string = dirpath.concat(
				dirpath.endsWith(platformFileSep) ? configFileName : sepConfigFileName
			);
			var xml = new XMLHttpRequest();
			xml.onreadystatechange = () => {
				if (xml.readyState == 4) {
					if (xml.status == 200) {
						resolve(<PluginConfig>JSON.parse(xml.responseText));
					} else {
						reject(xml.responseText);
					}
				}
			};
			xml.open('GET', '/read-json-file?'.concat(new URLSearchParams({ path }).toString()), true);
			xml.send();
		});
	};

	const getPluginConfigPromiseFromID = (id: string): Promise<PluginConfig> | null => {
		const list = bolt.pluginList;
		const meta = list[id];
		if (!meta) return null;
		const path = meta.path;
		return path ? getPluginConfigPromise(path) : null;
	};

	// new plugin handler
	const unnamedPluginName = '(unnamed)';
	const unnamedClientName = '(new character)';
	const handleNewPlugin = (folderPath: string, configPath: string) => {
		getPluginConfigPromise(folderPath)
			.then((plugin: PluginConfig) => {
				do {
					selectedPlugin = crypto.randomUUID();
				} while (Object.keys(bolt.pluginList).includes(selectedPlugin));
				bolt.pluginList[selectedPlugin] = {
					name: plugin.name ?? unnamedPluginName,
					path: folderPath
				};
				pluginConfigDirty = true;
			})
			.catch((reason) => {
				console.error(`Config file '${configPath}' couldn't be fetched, reason: ${reason}`);
			});
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
					if (xml.responseText.endsWith(sepConfigFileName)) {
						const subpath: string = xml.responseText.substring(
							0,
							xml.responseText.length - configFileName.length
						);
						handleNewPlugin(subpath, xml.responseText);
					} else {
						console.log(`Selection '${xml.responseText}' is not named bolt.json; ignored`);
					}
				}
			}
		};
		xml.open('GET', '/json-file-picker', true);
		xml.send();
	};

	// get connected client list
	requestNewClientListPromise();
	$: {
		if (!$clientList.some((x) => x.uid === selectedClientId)) {
			isClientSelected = false;
		}
	}

	// function to start a plugin
	const startPlugin = (client: number, id: string, path: string, main: string) => {
		const pathWithCorrectSeps: string =
			bolt.platform === 'windows' ? path.replaceAll('\\', '/') : path;
		const newPath = pathWithCorrectSeps.endsWith(platformFileSep)
			? pathWithCorrectSeps
			: pathWithCorrectSeps.concat('/');

		var xml = new XMLHttpRequest();
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				requestNewClientListPromise();
				logger.info(`Start-plugin status: ${xml.statusText.trim()}`);
			}
		};
		xml.open(
			'GET',
			'/start-plugin?'.concat(
				new URLSearchParams({ client: client.toString(), id, path: newPath, main }).toString()
			),
			true
		);
		xml.send();
	};

	// function to stop a plugin, by the client ID and plugin activation ID
	const stopPlugin = (client: number, uid: number) => {
		var xml = new XMLHttpRequest();
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				requestNewClientListPromise();
				logger.info(`Stop-plugin status: ${xml.statusText.trim()}`);
			}
		};
		xml.open(
			'GET',
			'/stop-plugin?'.concat(
				new URLSearchParams({ client: client.toString(), uid: uid.toString() }).toString()
			),
			true
		);
		xml.send();
	};

	// plugin management interface - currently-selected plugin
	var selectedPlugin: string;
	$: selectedPluginMeta = bolt.pluginList[selectedPlugin];
	$: selectedPluginPath = selectedPluginMeta ? selectedPluginMeta.path : null;
	$: managementPluginPromise = getPluginConfigPromiseFromID(selectedPlugin);
	$: if (managementPluginPromise) {
		managementPluginPromise.then((x) => {
			// if the name in bolt.json has been changed, update it in the PluginMeta and our plugin config file
			if (x.name !== selectedPluginMeta.name) {
				selectedPluginMeta.name = x.name;
				selectedPluginMeta = selectedPluginMeta;
				pluginConfigDirty = true;
			}
		});
	}

	// connected clients list
	var isClientSelected: boolean = false;
	var selectedClientId: number;

	let pluginConfigDirty: boolean = false;
</script>

<Modal
	bind:this={modal}
	class="h-[90%] w-[90%] text-center"
	on:close={() => {
		if (pluginConfigDirty) {
			savePluginConfig();
			pluginConfigDirty = false;
		}
	}}
>
	<div
		class="left-0 float-left h-full w-[min(180px,_50%)] overflow-hidden border-r-2 border-slate-300 pt-2 dark:border-slate-800"
	>
		<button
			class="mx-auto mb-2 w-[95%] rounded-lg border-2 {isClientSelected
				? 'border-blue-500 text-black dark:text-white'
				: 'border-black bg-blue-500 text-black'} p-2 font-bold hover:opacity-75"
			on:click={() => (isClientSelected = false)}
		>
			Manage Plugins
		</button>
		<hr class="p-1 dark:border-slate-700" />
		{#if $clientList.length == 0}
			<p>(start an RS3 game client with plugins enabled and it will be listed here.)</p>
		{:else}
			{#each $clientList as client}
				<button
					on:click={() => {
						selectedClientId = client.uid;
						isClientSelected = true;
					}}
					class="m-1 h-[28px] w-[95%] rounded-lg border-2 {isClientSelected &&
					selectedClientId === client.uid
						? 'border-black bg-blue-500 text-black'
						: 'border-blue-500 text-black dark:text-white'} hover:opacity-75"
				>
					{client.identity || unnamedClientName}
				</button>
				<br />
			{/each}
		{/if}
	</div>
	<div class="h-full pt-10">
		{#if bolt.hasBoltPlugins}
			<select
				bind:value={selectedPlugin}
				class="mx-auto mb-4 w-[min(280px,_45%)] cursor-pointer rounded-lg border-2 border-slate-300 bg-inherit p-2 text-inherit duration-200 hover:opacity-75 dark:border-slate-800"
			>
				{#each Object.entries(bolt.pluginList) as [id, plugin]}
					<option class="dark:bg-slate-900" value={id}>{plugin.name ?? unnamedPluginName}</option>
				{/each}
			</select>
			{#if !isClientSelected}
				<button
					class="aspect-square w-8 rounded-lg border-2 border-blue-500 text-[20px] font-bold duration-200 enabled:hover:opacity-75 disabled:border-gray-500"
					on:click={jsonFilePicker}
					disabled={disableButtons}
				>
					+
				</button>
				<br />
				{#if Object.entries(bolt.pluginList).length !== 0}
					{#if Object.keys(bolt.pluginList).includes(selectedPlugin) && managementPluginPromise !== null}
						{#await managementPluginPromise}
							<p>loading...</p>
						{:then plugin}
							<p class="pb-4 text-xl font-bold">
								{plugin.name ?? unnamedPluginName}
							</p>
							<p class={plugin.description ? null : 'italic'}>
								{plugin.description ?? 'no description'}
							</p>
							<br />
						{:catch}
							<p>error</p>
							<br />
						{/await}
						<button
							class="mx-auto mb-1 w-[min(144px,_25%)] rounded-lg p-2 font-bold text-black duration-200 enabled:bg-rose-500 enabled:hover:opacity-75 disabled:bg-gray-500"
							on:click={() => {
								managementPluginPromise = null;
								pluginConfigDirty = true;
								let list = bolt.pluginList;
								delete list[selectedPlugin];
								bolt.pluginList = list;
							}}
						>
							Remove
						</button>
						<button
							class="mx-auto mb-1 w-[min(144px,_25%)] rounded-lg p-2 font-bold text-black duration-200 enabled:bg-blue-500 enabled:hover:opacity-75 disabled:bg-gray-500"
							on:click={() => {
								managementPluginPromise = getPluginConfigPromiseFromID(selectedPlugin);
							}}
						>
							Reload
						</button>
					{/if}
				{:else}
					<p>
						You have no plugins installed. Click the + button and select a plugin's bolt.json file
						to add it.
					</p>
				{/if}
			{:else}
				<br />
				{#await managementPluginPromise}
					<p>loading...</p>
				{:then plugin}
					{#if plugin && plugin.main && Object.keys(bolt.pluginList).includes(selectedPlugin)}
						{#if selectedPluginPath}
							<button
								class="mx-auto mb-1 w-auto rounded-lg bg-emerald-500 p-2 font-bold text-black duration-200 hover:opacity-75"
								on:click={() =>
									startPlugin(
										selectedClientId,
										selectedPlugin,
										selectedPluginPath,
										plugin.main ?? defaultMainLuaFilename
									)}
							>
								Start {plugin.name}
							</button>
						{:else}
							<p>can't start plugin: no path is configured</p>
						{/if}
					{:else if Object.entries(bolt.pluginList).length === 0}
						<p>(no plugins installed)</p>
					{:else}
						<p>can't start plugin: does not appear to be configured</p>
					{/if}
					<br />
					<br />
					<hr class="p-1 dark:border-slate-700" />
					{#each $clientList as client}
						{#if client.uid === selectedClientId}
							{#each client.plugins as activePlugin}
								{#if Object.keys(bolt.pluginList).includes(activePlugin.id)}
									<p>
										{bolt.pluginList[activePlugin.id].name ?? activePlugin.id}
										<button
											class="rounded-sm bg-rose-500 shadow-lg hover:opacity-75"
											on:click={() => {
												stopPlugin(selectedClientId, activePlugin.uid);
											}}
										>
											<img src="svgs/xmark-solid.svg" class="h-4 w-4" alt="Close" />
										</button>
									</p>
								{:else}
									<p>{activePlugin.id}</p>
								{/if}
							{/each}
						{/if}
					{/each}
				{:catch}
					<p>error</p>
				{/await}
			{/if}
		{:else}
			<p>error</p>
		{/if}
	</div>
</Modal>
