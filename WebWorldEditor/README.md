# MU Web World Editor (Season 5.2)

A browser-based **viewer/editor for MU Online Season 5.2 world files**. It opens
map data **locally** (offline, in-game files stay on your machine — nothing is
uploaded) and renders the terrain, lighting, collision attributes and object
placements in 3D. It is a static single-page app, so it deploys as-is on Vercel
(or any static host).

This is the *offline / local-file* counterpart to `VDraven/MuOnline-WorldEditor`,
which is an in-game C++ DLL editor for Season 16 — that project can't run in a
browser, so this is a fresh, format-compatible implementation for 5.2.

## What it does

- **Load a map** — pick a map folder (`Data\World1`, Chromium's directory
  picker), select files, or drag-and-drop. It looks for `EncTerrain{N}.att`,
  `EncTerrain{N}.obj`, `TerrainHeight.OZB` and `TerrainLight.OZB`.
- **Render terrain** from the heightmap with the map's baked vertex light.
- **Attribute overlay** — colour-coded TW_* collision flags (safe zone, no-move,
  water, no-ground, …).
- **Objects** — every placement drawn as a colour-per-type marker at its real
  world position/rotation/scale.
- **Edit attributes** — Shift-drag to paint/erase any TW_* flag with a
  round brush.
- **Export** — writes a new, correctly **encrypted** `EncTerrain{N}.att` /
  `EncTerrain{N}.obj` you can drop straight back into the client.

## Format fidelity

Every parser is a byte-for-byte port of the canonical 5.2 client loaders, so what
the editor reads/writes is exactly what the game reads/writes:

| File | Client reference | Notes |
|------|------------------|-------|
| Encryption | `ZzzLodTerrain.h` `MapFileDecrypt`/`MapFileEncrypt`, `Util.cpp` `BuxConvert` | ported in `src/crypto.ts` |
| `EncTerrain{N}.att` | `ZzzLodTerrain.cpp::OpenTerrainAttribute` | classic (65540) & ext 16-bit (131076) |
| `EncTerrain{N}.obj` | `ZzzObject.cpp::OpenObjectsEnc` | versions 0–3 on load, writes v0 |
| `TerrainHeight.OZB` | `OpenTerrainHeight` / `OpenTerrainHeightNew` | 8-bit (×1.5) and 24-bit variants |
| `TerrainLight.OZB` | `OpenBMPBuffer` | per-tile RGB light |

### Not yet supported
- Files wrapped in the newer **`ATT1`/`MAP1` (ModulusDecrypt2)** container — that
  routine is proprietary and not part of the client source, so those maps are
  detected and reported rather than mis-decoded.
- **BMD model rendering** — objects are shown as markers, not their meshes.
- `OZJ` (JPEG) terrain light and `.map` texture layers — parsed but not textured.

These are the natural next steps.

## Develop

```bash
npm install
npm run dev      # local dev server
npm run build    # type-check + production build to dist/
npm run preview  # serve the production build
```

## Deploy on Vercel

This repository is a standard Vite app at its root — import it into Vercel and
deploy with no extra configuration:
- Framework preset: **Vite**
- Build command: `npm run build`
- Output directory: `dist`

`vercel.json` already declares these.
