// Parsers/serializers for MU Online Season 5.2 world files.
// Byte layouts mirror the client loaders in ZzzLodTerrain.cpp / ZzzObject.cpp.

import { mapFileDecrypt, mapFileEncrypt, buxConvert, hasModulusHeader } from "./crypto";

export const TERRAIN_SIZE = 256; // _define.h TERRAIN_SIZE
export const TERRAIN_SCALE = 100; // _define.h TERRAIN_SCALE
const TILES = TERRAIN_SIZE * TERRAIN_SIZE;

// TerrainWall attribute flags (_define.h TW_*)
export const TW = {
  SAFEZONE: 0x01,
  CHARACTER: 0x02,
  NOMOVE: 0x04,
  NOGROUND: 0x08,
  WATER: 0x10,
  ACTION: 0x20,
  HEIGHT: 0x40,
  CAMERA_UP: 0x80
} as const;

export class UnsupportedEncryption extends Error {}

// ---------------------------------------------------------------------------
// EncTerrain{N}.att  — collision / attribute grid
// Load path: OpenTerrainAttribute() — MapFileDecrypt over the whole file, then
// BuxConvert over the decrypted buffer, then [Ver, Map, 255, 255] + wall grid.
// Classic maps store the wall grid as bytes (size 65540); "ext" maps store it
// as WORD per tile (size 131076).
// ---------------------------------------------------------------------------
export interface AttData {
  map: number;
  ext: boolean; // true => 16-bit per tile
  wall: Uint16Array; // TILES entries
}

export function parseAtt(bytes: Uint8Array): AttData {
  if (hasModulusHeader(bytes)) {
    throw new UnsupportedEncryption(
      ".att uses the newer ATT1 (ModulusDecrypt2) container, which this editor can't decode."
    );
  }
  const buf = mapFileDecrypt(bytes);
  buxConvert(buf);

  const size = buf.length;
  if (size !== 131076 && size !== 65540) {
    throw new Error(`Unexpected .att size ${size} (expected 65540 or 131076).`);
  }
  const ext = size === 131076;
  const map = buf[1];
  const wall = new Uint16Array(TILES);

  if (ext) {
    const dv = new DataView(buf.buffer, buf.byteOffset + 4, TILES * 2);
    for (let i = 0; i < TILES; i++) wall[i] = dv.getUint16(i * 2, true);
  } else {
    for (let i = 0; i < TILES; i++) wall[i] = buf[4 + i];
  }
  return { map, ext, wall };
}

export function serializeAtt(data: AttData): Uint8Array {
  const bodySize = data.ext ? 4 + TILES * 2 : 4 + TILES;
  const plain = new Uint8Array(bodySize);
  plain[0] = 0; // Version
  plain[1] = data.map & 0xff;
  plain[2] = 255; // Width
  plain[3] = 255; // Height
  if (data.ext) {
    const dv = new DataView(plain.buffer, 4, TILES * 2);
    for (let i = 0; i < TILES; i++) dv.setUint16(i * 2, data.wall[i], true);
  } else {
    for (let i = 0; i < TILES; i++) plain[4 + i] = data.wall[i] & 0xff;
  }
  buxConvert(plain); // load does BuxConvert after decrypt, so encrypt inverts it first
  return mapFileEncrypt(plain);
}

// ---------------------------------------------------------------------------
// EncTerrain{N}.obj  — object placements
// Load path: OpenObjectsEnc() — MapFileDecrypt only. Header [Ver, Map, Count:u16]
// then Count records: Type:i16, Position:vec3f, Angle:vec3f, Scale:f32,
// plus optional light fields for Version 1/2/3.
// ---------------------------------------------------------------------------
export interface MapObject {
  type: number;
  position: [number, number, number];
  angle: [number, number, number];
  scale: number;
}

export interface ObjData {
  version: number;
  map: number;
  objects: MapObject[];
}

