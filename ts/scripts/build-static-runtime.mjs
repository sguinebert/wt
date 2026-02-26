import { createHash } from 'node:crypto';
import { mkdir, readdir, rm, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { build } from 'esbuild';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const tsRoot = path.resolve(__dirname, '..');
const entryFile = path.resolve(tsRoot, 'src/qml-static/bootstrap.ts');
const outDir = path.resolve(tsRoot, 'public/js/static');
const manifestPath = path.resolve(outDir, 'qml-static-manifest.json');

const result = await build({
  entryPoints: [entryFile],
  bundle: true,
  format: 'esm',
  target: 'es2020',
  platform: 'browser',
  minify: true,
  legalComments: 'none',
  sourcemap: false,
  write: false,
});

if (!result.outputFiles || result.outputFiles.length !== 1) {
  throw new Error('Expected a single bundled output file for qml-static runtime.');
}

const output = result.outputFiles[0].contents;
const sha256Hex = createHash('sha256').update(output).digest('hex');
const sha256B64 = createHash('sha256').update(output).digest('base64');
const shortHash = sha256Hex.slice(0, 16);
const runtimeFile = `qml-static-runtime.${shortHash}.mjs`;
const runtimePath = path.resolve(outDir, runtimeFile);

await mkdir(outDir, { recursive: true });

for (const name of await readdir(outDir)) {
  if (name.startsWith('qml-static-runtime.') && name.endsWith('.mjs')) {
    await rm(path.resolve(outDir, name), { force: true });
  }
}

await writeFile(runtimePath, output);

const manifest = {
  runtime: `/js/static/${runtimeFile}`,
  integrity: `sha256-${sha256B64}`,
  hash: sha256Hex,
};

await writeFile(manifestPath, `${JSON.stringify(manifest, null, 2)}\n`, 'utf8');

console.log(`Built static runtime: ${runtimeFile}`);
console.log(`Manifest: ${manifestPath}`);
