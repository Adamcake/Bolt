import { GlobalState } from '$lib/State/GlobalState';
import { Client, Game } from '$lib/Util/interfaces';
import { logger } from '$lib/Util/Logger';
import { onWritableChange } from '$lib/Util/onWritableChange';

type UserDetails = {
	account_id: string;
};

export interface Config {
	use_dark_theme: boolean;
	rs_plugin_loader: boolean;
	close_after_launch: boolean;
	check_announcements: boolean;
	flatpak_rich_presence: boolean;
	runelite_use_custom_jar: boolean;
	use_custom_rs_config_uri: boolean;
	rs_config_uri?: string;
	rs_launch_command: string | null;
	osrs_launch_command: string | null;
	runelite_custom_jar: string | null;
	runelite_launch_command: string | null;
	hdos_launch_command: string | null;
	selected: {
		game: Game;
		client: Client;
		user_id: string | null;
	};
	// Each user has data associated with it that is recalled on sign-in or when switching between them
	userDetails: {
		[user_id: string]: UserDetails | undefined;
	};
}

export const defaultConfig: Config = {
	use_dark_theme: true,
	rs_plugin_loader: false,
	close_after_launch: false,
	check_announcements: true,
	flatpak_rich_presence: false,
	runelite_use_custom_jar: false,
	use_custom_rs_config_uri: false,
	rs_launch_command: null,
	osrs_launch_command: null,
	runelite_custom_jar: null,
	runelite_launch_command: null,
	hdos_launch_command: null,
	selected: {
		game: Game.osrs,
		client: Client.runelite,
		user_id: null
	},
	userDetails: {}
};

export function initConfig() {
	const params = new URLSearchParams(window.location.search);
	const configParam = params.get('config');

	function isConfigValid(conf: Config): conf is Config {
		return (
			typeof conf === 'object' &&
			typeof conf.use_dark_theme === 'boolean' &&
			typeof conf.rs_plugin_loader === 'boolean' &&
			typeof conf.close_after_launch === 'boolean' &&
			typeof conf.check_announcements === 'boolean' &&
			typeof conf.flatpak_rich_presence === 'boolean' &&
			typeof conf.runelite_use_custom_jar === 'boolean' &&
			typeof conf.selected === 'object' &&
			Object.values(Game).includes(conf.selected.game) &&
			Object.values(Client).includes(conf.selected.client) &&
			(typeof conf.selected.user_id === 'string' || conf.selected.user_id === null) &&
			typeof conf.userDetails === 'object'
		);
	}

	const { config } = GlobalState;

	if (configParam) {
		try {
			const parsedConfig = JSON.parse(configParam);
			if (typeof parsedConfig.check_announcements === 'undefined') {
				parsedConfig.check_announcements = true;
				GlobalState.configHasPendingChanges = true;
			}
			if (typeof parsedConfig.close_after_launch === 'undefined') {
				parsedConfig.close_after_launch = false;
				GlobalState.configHasPendingChanges = true;
			}
			if (isConfigValid(parsedConfig)) {
				config.set(parsedConfig);
			} else {
				logger.error(
					'The config saved on disk is not the correct format. It has been restored to default.'
				);
				GlobalState.configHasPendingChanges = true;
			}
		} catch (e) {
			logger.error('Unable to parse config, restoring to default');
			GlobalState.configHasPendingChanges = true;
		}
	} else {
		GlobalState.configHasPendingChanges = true;
	}

	onWritableChange(GlobalState.config, () => {
		GlobalState.configHasPendingChanges = true;
	});
}
