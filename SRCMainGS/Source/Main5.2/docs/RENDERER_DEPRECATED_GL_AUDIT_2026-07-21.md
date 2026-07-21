# Renderer Deprecated / Fixed-Function OpenGL Audit

Date: 2026-07-21

Scope: `SRCMainGS\Source\Main5.2\source`, `Client\Shaders`, `Client\Data\Effect\VBO`

Method: read-only static scan of the authoritative local worktree. Third-party
trees (`dependencies\ImGui`, GLEW headers) are excluded from the counts because
they are vendored and migrate as a unit, not per call site.

Build/run status: intentionally not built or run. Every count below is a source
fact; runtime behavior (which driver context is actually granted, whether a
given shader compiles on a given GPU) is identified as runtime-dependent.

Purpose: this is the **repository-wide deprecated-API audit that gates the
OpenGL 3.3 Core request**. The staged plan (see the skill workflow and the
2026-07-15 investigation) is: stabilize compatibility renderer → opt-in 3.3
Compatibility → modernize subsystems incrementally → remove fixed-function →
request Core only after this audit reaches zero Core-blocking calls on the
active render paths. This document turns "remaining fixed-function surface"
into a concrete, ordered worklist and defines the Core gate.

## 1. Executive summary

The active renderer is still a **compatibility renderer**. A 3.3 context is
available only opt-in via `-gl33compat`, and it deliberately requests the
**Compatibility** profile (`WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB`,
`Winmain.cpp` `SelectRendererContext`) precisely because the scene still depends
on fixed-function state. Requesting Core today would break the terrain, character,
effect, sprite, shadow, and most UI/3D-preview paths.

Two GLSL owners coexist:

- `CShaderScene` (`SHADER_PIPELINE`) — terrain + character programs, written as
  `#version 330 compatibility` and fed by the **legacy fixed-function draw**
  (immediate mode / client arrays). The shaders read `gl_ModelViewProjectionMatrix`,
  `gl_ModelViewMatrix`, `gl_NormalMatrix`, `gl_Vertex`, `gl_Normal`,
  `gl_MultiTexCoord0`, `gl_Color`. **These built-ins do not exist in a Core
  profile.**
- `CShaderGL` (`SHADER_VERSION_TEST`) — the BMD Model VBO / GPU-skinning path.
  Its shaders are already `#version 330 core` with explicit attributes and
  uniforms (`Client\Data\Effect\VBO\Model.vs/.fs`). This path is Core-clean but
  still opt-in/partial (world-object gate by default; `-vbotranslate` extends it
  to translated character/equipment meshes; CPU transforms remain authoritative).

As of this branch, the **ImGui backend has been migrated `opengl2` → `opengl3`**
(programmable pipeline), removing one fixed-function surface from the UI overlay.

The remaining Core blockers are concentrated in a small number of large,
central files. `ZzzOpenglUtil.cpp` is the single densest source of every
deprecated category and is the correct backbone to modernize first.

## 2. Deprecated-API inventory (client source only)

