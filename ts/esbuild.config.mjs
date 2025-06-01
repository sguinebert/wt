// esbuild.config.mjs
import { build } from 'esbuild';

const shared = {
  entryPoints: ['src/index.ts'],
  bundle: true,
  format: 'esm',
  target: 'es2020',
  sourcemap: true,
  outdir: 'public/js',
  platform: 'browser',
  // Optional: externalise big libs you prefer as separate <script> tags:
  // external: ['chart.js', 'slickgrid', 'sortablejs', 'swup', '@floating-ui/dom']
};

const watch = process.argv.includes('--watch');
build({ watch, ...shared })
  .then(() =>
    console.log(
      watch ? '👀 esbuild is watching…' : '✅ esbuild build finished.'
    )
  )
  .catch(() => process.exit(1));
