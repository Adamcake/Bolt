<script lang="ts">
	import { onDestroy, onMount } from 'svelte';
	import { msg, err, bolt_sub } from '../main';
	import { account_list, config, credentials, is_config_dirty, selected_play } from '../store';
	import type { Credentials } from '../interfaces';
	import { checkRenewCreds, loginClicked, revokeOauthCreds, saveAllCreds } from '../functions';

	// props
	export let show_account_dropdown: boolean;
	export let hover_account_button: boolean;

	// values gather from s()
	const s_origin = atob(bolt_sub.origin);
	const client_id = atob(bolt_sub.clientid);
	const exchange_url = s_origin.concat('/oauth2/token');
	const revoke_url = s_origin.concat('/oauth2/revoke');

	let moused_over: boolean = false;
	let account_select: HTMLSelectElement;

	// callback function for 'mousedown' events
	// closes the dropdown, unless they press the button that triggers it
	function check_click_outside(evt: MouseEvent): void {
		if (hover_account_button) return;

		if (evt.button === 0 && show_account_dropdown && !moused_over) {
			show_account_dropdown = false;
		}
	}

	// upate selected_play and account_list when logout is clicked
	function logout_clicked(): void {
		if (account_select.options.length == 0) {
			msg('Logout unsuccessful: no account selected');
			return;
		}

		let creds: Credentials | undefined = $selected_play.credentials;
		if ($selected_play.account) {
			$account_list.delete($selected_play.account?.userId);
			$account_list = $account_list; // certain data structure methods won't trigger an update, so we force one manually
		}
		// account_select will update after this function, so we need to change it manually now
		// Try i-1 and i+1, else reset selected_play
		const index = account_select.selectedIndex;
		if (index > 0) account_select.selectedIndex = index - 1;
		else if (index == 0 && $account_list.size > 0) {
			account_select.selectedIndex = index + 1;
		} else {
			delete $selected_play.account;
			delete $selected_play.character;
			delete $selected_play.credentials;
		}

		account_changed();

		if (!creds) return;

		checkRenewCreds(creds, exchange_url, client_id).then((x) => {
			if (x === null) {
				revokeOauthCreds(creds!.access_token, revoke_url, client_id).then(
					(res: unknown) => {
						if (res === 200) {
							msg('Successful logout');
							removeLogin(<Credentials>creds);
						} else {
							err(`Logout unsuccessful: status ${res}`, false);
						}
					}
				);
			} else if (x === 400 || x === 401) {
				msg('Logout unsuccessful: credentials are invalid, so discarding them anyway');
				if (creds) removeLogin(creds);
			} else {
				err(
					'Logout unsuccessful: unable to verify credentials due to a network error',
					false
				);
			}
		});
	}

	// clear credentials when logout is clicked
	function removeLogin(creds: Credentials): void {
		$credentials.delete(creds.sub);
		saveAllCreds();
	}

	// updated active account in selected_play store
	function account_changed(): void {
		is_config_dirty.set(true);

		const key: string = <string>(
			account_select[account_select.selectedIndex].getAttribute('data-id')
		);
		$selected_play.account = $account_list.get(key);
		$config.selected_account = key;
		$selected_play.credentials = $credentials.get(<string>$selected_play.account?.userId);

		// state updates can be weird, for some reason, changing the 'options' under the character_select
		// does not trigger the 'on:change', so we force update it.
		// I dislike this bit of code but couldn't think of another way to solve the problem other than props from the parent
		if ($selected_play.account && $selected_play.account.characters) {
			const char_select: HTMLSelectElement = <HTMLSelectElement>(
				document.getElementById('character_select')
			);
			char_select!.selectedIndex = 0;
			const [first_key] = $selected_play.account.characters.keys();
			$selected_play.character = $selected_play.account.characters.get(first_key);
		}
	}

	// check if user clicks outside of the dropdown,
	addEventListener('mousedown', check_click_outside);

	// checks the config and updates the selected_play and select
	onMount(() => {
		let index: number = 0;
		$account_list.forEach((value, _key) => {
			if (value.displayName == $selected_play.account?.displayName) {
				account_select.selectedIndex = index;
			}
			index++;
		});

		$selected_play.credentials = $credentials.get(<string>$selected_play.account?.userId);
	});

	// When the component is removed, delete the event listener also
	onDestroy(() => {
		removeEventListener('mousedown', check_click_outside);
	});
</script>

<div
	class="w-48 rounded-lg shadow p-3 border-2 border-slate-300 dark:border-slate-800 z-10 bg-slate-100 dark:bg-slate-900"
	role="none"
	on:mouseenter={() => {
		moused_over = true;
	}}
	on:mouseleave={() => {
		moused_over = false;
	}}>
	<select
		name="account_select"
		id="account_select"
		class="cursor-pointer w-full p-2 rounded-lg bg-inherit border-2 border-inherit text-center"
		bind:this={account_select}
		on:change={() => {
			account_changed();
		}}>
		{#each $account_list as account}
			<option data-id={account[1].userId} class="dark:bg-slate-900"
				>{account[1].displayName}</option>
		{/each}
	</select>
	<div class="flex mt-5">
		<button
			class="font-bold p-2 duration-200 mx-auto rounded-lg text-black bg-blue-500 hover:opacity-75 mr-2"
			on:click={() => {
				loginClicked();
			}}>
			Log In
		</button>
		<button
			class="font-bold p-2 duration-200 mx-auto border-2 rounded-lg border-blue-500 hover:opacity-75"
			on:click={() => {
				logout_clicked();
			}}>
			Log Out
		</button>
	</div>
</div>
