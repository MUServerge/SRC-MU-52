# SRC-MU-52 Architecture Migration Register

This register tracks architectural migrations without turning temporary dual paths into
permanent architecture. The root `AGENTS.md` is authoritative. A register entry records
state; it does not authorize implementation or removal.

## Migration states

- **AUDIT:** current behavior, owners and invariants are being established.
- **DEPENDENCY MAP:** direct and indirect consumers, assets, configuration and teardown
  are being mapped.
- **TARGET DEFINED:** the final responsibility and authoritative owner are documented.
- **ADDITIVE MIGRATION:** a bounded replacement path exists beside the proven fallback.
- **VALIDATION:** build, runtime, compatibility and failure behavior are being compared.
- **CONSUMERS MIGRATED:** every intended consumer uses the replacement; unexplained
  legacy dependencies remain a blocker.
- **REMOVAL APPROVED:** every removal gate is satisfied and the user has approved the
  concrete deletion.
- **FINAL:** superseded code is removed, the final state is documented and verification
  evidence is recorded.
- **PAUSED/BLOCKED:** the entry states the missing evidence or decision explicitly.

## Removal gates

Superseded implementation may be removed only when all of these gates are recorded as
satisfied for the specific migration:

1. **Ownership mapped:** owner, entry paths, callers, callbacks, function pointers,
   macros, scripts, assets, configuration, initialization and teardown are known.
2. **Contracts preserved:** required source, ABI, layout, packet, file-format, gameplay,
   timing, visual, render-order and lifecycle behavior is unchanged or separately
   approved.
3. **Consumers migrated:** all intended consumers have moved, and repository-wide
   searches contain no unexplained reference to the superseded implementation.
4. **Fallback and failures tested:** supported capability fallback, initialization
   failure, reload, scene transition and shutdown cases relevant to the subsystem pass.
5. **Build and runtime evidence:** the affected solution builds in Release/x86 and the
   component is run with equivalence evidence appropriate to its risk.
6. **Rollback available:** the pre-removal state is recoverable and the removal is
   bounded.
7. **Change isolated:** removal is separate from unrelated refactoring and does not hide
   new behavior.
8. **Specific approval:** the user explicitly approves removal of the named legacy path.
9. **Final records updated:** this register, the appropriate architecture/skill reference
   and `CHANGELOG.txt` describe the final authoritative owner and remaining debt.

Missing any gate means the legacy path remains. Lack of a normal source call is not proof
of non-use; callbacks, exports, scripts, resources, feature flags and runtime lookup must
also be considered.

## CONFIRMED CURRENT STATE

| Area | Confirmed current owner/state | Evidence location |
|---|---|---|
| Project governance | Root `AGENTS.md` is authoritative; migration is additive-first and compatibility-preserving. | `AGENTS.md` |
| Timing/frame pacing | `csteady_clock` supplies render delta, legacy timing factors, fixed-step scheduling and pacing. `MainScene` remains substantially render-driven. | `steady_clock.h/.cpp`, `ZzzScene.cpp::MainScene` |
| Shader program binding | `CShaderScene` coordinates active GLSL program binding and restoration; `CShaderGL` uses it for the Model path. | `CShaderScene.cpp`, `CShaderGL.cpp` |
| BMD GPU resources | Mesh VAO/VBO/EBO handles are owned and cleared by BMD release logic while a context is available. | `ZzzBMD.cpp::BMD::Release` |
| Renderer diagnostics | `RenderProfiler` is the existing profiling/diagnostics implementation. | `RenderProfiler.h/.cpp` |
| Renderer fallback | The compatibility renderer remains authoritative when the gated Model GPU path or its bone transport is unavailable. | `CShaderGL.cpp`, `ZzzBMD.cpp::RenderMeshVBO` |

## TARGET ARCHITECTURE

