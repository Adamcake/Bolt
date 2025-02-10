<script lang="ts">
	import Modal from '$lib/Components/CommonUI/Modal.svelte';
	import { bolt } from '$lib/State/Bolt';
	import { BoltService } from '$lib/Services/BoltService';
	import { requestNewClientListPromise } from '$lib/Util/functions';
	import {
		type PluginConfig,
		type PluginMeta,
		type PluginUpdaterConfig
	} from '$lib/Util/interfaces';
	import { logger } from '$lib/Util/Logger';
	import { clientList } from '$lib/Util/store';
	import { GlobalState } from '$lib/State/GlobalState';

	let modal: Modal;

	const platformFileSep: string = bolt.platform === 'windows' ? '\\' : '/';
	const configFileName: string = 'bolt.json';
	const sepConfigFileName: string = platformFileSep.concat(configFileName);
	const defaultMainLuaFilename: string = 'main.lua';

	export function open() {
		showURLEntry = false;
		modal.open();
	}

	const getPluginConfigPromiseFromPath = (dirpath: string): Promise<PluginConfig> => {
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

	const getPluginConfigPromiseFromDataDir = (id: string): Promise<PluginConfig> => {
		return fetch('/get-plugindir-json?'.concat(new URLSearchParams({ id }).toString())).then(
			(x) => {
				if (!x.ok) return null;
				return x.json();
			}
		);
	};

	const getPluginConfigPromiseFromID = (id: string): Promise<PluginConfig> | null => {
		const list = bolt.pluginConfig;
		const meta = list[id];
		if (!meta) return null;
		const path = meta.path;
		if (path) return getPluginConfigPromiseFromPath(path);
		return getPluginConfigPromiseFromDataDir(id);
	};

	const getNewPluginID = () => {
		const ids = Object.keys(bolt.pluginConfig);
		let id;
		do {
			id = crypto.randomUUID();
		} while (ids.includes(id));
		return id;
	};

	// new plugin handlers
	const unnamedPluginName = '(unnamed)';
	const unnamedClientName = '(new character)';
	const addPluginFromPath = (folderPath: string, configPath: string) => {
		getPluginConfigPromiseFromPath(folderPath)
			.then((plugin: PluginConfig) => {
				selectedPlugin = getNewPluginID();
				bolt.pluginConfig[selectedPlugin] = {
					name: plugin.name ?? unnamedPluginName,
					path: folderPath,
					version: plugin.version
				};
				GlobalState.pluginConfigHasPendingChanges = true;
			})
			.catch((reason) => {
				console.error(`Config file '${configPath}' couldn't be fetched, reason: ${reason}`);
			});
	};
	const addPluginFromUpdaterURL = (url: string) => {
		fetch(url).then(async (x) => {
			if (!x.ok) return;
			const config: PluginUpdaterConfig = await x.json();
			if (config.url) {
				const data = await fetch(config.url).then((x) => x.arrayBuffer());
				if (config.sha256) {
					const hash = await crypto.subtle.digest('SHA-256', data);
					const hashStr = Array.from(new Uint8Array(hash))
						.map((x) => x.toString(16).padStart(2, '0'))
						.join('');
					if (config.sha256 !== hashStr) {
						console.error(`not installing plugin: incorrect file hash`);
						return;
					}
				}

				const id = getNewPluginID();
				const r = await fetch('/install-plugin?'.concat(new URLSearchParams({ id }).toString()), {
					method: 'POST',
					body: data
				});
				if (!r.ok) return;
				const plugin = await getPluginConfigPromiseFromDataDir(id);
				if (plugin) {
					selectedPlugin = id;
					bolt.pluginConfig[selectedPlugin] = {
						name: plugin.name ?? unnamedPluginName,
						version: plugin.version,
						updaterURL: url,
						sha256: config.sha256
					};
					GlobalState.pluginConfigHasPendingChanges = true;
				}
			}
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
						addPluginFromPath(subpath, xml.responseText);
					} else {
						console.log(`Selection '${xml.responseText}' is not named ${configFileName}; ignored`);
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
	const startPlugin = (client: number, id: string, path: string | null, main: string) => {
		let newPath = null;
		if (path) {
			const pathWithCorrectSeps: string =
				bolt.platform === 'windows' ? path.replaceAll('\\', '/') : path;
			newPath = pathWithCorrectSeps.endsWith(platformFileSep)
				? pathWithCorrectSeps
				: pathWithCorrectSeps.concat('/');
		}

		var xml = new XMLHttpRequest();
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				requestNewClientListPromise();
				logger.info(`Start-plugin status: ${xml.statusText.trim()}`);
			}
		};
		if (newPath) {
			xml.open(
				'GET',
				'/start-plugin?'.concat(
					new URLSearchParams({ client: client.toString(), id, main, path: newPath }).toString()
				),
				true
			);
		} else {
			xml.open(
				'GET',
				'/start-plugin?'.concat(
					new URLSearchParams({ client: client.toString(), id, main }).toString()
				),
				true
			);
		}
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

	// update a plugin by downloading the contents of its updater URL and checking for a new version
	const updatePlugin = (meta: PluginMeta, id: string) => {
		// this function can only be called for PluginMetas that have an updaterURL
		const url: string = <string>meta.updaterURL;
		fetch(url).then(async (x) => {
			let config: PluginUpdaterConfig = await x.json();
			let downloadNeeded = false;
			if (config.sha256) {
				if (meta.sha256 !== config.sha256) {
					downloadNeeded = true;
				}
			} else if (config.version) {
				if (meta.version !== config.version) {
					downloadNeeded = true;
				}
			}

			if (downloadNeeded && config.url) {
				const r = await fetch(config.url);
				if (!r.ok) return null;
				const data = await r.arrayBuffer();
				if (config.sha256) {
					const hash = await crypto.subtle.digest('SHA-256', data);
					const hashStr = Array.from(new Uint8Array(hash))
						.map((x) => x.toString(16).padStart(2, '0'))
						.join('');
					if (config.sha256 !== hashStr) {
						console.error(`not updating plugin '${meta.name}': incorrect file hash`);
						return;
					}
				}
				fetch('/install-plugin?'.concat(new URLSearchParams({ id }).toString()), {
					method: 'POST',
					body: data
				});
				const plugin = await getPluginConfigPromiseFromDataDir(id);
				if (plugin) {
					if (config.sha256) meta.sha256 = config.sha256;
					if (plugin.name) meta.name = plugin.name;
					if (plugin.version) meta.version = plugin.version;
					GlobalState.pluginConfigHasPendingChanges = true;
				}
			}
		});
	};

	// plugin management interface - currently-selected plugin
	var selectedPlugin: string;
	$: selectedPluginMeta = bolt.pluginConfig[selectedPlugin];
	$: selectedPluginPath = selectedPluginMeta ? selectedPluginMeta.path : null;
	$: managementPluginPromise = getPluginConfigPromiseFromID(selectedPlugin);
	$: if (managementPluginPromise) {
		managementPluginPromise.then((x) => {
			// if the name in bolt.json has been changed, update it in the PluginMeta and our plugin config file
			let dirty = false;
			if (x.name !== selectedPluginMeta.name) {
				selectedPluginMeta.name = x.name;
				dirty = true;
			}
			if (x.version !== selectedPluginMeta.version) {
				selectedPluginMeta.version = x.version;
				dirty = true;
			}
			if (dirty) {
				selectedPluginMeta = selectedPluginMeta;
				GlobalState.pluginConfigHasPendingChanges = true;
			}
		});
	}

	const openAboutPlugins = () =>
		fetch('/open-external-url', { method: 'POST', body: 'https://bolt.adamcake.com/plugins' });

	// connected clients list
	var isClientSelected: boolean = false;
	var selectedClientId: number;

	let showURLEntry: boolean = false;
	let textURLEntry: string;
</script>

<Modal
	bind:this={modal}
	class="h-[90%] w-[90%] text-center"
	on:close={() => BoltService.savePluginConfig(true)}
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
						showURLEntry = false;
						textURLEntry = '';
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
				{#each Object.entries(bolt.pluginConfig) as [id, plugin]}
					<option class="dark:bg-slate-900" value={id}>{plugin.name ?? unnamedPluginName}</option>
				{/each}
			</select>
			{#if !isClientSelected}
				<span class="align-middle">
					<button
						class="mx-1 aspect-square w-9 rounded-md bg-blue-500 p-1 text-[20px] font-bold duration-200 enabled:hover:opacity-75 disabled:bg-gray-500"
						on:click={() => {
							showURLEntry = !showURLEntry;
							textURLEntry = '';
						}}
						disabled={!bolt.hasLibArchive || disableButtons}
						title="Install plugin from updater URL"
					>
						<img src="svgs/download-solid.svg" alt="Install plugin from updater URL" />
					</button>
					<button
						class="aspect-square w-9 rounded-md bg-blue-500 p-1 text-[20px] font-bold duration-200 enabled:hover:opacity-75 disabled:bg-gray-500"
						on:click={jsonFilePicker}
						disabled={disableButtons}
						title="Install plugin from local directory"
					>
						<img src="svgs/folder-solid.svg" alt="Install plugin from local directory" />
					</button>
				</span>
				<br />
				{#if showURLEntry}
					<label for="plugin-updater-url-input">URL:</label>
					<textarea
						rows="1"
						id="plugin-updater-url-input"
						class="w-[50%] max-w-[60%] resize-x rounded border-2 border-slate-300 bg-slate-100 text-slate-950 dark:border-slate-800"
						bind:value={textURLEntry}
					/>
					<button
						title="Confirm"
						on:click={() => {
							addPluginFromUpdaterURL(textURLEntry);
							showURLEntry = false;
							textURLEntry = '';
						}}
						class="rounded-sm bg-emerald-500 hover:opacity-75"
						><img src="svgs/check-solid.svg" class="h-5 w-5" alt="Confirm" /></button
					>
					<button
						title="Close URL entry"
						on:click={() => {
							showURLEntry = false;
							textURLEntry = '';
						}}
						class="rounded-sm bg-rose-500 hover:opacity-75"
						><img src="svgs/xmark-solid.svg" class="h-5 w-5" alt="Close" /></button
					>
					<br /><br />
				{/if}
				{#if Object.entries(bolt.pluginConfig).length !== 0}
					{#if Object.keys(bolt.pluginConfig).includes(selectedPlugin) && managementPluginPromise !== null}
						{#await managementPluginPromise}
							<p>loading...</p>
						{:then plugin}
							<p class="pb-4 text-xl font-bold">
								{plugin.name ?? unnamedPluginName}
								{#if plugin.version}
									<br />
									<span class="pb-4 text-xl font-bold italic text-slate-600">
										v{plugin.version}
									</span>
								{/if}
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
								GlobalState.pluginConfigHasPendingChanges = true;
								let list = bolt.pluginConfig;
								delete list[selectedPlugin];
								bolt.pluginConfig = list;
							}}
						>
							Remove
						</button>
						{#if selectedPluginMeta.updaterURL}
							&nbsp;
							<button
								class="mx-auto mb-1 w-[min(144px,_25%)] rounded-lg p-2 font-bold text-black duration-200 enabled:bg-blue-500 enabled:hover:opacity-75 disabled:bg-gray-500"
								on:click={() => updatePlugin(selectedPluginMeta, selectedPlugin)}
							>
								Check updates
							</button>
						{/if}
					{/if}
				{:else}
					<p>
						You have no plugins installed. You can install plugins either from an updater URL, or by
						downloading them onto your computer and selecting the "bolt.json" file.
					</p>
				{/if}
			{:else}
				<br />
				{#await managementPluginPromise}
					<p>loading...</p>
				{:then plugin}
					{#if plugin && plugin.main && Object.keys(bolt.pluginConfig).includes(selectedPlugin)}
						<button
							class="mx-auto mb-1 w-auto rounded-lg bg-emerald-500 p-2 font-bold text-black duration-200 hover:opacity-75"
							on:click={() =>
								startPlugin(
									selectedClientId,
									selectedPlugin,
									selectedPluginPath ?? null,
									plugin.main ?? defaultMainLuaFilename
								)}
						>
							Start {plugin.name}
						</button>
					{:else if Object.entries(bolt.pluginConfig).length === 0}
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
								{#if Object.keys(bolt.pluginConfig).includes(activePlugin.id)}
									<p>
										{bolt.pluginConfig[activePlugin.id].name ?? activePlugin.id}
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
	<div class="absolute bottom-2 right-4">
		<button
			class="m-0 cursor-pointer border-none bg-transparent p-0 text-sm text-gray-500 underline"
			on:click={openAboutPlugins}>about plugins</button
		>
	</div>
</Modal>
