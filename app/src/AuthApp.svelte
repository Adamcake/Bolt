<script lang="ts">
	import MainLayout from '$lib/Components/MainLayout.svelte';
	import { AuthService, type AuthTokens } from '$lib/Services/AuthService';
	import { CookieService } from '$lib/Services/CookieService';
	import { bolt } from '$lib/State/Bolt';
	import type { BoltMessage } from '$lib/Util/interfaces';
	import { onDestroy, onMount } from 'svelte';

	const parentWindow = window.opener as {
		postMessage: (event: BoltMessage, allowedOrigin: string) => void;
	};

	if (bolt.env == null) {
		fail('BoltEnv is not defined. Please close and re-open Bolt to try again.');
	}

	async function retrieveOAuthToken(authCode: string): Promise<AuthTokens | null> {
		const verifier = CookieService.get('auth_verifier') as string;
		if (!verifier) {
			fail('Verifier token has expired. Please try signing in again.');
			return null;
		}
		const { clientid, redirect } = bolt.env;
		const tokenResult = await AuthService.getOAuthToken(
			bolt.env.origin,
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
		const sessionIdResult = await AuthService.getSessionId(bolt.env.auth_api, idToken);
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
		parentWindow.postMessage(event, bolt.internalUrl);
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
			AuthService.navigateToAuthConsent(bolt.env.origin, authTokens.id_token, nonce);
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

<MainLayout>
	<div class="flex h-full items-center justify-center">
		<img class="h-60 w-60 animate-spin" src="svgs/circle-notch-solid.svg" alt="loading" />
	</div>
</MainLayout>
