# Renderer Performance Audit R0 — Models/Characters

Date: 2026-08-30  
Scope: audit only; no renderer, shader, GL-state, asset or fallback behavior change.

## Evidence boundary

### CONFIRMED SRC-MU-52 FACTS

Current source is authoritative for ownership and behavior. The primary runtime evidence
is `Client/RenderProfiler_T01_2026-08-30_Lorencia_steady.log`, reports 48-159: scene 5,
112 stable Lorencia windows and 8,734 frames. The frozen log SHA-256 is
`9ACC6A0835B41E32A80D2F686B50D68E4C9549DA51A191A7971DEAB7AEB4F7EC`.

The 2026-07-18 renderer baseline is historical comparison evidence only. The current
Model VBO path is an incremental compatibility path, not the final architecture.

### BLESS OBSERVATION / EXTERNAL EVIDENCE

Bless Reforged source code is unavailable. The external evidence used here is limited
to the installed runtime's `MuError.log` and files under `Data/Shader`:

- the log reports a 256-instance VBO, a 51,200-matrix SSBO, a 256-command indirect
  buffer, per-lane model/shadow batch pools, an active MDI path, mega VBO/IBO ownership,
  and 2,245 eligible meshes loaded into the mega-buffer;
- `model_instanced.vert` and `shadow_instanced*.vert` use per-instance attributes,
  `gl_InstanceID` and std430 bone-matrix buffers;
- `model_instanced_depth_only.*`, `depth_prepass.*`, `shadow*` and `composition.*`
  demonstrate separately named depth, shadow and composition shader stages;
- `bone_animation.comp` and `full_skinning.comp` demonstrate compute-capable animation
  and skinning experiments using SSBO inputs/outputs;
- model, instanced, merged, depth-only and sync shader variants demonstrate an intended
  architecture broader than one per-mesh VBO draw.

File names, shader interfaces and emitted logs are observations. They do not prove the
unseen C++ scheduling, correctness, production use of every shader, synchronization
strategy or performance benefit. Those remain architectural inference.

## CONFIRMED CURRENT STATE

### Active `RP_RENDER_MODELS` call graph

`ZzzScene.cpp::RenderMainScene` enters `RP_RENDER_MODELS` twice per main-scene frame.
The profiler therefore reports two owner-level calls per frame and includes more than
players and monsters:

```text
RenderMainScene
├─ RP_RENDER_MODELS (pre-character)
│  ├─ RenderObjects
│  │  └─ RenderObject -> Calc_RenderObject -> Draw_RenderObject
│  │     ├─ BMD::Animation
│  │     ├─ BMD::Transform (CPU-required or deferred)
│  │     └─ BMD::RenderBody / direct BMD::RenderMesh calls
│  ├─ RenderEffectShadows
│  ├─ RenderBoids
│  ├─ CShaderScene::Use(Character)
│  ├─ RenderCharactersClient
│  │  └─ RenderCharacter
│  │     ├─ player: Calc_ObjectAnimation + body/equipment/wing/back-item parts
│  │     ├─ monster/NPC: RenderObject and map/event specialization hooks
│  │     ├─ RenderPartObject / RenderPartObjectEffect
│  │     ├─ selection edges, shadows and special material passes
│  │     └─ giPetManager::RenderPet
│  └─ CShaderScene::Unuse
└─ RP_RENDER_MODELS (post-character)
   ├─ RenderItems
   ├─ RenderFishs
   ├─ CGoboidManager::RenderBugs
   ├─ RenderLeaves
   ├─ PetProcess::RenderPets
   ├─ RenderBoids(after-character)
   └─ RenderObjects_AfterCharacter
```

Consequently, `RP_RENDER_MODELS` is an ordered world-object/character/attachment/item
pass family. It is not a single character renderer and cannot safely be replaced by one
batched draw without first preserving its ordering and special hooks.

### Mesh and GPU-path ownership

- `BMD::RenderBody` owns the mesh loop, hidden-mesh filtering, texture-script bright
  selection and optional texture-script shadow repeats. Direct callers also invoke
  `BMD::RenderMesh` for explicit material overlays.
- `BMD::RenderMeshInternal` owns material classification, texture/blend/depth state,
  VBO eligibility, special chrome/metal/oil/wave behavior and legacy client-array draw.
