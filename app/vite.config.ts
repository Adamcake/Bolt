import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';
import tsconfigPaths from 'vite-tsconfig-paths';
import tailwindcss from 'tailwindcss';

// https://vitejs.dev/config/

export default defineConfig(({ mode }) => {
	const dev = mode === 'development';
	return {
		build: {
			sourcemap: dev ? 'inline' : false,
			outDir: 'dist'
		},
		plugins: [tsconfigPaths(), svelte()],
		css: {
			postcss: {
				plugins: [tailwindcss()]
			}
		}
	};
});
