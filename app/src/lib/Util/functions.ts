import { get } from 'svelte/store';
import { type GameClient, type Direct6Token } from '$lib/Util/interfaces';
import { internalUrl } from '$lib/Util/store';
import { logger } from '$lib/Util/Logger';
import { BoltService } from '$lib/Services/BoltService';
import { bolt } from '$lib/State/Bolt';
import { GlobalState } from '$lib/State/GlobalState';

// asynchronously download and launch RS3's official .deb client using the given env variables
export function launchRS3Linux(
	jx_session_id: string,
	jx_character_id: string,
	jx_display_name: string
) {
	BoltService.saveConfig();

	const launch = (hash?: string, deb?: never) => {
		const xml = new XMLHttpRequest();
		const params: Record<string, string> = {};
		const config = get(GlobalState.config);
		if (hash) params.hash = hash;
		if (jx_session_id) params.jx_session_id = jx_session_id;
		if (jx_character_id) params.jx_character_id = jx_character_id;
		if (jx_display_name) params.jx_display_name = jx_display_name;
		if (config.rs_plugin_loader) params.plugin_loader = '1';
		if (config.use_custom_rs_config_uri && config.rs_config_uri) {
			params.config_uri = config.rs_config_uri;
		} else {
			params.config_uri = bolt.env.default_config_uri;
		}
		xml.open('POST', '/launch-rs3-deb?'.concat(new URLSearchParams(params).toString()), true);
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				logger.info(`Game launch status: '${xml.responseText.trim()}'`);
				if (xml.status == 200 && hash) {
					bolt.rs3DebInstalledHash = hash;
				}
			}
		};
		xml.send(deb);
	};

	const xml = new XMLHttpRequest();
	const contentUrl = bolt.env.content_url;
	const url = contentUrl.concat('dists/trusty/non-free/binary-amd64/Packages');
	xml.open('GET', url, true);
	xml.onreadystatechange = () => {
		if (xml.readyState == 4 && xml.status == 200) {
			const lines = Object.fromEntries(
				xml.response.split('\n').map((x: string) => {
					return x.split(': ');
				})
			);
			if (!lines.Filename || !lines.Size) {
				logger.error(`Could not parse package data from URL: ${url}`);
				launch();
				return;
			}
			if (lines.SHA256 !== bolt.rs3DebInstalledHash) {
				logger.info('Downloading RS3 client...');
				const exeXml = new XMLHttpRequest();
				exeXml.open('GET', contentUrl.concat(lines.Filename), true);
				exeXml.responseType = 'arraybuffer';
				exeXml.onprogress = (e) => {
					if (e.loaded) {
						const progress = (Math.round((1000.0 * e.loaded) / e.total) / 10.0).toFixed(1);
						logger.info(`Downloading RS3 client... ${progress}%`);
					}
				};
				exeXml.onreadystatechange = () => {
					if (exeXml.readyState == 4 && exeXml.status == 200) {
						launch(lines.SHA256, exeXml.response);
					}
				};
				exeXml.onerror = () => {
					logger.error(`Error downloading game client: from ${url}: non-http error`);
					launch();
				};
				exeXml.send();
			} else {
				logger.info('Latest client is already installed');
				launch();
			}
		}
	};
	xml.onerror = () => {
		logger.error(`Error: from ${url}: non-http error`);
		launch();
	};
	xml.send();
}