- Plain, lit, compatible meshes may call `BMD::RenderMeshVBO`. Translated,
  special-material, unlit, missing-VAO and excluded-flag cases retain legacy behavior.
- `CShaderScene` is the authoritative GLSL program-binding owner. `CShaderGL` adapts
  the Model program and owns its UBO/uniform-array bone transport.
- The current successful T0.1 path used the UBO transport. Uniform-array and legacy
  fallbacks remain required capability paths.
- Each BMD mesh owns its VAO plus vertex, normal, UV, bone and element buffers; BMD
  release clears those handles while a context is available.
- VBO mesh data is static, but bone palettes and material uniforms are submitted per
  successful mesh draw. The VBO draw binds/restores the Model program, binds/unbinds
  the VAO and buffers, disables generic attributes and restores the predecessor.
- `BMD::RenderMeshVBO` reads fixed-function model-view/projection matrices with
  `glGetFloatv`; a `RenderBody` snapshot avoids repeating the queries for every mesh in
  that body, while direct `RenderMesh` calls retain per-draw queries.

### T0.1 draw, bind and CPU results

Values below are stable-window means per frame. Global GL counters include all render
sections; BMD-specific counters are the closest current evidence for model work. The
profiler does not yet attach every global GL counter to an outer owner scope.

| Evidence | Per frame | Interpretation |
|---|---:|---|
| `RP_RENDER_MODELS` | 14.0459 ms | 69.12% of `RP_RENDER_SCENE` |
| `BMD::RenderMesh` | 9.4791 ms / 989.30 calls | dominant measured BMD CPU submission work |
| `BMD::RenderMeshLegacy` | 7.4157 ms / 499.87 calls | expensive CPU/client-array half of BMD draws |
| `BMD::RenderMeshVBO` | 1.5640 ms / 487.04 calls | cheaper half, but still per-mesh submission |
| `BMD::Transform` | 1.8823 ms / 565.75 calls | animation-derived model preparation |
| transform vertices / normals | 1.0116 / 0.8200 ms | measured CPU materialization cost |
| total / immediate draws | 6,036.54 / 2,857.64 | scene-wide draw granularity and fixed-function debt |
| VBO attempted / succeeded | 487.03 / 487.03 | no runtime rejection in the selected windows |
| translated first gate | 434.72 | largest VBO eligibility boundary |
| translated plain candidate | 213.38 | bounded candidate; not parity proof |
| translated material / unlit / no-VAO / excluded | 63.93 / 120.26 / 31.12 / 6.03 | special and compatibility dependencies |
| program requests / switches | 978.06 / 978.06 | approximately bind and restore for every VBO mesh |
| UBO bone-palette uploads | 487.03 | one palette upload per successful VBO mesh |
| uploaded bones | 1,594.80 | aggregate uploaded bone matrices |
| matrix / material uniform uploads | 533.66 / 2,922.19 | high per-mesh uniform submission volume |
| VAO binds | 974.14 | approximately bind and unbind for every VBO mesh |
| array / element buffer binds | 487.21 / 487.07 | compatibility restoration is performed per VBO mesh |
| uniform-buffer binds | 974.06 | approximately bind and restore per VBO mesh |
| texture requests / changes | 2,665.30 / 2,607.14 | only about 58 requests/frame were redundant under the current single-entry cache |
| CPU transforms required / skipped | 275.89 / 289.86 | both CPU and deferred-GPU-capable populations are substantial |
| deferred transforms materialized | 30.80 | some deferred work is later required by a legacy consumer |

The BMD subsection times total approximately 10.30 ms when transform and mesh work are
combined. They are nested in `RP_RENDER_MODELS` and other sections, so they must not be
added to the 14.0459 ms parent as separate frame cost. Their magnitude nevertheless
proves that CPU model preparation and mesh submission explain most of the measured
Models/Characters cost.

## Bottleneck comparison against the Bless-inspired target

### 1. Legacy translated and special-material mesh preparation

1. **CURRENT SRC-MU-52 FACT:** translated character/equipment and special chrome,
   metal, oil, wave, shadow and effect materials commonly require CPU-transformed
   vertices/normals and the legacy client-array draw.
