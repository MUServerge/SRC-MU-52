# FPS timing consumer audit — 2026-07-16

## Scope

This audit covers the client translation units selected by `Main.vcxproj` and the
timing helpers they call. It follows FPS Phases 1–3 and is the Phase 4 migration
map. It does not claim a runtime performance improvement; Windows build and
runtime cadence measurements remain pending.

## Existing authorities

- `csteady_clock` owns frame pacing and the bounded 25 Hz compatibility
  scheduler introduced in Phase 3.
- `WorldTime` is absolute elapsed time used primarily for visual oscillation and
  must remain independent of render-frame count.
- `timefac` and `timeNormalizer` scale continuous movement/animation by elapsed
  time. They are not fixed-step consumers.
- `CheckNormalizer` represents a legacy discrete 40 ms pulse. Its consumers must
  be migrated individually because random emission, lifetime and counter
  semantics differ.
- `standlimit`, `MoveSceneFrame`, `frame_scene_desplace` and `MacroTime`
  mix compatibility counters with render-frame state and require owner-specific
  review before replacement.

## Inventory summary

| Symbol/domain | Active source files | Observed uses | Classification |
| --- | ---: | ---: | --- |
| `CheckNormalizer` | 10 | 23 | Remaining discrete 25 Hz compatibility pulse |
| `timefac` | 35 | 1772 | Continuous elapsed-time scaling |
| `timeNormalizer` | 3 | 4 | Continuous normalization helper |
| `standlimit` | 6 | 55 | Mixed lifetime/modulo compatibility |
| `fps_new_system` | 3 source files plus one disabled define | 8 | Dormant experiment; removed in Phase 4A |
| `GetLimitFps` | 4 | 14 | Pacing/scheduler target access |
| `MoveSceneFrame` | 8 | 23 | Shared scene compatibility counter |
| `frame_scene_desplace` | 2 | project-local | Scene displacement compatibility |
| `MacroTime` | 2 | project-local | Render-frame counter |

The `CheckNormalizer` call sites are in pet, boid, minimap/UI and effect
specializations, including `CSPetSystem.cpp`, `GMHellas.cpp`, `GOBoid.cpp`,
`NewUIMiniMap.cpp`, `ZzzEffect.cpp`,
`ZzzEffectBlurSpark.cpp`, `ZzzEffectFireLeave.cpp`,
`ZzzEffectJoint.cpp` and `ZzzEffectParticle.cpp`. A commented
`GMEmpireGuardian4.cpp` reference is not an active consumer.

## Phase 4A change

The disabled `fps_new_system` path had no distinct behavior:

- both `CSParts.cpp` conditional branches called `PlayAnimation` with identical
  arguments;
- `w_BasePet.cpp` and `ZzzCharacter.cpp` assigned each local play-speed value
  to itself;
- the feature define was already commented out.

### Phase 4B-1: deterministic UI counters

The text-input caret and slide-notice blink counters in `UIControls.cpp` now
advance by `GetFixedUpdateStepCount()` instead of sampling the lossy
`CheckNormalizer` boolean. At the authored 25 Hz cadence both advance one unit
per step as before; at higher render rates they remain tied to 40 ms updates, and
after a bounded stall they consume the scheduler's recovered steps instead of
silently losing UI time. These counters affect presentation only.

`NewUIMiniMap.cpp` is explicitly not part of this UI group: its pulse gates
`Hero->Movement`, `SendMove` and auto-movement notice creation. It is a
gameplay/network-adjacent consumer and requires separate packet/cadence review.

Phase 4A removes only those duplicate/no-op branches and the obsolete commented
define. The surviving statements are the exact calls previously compiled by the
default path. No animation formula, velocity, order or ownership changes.

## Required separation

Do not globally replace these systems:

1. `WorldTime` visual phases must remain absolute-time based.
2. `timefac`/ `timeNormalizer` consumers must remain continuous unless a
   specific owner is proven to model discrete simulation.
3. `CheckNormalizer` random emitters cannot blindly loop for every recovered
   step: doing so changes RNG consumption and burst density after a stall.
4. `standlimit` modulo/lifetime users need their owning counter audited before
   conversion.
5. `MoveSceneFrame` is shared by gameplay presentation, physics and effects;
   changing it globally can double-normalize already delta-scaled code.

## Planned Phase 4B order

1. Completed: migrate deterministic caret/notice blink counters in `UIControls.cpp`.
2. Review minimap auto-movement separately; it has gameplay/network authority.
3. Migrate deterministic effect lifetime and blur consumers while preserving
   emitted counts and visible duration.
4. Migrate random pet/boid/Hellas emitters only with explicit stall and RNG rules.
5. Review `standlimit` and `MoveSceneFrame` owners last.
6. Remove a legacy helper only after active calls, callbacks, macros and runtime
   lookups are all proven absent.

Each group is a separate rollback point and requires a Release Win32 build plus
60/120 FPS visual/cadence comparison before performance conclusions.

## Runtime acceptance checks

At both 60 and 120 FPS, compare login/selection, empty and crowded maps,
pets/boids, minimap/UI animation, effect-heavy combat, water/grass, map
transition and reconnect. Record fixed steps and dropped steps, frame-time
median/p95/p99, effect lifetime/emission counts and animation duration. Gameplay,
packet cadence, movement speed and visual density must remain equivalent.