export function parseObj(bytes: Uint8Array): ObjData {
  const buf = mapFileDecrypt(bytes);
  const dv = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
  const version = buf[0];
  const map = buf[1];
  const count = dv.getInt16(2, true);
  let p = 4;
  const objects: MapObject[] = [];
  for (let i = 0; i < count; i++) {
    const type = dv.getInt16(p, true); p += 2;
    const position: [number, number, number] = [
      dv.getFloat32(p, true), dv.getFloat32(p + 4, true), dv.getFloat32(p + 8, true)
    ]; p += 12;
    const angle: [number, number, number] = [
      dv.getFloat32(p, true), dv.getFloat32(p + 4, true), dv.getFloat32(p + 8, true)
    ]; p += 12;
    const scale = dv.getFloat32(p, true); p += 4;
    if (version === 1 || version === 2 || version === 3) {
      p += 2; // EnableLight + EnableFullLight
      if (version === 2 || version === 3) {
        p += 1; // EnablePrimaryLight
        if (version === 3) p += 12; // Light vec3
      }
    }
    objects.push({ type, position, angle, scale });
  }
  return { version, map, objects };
}

// Writes Version 0 records (Type/Position/Angle/Scale), matching SaveObjects().
export function serializeObj(data: ObjData): Uint8Array {
  const recSize = 2 + 12 + 12 + 4;
  const plain = new Uint8Array(4 + data.objects.length * recSize);
  const dv = new DataView(plain.buffer);
  plain[0] = 0; // Version 0
  plain[1] = data.map & 0xff;
  dv.setInt16(2, data.objects.length, true);
  let p = 4;
  for (const o of data.objects) {
    dv.setInt16(p, o.type, true); p += 2;
    dv.setFloat32(p, o.position[0], true); dv.setFloat32(p + 4, o.position[1], true); dv.setFloat32(p + 8, o.position[2], true); p += 12;
    dv.setFloat32(p, o.angle[0], true); dv.setFloat32(p + 4, o.angle[1], true); dv.setFloat32(p + 8, o.angle[2], true); p += 12;
    dv.setFloat32(p, o.scale, true); p += 4;
  }
  return mapFileEncrypt(plain);
}

// ---------------------------------------------------------------------------
// TerrainHeight.OZB  — heightmap. Two variants (OpenTerrainHeight / ...New):
//   old: 4-byte prefix + 1080-byte BMP header + 256*256 8-bit grayscale
//   new: 4-byte prefix + 54-byte BMP header + 256*256 24-bit (height in RGB)
// The client scales the old format by 1.5 (x3 on the login scene).
// ---------------------------------------------------------------------------
const OLD_HEIGHT_SIZE = 4 + 1080 + TILES;

export function parseHeight(bytes: Uint8Array): Float32Array {
  const out = new Float32Array(TILES);
  if (bytes.length >= OLD_HEIGHT_SIZE && bytes.length < 4 + 1080 + TILES * 3) {
    // 8-bit grayscale variant
    const base = 4 + 1080;
    for (let i = 0; i < TILES; i++) out[i] = bytes[base + i] * 1.5;
  } else {
    // 24-bit variant: height = R<<16 | G<<8 | B  (src[0]=B..: pbyHeight[0]=src[2])
    const base = 4 + 54;
    for (let i = 0; i < TILES; i++) {
      const s = base + i * 3;
      out[i] = (bytes[s + 2] | (bytes[s + 1] << 8) | (bytes[s] << 16)) >>> 0;
    }
  }
  return out;
}

// ---------------------------------------------------------------------------
// TerrainLight.OZB — per-tile RGB light (OpenBMPBuffer):
//   4-byte prefix + 54-byte BMP header + 256*256 * 3 bytes (stored B,G,R).
// Returns normalized RGB triples per tile.
// ---------------------------------------------------------------------------
export function parseLight(bytes: Uint8Array): Float32Array {
  const out = new Float32Array(TILES * 3);
  const base = 4 + 54;
  for (let i = 0; i < TILES; i++) {
    const s = base + i * 3;
    if (s + 2 >= bytes.length) break;
    out[i * 3 + 0] = bytes[s + 2] / 255; // R
    out[i * 3 + 1] = bytes[s + 1] / 255; // G
    out[i * 3 + 2] = bytes[s + 0] / 255; // B
  }
  return out;
}