2. **CURRENT PERFORMANCE EVIDENCE:** legacy BMD draws average 499.87/frame and
   7.4157 ms/frame; CPU vertex/normal transform scopes add about 1.83 ms/frame.
   Translate is the first failed VBO gate for 434.72 meshes/frame.
3. **BLESS OBSERVATION / EXTERNAL EVIDENCE:** installed shaders expose translated
   body origin/scale, per-instance bone bases, SSBO bone matrices and separate material
   variants. Logs report batched instance and bone-buffer resources.
4. **ARCHITECTURAL GAP:** MU's compatibility semantics are decided and materialized
   mesh-by-mesh on the CPU; the external direction represents those inputs as explicit
   GPU data. Bless parity for MU materials is not proven.
5. **SAFE NEAR-TERM MIGRATION OPPORTUNITY:** isolate and validate only the measured
   plain-lit translated subset, with screenshots and fallback retained. Do not include
   special materials or effects in the same change.
6. **LATER-STAGE MODERNIZATION OPPORTUNITY:** GPU-oriented skinning with per-instance
   transforms and shared bone storage after animation, attachment and effect contracts
   are mapped. Compute skinning is a later candidate, not an immediate requirement.

Classification: **A CPU-bound preparation + B draw/state-bound**, confirmed.

### 2. Per-mesh draw and state submission

1. **CURRENT SRC-MU-52 FACT:** one eligible mesh draw binds/restores a program,
   uploads a bone palette and material uniforms, binds/unbinds a VAO, resets buffers and
   disables four attributes to preserve compatibility state.
2. **CURRENT PERFORMANCE EVIDENCE:** 487.03 VBO draws cause about 978 program
   switches, 974 VAO binds, 974 UBO binds and 2,922 material uploads per frame. The
   whole scene issues 6,036 draws/frame.
3. **BLESS OBSERVATION / EXTERNAL EVIDENCE:** the runtime log reports instance VBOs,
   per-lane batch pools, indirect commands and a mega VBO/IBO; instanced shaders consume
   per-instance attributes and SSBO bone ranges.
4. **ARCHITECTURAL GAP:** MU has explicit per-mesh GPU resources but no explicit
   submission queue that groups compatible material/state work while preserving order.
   The current VBO path reduces CPU vertex work but does not solve submission scale.
5. **SAFE NEAR-TERM MIGRATION OPPORTUNITY:** first add owner-attributed counters and
   map which adjacent plain draws share program, texture, blend/depth and ordering
   requirements. Then reduce only state restoration proven redundant inside a bounded
   pass, retaining the compatibility boundary.
6. **LATER-STAGE MODERNIZATION OPPORTUNITY:** explicit material/pass lanes,
   batching/instancing and eventually multi-draw/indirect submission where hardware and
   Win32/x86 compatibility permit it.

Classification: **B draw-call/state-bound**, confirmed on CPU submission; GPU benefit
from batching remains unverified.

### 3. Bone and uniform upload granularity

1. **CURRENT SRC-MU-52 FACT:** `CShaderGL` owns UBO or uniform-array transport, but
   `RenderMeshVBO` uploads the current model's bone range for every eligible mesh draw.
2. **CURRENT PERFORMANCE EVIDENCE:** 487.03 UBO palette uploads/frame accompany
   487.03 successful VBO draws; 1,594.8 bones/frame and 533.66 matrix plus 2,922.19
   material uniform uploads are recorded.
3. **BLESS OBSERVATION / EXTERNAL EVIDENCE:** std430 shaders address shared bone
   matrices by instance/base offset, and the log reports one 51,200-matrix SSBO.
4. **ARCHITECTURAL GAP:** MU transport is explicit but mesh-granular; it lacks a
   frame/pass-owned bone arena referenced by multiple meshes of one character.
5. **SAFE NEAR-TERM MIGRATION OPPORTUNITY:** prove repeated palette uploads for meshes
   sharing one character/model and define an upload-once reuse boundary without changing
   fallback or material order.
6. **LATER-STAGE MODERNIZATION OPPORTUNITY:** SSBO-backed frame bone storage and
   instance-indexed skinning; compute-generated bone/skin data only after synchronization,
   limits and fallback are designed.

Classification: **C shader/bone-upload-bound is strongly suspected**, but isolated CPU
time for upload calls and GPU consumption is not yet measured.

### 4. Render-pass ownership and repeated special passes

