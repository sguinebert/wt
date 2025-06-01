import { defineConfig } from 'rolldown';
import { dts } from 'rolldown-plugin-dts';   //  👉 remove if you don't need .d.ts bundles

export default defineConfig({
  // entry
  input: 'src/index.ts',

  // output bundle(s)
  output: {
    dir: 'public/js',
    format: 'es',
    sourcemap: true,
    minify: false,              // built-in minifier is still experimental
    entryFileNames: '[name].js'
  },

  // resolve TS -> JS out of the box; Rollup-compatible plugins work, too
  plugins: [
    dts()                       // bundles types to public/js/index.d.ts
  ],

  // Equivalent of Rollup’s “treeshake: true” is on by default
});