// locate runelite's .jar either from the user's config or by parsing github releases,
// then attempt to launch it with the given env variables
// last param indices whether --configure will be passed or not
export function launchRuneLite(
	jx_session_id: string,
	jx_character_id: string,
	jx_display_name: string,
	configure: boolean
) {
	BoltService.saveConfig();
	const launchPath = configure ? '/launch-runelite-jar-configure?' : '/launch-runelite-jar?';

	const launch = (id?: string | null, jar?: unknown, jarPath?: unknown) => {
		const xml = new XMLHttpRequest();
		const params: Record<string, string> = {};
		if (id) params.id = <string>id;
		if (jarPath) params.jar_path = <string>jarPath;
		if (jx_session_id) params.jx_session_id = jx_session_id;
		if (jx_character_id) params.jx_character_id = jx_character_id;
		if (jx_display_name) params.jx_display_name = jx_display_name;
		if (get(GlobalState.config).flatpak_rich_presence) params.flatpak_rich_presence = '';
		xml.open(jar ? 'POST' : 'GET', launchPath.concat(new URLSearchParams(params).toString()), true);
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				logger.info(`Game launch status: '${xml.responseText.trim()}'`);
				if (xml.status == 200 && id) {
					bolt.runeLiteInstalledId = id;
				}
			}
		};
		xml.send(<string>jar);
	};

	if (get(GlobalState.config).runelite_use_custom_jar) {
		launch(null, null, get(GlobalState.config).runelite_custom_jar);
		return;
	}

	const xml = new XMLHttpRequest();
	const url = 'https://api.github.com/repos/runelite/launcher/releases';
	xml.open('GET', url, true);
	xml.onreadystatechange = () => {
		if (xml.readyState == 4) {
			if (xml.status == 200) {
				const runelite = JSON.parse(xml.responseText)
					.map((x: Record<string, string>) => x.assets)
					.flat()
					.find((x: Record<string, string>) => x.name.toLowerCase() == 'runelite.jar');
				if (runelite.id != bolt.runeLiteInstalledId) {
					logger.info('Downloading RuneLite...');
					const xmlRl = new XMLHttpRequest();
					xmlRl.open('GET', runelite.browser_download_url, true);
					xmlRl.responseType = 'arraybuffer';
					xmlRl.onreadystatechange = () => {
						if (xmlRl.readyState == 4) {
							if (xmlRl.status == 200) {
								launch(runelite.id, xmlRl.response);
							} else {
								logger.error(
									`Error downloading from ${runelite.url}: ${xmlRl.status}: ${xmlRl.responseText}`
								);
							}
						}
					};
					xmlRl.onprogress = (e) => {
						if (e.loaded && e.lengthComputable) {
							const progress = (Math.round((1000.0 * e.loaded) / e.total) / 10.0).toFixed(1);
							logger.info(`Downloading RuneLite... ${progress}`);
						}
					};
					xmlRl.send();
				} else {
					logger.info('Latest JAR is already installed');
					launch();
				}
			} else {
				logger.error(`Error from ${url}: ${xml.status}: ${xml.responseText}`);
			}
		}
	};
	xml.send();
}

// locate hdos's .jar from their CDN, then attempt to launch it with the given env variables
export function launchHdos(
	jx_session_id: string,
	jx_character_id: string,
	jx_display_name: string
) {
	BoltService.saveConfig();

	const launch = (version?: string, jar?: string) => {
		const xml = new XMLHttpRequest();
		const params: Record<string, string> = {};
		if (version) params.version = version;
		if (jx_session_id) params.jx_session_id = jx_session_id;
		if (jx_character_id) params.jx_character_id = jx_character_id;
		if (jx_display_name) params.jx_display_name = jx_display_name;
		xml.open('POST', '/launch-hdos-jar?'.concat(new URLSearchParams(params).toString()), true);
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				logger.info(`Game launch status: '${xml.responseText.trim()}'`);
				if (xml.status == 200 && version) {
					bolt.hdosInstalledVersion = version;
				}
			}
		};
		xml.send(jar);
	};

	const xml = new XMLHttpRequest();
	const url = 'https://cdn.hdos.dev/client/getdown.txt';
	xml.open('GET', url, true);
	xml.onreadystatechange = () => {
		if (xml.readyState == 4) {
			if (xml.status == 200) {
				const versionRegex: RegExpMatchArray | null = xml.responseText.match(
					/^launcher\.version *= *(.*?)$/m
				);
				if (versionRegex && versionRegex.length >= 2) {
					const latestVersion = versionRegex[1];
					if (latestVersion !== bolt.hdosInstalledVersion) {
						const jarUrl = `https://cdn.hdos.dev/launcher/v${latestVersion}/hdos-launcher.jar`;
						logger.info('Downloading HDOS...');
						const xmlHdos = new XMLHttpRequest();
						xmlHdos.open('GET', jarUrl, true);
						xmlHdos.responseType = 'arraybuffer';
						xmlHdos.onreadystatechange = () => {
							if (xmlHdos.readyState == 4) {
								if (xmlHdos.status == 200) {
									launch(latestVersion, xmlHdos.response);
								} else {
									const runelite = JSON.parse(xml.responseText)
										.map((x: Record<string, string>) => x.assets)
										.flat()
										.find((x: Record<string, string>) => x.name.toLowerCase() == 'runelite.jar');
									logger.error(
										`Error downloading from ${runelite.url}: ${xmlHdos.status}: ${xmlHdos.responseText}`
									);
								}
							}
						};
						xmlHdos.onprogress = (e) => {
							if (e.loaded && e.lengthComputable) {
								const progress = (Math.round((1000.0 * e.loaded) / e.total) / 10.0).toFixed(1);
								logger.info(`Downloading HDOS... ${progress}%`);
							}
						};
						xmlHdos.send();
					} else {
						logger.info('Latest JAR is already installed');
						launch();
					}
				} else {
					logger.info("Couldn't parse latest launcher version");
					launch();
				}
			} else {
				logger.error(`Error from ${url}: ${xml.status}: ${xml.responseText}`);
			}
		}
	};
	xml.send();
}

