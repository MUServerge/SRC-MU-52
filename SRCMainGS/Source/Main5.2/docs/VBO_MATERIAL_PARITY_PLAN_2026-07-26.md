# VBO Material Parity — removing the glow flicker at its cause

Date: 2026-07-26
Branch: `claude/session-d70nxt`

## Why this exists

Measured on one crowded spot, matched density (~1280 mesh draws/frame):

| config | RenderMeshVBO/f | Legacy/f | BuildCPU | SubmitGL | Render | FPS |
|---|---:|---:|---:|---:|---:|---:|
| `gl33char` ON (Phase 15 default) | 533 | 772 | 7.7 ms | 9.7 ms | 32.8 ms | 30 |
| `gl33char` OFF (legacy arrays) | 533 | 772 | 7.7 ms | 2.9 ms | 24.8 ms | 38-42 |
| + `vbotranslate.on` | 875 | 403 | 2.9 ms | 1.1 ms | 18.4 ms | 50-53 |

A mesh that reaches `BMD::RenderMeshVBO` skips **both** `MeshBuildCPU` and
`MeshSubmitGL`, because the VBO draw returns before the CPU array build. That is
the whole win, and it is the only lever measured so far that moves FPS.

The blocker is the **glow flicker**, and it has one cause:

> When a mesh's base pass is GPU-skinned but an overlay pass on the SAME geometry
> is CPU-skinned, the two vertex positions differ by floating-point noise. Two
> coplanar surfaces that differ by noise z-fight, and z-fighting depends on depth
> precision — which is why it appears only at certain camera distances.

Same class as the Phase 14 base-only-Core terrain z-fight. The rule is the one
that phase established: **never render one surface through two different
geometry paths.**

Fixed already (commit c839fa6): `RENDER_BRIGHT` was excluded from the VBO gate
wholesale, but `RENDER_TEXTURE | RENDER_BRIGHT` resolves to `renderFlags ==
RENDER_TEXTURE` — a plain textured draw whose only difference is the
fixed-function `EnableAlphaBlend()`, which no shader can observe. Flicker
measurably reduced, not gone.

## What is left

`BMD::runtime_render_level` (`ZzzBMD.cpp`) draws the same mesh three times:

```
RenderMesh(GroupId, RENDER_TEXTURE | RENDER_BRIGHT | RENDER_CHROME2, ...)  // now VBO
RenderMesh(GroupId, RENDER_BRIGHT  | RENDER_CHROME,  ..., BITMAP_CHROME)   // still CPU
RenderMesh(GroupId, RENDER_BRIGHT  | RENDER_METAL,   ..., BITMAP_SHINY)    // still CPU
```

The chrome/metal passes set `renderFlags` to `RENDER_CHROME` / `RENDER_METAL`,
so `IsVboMaterialEligible` (which demands `RENDER_TEXTURE`) rejects them.

## The fix: teach Model.vs the chrome texcoord modes

Every `g_chrome` formula (`ZzzBMD.cpp` ~1762-1819) is a **pure function of the
transformed normal** plus two time scalars and two direction vectors — all of
which the vertex shader either already has or can take as a uniform. Nothing
needs the CPU.

`Wave = ((int)WorldTime % 10000) * 0.0001`, `Wave2 = ((int)WorldTime % 5000) *
0.00024 - 0.4`, `L = (cos(WorldTime*0.001), sin(WorldTime*0.002), 1.0)`,
`LightVector` is the existing scene light direction.

| mode | u | v |
|---|---|---|
| default (`RENDER_METAL` etc.) | `N.z*0.5 + 0.2` | `N.y*0.5 + 0.5` |
| `RENDER_CHROME` | `N.z*0.5 + Wave` | `N.y*0.5 + Wave*2` |
| `RENDER_CHROME2` | `(N.z+N.x)*0.8 + Wave2*2` | `(N.y+N.x)*1.0 + Wave2*3` |
| `RENDER_CHROME3` | `dot(N, LightVector)` | `1 - dot(N, LightVector)` |
| `RENDER_CHROME4` | `dot(N,L) + N.y*0.5 + L.y*3` | `1-dot(N,L) - (N.z*0.5 + Wave*3)` |
| `RENDER_CHROME5` | `dot(N,L) + N.y*3 + L.y*5` | `1-dot(N,L) - (N.z*2.5 + Wave*1)` |
| `RENDER_CHROME6` | `(N.z+N.x)*0.8 + Wave2*2` | same as u |
| `RENDER_CHROME7` | `(N.z+N.x)*0.8 + WorldTime*0.00006` | same as u |
| `RENDER_CHROME8`, `RENDER_OIL` | `N.x` | `N.y` |

Then the per-vertex application from the legacy build loop:

- `RENDER_CHROME` -> texcoord = chrome
- `RENDER_CHROME4` / `RENDER_CHROME8` -> texcoord = chrome + `BlendMeshTexCoord`
- `RENDER_OIL` -> texcoord = `chrome * aTex + BlendMeshTexCoord`

### Implementation shape

1. `Model.vs`: add `uniform int u_texCoordMode;` plus `u_wave`, `u_wave2`,
   `u_L`, `u_lightVector`, `u_blendMeshTexCoord`. Compute `vTex` from the table
   above instead of `aTex` when the mode is non-zero. Geometry and skinning are
   untouched, so positions stay bit-identical to the base pass — which is the
   entire point.
2. `ZzzBMD.cpp`: pass the resolved mode + scalars to `RenderMeshVBO`; relax
   `IsVboMaterialEligible` to accept the chrome/metal/oil `renderFlags` once the
   shader supports them.
3. Verify per material in the Release run, then measure.

Do them one material at a time, each with a before/after `RenderProfiler.log`.

## Audit debt the user explicitly asked to clear (no masking)

1. **`Model.vs` line ~77**:
   `color.rgb = clamp(color.rgb, 0.0, 1.0) * (u_translate != 0 ? 1.0 : 0.85);`
   The `0.85` has **no counterpart in the CPU path** (`ZzzBMD.cpp:437` is only
   `dot(N,L)*0.8 + 0.4`, floored at `0.2`). World-object VBO lighting therefore
   never matched legacy and was eyeballed into place. Removing it brightens world
   objects ~18%, so it needs its own verified slice — but it must be removed and
   the real difference found, not kept.
2. **`u_translate` does double duty** — it selects the body transform AND the
   brightness trim above. Two unrelated concerns on one uniform; split them.
3. **`ApplyBoneNormal` normalizes, the CPU `VectorRotate` does not.** Harmless
   for orthonormal bone matrices but it is still a divergence between the two
   implementations of the same operation.

## Marker files (the protection wrapper drops env vars AND the command line)

`renderprofiler.enable`, `gl33char.disable`, `gl33effect.enable`,
`vbotranslate.on`. The plain-text profiler report is `Client\RenderProfiler.log`,
truncated per launch, readable while the client runs.
