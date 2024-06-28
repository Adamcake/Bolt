import type { Session } from '$lib/Services/AuthService';
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
	default_config_uri: string;
	games: string[];
}

export interface Bolt {
	env: BoltEnv;
	platform: Platform | null;
	rs3InstalledHash: string | null;
	runeLiteInstalledId: string | null;
	hdosInstalledVersion: string | null;
	isFlathub: boolean;
	hasBoltPlugins: boolean;
	pluginList: { [key: string]: PluginMeta }; // May need to be a writable
	sessions: Session[];
}

declare const s: () => BoltEnv;

export const bolt: Bolt = {
	env: ParseUtils.decodeBolt(s()),
	platform: null,
	rs3InstalledHash: null,
	runeLiteInstalledId: null,
	hdosInstalledVersion: null,
	isFlathub: false,
	hasBoltPlugins: false,
	pluginList: {},
	sessions: []
};