export function getNewClientListPromise(): Promise<GameClient[]> {
	return new Promise((resolve, reject) => {
		const xml = new XMLHttpRequest();
		const url = get(internalUrl).concat('/list-game-clients');
		xml.open('GET', url, true);
		xml.onreadystatechange = () => {
			if (xml.readyState == 4) {
				if (xml.status == 200 && xml.getResponseHeader('content-type') === 'application/json') {
					const dict = JSON.parse(xml.responseText);
					resolve(
						Object.keys(dict).map(
							(uid) => <GameClient>{ uid, identity: dict[uid].identity || null }
						)
					);
				} else {
					reject(`error (${xml.responseText})`);
				}
			}
		};
		xml.send();
	});
}

export function savePluginConfig(): void {
	const xml = new XMLHttpRequest();
	xml.open('POST', '/save-plugin-config', true);
	xml.setRequestHeader('Content-Type', 'application/json');
	xml.onreadystatechange = () => {
		if (xml.readyState == 4) {
			logger.info(`Save-plugin-config status: ${xml.responseText.trim()}`);
		}
	};
	xml.send(JSON.stringify(bolt.pluginList));
}

// download and attempt to launch an official windows or mac client
// (not the official linux client - that comes from a different url, see launchRS3Linux)
export async function launchOfficialClient(
	windows: boolean,
	osrs: boolean,
	jx_session_id: string,
	jx_character_id: string,
	jx_display_name: string
) {
	BoltService.saveConfig();
	const metaPath: string = `${osrs ? 'osrs' : bolt.env.provider}-${windows ? 'win' : 'mac'}`;
	let installedHash = windows
		? osrs
			? bolt.osrsExeInstalledHash
			: bolt.rs3ExeInstalledHash
		: osrs
			? bolt.osrsAppInstalledHash
			: bolt.rs3AppInstalledHash;

	const launch = async (hash?: string, exe?: Promise<ArrayBuffer>) => {
		const params: Record<string, string> = {};
		if (hash) params.hash = hash;
		if (jx_session_id) params.jx_session_id = jx_session_id;
		if (jx_character_id) params.jx_character_id = jx_character_id;
		if (jx_display_name) params.jx_display_name = jx_display_name;
		const response = await fetch(
			`/launch-${osrs ? 'osrs' : 'rs3'}-${windows ? 'exe' : 'app'}?${new URLSearchParams(params).toString()}`,
			{
				method: 'POST',
				headers: { 'Content-Type': 'application/octet-stream' },
				body: await exe
			}
		);
		response.text().then((text) => logger.info(`Game launch status: '${text.trim()}'`));
		if (response.status == 200 && hash) {
			installedHash = hash;
		}
	};

	// download a list of all the environments, find the "production" environment
	const primaryUrl: string = `${bolt.env.direct6_url}${metaPath}/${metaPath}.json`;
	const primaryUrlResponse = await fetch(primaryUrl, { method: 'GET' });
	const primaryUrlText = await primaryUrlResponse.text();
	if (primaryUrlResponse.status !== 200) {
		logger.error(`Error from ${primaryUrl}: ${primaryUrlResponse.status}: ${primaryUrlText}`);
		return;
	}
	const metaToken: Direct6Token = JSON.parse(atob(primaryUrlText.split('.')[1])).environments
		.production;

	if (installedHash === metaToken.id) {
		logger.info('Latest client is already installed');
		launch();
		return;
	}
	logger.info(`Downloading client version ${metaToken.version}`);

	// download the catalog for the production environment, which contains info about all the files available for download
	const catalogUrl: string = `${bolt.env.direct6_url}${metaPath}/catalog/${metaToken.id}/catalog.json`;
	const catalogUrlResponse = await fetch(catalogUrl, { method: 'GET' });
	const catalogText = await catalogUrlResponse.text();
	if (catalogUrlResponse.status !== 200) {
		logger.error(`Error from ${catalogUrl}: ${catalogUrlResponse.status}: ${catalogText}`);
		return;
	}
	const catalog = JSON.parse(atob(catalogText.split('.')[1]));

	// download the metafile that's linked in the catalog, used to download the actual files
	const metafileUrl = catalog.metafile.replace(/^http:/i, 'https:');
	const metafileUrlResponse = await fetch(metafileUrl, { method: 'GET' });
	const metafileText = await metafileUrlResponse.text();
	if (metafileUrlResponse.status !== 200) {
		logger.error(`Error from ${metafileUrl}: ${metafileUrlResponse.status}: ${metafileText}`);
		return;
	}
	const metafile = JSON.parse(atob(metafileText.split('.')[1]));

	// download each available gzip blob, decompress them, and keep them in the correct order
	const chunk_promises: Promise<Blob>[] = metafile.pieces.digests.map((x: string) => {
		const hex_chunk: string = atob(x)
			.split('')
			.map((c) => c.charCodeAt(0).toString(16).padStart(2, '0'))
			.join('');
		const chunk_url: string = catalog.config.remote.baseUrl
			.replace(/^http:/i, 'https:')
			.concat(
				catalog.config.remote.pieceFormat
					.replace('{SubString:0,2,{TargetDigest}}', hex_chunk.substring(0, 2))
					.replace('{TargetDigest}', hex_chunk)
			);
		return fetch(chunk_url, { method: 'GET' }).then((x: Response) =>
			x.blob().then((blob) => {
				const ds = new DecompressionStream('gzip');
				return new Response(blob.slice(6).stream().pipeThrough(ds)).blob();
			})
		);
	});

	// while all that stuff is downloading, read the file list and find the exe's location in the data blob
	let exeOffset: number = 0;
	let exeSize: number | null = null;
	for (let i = 0; i < metafile.files.length; i += 1) {
		const isTargetExe: boolean = windows
			? metafile.files[i].name.endsWith('.exe')
			: metafile.files[i].name.includes('.app/Contents/MacOS/');
		if (isTargetExe) {
			if (exeSize !== null) {
				logger.error(
					`Error parsing ${metafileUrl}: file list has multiple possibilities for main exe`
				);
				return;
			} else {
				exeSize = metafile.files[i].size;
			}
		} else if (exeSize === null) {
			exeOffset += metafile.files[i].size;
		}
	}
	if (exeSize === null) {
		logger.error(`Error parsing ${metafileUrl}: file list has no possibilities for main exe`);
		return;
	}

	// stitch all the data together, slice the exe out of it, and launch the game
	Promise.all(chunk_promises).then((x) => {
		const exeFile = new Blob(x).slice(exeOffset, exeOffset + <number>exeSize);
		launch(metafile.id, exeFile.arrayBuffer());
	});
}
