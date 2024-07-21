import { BoltService } from '$lib/Services/BoltService';
import { GlobalState } from '$lib/State/GlobalState';
import { Client, Game } from '$lib/Util/interfaces';
import { logger } from '$lib/Util/Logger';
import { onWritableChange } from '$lib/Util/onWritableChange';
import { get } from 'svelte/store';

export interface Config {
	use_dark_theme: boolean;
	rs_plugin_loader: boolean;
	flatpak_rich_presence: boolean;
	runelite_use_custom_jar: boolean;
	selected_game: Game;
	selected_client: Client;
	use_custom_rs_config_uri: boolean;
	rs_config_uri?: string;
	runelite_custom_jar?: string;
	selected_user_id?: string;
	selected_account_id?: string;
}

export const defaultConfig: Config = {
	use_dark_theme: true,
	rs_plugin_loader: false,
	flatpak_rich_presence: false,
	runelite_use_custom_jar: false,
	use_custom_rs_config_uri: false,
	selected_game: Game.osrs,
	selected_client: Client.runelite
};

export function initConfig() {
	const params = new URLSearchParams(window.location.search);
	const configParam = params.get('config');

	function isConfigValid(conf: Config): conf is Config {
		return (
			typeof conf === 'object' &&
			typeof conf.use_dark_theme === 'boolean' &&
			typeof conf.rs_plugin_loader === 'boolean' &&
			typeof conf.flatpak_rich_presence === 'boolean' &&
			typeof conf.runelite_use_custom_jar === 'boolean' &&
			Object.values(Game).includes(conf.selected_game) &&
			Object.values(Client).includes(conf.selected_client)
		);
	}

	const { config } = GlobalState;

	if (configParam) {
		try {
			const parsedConfig = JSON.parse(configParam);
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
		BoltService.saveConfig(false);
	}

	onWritableChange(GlobalState.config, () => {
		GlobalState.configHasPendingChanges = true;
	});
}

export function selectFirstSession() {
	const { config, sessions: sessionsStore } = GlobalState;
	config.update((_config) => {
		const sessions = get(sessionsStore);
		if (sessions.length > 0) {
			const firstSession = sessions[0];
			_config.selected_user_id = firstSession.user.userId;
			_config.selected_account_id = firstSession.accounts[0].accountId;
		} else {
			_config.selected_user_id = undefined;
			_config.selected_account_id = undefined;
		}
		return _config;
	});
}
