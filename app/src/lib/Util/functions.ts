import { BoltService } from '$lib/Services/BoltService';
import { bolt } from '$lib/State/Bolt';
import { GlobalState } from '$lib/State/GlobalState';
import { type Direct6Token } from '$lib/Util/interfaces';
import { logger } from '$lib/Util/Logger';
import { get } from 'svelte/store';

const requestInitGet: RequestInit = { method: 'GET' };
const requestHeadersUpload: HeadersInit = { 'Content-Type': 'application/octet-stream' };

let runeliteLastUpdateCheck: number | null = null;
const runeLiteCooldownMillis = 2 * 60 * 1000;
const runeLiteReleasesURL = 'https://api.github.com/repos/runelite/launcher/releases';
const hdosGetdownURL = 'https://cdn.hdos.dev/client/getdown.txt';

// returns the RequestInit that should be passed to `fetch` for the given request body
// the request body is optional: method will be GET if no request body is given, POST otherwise
const createInitFromUpload = (body?: Uint8Array | ArrayBuffer | null): RequestInit => {
	if (!body) return requestInitGet;
	return { method: 'POST', headers: requestHeadersUpload, body };
};

// fetches the URL, and if the response is 2xx then calls the response handler
// otherwise, logs an error, calls on error, and returns
const tryFetch = (
	url: string,
	init: RequestInit,
	onresponse: (x: Response) => void,
	onerror: () => void
) => {
	fetch(url, init)
		.then((response) => {
			if (response.ok) {
				onresponse(response);
			} else {
				response
					.text()
					.then((x) => logger.error(`request failed: ${response.url}: ${response.status}: ${x}`));
				onerror();
			}
		})
		.catch(() => {
			logger.error(`request failed: ${url}: connection error`);
			onerror();
		});
};

// from a fetch Response object, logs a message and updates it about download progress
// until the download is complete, then calls the handler with args.
// in the event of an error, logs the error and calls the handler with no args.
// this is designed for the handler to be the "launch" function found in most functions
// in this file.
const downloadWithProgress = async (
	response: Response,
	logStr: string,
	handler: (c: Uint8Array) => void,
	onerror: () => void,
	sizeHint?: string
) => {
	const reportError = (e: string) => {
		logger.error(`error downloading: ${e}`);
		onerror();
	};

	logger.info(logStr);
	const updateLog = (suffix: string) => logger.updateLogAtIndex(0, `${logStr} ${suffix}`);

	if (!response.body) {
		return reportError('no HTTP response body');
	}
	const reader = response.body.getReader();
	if (!reader) {
		return reportError('invalid HTTP response');
	}

	const downloadSizeStr = response.headers.get('content-length') ?? sizeHint;
	if (!downloadSizeStr) {
		return reportError('cannot estimate download size');
	}
	const downloadSize = parseInt(downloadSizeStr);
	if (!downloadSizeStr) {
		return reportError('cannot estimate download size');
	}

	const content = new Uint8Array(downloadSize);
	let received = 0;
	while (true) {
		const readable = await reader.read();
		if (readable.done) break;
		const val = readable.value;
		content.set(val, received);
		received += val.length;
		const progress = (Math.round((1000.0 * received) / downloadSize) / 10.0).toFixed(1);
		updateLog(progress);
	}
	updateLog('done');
	handler(content);
};

