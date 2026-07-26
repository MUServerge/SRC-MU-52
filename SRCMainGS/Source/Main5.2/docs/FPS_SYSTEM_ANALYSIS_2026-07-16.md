# FPS / Timing System — Full Analysis

Date: 2026-07-16
Method: read-only static analysis of `main` after the FPS Phase 1–4B work merged.
Scope: `SRCMainGS/Source/Main5.2` client. No source changed for this analysis.

This catalogs the **whole** client timing system — the core clock, the two
schedulers, every consumer family (migrated, legacy-by-design, continuous,
frame-counter), the scattered/duplicated pieces, and the commented-out or
unfinished parts — and states precisely what "FPS done" means.

## 0. Does FPS involve the Encoder / MainInfo?

**Yes — the Encoder (MainInfo) is the FPS-cap configuration source.** (An earlier
draft wrongly excluded it after only checking the `Encoder/` folder's top level;
this section is the corrected finding.)

The `Encoder/Encoder/*` source (icon `maininfo.ico`, `MemScript` parser) builds
`MainInfo/Encoder.exe` — the MainInfo config tool. It authors the packed client
config (`av-code45.pak` / `MAIN_FILE_INFO`) from `MainInfo.ini`, and that config
includes the FPS settings:

```
MainInfo.ini [Custom]
  FpsRenderMax     = 60   ; "render FPS cap"
  FpsWindowsOption = 0    ; "Prompt for FPS when entering game (On:1/Off:0)"
```

Chain, end to end:

1. `MainInfo.ini [Custom] FpsRenderMax / FpsWindowsOption` — author the values.
2. `Encoder.cpp:425-427` reads them into the packed struct:
   `info.ajust_fps_render = GetPrivateProfileInt("Custom","FpsRenderMax",60,...)`;
   `info.shutdown_popup = GetPrivateProfileInt("Custom","FpsWindowsOption",1,...)`.
3. The client decodes them into `MAIN_FILE_INFO` (`CGMProtect.h:99-100`:
   `BYTE shutdown_popup; BYTE ajust_fps_render;`).

**But the open client source never applies them.** A fixed-string search finds
no read of `gmProtect->ajust_fps_render` or `->shutdown_popup` anywhere; the only
`ajust_fps_render` hits in `GMHellas.cpp` are an unrelated *local* variable
initialised from the `frame_scene_desplace` frame counter. `GetLimitFps()`
returns a hard-coded `60`, and no "prompt for FPS" popup exists in source.

So the FPS-cap config is **plumbed but not consumed in the open source**. Either
(a) the protection/packing layer (Themida / GMProtect binary, outside this source
tree) applies `ajust_fps_render` at runtime, or (b) it is currently unwired dead
config. This must be settled by a runtime check, and it is the correct place to
wire a real, config-driven FPS cap (see §7/§8). The earlier claim that
`GetLimitFps()` is "simply hard-coded 60" is therefore incomplete: a config path
for the cap exists and reaches the client struct — it just is not read in source.

## 1. Executive summary

- The timing core is `csteady_clock` (singleton, `steady_clock.cpp/.h`), plus
  free helpers `timefac` / `timepow` / `timeNormalizer` / `standlimit` and the
  globals `WorldTime` and `DeltaT`.
- There are **two** independent schedulers: (a) the **frame pacer** (VSync vs
  software limiter, Phase 2) and (b) the **bounded 25 Hz fixed-step scheduler**
  (Phase 3). `REFERENCE_FPS = 25.0`; the render target (`GetLimitFps()`) is
  hard-coded **60**.
- Consumers split into four families: **discrete 25 Hz pulse** (`CheckNormalizer`
  — the Phase 4B migration target), **continuous elapsed-time scaling**
  (`timefac`/`timepow`/`timeNormalizer` — already FPS-independent, ~3800 uses,
  intentionally untouched), **render-frame counters** (`MoveSceneFrame`,
  `MacroTime`, `frame_scene_desplace` — still FPS-dependent, not addressed), and
  **FPS-aware RNG** (`rand_calc_check`).
