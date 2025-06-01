// vite.config.ts
import { defineConfig } from 'vite';
import tsconfigPaths from 'vite-tsconfig-paths';

export default defineConfig({
  plugins: [
    tsconfigPaths()          // ♥ aligns Vite with compilerOptions.paths
  ],
  build: {
    target: 'es2020',
    outDir: 'public/js',
    sourcemap: true,
    emptyOutDir: false       // keep other builds (esbuild/webpack) if any
  }
});
