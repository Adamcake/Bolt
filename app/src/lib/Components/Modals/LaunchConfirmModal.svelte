<script lang="ts">
	import Modal from '$lib/Components/CommonUI/Modal.svelte';
	import { Client, Game } from '$lib/Util/Interfaces';

	let modal: Modal;
	let launchGame: Game;
	let launchClient: Client;
	let launchFunction: (game: Game, client: Client) => void;
	export function open(launch: (game: Game, client: Client) => void, game: Game, client: Client) {
		launchFunction = launch;
		launchGame = game;
		launchClient = client;
		modal.open();
	}
</script>

<Modal bind:this={modal} class="h-fit w-3/5 px-5 py-3 text-center">
	<div>
		<h2 class="text-3xl font-extrabold">Game is offline</h2>
		<br />
		<p>This game appears to be offline for maintenance or an update. Launch anyway?</p>
		<div class="align-bottom">
			<button
				class="mx-1 mb-2 mt-5 w-[45%] rounded-lg bg-blue-500 p-2 font-bold text-black duration-200 hover:opacity-75"
				onclick={() => {
					modal.close();
				}}>No</button
			>
			<button
				class="mx-1 mb-2 mt-5 w-[45%] rounded-lg bg-emerald-500 p-2 font-bold text-black duration-200 hover:opacity-75"
				onclick={() => {
					launchFunction(launchGame, launchClient);
					modal.close();
				}}>Yes</button
			>
		</div>
	</div>
</Modal>