// asynchronously download and launch RS3's official .deb client using the given env variables
export function launchRS3Linux(
	jx_session_id: string,
	jx_character_id: string,
	jx_display_name: string
) {
	BoltService.saveConfig();

	const launch = (hash?: string, deb?: Uint8Array) => {
		const params: Record<string, string> = {};
		const config = get(GlobalState.config);
		if (hash) params.hash = hash;
		if (jx_session_id) params.jx_session_id = jx_session_id;
		if (jx_character_id) params.jx_character_id = jx_character_id;
		if (jx_display_name) params.jx_display_name = jx_display_name;
		if (config.rs_launch_command && config.rs_launch_command.length > 0) {
			params.launch_command = config.rs_launch_command;
		}
		if (config.rs_plugin_loader) params.plugin_loader = '1';
		if (config.use_custom_rs_config_uri && config.rs_config_uri) {
			params.config_uri = config.rs_config_uri;
		} else {
			params.config_uri = bolt.env.default_config_uri;
		}

		fetch(
			'/launch-rs3-deb?'.concat(new URLSearchParams(params).toString()),
			createInitFromUpload(deb)
		).then((response) => {
			response.text().then((x) => logger.info(`game launch status: '${x.trim()}'`));
			if (response.ok) {
				if (hash) bolt.rs3debInstalledHash = hash;
				if (config.close_after_launch && !config.rs_plugin_loader) fetch('/close');
			}
		});
	};
	const tryLaunchExisting = () => {
		if (bolt.rs3debInstalledHash) launch();
	};

	const contentUrl = bolt.env.content_url;
	const url = contentUrl.concat('dists/trusty/non-free/binary-amd64/Packages');
	tryFetch(
		url,
		requestInitGet,
		(response) =>
			response.text().then((text) => {
				const lines = Object.fromEntries(text.split('\n').map((x: string) => x.split(': ')));
				if (!lines.Filename || !lines.Size || !lines.SHA256) {
					logger.error(`could not parse package data from URL: ${url}`);
					tryLaunchExisting();
					return;
				}

				if (lines.SHA256 === bolt.rs3debInstalledHash) {
					logger.info('game client is up-to-date');
					launch();
					return;
				}

				tryFetch(
					contentUrl.concat(lines.Filename),
					requestInitGet,
					(response) =>
						downloadWithProgress(
							response,
							'downloading RS3 client...',
							(c) => launch(lines.SHA256, c),
							launch,
							lines.Size
						),
					tryLaunchExisting
				);
			}),
		tryLaunchExisting
	);
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
	const config = get(GlobalState.config);

	const launch = (id?: string | null, jar?: Uint8Array | null, jarPath?: string | null) => {
		const params: Record<string, string> = {};
		if (id) params.id = id;
		if (jarPath) params.jar_path = jarPath;
		if (jx_session_id) params.jx_session_id = jx_session_id;
		if (jx_character_id) params.jx_character_id = jx_character_id;
		if (jx_display_name) params.jx_display_name = jx_display_name;
		if (config.runelite_launch_command && config.runelite_launch_command !== '') {
			params.launch_command = config.runelite_launch_command;
		}
		if (config.flatpak_rich_presence) params.flatpak_rich_presence = '';

		fetch(
			launchPath.concat(new URLSearchParams(params).toString()),
			createInitFromUpload(jar)
		).then((response) => {
			response.text().then((x) => logger.info(`game launch status: '${x.trim()}'`));
			if (response.ok) {
				if (id) bolt.runeLiteInstalledId = id;
				if (config.close_after_launch && !configure) fetch('/close');
			}
		});
	};
	const tryLaunchExisting = () => {
		if (bolt.runeLiteInstalledId) launch();
	};

	if (config.runelite_use_custom_jar) {
		launch(null, null, config.runelite_custom_jar);
		return;
	}

	if (
		bolt.runeLiteInstalledId !== null &&
		runeliteLastUpdateCheck !== null &&
		runeliteLastUpdateCheck + runeLiteCooldownMillis > Date.now()
	) {
		launch();
		return;
	}

	tryFetch(
		runeLiteReleasesURL,
		requestInitGet,
		(x: Response) =>
			x.json().then((response) => {
				runeliteLastUpdateCheck = Date.now();
				const runelite = response
					.map((x: Record<string, string>) => x.assets)
					.flat()
					.find((x: Record<string, string>) => x.name.toLowerCase() == 'runelite.jar');
				if (!runelite) {
					logger.info(`note: no runelite releases found, so skipping update check`);
					tryLaunchExisting();
					return;
				}

				// note: runelite.id is a number, runeLiteInstalledId is a string
				if (runelite.id == bolt.runeLiteInstalledId) {
					logger.info('game is up-to-date');
					launch();
					return;
				}

				tryFetch(
					runelite.browser_download_url,
					requestInitGet,
					(response) =>
						downloadWithProgress(
							response,
							'downloading RuneLite launcher...',
							(x) => launch(runelite.id, x),
							tryLaunchExisting,
							runelite.size
						),
					tryLaunchExisting
				);
			}),
		tryLaunchExisting
	);
}

