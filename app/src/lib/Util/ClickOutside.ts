interface IOptions {
	callback: () => void;
	ignore?: HTMLElement[];
}

// https://github.com/Th1nkK1D/svelte-use-click-outside
function clickOutside(
	node: HTMLElement,
	{ callback, ignore = [] }: IOptions
): { destroy: () => void } {
	const onClick = (event: MouseEvent) => {
		const targetIsIgnored = ignore.some((el) => el.contains(event.target as HTMLElement));

		if (
			node &&
			!node.contains(event.target as HTMLElement) &&
			!event.defaultPrevented &&
			!targetIsIgnored
		) {
			callback();
		}
	};

	document.addEventListener('click', onClick, true);
	document.addEventListener('contextmenu', onClick, true);

	return {
		destroy() {
			document.removeEventListener('click', onClick, true);
			document.removeEventListener('contextmenu', onClick, true);
		}
	};
}

export default clickOutside;