1. **CURRENT SRC-MU-52 FACT:** `RP_RENDER_MODELS` is split into two broad ordered
   scopes. `RenderBody`, texture scripts, selection edges, character equipment, glow/
   chrome overlays, shadows and map hooks can redraw a body or mesh. Pass intent is
   encoded in call order and flags rather than a central pass description.
2. **CURRENT PERFORMANCE EVIDENCE:** 989.30 BMD mesh calls/frame versus 565.75 BMD
   transforms/frame proves multiple mesh submissions per transformed object, but current
   counters cannot distinguish necessary meshes from duplicate material passes or
   normalize them by visible character count.
3. **BLESS OBSERVATION / EXTERNAL EVIDENCE:** separately named depth-only, depth
   prepass, shadow, model, instanced, merged and composition shaders show explicit pass
   roles. Their exact runtime ordering is not known.
4. **ARCHITECTURAL GAP:** MU has preserved ordering but weak pass ownership and no
   machine-readable boundary for opaque, alpha, glow/material and shadow work.
5. **SAFE NEAR-TERM MIGRATION OPPORTUNITY:** document and counter-classify existing
   body, direct-mesh, shadow, alpha and special-material submissions before regrouping
   anything.
6. **LATER-STAGE MODERNIZATION OPPORTUNITY:** explicit render queues/passes with stable
   ordering rules and dedicated depth/shadow/material lanes.

Classification: **B draw/state-bound + E requires population/pass instrumentation**.

### 5. GPU-bound status

1. **CURRENT SRC-MU-52 FACT:** `RenderProfiler` measures CPU submission scopes. Swap is
   separate, but no non-blocking GPU timer query brackets the Models pass.
2. **CURRENT PERFORMANCE EVIDENCE:** Models consumes 14.0459 CPU ms/frame and legacy
   CPU work is directly measured. This proves CPU pressure, not that the GPU is idle.
3. **BLESS OBSERVATION / EXTERNAL EVIDENCE:** external logs record modern buffers and
   MDI availability but provide no comparable frame-time or GPU-query result.
4. **ARCHITECTURAL GAP:** MU lacks pass-level GPU completion evidence.
5. **SAFE NEAR-TERM MIGRATION OPPORTUNITY:** add delayed, non-blocking GPU timer-query
   instrumentation around existing owner-level passes, behind RenderProfiler.
6. **LATER-STAGE MODERNIZATION OPPORTUNITY:** pass-level CPU/GPU timeline diagnostics
   integrated with explicit render-pass ownership.

Classification: **D GPU-bound: unknown**. No GPU-bound claim is authorized by R0.

## State, allocation and synchronization findings

- Program requests equal switches in T0.1; this is not repeated binding of the same
  program. It is systematic Model-program bind/previous-program restore churn.
- Texture binding shows only about 2.2% redundant requests under the current last-bind
  counter. The larger issue is high absolute texture/state change volume, not a proven
  same-texture spam loop.
- `RenderMeshVBO` uses `glGetFloatv` for compatibility matrices. `RenderBody` caches a
  snapshot per body, but direct mesh calls can query twice per draw. These calls are
  synchronization/stall risks; R0 does not quantify their driver cost.
- Program/uniform capability `glGet*` calls in `CShaderGL`/`CShaderScene` are primarily
  initialization or cached-location work. T0.1 recorded no per-frame current-program or
  uniform-location query counters.
- BMD vertex/index buffers are created during model loading with static data; R0 found
  no per-frame `glBufferData` in the active BMD draw path. Bone UBO updates are per draw.
- Lazy cloth allocations exist in character/part render paths when cloth objects are
  first required. R0 does not prove per-frame allocation churn; allocation counters or
  repeated spawn/reload testing are required before classifying them as a steady-state
  bottleneck.
- Compatibility restoration after every VBO draw is currently a correctness boundary.
  It may be locally redundant, but removal is unsafe until outer pass state ownership is
  explicit and screenshot/state parity is demonstrated.

## Ownership map and legacy dependencies

