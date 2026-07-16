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
| `CheckNormalizer` | 4 | 8 | Remaining by design: gameplay, network, tail state machine, dead code |
| `timefac` | 35 | 1772 | Continuous elapsed-time scaling |
| `timeNormalizer` | 3 | 4 | Continuous normalization helper |
| `standlimit` | 6 | 55 | Mixed lifetime/modulo compatibility |
| `fps_new_system` | 3 source files plus one disabled define | 8 | Dormant experiment; removed in Phase 4A |
| `GetLimitFps` | 4 | 14 | Pacing/scheduler target access |
| `MoveSceneFrame` | 8 | 23 | Shared scene compatibility counter |
| `frame_scene_desplace` | 2 | project-local | Scene displacement compatibility |
| `MacroTime` | 2 | project-local | Render-frame counter |

After Phase 4B-5, the deterministic and visual-emission consumers are all
migrated to the scheduler. The four remaining `CheckNormalizer` sites are
retained on the legacy pulse **by design**, each for a specific reason:

- `ZzzEffect.cpp:9855` — gameplay: gates `AttackCharacterRange()`; a cadence
  change would alter client-side combat timing and needs runtime validation.
- `NewUIMiniMap.cpp:382` — network: gates `Hero->Movement` and `SendMove`;
  its cadence is packet-facing and must not change without a protocol review.
- `ZzzEffectJoint.cpp:6726/6769/6774/6848` — the `BITMAP_FLARE_FORCE`
  tail-generation state machine (`MultiUse`/`Weapon`/`MaxTails`); it needs a
  dedicated fixed-step update stage, not a per-counter gate swap.
- `ZzzEffectBlurSpark.cpp:113/287` — `MoveBlurs()`/`MoveObjectBlurs()` have no
  active caller (dead/dormant); migrating them would have no runtime effect.

`CheckNormalizer` is already FPS-independent (a 40 ms time gate), so these four
sites keep their exact current 25 Hz cadence. A commented `GMEmpireGuardian4.cpp`
reference is not an active consumer.

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

### Phase 4B-2: deterministic weather counters

`MoveLeaves()` is an active scene-update consumer. Its rain-intensity convergence
and rain-position wrap now consume every bounded 25 Hz scheduler step. The
per-step operations remain identical: `RainCurrent` moves one unit toward
`RainTarget`, while `RainPosition` advances by 20 and wraps at 2000. No random
number generation, leaf allocation, per-frame movement, `WorldTime` oscillation
or map selection changed.

The blur ageing candidates were not edited. `MoveBlurs()` and
`MoveObjectBlurs()` are defined and declared, but repository-wide symbol search
found no active caller while `RenderBlurs()` is called by the scene. That is a
dormant or incomplete lifecycle path, not a safe optimization target. It needs a
separate decision to reconnect or remove it after Windows runtime evidence.

### Phase 4B-3: non-burst synchronized visual emission

`csteady_clock::ShouldRunFixedVisualEmission()` is the explicit policy for
render-called visual emission: it returns true when the current render frame
contains at least one fixed update step, but never asks a caller to replay
multiple emissions after a stall. `CreateEffectSync()`, `CreateJointSync()`
and `CreateParticleSync()` now use this policy instead of `CheckNormalizer`.
At 25 Hz they remain enabled every authored step; at 60/120 FPS they receive
approximately 25 emission opportunities per second; a recovered multi-step frame
still provides one opportunity, preventing a particle/joint burst.

The direct `BITMAP_FLARE_FORCE` counters remain unchanged. `MultiUse`,
`Weapon` and `MaxTails` participate in a next-frame tail-generation state
machine, so replacing each boolean increment independently would either burst
tail creation or delay a state transition. That path requires a dedicated
fixed-step update stage rather than arithmetic substitution.

### Phase 4B-4: deterministic pet fly-effect spark

`CSPetSystem.cpp` emits a purely visual spark for flying pets
(`CreateParticle(BITMAP_SPARK + 1, ...)`) once per legacy pulse, gated by
`!eBuff_Cloaking && CheckNormalizer`. That gate is now
`!eBuff_Cloaking && ShouldRunFixedVisualEmission()`, the same non-burst policy
as Phase 4B-3: one emission opportunity per render frame, never replayed as a
burst after a stall. The body creates only client visual particles.

### Phase 4B-5: ambient visual emitters

The remaining purely visual emission gates now use
`ShouldRunFixedVisualEmission()` instead of `CheckNormalizer`. Each is a boolean
cadence-source swap only — one opportunity per render frame at the same ~25 Hz,
never a `for`-loop over recovered steps, so no burst and no extra RNG draw:

- `GMHellas.cpp:679` — event big-monster gravity flip (ambient motion).
- `GOBoid.cpp:898/1479/1556/1977` — decorative boid flight change, event fire
  effect, and ambient dragon/fish spawns (client Boids/Fishs arrays only).
- `ZzzEffect.cpp:8213` — `CreateBomb3` visual burst on a summon animation frame.
- `ZzzEffectJoint.cpp:7164` — bone-trail joint effect during an animation window.

In every case the guard's other terms (`rand() % N == 0`, animation-frame and
`!o->Live` checks) are unchanged, and any `rand()` calls stay in the same
positions and run at the same per-frame rate, so RNG consumption and emission
density are preserved. This completes the visual-emission migration; the four
sites listed under "Inventory summary" are retained on the legacy pulse by
design.

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

1. Completed (4B-1): deterministic caret/notice blink counters in `UIControls.cpp`.
2. Completed (4B-2): rain intensity/position counters in `MoveLeaves()`.
3. Completed (4B-3): the three Sync wrappers, non-burst emission gate.
4. Completed (4B-4): deterministic pet fly-effect spark gate in `CSPetSystem.cpp`.
5. Completed (4B-5): ambient visual emitters in `GMHellas.cpp`, `GOBoid.cpp`,
   `ZzzEffect.cpp` (`CreateBomb3`) and `ZzzEffectJoint.cpp` (bone-trail).

Retained on the legacy pulse **by design** (not further migrated in this pass):

6. `NewUIMiniMap.cpp` auto-movement — gameplay/network authority; needs a packet
   review before any cadence change.
7. `ZzzEffect.cpp:9855` combat `AttackCharacterRange` — gameplay; needs runtime
   validation before a cadence change.
8. `ZzzEffectJoint` tail-generation counters — need a dedicated fixed-step state
   stage, not a per-counter gate swap.
9. `ZzzEffectBlurSpark` blur-move — dead/dormant (no active caller); decide
   reconnect vs removal separately.
10. `standlimit` / `MoveSceneFrame` owners and any legacy-helper removal remain
    future work, only after active calls, callbacks and macros are proven absent.

Each group is a separate rollback point and requires a Release Win32 build plus
60/120 FPS visual/cadence comparison before performance conclusions. Because
`CheckNormalizer` is already an FPS-independent 40 ms time gate, the four retained
sites keep their exact current behavior; migrating them offers no cadence benefit
and only gameplay/network/state risk, so they are intentionally left in place.

## Runtime acceptance checks

At both 60 and 120 FPS, compare login/selection, empty and crowded maps,
pets/boids, minimap/UI animation, effect-heavy combat, water/grass, map
transition and reconnect. Record fixed steps and dropped steps, frame-time
median/p95/p99, effect lifetime/emission counts and animation duration. Gameplay,
packet cadence, movement speed and visual density must remain equivalent.
