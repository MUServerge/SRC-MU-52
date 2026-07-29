# Render architecture map — MU 5.2 client

Read-only study. No code changed to produce this.

Written because every optimisation attempt so far was aimed at one or two
functions and missed how they are wired together. The measurements are from
`Client\RenderProfiler.log` on a live crowded map.

---

## 1. Frame structure

`Scene()` → `MainScene()` → `RenderMainScene()` (`ZzzScene.cpp:2499`).

Pass order inside one frame, in submission order:

| # | pass | notes |
|---|---|---|
| 1 | `BeginOpengl` | world camera; `gluPerspective2` + camera rotate/translate |
| 2 | `RenderTerrain(false)` | ground, grass, water tiles |
| 3 | `RenderObjects()` | static world objects |
| 4 | `RenderEffectShadows()`, `RenderBoids()` | |
| 5 | **`RenderCharactersClient()`** | **players, NPCs, monsters + all equipment** |
| 6 | `RenderTerrain(true)` | second terrain pass (alpha/after) |
| 7 | `RenderItems`, `RenderFishs`, `RenderBugs`, `RenderLeaves`, `RenderPets`, `RenderBoids(true)`, `RenderObjects_AfterCharacter()` | |
| 8 | `RenderJoints`, `RenderEffects`, `RenderBlurs` | skill trails, effects |
| 9 | `BeginSprite()` … `EndSprite()` | `RenderSprites`, `RenderParticles`, `RenderPoints` — modelview is IDENTITY here (sprites are pre-transformed on the CPU by `CameraMatrix`) |
| 10 | `RenderAfterEffects()` | |
| 11 | water map only: a second `BeginOpengl`/`EndOpengl` block repeating 8–9 | `RenderWaterTerrain` + effects again |
| 12 | `BeginBitmap()` … `EndBitmap()` | 2D UI: `RenderInterface`, `NewUISystem`, `RenderInfomation`, `RenderCursor`. Projection is `gluOrtho2D`, modelview identity, depth test OFF |

Consequences that matter:

- The 2D UI runs under a **different projection** than the world. Anything
  feeding a shader `uProj` in the UI must use the ortho matrix, never the
  perspective one.
- Sprites/particles run under an **identity modelview**. Their world placement
  is already baked in on the CPU (`VectorTransform(Position, CameraMatrix)`).
- Effects are **blended**, so their submission order is part of the image.
  Anything that defers or reorders their draws changes the picture.

---

## 2. Where the frame time goes (measured, crowded map)

```
scene 5   FPS 41   Render 22.7 ms   Sleep 0.0   →  fully render-bound
  BMD::RenderMesh        ~1284 calls/frame
  ItemGlowPass           48 calls, 0.03 ms      → negligible
  DrawCalls              ~5800/frame
```

`Sleep = 0` and `Render` ≈ the whole frame: the client is CPU-bound submitting
mesh draws. It is **not** the frame limiter (`GetLimitFps()` is a hard 60,
`thread_sleep` sleeps to the deadline then spins), and it is **not** the item
glow as a material.

The single number that governs FPS here is **mesh draws per frame**.

---

## 3. Why a player costs so much more than a monster

A monster is normally **one** `BMD` model.

A player is a body **plus one model per equipped part**, each drawn by its own
`RenderPartObject*` call — `ZzzCharacter.cpp` has ~25 such call sites (helm,
armour, gloves, pants, boots, weapon, shield, wings, pets, marks…).

Then `RenderPartObjectEffect` (`ZzzObject.cpp:11783`) adds passes **per item
level**:

```
Level = (ItemLevel >> 3) & 15;      // then a large per-item-type switch
...
b->RenderBody(RENDER_TEXTURE, ...);                       // base
if (Level == 1) ... RenderMesh(0, RENDER_BRIGHT|RENDER_CHROME, ...)
else if (Level == 2) ... RenderMesh(1, ...)
else if (Level == 3) ... RenderMesh(1, ...); RenderMesh(2, ...)
...
b->RenderBody(RENDER_METAL, ...);
b->RenderBody(RENDER_BRIGHT|RENDER_CHROME, ...);
```

So the extra passes are a function of **item level and item type**, not of the
character count. That is exactly why "different characters count differently"
and why FPS holds on monsters but drops on geared players.

`BMD::runtime_render_level` is the same shape for socket/excellent glow: it
draws the same mesh three times (base, `RENDER_CHROME`, `RENDER_METAL`).

Observed flags in a real geared scene (`PathTrace.log`, decoded):

| RenderFlag | resolves to | count |
|---|---|---|
| `0x0044` `BRIGHT\|CHROME` | `RENDER_CHROME` | 220 |
| `0x0048` `BRIGHT\|METAL` | `RENDER_CHROME` | 195 |
| `0x1040` `BRIGHT\|CHROME4` | `RENDER_CHROME4` | 175 |

