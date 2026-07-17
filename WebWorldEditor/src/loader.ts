// Local file loading. Everything runs in the browser: the File System Access
// API when available (whole folder), with a multi-file <input> / drag-drop
// fallback. No network, no upload.

import {
  parseAtt, parseObj, parseHeight, parseLight,
  AttData, ObjData, UnsupportedEncryption
} from "./formats";

export interface LoadedMap {
  name: string;
  attName?: string;
  objName?: string;
  att?: AttData;
  obj?: ObjData;
  height?: Float32Array;
  light?: Float32Array;
  warnings: string[];
}

// A grab-bag of the raw bytes we found, keyed by lowercased base filename.
type RawFiles = Map<string, { name: string; bytes: Uint8Array }>;

function pickTerrainNumber(files: RawFiles): number | null {
  let found: number | null = null;
  for (const key of files.keys()) {
    const m = key.match(/^encterrain(\d+)\.att$/);
    if (m) {
      const n = parseInt(m[1], 10);
      if (found === null || n < found) found = n;
    }
  }
  return found;
}

export function assembleMap(files: RawFiles, label: string): LoadedMap {
  const warnings: string[] = [];
  const out: LoadedMap = { name: label, warnings };

  const num = pickTerrainNumber(files);

  const getBytes = (name: string) => files.get(name.toLowerCase())?.bytes;

  if (num !== null) {
    const attName = `EncTerrain${num}.att`;
    const objName = `EncTerrain${num}.obj`;
    const attBytes = getBytes(attName);
    const objBytes = getBytes(objName);
    out.attName = attName;
    out.objName = objName;
    if (attBytes) {
      try { out.att = parseAtt(attBytes); }
      catch (e) { warnings.push(describe(attName, e)); }
    }
    if (objBytes) {
      try { out.obj = parseObj(objBytes); }
      catch (e) { warnings.push(describe(objName, e)); }
    }
  } else {
    warnings.push("No EncTerrain*.att found in the selection.");
  }

  const hBytes = getBytes("TerrainHeight.OZB");
  if (hBytes) {
    try { out.height = parseHeight(hBytes); }
    catch (e) { warnings.push(describe("TerrainHeight.OZB", e)); }
  } else {
    warnings.push("TerrainHeight.OZB missing — terrain will be flat.");
  }

  const lBytes = getBytes("TerrainLight.OZB");
  if (lBytes) {
    try { out.light = parseLight(lBytes); }
    catch (e) { warnings.push(describe("TerrainLight.OZB", e)); }
  }

  return out;
}

function describe(name: string, e: unknown): string {
  if (e instanceof UnsupportedEncryption) return `${name}: ${e.message}`;
  return `${name}: ${(e as Error).message ?? String(e)}`;
}

// --- Sources -------------------------------------------------------------

function baseName(path: string): string {
  const i = Math.max(path.lastIndexOf("/"), path.lastIndexOf("\\"));
  return i >= 0 ? path.slice(i + 1) : path;
}

const WANTED = /\.(att|obj|ozb)$/i;

export async function loadFromFileList(fileList: FileList | File[]): Promise<RawFiles> {
  const files: RawFiles = new Map();
  for (const f of Array.from(fileList)) {
    const name = baseName((f as any).webkitRelativePath || f.name);
    if (!WANTED.test(name)) continue;
    const bytes = new Uint8Array(await f.arrayBuffer());
    files.set(name.toLowerCase(), { name, bytes });
  }
  return files;
}

// File System Access API (Chromium). Returns null if unavailable/cancelled.
export async function loadFromDirectoryPicker(): Promise<{ files: RawFiles; label: string } | null> {
  const picker = (window as any).showDirectoryPicker;
  if (typeof picker !== "function") return null;
  let dir: any;
  try {
    dir = await picker({ mode: "read" });
  } catch {
    return null; // user cancelled
  }
  const files: RawFiles = new Map();
  for await (const entry of dir.values()) {
    if (entry.kind !== "file") continue;
    if (!WANTED.test(entry.name)) continue;
    const file = await entry.getFile();
    const bytes = new Uint8Array(await file.arrayBuffer());
    files.set(entry.name.toLowerCase(), { name: entry.name, bytes });
  }
  return { files, label: dir.name };
}

export async function loadFromDataTransfer(dt: DataTransfer): Promise<RawFiles> {
  return loadFromFileList(dt.files);
}