// locate hdos's .jar from their CDN, then attempt to launch it with the given env variables
export function launchHdos(
	jx_session_id: string,
	jx_character_id: string,
	jx_display_name: string
) {
	BoltService.saveConfig();
	const config = get(GlobalState.config);

	const launch = (version?: string, jar?: Uint8Array) => {
		const params: Record<string, string> = {};
		if (version) params.version = version;
		if (jx_session_id) params.jx_session_id = jx_session_id;
		if (jx_character_id) params.jx_character_id = jx_character_id;
		if (jx_display_name) params.jx_display_name = jx_display_name;
		if (config.hdos_launch_command && config.hdos_launch_command.length > 0) {
			params.launch_command = config.hdos_launch_command;
		}

		fetch(
			'/launch-hdos-jar?'.concat(new URLSearchParams(params).toString()),
			createInitFromUpload(jar)
		).then((response) => {
			response.text().then((x) => logger.info(`game launch status: '${x.trim()}'`));
			if (response.ok) {
				if (version) bolt.hdosInstalledVersion = version;
				if (config.close_after_launch) fetch('/close');
			}
		});
	};
	const tryLaunchExisting = () => {
		if (bolt.hdosInstalledVersion) launch();
	};

	tryFetch(
		hdosGetdownURL,
		requestInitGet,
		(x) =>
			x.text().then((response) => {
				const versionRegex: RegExpMatchArray | null = response.match(
					/^launcher\.version *= *(.*?)$/m
				);
				if (!versionRegex || versionRegex.length < 2) {
					logger.error("couldn't parse latest launcher version");
					tryLaunchExisting();
					return;
				}

				const latestVersion = versionRegex[1];
				if (latestVersion === bolt.hdosInstalledVersion) {
					logger.info('game is up-to-date');
					launch();
					return;
				}

				tryFetch(
					`https://cdn.hdos.dev/launcher/v${latestVersion}/hdos-launcher.jar`,
					requestInitGet,
					(response) =>
						downloadWithProgress(
							response,
							'downloading HDOS launcher...',
							(x) => launch(latestVersion, x),
							tryLaunchExisting
						),
					tryLaunchExisting
				);
			}),
		tryLaunchExisting
	);
}

export function requestNewClientListPromise(): void {
	fetch('/list-game-clients', requestInitGet).then((response) => {
		if (!response.ok) {
			response
				.text()
				.then((x) => logger.error(`failed to get client list: ${response.status}: ${x}`));
		}
	});
}

