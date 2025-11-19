import { promises as fsPromises } from "fs";
import { createReadStream, createWriteStream } from "fs";
import path from "path";
import { fileURLToPath } from "url";
import zlib from "zlib";
import { pipeline } from "stream/promises";

const { access, mkdir, readdir, rm, rename, stat } = fsPromises;

const __dirname = fileURLToPath(new URL(".", import.meta.url));
const distDir = path.join(__dirname, "dist");
const dataDir = path.join(__dirname, "..", "data");

async function pathExists(p) {
  try {
    await access(p);
    return true;
  } catch {
    return false;
  }
}

async function rmIfExists(p) {
  if (await pathExists(p)) {
    await rm(p, { recursive: true, force: true });
  }
}

async function ensureDir(p) {
  await mkdir(p, { recursive: true });
}

async function copyDir(src, dest) {
  await ensureDir(dest);
  const entries = await readdir(src, { withFileTypes: true });
  for (const entry of entries) {
    const srcPath = path.join(src, entry.name);
    const destPath = path.join(dest, entry.name);
    if (entry.isDirectory()) {
      await copyDir(srcPath, destPath);
    } else if (entry.isFile()) {
      await fsPromises.copyFile(srcPath, destPath);
    }
  }
}

async function* walk(dir) {
  const entries = await readdir(dir, { withFileTypes: true });
  for (const entry of entries) {
    const fullPath = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      yield* walk(fullPath);
    } else if (entry.isFile()) {
      yield fullPath;
    }
  }
}

async function gzipFile(file) {
  const gzPath = `${file}.gz`;
  const source = createReadStream(file);
  const dest = createWriteStream(gzPath);
  const gzip = zlib.createGzip();
  await pipeline(source, gzip, dest);
}

async function main() {
  if (!(await pathExists(distDir))) {
    console.error("dist/ not found. Run `vite build` first.");
    process.exit(1);
  }

  await ensureDir(dataDir);

  // Remove old assets and index
  await rmIfExists(path.join(dataDir, "assets"));
  await rmIfExists(path.join(dataDir, "index.htm"));
  await rmIfExists(path.join(dataDir, "index.htm.gz"));

  // Copy new build into data/
  await copyDir(distDir, dataDir);

  // Rename index.html -> index.htm
  const indexHtml = path.join(dataDir, "index.html");
  if (await pathExists(indexHtml)) {
    await rename(indexHtml, path.join(dataDir, "index.htm"));
  }

  // Gzip .htm, .css, .js files
  const exts = new Set([".htm", ".css", ".js"]);
  for await (const file of walk(dataDir)) {
    const ext = path.extname(file);
    if (exts.has(ext)) {
      await gzipFile(file);
    }
  }
}

main().catch((err) => {
  console.error("postbuild failed:", err);
  process.exit(1);
});

