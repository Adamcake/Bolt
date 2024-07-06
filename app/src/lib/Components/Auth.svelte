<script lang="ts">
	import { AuthService, type AuthTokens } from '$lib/Services/AuthService';
	import { CookieService } from '$lib/Services/CookieService';
	import { LocalStorageService } from '$lib/Services/LocalStorageService';
	import type { BoltEnv } from '$lib/State/Bolt';
	import type { BoltMessage } from '$lib/Util/interfaces';
	import { onDestroy, onMount } from 'svelte';

	const parentWindow = window.opener as {
		postMessage: (event: BoltMessage, allowedOrigin: string) => void;
	};

	const boltEnv = LocalStorageService.get('boltEnv') as BoltEnv;

	if (boltEnv == null) {
		fail('BoltEnv is not defined. Please close and re-open Bolt to try again.');
	}

	async function retrieveOAuthToken(authCode: string): Promise<AuthTokens | null> {
		const verifier = CookieService.get('auth_verifier') as string;
		if (!verifier) {
			fail('Verifier token has expired. Please try signing in again.');
			return null;
		}
		const { clientid, redirect } = boltEnv;
		const tokenResult = await AuthService.getOAuthToken(
			boltEnv.origin,
			clientid,
			verifier,
			redirect,
			authCode
		);
		if (!tokenResult.ok) {
			fail(`Fetching OAuth token failed. ${tokenResult.error}`);
			return null;
		}
		return tokenResult.value;
	}

	async function retrieveSessionId(idToken: string) {
		const expectedNonce = CookieService.get('auth_nonce');
		const validateResult = AuthService.validateIdToken(idToken, expectedNonce ?? '');
		if (!validateResult.ok) {
			fail(validateResult.error);
			return null;
		}
		const sessionIdResult = await AuthService.getSessionId(boltEnv.auth_api, idToken);
		if (!sessionIdResult.ok) {
			fail(`Unable to retreive session id. ${sessionIdResult.error}`);
			return null;
		}
		return sessionIdResult.value;
	}

	function fail(reason: string) {
		message({ type: 'authFailed', reason });
		window.close();
	}

	/**
	 * Messages the parent window with an event name and associated data
	 */
	function message(event: BoltMessage) {
		const internalUrl = 'https://bolt-internal';
		parentWindow.postMessage(event, internalUrl);
	}

	onMount(async () => {
		const params = new URLSearchParams(window.location.search);
		const code = params.get('code');
		const state = params.get('state');
		const id_token = params.get('id_token');
		if (id_token == null && code && state) {
			// First step, after the user has signed in
			const authTokens = await retrieveOAuthToken(code);
			if (!authTokens) return fail('tokens object is null.');

			message({ type: 'authTokenUpdate', tokens: authTokens });
			const nonce = crypto.randomUUID();
			AuthService.navigateToAuthConsent(boltEnv.origin, authTokens.id_token, nonce);
		} else if (id_token && code && state) {
			// Second step, after retrieving the authTokens and consent request has returned
			const sessionId = await retrieveSessionId(id_token);
			if (!sessionId) return fail('sessionId is null');

			message({ type: 'authSessionUpdate', sessionId });
			window.close();
		} else {
			fail(
				'Authentication server did not response with the appropriate parameters. Please try again later.'
			);
		}
	});

	onDestroy(() => {
		CookieService.remove('auth_verifier');
		CookieService.remove('auth_nonce');
	});
</script>

<div
	class="container mx-auto bg-slate-100 p-5 text-center text-slate-900 dark:bg-slate-900 dark:text-slate-50"
>
	<img src="svgs/circle-notch-solid.svg" alt="" class="mx-auto h-1/6 w-1/6 animate-spin" />
</div>