| Responsibility | Current owner | Dependency that blocks removal/reordering |
|---|---|---|
| Models pass order | `RenderMainScene` | world/map hooks, before/after-character order |
| Character visibility | `RenderCharactersClient` | cloaking, selection, battle/event rules |
| Character/equipment assembly | `RenderCharacter`, `RenderPartObject` | class/items/wings/pets/cloth/effects |
| Object model preparation | `Calc_RenderObject`, `BMD::Animation/Transform` | OBB, CPU effect consumers, translated semantics |
| Mesh/material decision | `BMD::RenderMeshInternal` | alpha/blend/depth/chrome/wave/texture scripts |
| GPU mesh resources | each `BMD::Mesh_t` | VAO/VBO/EBO capability and lifecycle |
| Program state | `CShaderScene` | fixed-function and nested shader restoration |
| Model shader transport | `CShaderGL` | UBO, uniform-array and legacy fallbacks |
| Legacy draw | `BMD::RenderMeshInternal` client arrays | special materials and CPU-transformed data |

No legacy path is approved for removal.

## Bottleneck classification

| Class | R0 result |
|---|---|
| A. CPU-bound preparation | **Confirmed**: transform/materialization and legacy vertex assembly are material costs. |
| B. Draw-call/state-bound | **Confirmed**: near-1,000 BMD mesh calls and per-mesh bind/restore behavior. |
| C. Shader/bone upload-bound | **Suspected/high confidence**: one palette upload per VBO mesh, but isolated time is missing. |
| D. GPU-bound | **Unknown**: no GPU timer evidence. |
| E. Requires runtime instrumentation | visible character/model counts, pass categories, outer-scope GL attribution and GPU time. |

## Ranked R1 candidates — not authorized for implementation

| Rank | Candidate | Risk | Required validation |
|---:|---|---|---|
| 1 | Attribute draw/program/texture/buffer counters to `RP_RENDER_MODELS`; add visible character, rendered body, direct-mesh, material and repeated-pass aggregate counts. | Low, diagnostics only | disabled-path cost review; same T0.1 Lorencia capture plus empty/crowded comparison |
| 2 | Add delayed GPU timer queries around existing Models/World/Effects/UI scopes. | Medium | no blocking result reads; driver support/fallback; CPU overhead and query latency |
| 3 | Upload one compatible bone palette per rendered character/model and reuse it across that model's meshes. | Medium-high | equipment/attachment ownership, UBO offsets/limits, animation and screenshot parity, all fallbacks |
| 4 | Reduce Model program/VAO/buffer restoration within a bounded explicit compatibility pass. | High | full GL-state snapshot/parity across terrain, effects, UI, inventory previews and fallback |
| 5 | Extend GPU handling only to the measured plain-lit translated subset. | High | character/equipment/set-effect screenshots, animation parity, map/event matrix, A/B frame time |
| 6 | Build explicit opaque/alpha/special/shadow submission lanes while retaining original order constraints. | Very high | complete pass map, transparency/sorting/effect parity, selection/shadow/map hooks |
| 7 | Batch or instance proven-compatible repeated meshes/materials. | Very high | stable sort keys, texture/material compatibility, attachment transforms, culling and visual parity |
| 8 | SSBO bone arena and modern multi-draw/indirect submission. | Very high/later stage | GL capability matrix, x86 limits, synchronization, rollback, GPU/CPU measurements |
| 9 | Compute animation/skinning. | Experimental/later stage | exact legacy animation math, hierarchy/action blending, readback avoidance, synchronization and fallback |

Candidates 8-9 are Bless-aligned later-stage architecture, not prerequisites for R1.
Candidate 5 is not the target architecture by itself; it is only a potential incremental
migration seam toward broader explicit GPU ownership.

## UNVERIFIED / REQUIRES RUNTIME VALIDATION

- Exact visible player, monster/NPC, pet, item, world-object and rendered-BMD counts for
  the T0.1 window.
- Exact `RP_RENDER_MODELS` attribution of global GL draw/state counters.
- Necessary base-mesh count versus material/glow/shadow/selection repeat count.
- CPU time specifically spent in bone uploads, uniform uploads, binds and `glGetFloatv`.
- GPU Models pass time, GPU bubbles/stalls and CPU/GPU overlap.
- Whether adjacent submissions can be reordered or batched without changing alpha,
  glow, depth, selection, shadow, attachment or event behavior.
- Whether every observed Bless shader path is active in production. Shader presence and
  log messages do not prove complete runtime use or suitability for MU.

R0 stops at audit and ranking. It does not authorize R1 or removal of any fallback.