Counts are call-site hits across `.cpp`/`.h` in `source\`, excluding vendored
trees. All categories listed are **removed in a Core profile** unless noted.

### 2.1 Immediate mode (`glBegin`/`glEnd` blocks)

| Call family | Files | Hits |
|---|---:|---:|
| `glBegin` / `glEnd` | 16 | 140 |
| `glVertex2/3/4*` | 14 | 227 |
| `glColor3/4*` | 154 | 1071 |
| `glTexCoord1/2/3/4*` | 12 | 208 |
| `glNormal3*` | 2 | 5 |
| `glMultiTexCoord*` | 0 | 0 |

Note: the `glColor*` count is inflated — many calls are per-object tint values
that pair with the fixed-function pipeline but are trivially portable to a
uniform. The Core-blocking geometry emission is the `glBegin`/`glVertex`/
`glTexCoord` triples (16 files).

### 2.2 Matrix stack

| Call family | Files | Hits |
|---|---:|---:|
| `glMatrixMode` | 9 | 37 |
| `glLoadMatrix` / `glMultMatrix` / `glLoadIdentity` | 9 | 24 |
| `glPushMatrix` / `glPopMatrix` | 15 | 62 |
| `glTranslate` / `glRotate` / `glScale` | 10 | 23 |
| `glOrtho` / `glFrustum` / `gluPerspective` / `gluLookAt` | 1 | 3 |

Replacement: a CPU matrix stack (or a small helper) producing explicit
`uProj`/`uView`/`uModel` uniforms, as already done for the Model VBO path.

### 2.3 Client-side vertex arrays

| Call family | Files | Hits |
|---|---:|---:|
| `glVertexPointer` / `glColorPointer` / `glTexCoordPointer` / `glNormalPointer` | 4 | 24 |
| `glEnableClientState` / `glDisableClientState` / `glClientActiveTexture` | 4 | 46 |

These are the legacy feed for terrain and BMD. Replacement: VBO + generic
`glVertexAttribPointer` inside a VAO (the CShaderGL Model path is the reference
implementation).

### 2.4 Fixed-function state

| Call family | Files | Hits | Core status |
|---|---:|---:|---|
| `glAlphaFunc` / `GL_ALPHA_TEST` | 9 | 29 | Removed — move to shader `discard` (the scene `character.fs` already does this) |
| `glLight*` / `glMaterial*` / `GL_LIGHTING` | 0 | 0 | Already shader-side |
| `glShadeModel` | 0 | 0 | n/a |
| `glTexEnv*` | 4 | 5 | Removed — fold into fragment shader |
| `glFog*` | 2 | 9 | Removed — move to shader fog |

### 2.5 Other deprecated

| Call family | Files | Hits | Note |
|---|---:|---:|---|
| Display lists (`glNewList`/`glCallList`/`glGenLists`) | 0 | 0 | Clean |
| `glPushAttrib` / `glPopAttrib` | 0 | 0 | Clean |
| `GL_QUADS` / `GL_POLYGON` primitives | 11 | 30 | Removed in Core — convert quads to two triangles |
| Raster ops (`glRasterPos`/`glDrawPixels`/`glBitmap`) | 0 | 0 | Clean |
| `glLineStipple` / `glPolygonStipple` | 0 | 0 | Clean |

Positive finding: the codebase has **no display lists, no attrib stack, no
raster/bitmap ops, no stipple, and no fixed-function lighting/material calls**.
The Core surface is limited to immediate mode, the matrix stack, client arrays,
`GL_QUADS`, alpha test, tex-env, and fog — all with a known replacement pattern.

## 3. Subsystem hot-spots (ordered by modernization weight)

Ranked by combined immediate-mode + matrix + client-array density.

| File | Subsystem | Immediate geo | Matrix | Client arrays |
|---|---|---:|---:|---:|
| `ZzzOpenglUtil.cpp` | Core GL helpers, primitive/quad draw, camera | 126 | 30 | 27 |
| `ZzzLodTerrain.cpp` | Terrain / water / grass | 87 | — | 9 |
| `ZzzBMD.cpp` | BMD models (legacy fallback beside the VBO path) | 73 | 6 | 16 |
| `ZzzEffectNoUse.cpp` | Effects | 53 | 8 | — |
| `ZzzEffectJoint.cpp` | Joint/trail effects | 48 | 4 | 18 |
| `ZzzObject.cpp` | Object placement/transforms | 27 | 10 | — |
| `SideHair.cpp` | Hair/cloth | 24 | — | — |
| `ZzzEffectBlurSpark.cpp` | Blur/spark effects | 20 | — | — |
| `ZzzScene.cpp` | Scene/character pass setup | — | 14 | — |
| `CSWaterTerrain.cpp` | Water terrain | 12 | — | — |
| UI (`UIWindows.cpp`, `NewUI*`, `GameShop`) | 2D/3D UI + previews | — | ~75 combined | — |

`ZzzOpenglUtil.cpp` is the shared substrate (primitive drawing, camera/matrix
setup, quad helpers) that most other files call through. Modernizing its helpers
first gives every downstream subsystem a Core-ready primitive/matrix API to
adopt, instead of each file open-coding immediate mode.

## 4. Ordered migration worklist

Each phase keeps the tested legacy fallback until the new path matches materials,
lighting, alpha, and hardware behavior, and each requires Windows build +
reference-screenshot + frame-time verification before merge.

1. **Phase 12 (done, this branch)** — ImGui `opengl2 → opengl3`. Removes the
   UI-overlay fixed-function surface.
2. **Phase 13 — `ZzzOpenglUtil` matrix + primitive backbone.** Introduce a CPU
   matrix helper feeding `uProj`/`uView`/`uModel` uniforms and a small
   VBO-backed primitive/quad emitter to replace `glBegin`/`glVertex` and
   `GL_QUADS`. Keep the fixed-function versions behind the compat fallback.
3. **Phase 14 — Terrain.** Convert `ZzzLodTerrain` (ground, water, grass, alpha
   passes) from client arrays to VBO+VAO+attributes; rewrite `terrain.vs/.fs`
   from `330 compatibility` to `330 core` (explicit matrices/attributes, fog and
   alpha test folded into the shader). Highest visual-risk phase — validate per
   map, and water/grass/alpha specifically.
4. **Phase 15 — Character / BMD legacy fallback.** Rewrite `character.vs/.fs` to
   core and route the remaining legacy BMD draws through the existing VBO Model
   path; make GPU skinning the default once it covers the same materials.
5. **Phase 16 — Effects, sprites, shadow, hair.** `ZzzEffect*`, `Sprite.cpp`,
   `ShadowVolume.cpp`, `SideHair.cpp` — VBO/attribute conversion; tex-env into
   shaders.
6. **Phase 17 — UI / 3D previews.** `UIWindows`, `NewUI*`, `GameShop`,
   `NewUI3DRenderMng` matrix-stack removal.
7. **Phase 18 — Core switch.** Re-run this audit; when the active render paths
   report zero Core-blocking calls, add a `-gl33core` option requesting
   `WGL_CONTEXT_CORE_PROFILE_BIT_ARB`, and only then consider making it default.

## 5. Core-readiness gate

The 3.3 **Core** request must not be made until all of the following hold on the
active (non-fallback) render paths:

- Zero `glBegin`/`glEnd`, `glVertex*`, `glTexCoord*`, `glNormal*`, `glColor*`
  geometry emission.
- Zero matrix-stack calls (`glMatrixMode`, `glLoad/MultMatrix`, `glPush/PopMatrix`,
  `glTranslate/Rotate/Scale`, `glOrtho/Frustum`, `glu*`).
- Zero client-side arrays (`gl*Pointer`, `gl*ClientState`).
- Zero `GL_QUADS`/`GL_POLYGON`, `glAlphaFunc`/`GL_ALPHA_TEST`, `glTexEnv*`,
  `glFog*`.
- No shader uses a `compatibility` profile or any `gl_*` fixed-function built-in.
- A repository-wide re-run of this scan returns clean, and every subsystem in
  §3 has passed Windows build + reference-screenshot + frame-time verification.

Changing only the requested context version is not a migration and not a
performance optimization; the gate above is the real work.

## 6. Reproducing this audit

From `SRCMainGS\Source\Main5.2\source`, the categories in §2 were produced by
`grep -rEc` over `--include=*.cpp --include=*.h`, excluding `dependencies`,
`imgui`, and `glew`. Re-running the same patterns after each phase measures
progress against the §5 gate.
