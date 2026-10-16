#!/usr/bin/env node
/*
 * Prepare Nintendo DS graphics for the PSP port from an extracted project
 * resource tree.  The source is local user-owned data; no ROM is read or
 * embedded by this tool.
 *
 * node tools/build_assets.mjs --source pokeplatinum-main/res --output psp-assets
 */
import { createHash } from "node:crypto";
import { cp, mkdir, readdir, readFile, stat, writeFile } from "node:fs/promises";
import { basename, extname, join, relative } from "node:path";

const TYPES = new Map([
  [".nclr", "palette"], [".ncgr", "tiles"], [".nscr", "screen"],
  [".ncer", "cell"], [".nanr", "animation"], [".narc", "archive"],
]);
const MAGICS = new Map([
  [".nclr", ["RLCN", "RPCN"]], [".ncgr", ["RGCN"]], [".nscr", ["RCSN"]],
  [".ncer", ["RECN"]], [".nanr", ["RNAN"]], [".narc", ["NARC"]],
]);

function option(name) {
  const i = process.argv.indexOf(name);
  return i >= 0 ? process.argv[i + 1] : undefined;
}
async function filesUnder(dir) {
  const result = [];
  for (const entry of await readdir(dir, { withFileTypes: true })) {
    const path = join(dir, entry.name);
    if (entry.isDirectory()) result.push(...await filesUnder(path));
    else if (entry.isFile()) result.push(path);
  }
  return result;
}
function sha256(bytes) { return createHash("sha256").update(bytes).digest("hex"); }

const source = option("--source");
const output = option("--output");
if (!source || !output || process.argv.includes("--help")) {
  console.error("Usage: node tools/build_assets.mjs --source <extracted-res> --output <psp-assets>");
  process.exit(process.argv.includes("--help") ? 0 : 2);
}
if (!(await stat(source)).isDirectory()) throw new Error(`Not a directory: ${source}`);

const assets = [];
for (const input of (await filesUnder(source)).sort()) {
  const type = TYPES.get(extname(input).toLowerCase());
  if (!type) continue;
  const path = relative(source, input).split("\\").join("/");
  const target = join(output, path);
  await mkdir(join(target, ".."), { recursive: true });
  await cp(input, target, { force: true });
  const bytes = await readFile(input);
  const magic = bytes.subarray(0, 4).toString("ascii");
  if (!MAGICS.get(extname(input).toLowerCase()).includes(magic)) {
    throw new Error(`Unexpected ${type} header in ${input}: ${JSON.stringify(magic)}`);
  }
  assets.push({ path, type, bytes: bytes.length, sha256: sha256(bytes), stem: basename(path, extname(path)) });
}
await mkdir(output, { recursive: true });
await writeFile(join(output, "assets.json"), JSON.stringify({ format: 1, assets }, null, 2) + "\n");
console.log(`Prepared ${assets.length} DS graphics/archive assets in ${output}`);