- "FPS done" = the **discrete-pulse and visual-emission migration is complete**.
  It does **not** mean the whole timing system was rewritten; most of it
  (continuous scaling) was already FPS-independent and was deliberately left
  alone.
- Real scattered/duplicated architecture remains: `WorldTime` and `DeltaT` each
  have **two writers with different time bases**, several accessor names are
  redundant aliases, `GetFixedUpdateAlpha` has no consumer, and the
  `steady_clock_` helper class' role is unclear.

## 2. Timing core (`csteady_clock`)

Constants/macros (`steady_clock.h:1-6`):

| Symbol | Value / meaning |
|---|---|
| `REFERENCE_FPS` | `25.0` — MU's authored update rate; fixed step = 40 ms |
| `checkNormalizer` | macro → `gsteady_clock->CheckNormalizer()` |
| `FPS_ANIMATION_FACTOR` | macro → `GetLegacyVisualStep()` (6 uses) |
| `rand_fps_check(r)` | macro → `rand_calc_check(r)` |
| `gsteady_clock` | macro → `csteady_clock::Instance()` |

Per-frame update: `LoadInformationFps()` (`steady_clock.cpp`) runs once at the
top of `MainScene`. It computes:

- `realDeltaTime` = wall-clock seconds since the previous call (from `CTimer`);
- `WorldTime` = session-relative ms (absolute `timeGetTime` overflows float);
- `fpsNormalizer = 1/realDeltaTime`;
- `realLegacyStep = realDeltaTime * 25` (elapsed measured in 25 Hz steps);
- `speedNormalizer = min(realLegacyStep, 1.0)` (capped visual step);
- `deltaAccumulated += speedNormalizer` (running total);
- then calls `UpdateFixedUpdateScheduler()` and `normalizefps()`.

### Redundant accessors (aliasing — cleanup candidate)

The same three underlying values are exposed under six names:

| Underlying | Names |
|---|---|
| `realDeltaTime` | `GetFrameDeltaSeconds`, `GetDeltaTimeSeconds` |
| `realLegacyStep` | `GetLegacyUpdateStep`, `GetRealLegacyStep` |
| `speedNormalizer` | `GetLegacyVisualStep`, `GetVisualLegacyStep`, `GetNormalizerFps` |

## 3. Scheduler A — frame pacing (Phase 2)

`Winmain.cpp` (`WinMain`, under `V_SYNCRONIZE`) selects exactly one pacing
authority at startup:

