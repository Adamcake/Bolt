<script lang="ts">
	import Modal from '$lib/Components/CommonUI/Modal.svelte';
	import { bolt } from '$lib/State/Bolt';
	import { BoltService } from '$lib/Services/BoltService';
	import { requestNewClientListPromise } from '$lib/Util/Functions';
	import {
		type PluginConfig,
		type PluginMeta,
		type PluginUpdaterConfig
	} from '$lib/Util/Interfaces';
	import { logger } from '$lib/Util/Logger';
	import { clientList } from '$lib/Util/Store';
	import { GlobalState } from '$lib/State/GlobalState';

	let modal: Modal;

	let messageText: string | null = $state(null);
	let messageIsError: boolean = $state(false);
	let pluginList: { [key: string] : PluginMeta } = $state(bolt.pluginConfig);

	const platformFileSep: string = bolt.platform === 'windows' ? '\\' : '/';
	const configFileName: string = 'bolt.json';
	const sepConfigFileName: string = platformFileSep.concat(configFileName);
	const defaultMainLuaFilename: string = 'main.lua';

	export function open() {
		showURLEntry = false;
		modal.open();
	}

	const close = () => {
		bolt.pluginConfig = pluginList;
		BoltService.savePluginConfig(true);
	}

	const setMessageInfo = (msg: string) => {
		console.log(msg);
		messageText = msg;
		messageIsError = false;
	};

	const setMessageError = (msg: string) => {
		console.error(msg);
		messageText = msg;
		messageIsError = true;
	};

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
		const meta = pluginList[id];
		if (!meta) return null;
		const path = meta.path;
		if (path) return getPluginConfigPromiseFromPath(path);
		return getPluginConfigPromiseFromDataDir(id);
	};

	const getNewPluginID = () => {
		const ids = Object.keys(pluginList);
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
				pluginList[selectedPlugin] = {
					name: plugin.name ?? unnamedPluginName,
					path: folderPath,
					version: plugin.version
				};
				GlobalState.pluginConfigHasPendingChanges = true;
			})
			.catch((reason) =>
				setMessageError(`config file '${configPath}' couldn't be fetched, reason: ${reason}`)
			);
	};
	const addPluginFromUpdaterURL = (url: string) => {
		setMessageInfo('downloading...');
		fetch(url)
			.then(async (x) => {
				if (!x.ok) {
					setMessageError(
						`can't install plugin: updater URL returned ${x.status}: ${x.statusText}`
					);
					return;
				}
				const config: PluginUpdaterConfig = await x.json();
				if (config.url) {
					const configUrlResponse = await fetch(config.url);
					if (!configUrlResponse.ok) {
						setMessageError(
							`can't install plugin: remote download URL returned ${x.status}: ${x.statusText}`
						);
						return;
					}
					const data = await configUrlResponse.arrayBuffer();
					if (config.sha256) {
						const hash = await crypto.subtle.digest('SHA-256', data);
						const hashStr = Array.from(new Uint8Array(hash))
							.map((x) => x.toString(16).padStart(2, '0'))
							.join('');
						if (config.sha256 !== hashStr) {
							setMessageError(`can't install plugin: incorrect file hash`);
							return;
						}
					}

					const id = getNewPluginID();
					const r = await fetch('/install-plugin?'.concat(new URLSearchParams({ id }).toString()), {
						method: 'POST',
						body: data
					});
					if (!r.ok) {
						setMessageError(`can't install plugin: ${x.statusText}`);
						return;
					}
					const plugin = await getPluginConfigPromiseFromDataDir(id);
					if (plugin) {
						selectedPlugin = id;
						pluginList[selectedPlugin] = {
							name: plugin.name ?? unnamedPluginName,
							version: plugin.version,
							updaterURL: url,
							sha256: config.sha256
						};
						GlobalState.pluginConfigHasPendingChanges = true;
						setMessageInfo(`plugin '${plugin.name}' installed`);
					} else {
						setMessageError(`can't install plugin: ${configFileName} not found`);
					}
				}
			})
			.catch(() => {
				setMessageError(`can't install plugin: unhandled exception`);
			});
	};

	// json file picker
	let disableButtons: boolean = $state(false);
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
						setMessageError(
							`selection '${xml.responseText}' is not named ${configFileName}; ignored`
						);
					}
				}
			}
		};
		xml.open('GET', '/json-file-picker', true);
		xml.send();
	};

	// get connected client list
	requestNewClientListPromise();

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
			if (!x.ok) {
				setMessageError(`can't update plugin: updater URL returned ${x.status}: ${x.statusText}`);
				return;
			}

			let config: PluginUpdaterConfig = await x.json();
			if (!config.url) {
				setMessageInfo(`can't update plugin '${meta.name}': no remote download URL is configured`);
				return;
			}

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

			if (downloadNeeded) {
				const r = await fetch(config.url);
				if (!r.ok) {
					setMessageError(
						`can't update plugin: remote download URL returned ${r.status}: ${r.statusText}`
					);
					return;
				}
				const data = await r.arrayBuffer();
				if (config.sha256) {
					const hash = await crypto.subtle.digest('SHA-256', data);
					const hashStr = Array.from(new Uint8Array(hash))
						.map((x) => x.toString(16).padStart(2, '0'))
						.join('');
					if (config.sha256 !== hashStr) {
						setMessageError(`can't update plugin '${meta.name}': incorrect file hash`);
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
					setMessageInfo(`plugin '${plugin.name}' updated`);
				} else {
					setMessageError(`can't update plugin '${meta.name}': ${configFileName} not found`);
				}
			} else {
				setMessageInfo(`plugin '${meta.name}' is already up-to-date`);
			}
		});
	};

	// plugin management interface - currently-selected plugin
	var selectedPlugin: string = $state('');

	const openAboutPlugins = () =>
		fetch('/open-external-url', { method: 'POST', body: 'https://bolt.adamcake.com/plugins' });

	// connected clients list
	var isClientSelected: boolean = $state(false);
	var selectedClientId: number = $state(0);

	let showURLEntry: boolean = $state(false);
	let textURLEntry: string = $state('');
	$effect(() => {
		if (!$clientList.some((x) => x.uid === selectedClientId)) {
			isClientSelected = false;
		}
	});
	let selectedPluginMeta = $derived(pluginList[selectedPlugin]);
	let managementPluginPromise = $derived(getPluginConfigPromiseFromID(selectedPlugin));
	$effect(() => {
		if (managementPluginPromise) {
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
	});
	let selectedPluginPath = $derived(selectedPluginMeta ? selectedPluginMeta.path : null);
</script>

<Modal
	bind:this={modal}
	class="h-[90%] w-[90%] text-center"
	onClose={() => close() }
>
	<div
		class="left-0 float-left h-full w-[min(180px,_50%)] overflow-hidden border-r-2 border-slate-300 pt-2 dark:border-slate-800"
	>
		<button
			class="mx-auto mb-2 w-[95%] select-none rounded-lg border-2 {isClientSelected
				? 'border-blue-500 text-black dark:text-white'
				: 'border-black bg-blue-500 text-black'} p-2 font-bold hover:opacity-75"
			onclick={() => (isClientSelected = false)}
		>
			Manage Plugins
		</button>
		<hr class="p-1 dark:border-slate-700" />
		{#if $clientList.length == 0}
			<p>(start an RS3 game client with plugins enabled and it will be listed here.)</p>
		{:else}
			{#each $clientList as client}
				<button
					onclick={() => {
						selectedClientId = client.uid;
						isClientSelected = true;
						showURLEntry = false;
						textURLEntry = '';
						messageText = null;
					}}
					class="m-1 h-[28px] w-[95%] select-none rounded-lg border-2 {isClientSelected &&
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
				onchange={() => (messageText = null)}
			>
				{#each Object.entries(pluginList) as [id, plugin]}
					<option class="dark:bg-slate-900" value={id}>{plugin.name ?? unnamedPluginName}</option>
				{/each}
			</select>
			{#if !isClientSelected}
				<span class="align-middle">
					<button
						class="mx-1 aspect-square w-9 select-none rounded-md bg-blue-500 p-1 text-[20px] font-bold duration-200 enabled:hover:opacity-75 disabled:bg-gray-500"
						onclick={() => {
							showURLEntry = !showURLEntry;
							textURLEntry = '';
						}}
						disabled={!bolt.hasLibArchive || disableButtons}
						title="Install plugin from updater URL"
					>
						<img src="svgs/download-solid.svg" alt="Install plugin from updater URL" />
					</button>
					<button
						class="aspect-square w-9 select-none rounded-md bg-blue-500 p-1 text-[20px] font-bold duration-200 enabled:hover:opacity-75 disabled:bg-gray-500"
						onclick={jsonFilePicker}
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
					></textarea>
					<button
						title="Confirm"
						onclick={() => {
							addPluginFromUpdaterURL(textURLEntry);
							showURLEntry = false;
							textURLEntry = '';
						}}
						class="select-none rounded-sm bg-emerald-500 hover:opacity-75"
						><img src="svgs/check-solid.svg" class="h-5 w-5" alt="Confirm" /></button
					>
					<button
						title="Close URL entry"
						onclick={() => {
							showURLEntry = false;
							textURLEntry = '';
						}}
						class="select-none rounded-sm bg-rose-500 hover:opacity-75"
						><img src="svgs/xmark-solid.svg" class="h-5 w-5" alt="Close" /></button
					>
					<br /><br />
				{/if}
				{#if Object.entries(pluginList).length !== 0}
					{#if Object.keys(pluginList).includes(selectedPlugin) && managementPluginPromise !== null}
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
							class="mx-auto mb-1 w-[min(144px,_25%)] select-none rounded-lg bg-blue-500 p-2 font-bold text-black duration-200 hover:opacity-75"
							onclick={() => {
								const path = pluginList[selectedPlugin].path;
								if (path) {
									fetch('/browse-directory?'.concat(new URLSearchParams({ path }).toString()));
								} else {
									fetch(
										'/browse-plugin-data?'.concat(
											new URLSearchParams({ id: selectedPlugin }).toString()
										)
									);
								}
							}}
						>
							Browse data
						</button>
						&nbsp;
						<button
							class="mx-auto mb-1 w-[min(144px,_25%)] select-none rounded-lg bg-blue-500 p-2 font-bold text-black duration-200 hover:opacity-75"
							onclick={() =>
								fetch(
									'/browse-plugin-config?'.concat(
										new URLSearchParams({ id: selectedPlugin }).toString()
									)
								)}
						>
							Browse config
						</button>
						<br />
						{#if selectedPluginMeta.updaterURL}
							<button
								class="m-1 mx-auto w-[min(144px,_25%)] select-none rounded-lg p-2 font-bold text-black duration-200 enabled:bg-blue-500 enabled:hover:opacity-75 disabled:bg-gray-500"
								onclick={() => updatePlugin(selectedPluginMeta, selectedPlugin)}
							>
								Check updates
							</button>
							&nbsp;
						{/if}
						<button
							class="m-1 mx-auto w-[min(144px,_25%)] select-none rounded-lg p-2 font-bold text-black duration-200 enabled:bg-rose-500 enabled:hover:opacity-75 disabled:bg-gray-500"
							onclick={() => {
								managementPluginPromise = null;
								GlobalState.pluginConfigHasPendingChanges = true;
								const meta = pluginList[selectedPlugin];
								if (meta) {
									fetch(
										'/uninstall-plugin?'.concat(
											new URLSearchParams({
												id: selectedPlugin,
												delete_data_dir: typeof meta.path === 'string' ? '0' : '1'
											}).toString()
										)
									);
									setMessageInfo(`plugin '${meta.name}' uninstalled`);
									delete pluginList[selectedPlugin];
								}
							}}
						>
							Remove
						</button>
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
					{#if plugin && plugin.main && Object.keys(pluginList).includes(selectedPlugin)}
						<button
							class="mx-auto mb-1 w-auto select-none rounded-lg bg-emerald-500 p-2 font-bold text-black duration-200 hover:opacity-75"
							onclick={() =>
								startPlugin(
									selectedClientId,
									selectedPlugin,
									selectedPluginPath ?? null,
									plugin.main ?? defaultMainLuaFilename
								)}
						>
							Start {plugin.name}
						</button>
					{:else if Object.entries(pluginList).length === 0}
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
								{#if Object.keys(pluginList).includes(activePlugin.id)}
									<p>
										{pluginList[activePlugin.id].name ?? activePlugin.id}
										<button
											class="select-none rounded-sm bg-rose-500 shadow-lg hover:opacity-75"
											onclick={() => {
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
		{#if messageText}
			<br /><br />
			{#if messageIsError}
				<p class="text-red-500">[error] {messageText}</p>
			{:else}
				<p>[info] {messageText}</p>
			{/if}
		{/if}
	</div>
	<div class="absolute bottom-2 right-4">
		<button
			class="m-0 cursor-pointer select-none border-none bg-transparent p-0 text-sm text-gray-500 underline"
			onclick={openAboutPlugins}>about plugins</button
		>
	</div>
</Modal>
