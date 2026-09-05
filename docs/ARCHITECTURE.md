# SRC-MU-52 Architecture Direction

This document records the durable architectural direction for SRC-MU-52. The root
`AGENTS.md` remains authoritative. This document does not authorize runtime changes,
compatibility breaks or removal of legacy code.

## CONFIRMED CURRENT STATE

- SRC-MU-52 is the canonical Season 5.2 client and server source tree. The client is
  Windows C++ and remains Win32/x86.
- The project is a long-running legacy system. Stability, compatibility and reuse take
  priority over rewriting or uncontrolled modernization.
- The required discovery order is search, reuse, extend, then write new code only when
  the repository has no suitable implementation.
- Source, binary, packet, file-format, gameplay, timing, rendering and lifecycle
  contracts must be preserved unless an explicitly approved change requires otherwise.
- Timing already has `csteady_clock` as a shared source for render delta, legacy timing
  factors, a bounded 25 Hz step count, frame pacing and related diagnostics. Important
  scene updates are still executed from the render-driven `MainScene` path, and direct
  operating-system timers remain in multiple domains.
- Rendering remains a compatibility-oriented OpenGL architecture with widespread
  fixed-function code. Active shader program binding is coordinated through
  `CShaderScene`; BMD GPU resources have explicit release code; the Model shader path
  retains UBO, uniform-array and legacy rendering fallbacks.
- `RenderProfiler` is the existing renderer diagnostics/profiling implementation.
- Detailed verified renderer and performance facts belong in the matching local skill
  references rather than being duplicated here.

## TARGET ARCHITECTURE

The long-term principle is:

> One responsibility has one final authoritative owner.

This is an ownership rule, not an instruction to create a new `Manager` class. Existing
owners must be found and understood before any new abstraction is proposed.

Conceptual responsibility boundaries are:

- **Platform:** Windows handles, OpenGL context, input, timing, threads, filesystem and
  operating-system primitives.
- **Graphics:** GPU resources, shader lifecycle, textures, render passes, GL state,
  models, terrain, effects, post-processing and capability fallbacks.
- **Simulation/game domain:** characters, objects, skills, maps, gameplay state,
  animation decisions and scene orchestration.
- **UI:** windows, layout, presentation input, tooltips and view state, without
  duplicating gameplay rules.
- **Network/data:** protocol serialization, validation, persistence boundaries, file
  formats, loaders, configuration and version compatibility.
- **Utilities:** small dependency-light helpers with a specific responsibility; no
  generic dumping grounds.

The agreed priority order is:

1. Project governance and architecture baseline.
2. Full architecture and technical-debt audit.
3. Timing and genuinely smooth 60+ FPS without gameplay-speed changes.
4. Renderer and OpenGL ownership cleanup.
5. Shader consolidation.
6. Glow, Bloom and PostProcess ownership.
7. Further OpenGL modernization where evidence justifies it.

Files and classes are not to be moved or created merely to resemble these conceptual
boundaries. Boundaries should emerge incrementally where verified work touches code.

### Graphics modernization reference

Bless Reforged is the primary external architectural reference for future Shader,
OpenGL, Renderer and PostProcess work. The current SRC-MU-52 graphics implementation is
the migration starting point and compatibility baseline; it is not automatically the
desired final architecture.

Future graphics audits should study and, where they fit MU's verified needs, adapt
Bless-style concepts such as:

- explicit shader/program ownership;
- explicit render passes and depth/shadow passes;
- framebuffer-based rendering;
- a coherent postprocess pipeline with bright-pass, blur and combine stages;
- explicit GPU resource lifetime and ownership;
- instancing and batching strategies;
- modern buffer-management strategies; and
- reduced redundant OpenGL state changes.

Bless is a direction and comparison model, not an implementation specification. Before
adopting any concept, map it to SRC-MU-52's current owners, render order, GL state,
assets, fallback paths and Win32/x86 constraints. Do not copy inferred behavior, assume
that every Bless feature belongs in MU, or bypass gameplay and visual compatibility.
Introduce each accepted concept incrementally, validate it against the established
baseline, migrate all consumers, and remove a superseded path only through the approved
removal gates.

Bless source code is not available. Demonstrated behavior, runtime logs and inspected
shader assets are evidence; any explanation of the unseen implementation remains an
inference and must be labeled accordingly. Persistent mapped buffers, indirect drawing,
compute skinning and newer OpenGL features are later-stage candidates, not immediate
requirements. They require a proven MU use case, compatible Win32/x86 support, measured
benefit and a safe incremental migration boundary before adoption.

## ACTIVE MIGRATION

An architectural replacement follows this lifecycle:

`AUDIT -> DEPENDENCY MAP -> DEFINE RESPONSIBILITY/OWNER -> TARGET ARCHITECTURE -> ADDITIVE MIGRATION -> VALIDATION -> MIGRATE ALL CONSUMERS -> REMOVE SUPERSEDED LEGACY CODE -> DOCUMENT FINAL STATE`

Additive-first is the migration strategy, not a requirement for permanent duplication.
During validation, the proven path remains available as the compatibility fallback.
After all consumers migrate, removal of the superseded path is a separate change. It is
permitted only when every removal gate in `docs/ARCHITECTURE_MIGRATIONS.md` is satisfied
and the user specifically approves that concrete removal.

Current architecture work must reuse the existing timing, shader-state, GPU-resource and
profiling owners. It must not introduce parallel FPS, shader, renderer, GL-state,
post-process or diagnostics systems without first proving that no suitable owner exists.

## UNVERIFIED / REQUIRES RUNTIME VALIDATION

- A 60 FPS render limit does not prove 60 unique simulation states per second.
- Complete 25/60/120 FPS parity for movement, attacks, skills, animation, effects,
  packets and server synchronization remains a runtime-validation concern.
- Every direct `GetTickCount`, `timeGetTime`, `QueryPerformanceCounter` and `Sleep`
  consumer must be classified by responsibility before timing consolidation.
- The safe interpolation consumers and contract for the existing fixed-step alpha are
  not yet established.
- Glow, Bloom, blur, HSL and other fullscreen-effect ownership requires a repository and
  runtime audit before a PostProcess boundary can be finalized.
- Bless Reforged is the primary external graphics architecture reference, but observed
  or inferred Bless implementation details are not confirmed SRC-MU-52 facts and require
  concept-by-concept mapping and validation. The Season 6.3 DLL project remains a
  secondary reference only.
- Renderer behavior and fallback parity across NVIDIA, AMD and Intel hardware requires
  runtime evidence where the hardware is available.

Unverified statements must remain labeled as hypotheses or validation debt. They must
not be promoted into project facts solely because they appeared in a previous chat.
