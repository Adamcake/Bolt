<script lang="ts">
	import { onDestroy, onMount } from 'svelte';
	import { msg, err, boltSub } from '../main';
	import { accountList, config, credentials, isConfigDirty, selectedPlay } from '../store';
	import type { Credentials } from '../interfaces';
	import { checkRenewCreds, loginClicked, revokeOauthCreds, saveAllCreds } from '../functions';

	// props
	export let showAccountDropdown: boolean;
	export let hoverAccountButton: boolean;

	// values gather from s()
	const sOrigin = atob(boltSub.origin);
	const clientId = atob(boltSub.clientid);
	const exchangeUrl = sOrigin.concat('/oauth2/token');
	const revokeUrl = sOrigin.concat('/oauth2/revoke');

	let mousedOver: boolean = false;
	let accountSelect: HTMLSelectElement;

	// callback function for 'mousedown' events
	// closes the dropdown, unless they press the button that triggers it
	function checkClickOutside(evt: MouseEvent): void {
		if (hoverAccountButton) return;

		if (evt.button === 0 && showAccountDropdown && !mousedOver) {
			showAccountDropdown = false;
		}
	}

	// upate selected_play and account_list when logout is clicked
	function logoutClicked(): void {
		if (accountSelect.options.length == 0) {
			msg('Logout unsuccessful: no account selected');
			return;
		}

		let creds: Credentials | undefined = $selectedPlay.credentials;
		if ($selectedPlay.account) {
			$accountList.delete($selectedPlay.account?.userId);
			$accountList = $accountList; // certain data structure methods won't trigger an update, so we force one manually
		}
		// account_select will update after this function, so we need to change it manually now
		// Try i-1 and i+1, else reset selected_play
		const index = accountSelect.selectedIndex;
		if (index > 0) accountSelect.selectedIndex = index - 1;
		else if (index == 0 && $accountList.size > 0) {
			accountSelect.selectedIndex = index + 1;
		} else {
			delete $selectedPlay.account;
			delete $selectedPlay.character;
			delete $selectedPlay.credentials;
		}

		accountChanged();

		if (!creds) return;

		checkRenewCreds(creds, exchangeUrl, clientId).then((x) => {
			if (x === null) {
				revokeOauthCreds(creds!.access_token, revokeUrl, clientId).then((res: unknown) => {
					if (res === 200) {
						msg('Successful logout');
						removeLogin(<Credentials>creds);
					} else {
						err(`Logout unsuccessful: status ${res}`, false);
					}
				});
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
	function accountChanged(): void {
		isConfigDirty.set(true);

		const key: string = <string>(
			accountSelect[accountSelect.selectedIndex].getAttribute('data-id')
		);
		$selectedPlay.account = $accountList.get(key);
		$config.selected_account = key;
		$selectedPlay.credentials = $credentials.get(<string>$selectedPlay.account?.userId);

		// state updates can be weird, for some reason, changing the 'options' under the character_select
		// does not trigger the 'on:change', so we force update it.
		// I dislike this bit of code but couldn't think of another way to solve the problem other than props from the parent
		if ($selectedPlay.account && $selectedPlay.account.characters) {
			const char_select: HTMLSelectElement = <HTMLSelectElement>(
				document.getElementById('character_select')
			);
			char_select!.selectedIndex = 0;
			const [first_key] = $selectedPlay.account.characters.keys();
			$selectedPlay.character = $selectedPlay.account.characters.get(first_key);
		}
	}

	// check if user clicks outside of the dropdown,
	addEventListener('mousedown', checkClickOutside);

	// checks the config and updates the selected_play and select
	onMount(() => {
		let index: number = 0;
		$accountList.forEach((value, _key) => {
			if (value.displayName == $selectedPlay.account?.displayName) {
				accountSelect.selectedIndex = index;
			}
			index++;
		});

		$selectedPlay.credentials = $credentials.get(<string>$selectedPlay.account?.userId);
	});

	// When the component is removed, delete the event listener also
	onDestroy(() => {
		removeEventListener('mousedown', checkClickOutside);
	});
</script>

<div
	class="z-10 w-48 rounded-lg border-2 border-slate-300 bg-slate-100 p-3 shadow dark:border-slate-800 dark:bg-slate-900"
	role="none"
	on:mouseenter={() => {
		mousedOver = true;
	}}
	on:mouseleave={() => {
		mousedOver = false;
	}}>
	<select
		name="account_select"
		id="account_select"
		class="w-full cursor-pointer rounded-lg border-2 border-inherit bg-inherit p-2 text-center"
		bind:this={accountSelect}
		on:change={() => {
			accountChanged();
		}}>
		{#each $accountList as account}
			<option data-id={account[1].userId} class="dark:bg-slate-900"
				>{account[1].displayName}</option>
		{/each}
	</select>
	<div class="mt-5 flex">
		<button
			class="mx-auto mr-2 rounded-lg bg-blue-500 p-2 font-bold text-black duration-200 hover:opacity-75"
			on:click={() => {
				loginClicked();
			}}>
			Log In
		</button>
		<button
			class="mx-auto rounded-lg border-2 border-blue-500 p-2 font-bold duration-200 hover:opacity-75"
			on:click={() => {
				logoutClicked();
			}}>
			Log Out
		</button>
	</div>
</div>