| Area | Final responsibility goal | Non-negotiable constraints |
|---|---|---|
| Timing/FPS | One authoritative timing source for real time, render delta, simulation timing, pacing, interpolation inputs and timing diagnostics. | Preserve movement, attack, skill, animation and packet semantics. Do not scatter compensation multipliers. |
| OpenGL state | One synchronized owner for state that crosses render boundaries. | Preserve fixed-function compatibility and exact state restoration during migration. |
| Shader lifecycle | One coherent load, compile, link, diagnostic, bind and destruction path. | Do not introduce another shader manager; retain capability fallback until full parity. |
| GPU resources | Explicit single ownership and symmetric creation/destruction for VAO/VBO/EBO, textures, programs and framebuffer resources. | Destruction requires a valid lifecycle/context strategy and reload/shutdown testing. |
| Diagnostics | One dependency-light profiling architecture with reproducible baselines. | No performance claim without measurement; avoid hot-loop logging. |
| PostProcess | One eventual owner for Glow, Bloom, blur, HSL and fullscreen composition. | Ownership is not yet audited; no implementation is authorized by this target. |

For Shader, OpenGL, Renderer and PostProcess migrations, Bless Reforged is the primary
external architectural reference. Candidate concepts include explicit program and GPU
resource ownership, explicit render/depth/shadow passes, framebuffer-based rendering,
bright-pass/blur/combine post-processing, instancing/batching, modern buffer management
and fewer redundant GL state changes. Each candidate must first be mapped onto the
current SRC-MU-52 owners and compatibility constraints. Bless observations or inferences
do not by themselves satisfy a migration or removal gate.

Bless source code is unavailable, so observed behavior, logs and shader assets must be
recorded separately from inferred implementation. Persistent mapped buffers, indirect
drawing, compute skinning and newer OpenGL features are later-stage candidates only;
they are not prerequisites for the current compatibility-first migration.

## ACTIVE MIGRATION

| Area | State | Current migration boundary | Removal status |
|---|---|---|---|
| Governance baseline | VALIDATION | Architecture direction, lifecycle and removal gates are now repository documentation. | No runtime removal involved. |
| Timing/FPS | DEPENDENCY MAP | `docs/FPS_TIMING_ARCHITECTURE_AUDIT_2026-08-30.md` classifies the current client/server timing domains and defines the evidence required before implementation. The fixed-step scheduler has selected consumers, while the main scene remains render-driven and OS timers span multiple domains. | No timing path is approved for removal. |
| Renderer/OpenGL | ADDITIVE MIGRATION | Compatibility rendering remains the fallback; shader binding, Model GPU transport and resource ownership have bounded modernized paths. `docs/RENDERER_PERFORMANCE_AUDIT_R0_2026-08-30.md` maps the active Models/Characters ownership and T0.1 bottleneck evidence against the Bless-inspired target without treating the current VBO path as final architecture. | No renderer fallback is approved for removal. |
| Shader consolidation | ADDITIVE MIGRATION | Active program binding/compilation converges through `CShaderScene`; the gated Model path is adapted through `CShaderGL`. | Asset/code removal requires a fresh usage and packaging audit plus approval. |
| Glow/Bloom/PostProcess | AUDIT | Final framebuffer, state and effect ownership has not been mapped. Bless-style bright-pass/blur/combine is the primary external target reference, pending mapping to current MU passes and resources. | No implementation or removal is approved. |

## UNVERIFIED / REQUIRES RUNTIME VALIDATION

- Timing equivalence across 25, 60 and 120 FPS for gameplay, animation, effects and
  network-visible behavior.
- Whether and where interpolation can safely expose additional visual states without
  advancing simulation or changing server synchronization.
- Complete classification of direct operating-system timer consumers.
- Full shader/effect asset usage across runtime packaging, feature switches and indirect
  loading.
- Which Bless-style render-pass, framebuffer, batching, buffer and state-management
  concepts fit MU after mapping them to the current renderer and measuring their value.
- Renderer fallback, reload and shutdown parity across supported GPU vendors/drivers.
- Current Glow/Bloom/PostProcess pass order, framebuffer lifetime and state restoration.

## Entry template

Use this template for future migrations. Do not mark a state or gate complete without
repository or runtime evidence.

```text
Migration:
State:
Current authoritative owner:
Target authoritative owner:
Problem/evidence:
Observable behavior and compatibility invariants:
Direct and indirect consumers:
Assets/configuration/feature flags:
Initialization/teardown:
Existing implementation reused:
Migration seam and fallback:
Validation evidence:
Removal gates 1-9:
Specific removal approval:
Remaining debt:
Final documentation/changelog:
```