// download and attempt to launch an official windows or mac client
// (not the official linux client - that comes from a different url, see launchRS3Linux)
export function launchOfficialClient(
	windows: boolean,
	osrs: boolean,
	jx_session_id: string,
	jx_character_id: string,
	jx_display_name: string
) {
	BoltService.saveConfig();
	const metaPath: string = `${osrs ? 'osrs' : bolt.env.provider}-${windows ? 'win' : 'mac'}`;
	const metaGameName: 'osrs' | 'rs3' = osrs ? 'osrs' : 'rs3';
	const metaFileType: 'exe' | 'app' = windows ? 'exe' : 'app';
	const installedHashName:
		| 'osrsexeInstalledHash'
		| 'rs3exeInstalledHash'
		| 'osrsappInstalledHash'
		| 'rs3appInstalledHash' = `${metaGameName}${metaFileType}InstalledHash`;

	const launch = (hash?: string, exe?: Promise<ArrayBuffer>) => {
		const params: Record<string, string> = {};
		const config = get(GlobalState.config);
		const launchCommand = osrs ? config.osrs_launch_command : config.rs_launch_command;
		if (hash) params.hash = hash;
		if (jx_session_id) params.jx_session_id = jx_session_id;
		if (jx_character_id) params.jx_character_id = jx_character_id;
		if (jx_display_name) params.jx_display_name = jx_display_name;
		if (launchCommand && launchCommand !== '') params.launch_command = launchCommand;
		if (!osrs) {
			if (config.rs_plugin_loader) params.plugin_loader = '1';
			params.config_uri =
				config.use_custom_rs_config_uri && config.rs_config_uri
					? config.rs_config_uri
					: bolt.env.default_config_uri;
		}

		const _launch = (init: RequestInit) =>
			fetch(
				`/launch-${metaGameName}-${metaFileType}?${new URLSearchParams(params).toString()}`,
				init
			).then((response) => {
				response.text().then((x) => logger.info(`game launch status: '${x.trim()}'`));
				if (response.ok) {
					if (hash) bolt[installedHashName] = hash;
					if (config.close_after_launch && (osrs || !config.rs_plugin_loader)) fetch('/close');
				}
			});
		if (exe) {
			exe.then((x) => _launch(createInitFromUpload(x)));
		} else {
			_launch(requestInitGet);
		}
	};
	const tryLaunchExisting = () => {
		if (bolt.hdosInstalledVersion) launch();
	};

	// download a list of all the environments, find the "production" environment
	tryFetch(
		`${bolt.env.direct6_url}${metaPath}/${metaPath}.json`,
		requestInitGet,
		(x) =>
			x.text().then((response) => {
				const metaToken: Direct6Token = JSON.parse(atob(response.split('.')[1])).environments
					.production;

				if (bolt[installedHashName] === metaToken.id) {
					logger.info('game client is up-to-date');
					launch();
					return;
				}
				logger.info(`downloading client version ${metaToken.version}...`);

				// download the catalog for the production environment, which contains info about all the files available for download
				tryFetch(
					`${bolt.env.direct6_url}${metaPath}/catalog/${metaToken.id}/catalog.json`,
					requestInitGet,
					(x) =>
						x.text().then((response) => {
							const catalog = JSON.parse(atob(response.split('.')[1]));

							// change http -> https, and also redirect a specific domain that has no certificate to one that does
							const fixUrl = (url: string): string => {
								return url
									.replace(
										/^http:\/\/(.{5})-akamai\.aws\.snxd\.com\//i,
										'https://$1.akamaized.net/'
									)
									.replace(/^http:/i, 'https:');
							};

							// download the metafile that's linked in the catalog, used to download the actual files
							tryFetch(
								fixUrl(catalog.metafile),
								requestInitGet,
								(x) =>
									x.text().then((response) => {
										const metafile = JSON.parse(atob(response.split('.')[1]));
										// download each available gzip blob, decompress them, and keep them in the correct order
										const chunk_promises: Promise<Blob>[] = metafile.pieces.digests.map(
											(x: string) => {
												const hex_chunk: string = atob(x)
													.split('')
													.map((c) => c.charCodeAt(0).toString(16).padStart(2, '0'))
													.join('');
												const chunk_url: string = fixUrl(catalog.config.remote.baseUrl).concat(
													catalog.config.remote.pieceFormat
														.replace('{SubString:0,2,{TargetDigest}}', hex_chunk.substring(0, 2))
														.replace('{TargetDigest}', hex_chunk)
												);
												return fetch(chunk_url, requestInitGet).then((x: Response) =>
													x.blob().then((blob) => {
														const ds = new DecompressionStream('gzip');
														return new Response(blob.slice(6).stream().pipeThrough(ds)).blob();
													})
												);
											}
										);

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
														`can't parse ${x.url}: file list has multiple possibilities for main exe`
													);
													tryLaunchExisting();
													return;
												} else {
													exeSize = metafile.files[i].size;
												}
											} else if (exeSize === null) {
												exeOffset += metafile.files[i].size;
											}
										}
										if (exeSize === null) {
											logger.error(
												`can't parse ${x.url}: file list has no possibilities for main exe`
											);
											tryLaunchExisting();
											return;
										}

										// stitch all the data together, slice the exe out of it, and launch the game
										Promise.all(chunk_promises).then((x) => {
											const exeFile = new Blob(x).slice(exeOffset, exeOffset + exeSize);
											launch(metafile.id, exeFile.arrayBuffer());
										});
									}),
								tryLaunchExisting
							);
						}),
					tryLaunchExisting
				);
			}),
		tryLaunchExisting
	);
}