---

## 4. The mesh draw itself

`BMD::RenderMeshInternal` (`ZzzBMD.cpp`) decides per mesh between:

1. **`RenderMeshVBO`** — GPU skinning. Static bind-pose VBO built once by
   `CreateVertexBuffer`, bone matrices uploaded to a UBO. Uploads **no vertices
   per frame**. Measured ~2.9 µs/mesh.
2. **Legacy client arrays** — CPU skinning expands the mesh into shared
   `RenderArrayVertices/TexCoords/Colors` every frame, then
   `glVertexPointer`+`glDrawArrays`. Measured ~18 µs/mesh, split roughly
   10 µs CPU array building + 4 µs submission.

The VBO path is rejected per mesh by, in order: scene, object eligibility
(`Translate` / bone scale / object scale), material (`renderFlags`), lighting,
wave, missing VAO, excluded render flags. In a crowded scene the dominant
rejection is `VBOGateTranslate` — character and equipment bones are world-placed
with a translation, which the default gate refuses.

`vbotranslate.on` lifts that one gate. Measured effect on the same scene:

```
off:  Render 22.7–23.3 ms,  FPS 41,  VBO draws 0
on :  Render 16.6–18.2 ms,  FPS 51–56, VBO draws 859 (347 translated)
```

**~6 ms and ~+12 FPS from one gate.** This is the largest measured lever in the
client, and it needs no migration — the path is already `#version 330 core`.

### Why it is not simply switched on

Turning it on splits a surface across two skinning implementations:

- The **base** mesh of an item can qualify for the VBO path.
- Its **glow overlay** (`RENDER_BRIGHT|RENDER_CHROME`, `renderFlags` = chrome)
  is rejected, so it draws CPU-skinned.

GPU and CPU skinning differ by floating-point noise, so the two coplanar
surfaces z-fight, which is depth-precision dependent — the glow "breaks" at
certain camera distances, and only on items whose base qualifies. Boots looked
fine while upper armour broke for exactly this reason.

The invariant needed is *one surface, one path*. It can only be enforced in one
direction today: an overlay can follow its base, because the base draws first.
The reverse — the base knowing an overlay is coming — needs information that
does not exist at that point in the frame. Previous-frame marking is the
available answer.

---

## 5. State ownership (the recurring trap)

The renderer is ordered global state with **several independent writers**.
Three concrete cases that each caused a regression:

- **`GL_TEXTURE_2D`.** `TextureEnable` was meant to mirror it, but 21 call sites
  across `UIControls`, `UIWindows`, `NewUIMessageBox`, `Sprite`, `CameraMove`
  and `ZzzInterface` called `glEnable/glDisable` directly, **and**
  `EnableAlphaTest` / `EnableAlphaBlend*` set the mirror as a side effect. A
  mirror with that many writers cannot be trusted; `glIsEnabled` is the only
  reliable source today.
- **Current colour.** Many draws carry no colour array and consume the
  fixed-function current colour, which callers set independently of
  `BodyLight`. Substituting a uniform there turns surfaces black.
- **Program binding.** `CShaderScene::Use/Unuse` is a stack. A pass-level bind
  over draws that are still fixed-function crashed the NVIDIA driver (15.4).
  Per-draw binds are safe but cost ~2 µs each; with effects converted this way
  the crowd profile showed **22,500 program switches/frame, ~45 ms**.

Rule that follows: before moving a draw to a shader, enumerate *every* piece of
fixed-function state it depends on — texture enable, current colour, alpha ref,
blend mode — and where each is set. Not one of them.

---

## 6. What this says about priorities

1. **The lever is mesh draw cost, not effects.** 1284 mesh draws at ~18 µs is
   the frame. Item glow as a material is 0.03 ms.
2. **The fix already exists in the tree**: GPU skinning, 6× cheaper per mesh,
   measured +12 FPS. Its only blocker is the path-split z-fight above.
3. **The GL 3.3 Core migration is not on this path.** Converting a family to a
   per-draw-bind helper is pixel-correct but structurally slower than what it
   replaces, and it does not compose into batching. Terrain (single bind per
   pass) is the one shape that paid off.
4. Batching effects requires sorting by texture first — particles change texture
   almost every draw, so a naive batch flushes every primitive and gains
   nothing while adding overhead.

---

## 7. Instrumentation available

- `renderprofiler.enable` → `Client\RenderProfiler.log`, plain text, per-2s,
  readable while the client runs. Sections and counters cover frame time, mesh
  paths, VBO gates, program/texture/buffer traffic.
- The profiler enables **only** via that marker: the protection wrapper
  re-spawns `Main.exe` and drops both the environment variable and the command
  line.