- display ≈ 60 Hz **and** WGL swap-interval-1 succeeds → `FRAME_PACING_VSYNC`
  (the software limiter's sleep is then bypassed in `thread_sleep`);
- otherwise (higher/unknown refresh, or swap control unavailable) →
  `FRAME_PACING_SOFTWARE`, requesting swap interval 0.

`thread_sleep()` implements the software 60 FPS cap (sleep to within ~1.5 ms of
the deadline, then spin). `GetLimitFps()` returns a hard-coded **60** — there is
no exposed 120 FPS option; a 120/144 Hz display is intentionally held to the
60 FPS software cap until the fixed-step migration is trusted.

## 4. Scheduler B — bounded 25 Hz fixed-step (Phase 3)

`UpdateFixedUpdateScheduler()`: accumulates `realDeltaTime`, emits up to
**5** fixed 40 ms steps per frame, drops excess after a long stall
(spiral-of-death guard), and exposes:

- `GetFixedUpdateStepCount()` — steps this frame (0..5);
- `GetDroppedFixedUpdateStepCount()` — steps discarded after a stall;
- `ShouldRunFixedVisualEmission()` — `stepCount > 0` (one non-burst opportunity);
- `GetFixedUpdateAlpha()` — 0..1 interpolation remainder.

**Unfinished:** `GetFixedUpdateAlpha()` has **no consumer** anywhere (declared +
defined only). It was exposed "for later consumers" that were never written —
render interpolation was never wired up, so between-step motion is not smoothed.

## 5. Consumer families

### 5a. Discrete 25 Hz pulse — `CheckNormalizer` (the Phase 4B target)

`CheckNormalizer()` returns true when ≥40 ms elapsed since it last returned
true — already FPS-independent (~25/sec at any render FPS), but it never catches
up after a stall. Phase 4B migrated the consumers that benefit from stall-catch-up
or that should share one timing source:

Migrated to the scheduler (Phases 4B-1…4B-5):
- deterministic counters → `GetFixedUpdateStepCount()` (catch-up): UI caret/notice
  (`UIControls.cpp`), rain intensity/position (`ZzzEffectFireLeave.cpp`);
- visual emitters → `ShouldRunFixedVisualEmission()` (non-burst): the three Sync
  wrappers (`ZzzEffect`/`ZzzEffectJoint`/`ZzzEffectParticle`), pet spark
  (`CSPetSystem`), ambient emitters (`GMHellas`, `GOBoid`, `ZzzEffect` CreateBomb3,
  `ZzzEffectJoint` bone-trail).

Retained on the legacy pulse **by design** (4 files / 8 uses):
- `ZzzEffect.cpp:9855` — gameplay (`AttackCharacterRange`);
- `NewUIMiniMap.cpp:382` — network (`Hero->Movement`, `SendMove`);
- `ZzzEffectJoint.cpp:6726/6769/6774/6848` — tail-generation state machine
  (`MultiUse`/`Weapon`/`MaxTails`); needs a dedicated fixed-step stage;
- `ZzzEffectBlurSpark.cpp:113/287` — dormant (`MoveBlurs`/`MoveObjectBlurs`
  have no active caller).

Commented-out (not active): `ZzzEffect.cpp:13442` `//if(checkNormalizer)`;
`GMEmpireGuardian4.cpp:765` `// || checkNormalizer == false`.

### 5b. Continuous elapsed-time scaling — untouched by design

| Helper | Formula (at 60 FPS render) | Uses |
|---|---|---:|
| `timefac(x)` | `x * realLegacyStep` (x per 40 ms, scaled to this frame) | ~1773 |
| `timepow(x)` | `pow(x, realLegacyStep)` | ~1006 |
| `timeNormalizer(x)` | `x * speedNormalizer` (capped) | ~5 |
| `standlimit(x)` | `x * GetLimitFps()/25` (lifetime/modulo scaling) | ~56 |

These already scale by elapsed time, so movement/animation/fade speeds are
FPS-independent. They are the **bulk** of the timing system (~3800 uses) and were
correctly **not** migrated. `timefac`/`timepow`/`standlimit` short-circuit to
`x` when `GetLimitFps() == 25`.

### 5c. Render-frame counters — still FPS-dependent (NOT addressed)

| Symbol | Type | Where | Behavior |
|---|---|---|---|
| `MoveSceneFrame` | `int` | `ZzzScene.cpp:115`, `++` at `:2875` | per-frame counter; used as `% 6`, `% 3` etc. |
| `MacroTime` | `int` | `ZzzInterface.cpp:174` | per-frame decrement/counter (9 uses) |
| `frame_scene_desplace` | `int` | `ZzzScene.h:8` | "fps render adjust" (`GMHellas.cpp:642`) (6 uses) |

`MoveSceneFrame % N` cycles **twice as fast at 120 FPS** — genuinely
FPS-dependent. The audit (separation rule 5) flags `MoveSceneFrame` as shared by
gameplay/physics/effects and defers it. This is real remaining work, not done.

### 5d. FPS-aware RNG — `rand_calc_check` / `rand_fps_check`

`rand_calc_check(fr)` returns true with probability `(1/fr) * speedNormalizer`,
i.e. random-event probability is scaled by the visual step so event rates stay
FPS-independent. 7 call sites (`ZzzEffectJoint`, `GMDeventer`, `ZzzEffect`). This
is the intended way to make random emitters FPS-independent — distinct from the
`rand()%N && checkNormalizer` emitters migrated in Phase 4B-5.

## 6. Scattered / duplicated / conflicting architecture

1. **`WorldTime` has two writers with different time bases.**
   - `ZzzAI.cpp:28` defines it; `ZzzAI.cpp:869` sets `WorldTime = timeGetTime()`
     (absolute ms since OS boot).
   - `steady_clock.cpp:216` sets `WorldTime = current_time` (session-relative ms).
   Whichever runs last per frame wins; the two bases differ by the process start
   offset. ~1014 reads depend on it. This is a real latent inconsistency.

2. **`DeltaT` has two writers with different sources.**
   - `ZzzAI.cpp:27` defines it; `ZzzAI.cpp:885` sets it from `clock()/CLOCKS_PER_SEC`.
   - `steady_clock.cpp` sets `DeltaT = realDeltaTime`.
   ~100 reads; the value depends on call order between the AI update and the
   frame clock.

3. **Redundant clock accessors** — six names for three values (see §2).

4. **`steady_clock_` class** (`steady_clock.h:78-218`) — an elaborate
   operator-overloaded counter (`_runvalue`/`_runvalueback`, `numeral`,
   `duration`, `residual_duration`, `factor_res`). No member of this type was
   found in active use in this pass; its role vs the plain-`int` `MoveSceneFrame`
   is unclear (possibly experimental/legacy). Needs a usage trace before any
   cleanup.

5. **Two `MainScene` timing scaffolds already removed** (Phase 3/4A): the
   commented `while (accumulatedTime …)` loop, the render-FPS-sized pseudo fixed
   delta, the unused post-frame accumulator, and the `fps_new_system` experiment.

## 7. What is actually finished vs pending

**Finished and on `main`:**
- Phase 1 — corrected software pacer (whole-frame deadline).
- Phase 2 — single pacing authority (VSync vs software), hardened WGL discovery.
- Phase 3 — bounded 25 Hz fixed-step scheduler + opt-in RenderProfiler counters.
- Phase 4A — timing-consumer audit; removed `fps_new_system` dead code.
- Phase 4B-1…4B-5 — all deterministic counters and visual emitters migrated.
- The CP949 source-encoding damage from the 4A/4B commits was repaired.

**Pending / not done (by design or by dependency):**
- Render **interpolation** using `GetFixedUpdateAlpha()` — exposed, no consumer;
  high-FPS motion between fixed steps is not smoothed.
- **Joint tail-generation state machine** (`MultiUse`/`Weapon`/`MaxTails`) — needs
  a dedicated fixed-step stage.
- **`MoveSceneFrame` / `MacroTime` / `frame_scene_desplace`** — FPS-dependent
  frame counters, not migrated.
- **Gameplay/network pulse** (`AttackCharacterRange`, minimap `SendMove`) —
  intentionally on legacy; needs runtime/packet validation before any change.
- **Dormant blur** (`MoveBlurs`/`MoveObjectBlurs`) — decide reconnect vs remove.
- **`WorldTime` / `DeltaT` dual-writer** cleanup — pick one authority per global.
- **Config-driven FPS cap** — the `FpsRenderMax` chain reaches the client struct
  (`MAIN_FILE_INFO::ajust_fps_render`) but the open source never reads it;
  `GetLimitFps()` returns hard-coded 60 (see §0). Wiring `GetLimitFps()` to the
  decoded `ajust_fps_render` (with a clamp and legacy fallback) is the intended
  way to make the cap configurable, and the prerequisite for a real 120 mode.
- **Actual 120 FPS enablement** — gated on the config-cap wiring above plus
  interpolation + the frame-counter migration + runtime validation.
- **Runtime validation** — no Windows `Release|Win32` build or 60/120 FPS
  cadence/visual-parity run has been performed; all conclusions here are static.

## 8. Recommended next steps (if the FPS track continues)

1. Pick a single authority for `WorldTime` and `DeltaT`; remove the second writer.
2. Wire `GetFixedUpdateAlpha()` into render interpolation (the prerequisite for a
   real 120 FPS mode) behind a flag, with a legacy path.
3. Design the joint tail-generation fixed-step stage (the one deferred emitter).
4. Audit `MoveSceneFrame`/`MacroTime` owners; convert periodic `% N` checks to a
   time-based cadence where they are visual, leave gameplay-owned ones.
5. Only then consider raising `GetLimitFps()` above 60, gated and validated.
