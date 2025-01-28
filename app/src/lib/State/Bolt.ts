import { ParseUtils } from '$lib/Util/ParseUtils';
import type { PluginMeta } from '$lib/Util/interfaces';

export const enum Platform {
	Windows = 'windows',
	Linux = 'linux',
	MacOS = 'mac'
}

export interface BoltEnv {
	provider: string;
	origin: string;
	origin_2fa: string;
	redirect: string;
	clientid: string;
	api: string;
	auth_api: string;
	profile_api: string;
	shield_url: string;
	content_url: string;
	direct6_url: string;
	default_config_uri: string;
	psa_url: string;
	games: string[];
}

export interface Bolt {
	env: BoltEnv;
	platform: Platform | null;
	// TODO: simplify installed hash system (1 variable instead of multiple?)
	rs3DebInstalledHash: string | null;
	rs3ExeInstalledHash: string | null;
	rs3AppInstalledHash: string | null;
	osrsExeInstalledHash: string | null;
	osrsAppInstalledHash: string | null;
	runeLiteInstalledId: string | null;
	hdosInstalledVersion: string | null;
	isFlathub: boolean;
	hasLibArchive: boolean;
	hasBoltPlugins: boolean;
	pluginConfig: { [key: string]: PluginMeta }; // I think this should be in GlobalState?
}

declare const s: () => BoltEnv;

export const bolt: Bolt = {
	env: ParseUtils.decodeBolt(s()),
	platform: null,
	rs3DebInstalledHash: null,
	rs3ExeInstalledHash: null,
	rs3AppInstalledHash: null,
	osrsExeInstalledHash: null,
	osrsAppInstalledHash: null,
	runeLiteInstalledId: null,
	hdosInstalledVersion: null,
	isFlathub: false,
	hasLibArchive: false,
	hasBoltPlugins: false,
	pluginConfig: {}
};
