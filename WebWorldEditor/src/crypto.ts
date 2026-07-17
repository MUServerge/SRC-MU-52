// MU Online map-file encryption, ported byte-for-byte from the Season 5.2
// client so the browser decodes/encodes exactly what the game does.
//
// Reference (canonical client source):
//   SRCMainGS/Source/Main5.2/source/ZzzLodTerrain.h  -> MapFileDecrypt / MapFileEncrypt
//   SRCMainGS/Source/Main5.2/source/Util.cpp         -> BuxConvert (bBuxCode)

const MAP_XOR_KEY = [
  0xd1, 0x73, 0x52, 0xf6, 0xd2, 0x9a, 0xcb, 0x27,
  0x3e, 0xaf, 0x59, 0x31, 0x37, 0xb3, 0xe7, 0xa2
];

const BUX_CODE = [0xfc, 0xcf, 0xab];

/**
 * Inverse of MapFileEncrypt. Matches the client loop:
 *   dst[i] = (src[i] ^ key[i%16]) - mapKey;  mapKey = (src[i] + 0x3D) & 0xFF
 * with mapKey seeded at 0x5E.
 */
export function mapFileDecrypt(src: Uint8Array): Uint8Array {
  const dst = new Uint8Array(src.length);
  let mapKey = 0x5e;
  for (let i = 0; i < src.length; i++) {
    dst[i] = ((src[i] ^ MAP_XOR_KEY[i % 16]) - mapKey) & 0xff;
    mapKey = (src[i] + 0x3d) & 0xff;
  }
  return dst;
}

/**
 * MapFileEncrypt — inverse of the above, matches the client loop:
 *   dst[i] = (src[i] + mapKey) ^ key[i%16];  mapKey = (dst[i] + 0x3D) & 0xFF
 * with mapKey seeded at 0x5E.
 */
export function mapFileEncrypt(src: Uint8Array): Uint8Array {
  const dst = new Uint8Array(src.length);
  let mapKey = 0x5e;
  for (let i = 0; i < src.length; i++) {
    dst[i] = ((src[i] + mapKey) & 0xff) ^ MAP_XOR_KEY[i % 16];
    mapKey = (dst[i] + 0x3d) & 0xff;
  }
  return dst;
}

/** BuxConvert — self-inverse XOR with the 3-byte bux code. Mutates in place. */
export function buxConvert(buf: Uint8Array): void {
  for (let i = 0; i < buf.length; i++) {
    buf[i] ^= BUX_CODE[i % 3];
  }
}

/**
 * True when the file starts with the newer modulus-crypto container header
 * ("ATT" / "MAP" + version 1). Those files are protected by the proprietary
 * ModulusDecrypt2 routine (external, not part of this client source), so the
 * web editor can't decode them and should tell the user.
 */
export function hasModulusHeader(bytes: Uint8Array): boolean {
  if (bytes.length < 4) return false;
  const s = String.fromCharCode(bytes[0], bytes[1], bytes[2]);
  return (s === "ATT" || s === "MAP") && bytes[3] === 1;
}
