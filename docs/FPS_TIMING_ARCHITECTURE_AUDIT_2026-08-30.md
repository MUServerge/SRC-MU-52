# Timing / FPS Architecture and Consumer Audit

Date: 2026-08-30  
Scope: repository-backed audit only; no runtime implementation change  
Authority: current `SRCMainGS` source. The historical 2026-07-18
`docs/FPS_TIMING_AUDIT.md` workflow was consulted, but it contains no consumer-level
facts and does not override current source.

## Executive conclusion

The client already has the correct starting owner: `csteady_clock`. It should be evolved,
not replaced by another `FPSCore` or parallel timing manager. It currently owns elapsed
frame time, the 60 FPS target, frame-pacing policy, legacy 25 FPS scaling, a bounded 25 Hz
scheduler, fixed-step alpha, FPS sampling and ping measurement.

It is not yet the authoritative simulation loop. `MainScene` calls the selected scene
move function, physics, notices and several counters once per rendered frame. Most legacy
visual motion is compensated by `timefac`, but some counters remain raw per-frame and the
fixed scheduler has only a small set of direct consumers. Therefore 60 rendered frames do
not prove 60 unique simulation states, and moving the whole scene graph into a fixed loop
would be unsafe.

"One authoritative timing system" means one client owner for frame/simulation timing and
explicit domain APIs. It does not mean forcing wall-clock UI deadlines, protocol timeouts,
server gameplay timers or profiler measurement through the simulation cadence. The
GameServer remains authoritative for combat, movement acceptance, effects and other
gameplay deadlines in its own process.

## Audit method and inventory

Searches covered client and service C/C++ sources for direct OS timers, central helpers,
fixed-step consumers, frame counters and known animation/movement/combat paths.

| Source tree | `GetTickCount` | `timeGetTime` | `QueryPerformanceCounter` | `Sleep` |
|---|---:|---:|---:|---:|
| Client `Main5.2/source` | 97 | 198 | 12 | 12 |
| GameServer | 519 | 4 | 4 | 6 |
| ConnectServer | 11 | 0 | 0 | 0 |
| DataServer | 7 | 0 | 0 | 0 |
| JoinServer | 14 | 0 | 0 | 0 |

Client helper use is also broad: approximately 1,773 `timefac(...)` calls, concentrated
in particles (848), effects (519), joints (195), boids (42) and characters (34).
Direct fixed-step count has only four production consumer call sites (six symbol
occurrences including its declaration and definition); non-burst fixed visual emission
has eleven production consumers (thirteen symbol occurrences including declaration and
definition). `GetFixedUpdateAlpha()` has no production consumer beyond its own
definition/declaration.

These counts are discovery evidence. A literal `25` is not automatically an FPS
dependency: many matches are item IDs, percentages, array counts, animation key indices
or geometry constants. Only timing-context matches belong in this audit.

## 1. CONFIRMED CURRENT TIMING ARCHITECTURE

### Client clock and legacy adapters

`steady_clock.h/.cpp::csteady_clock` owns:

- `REFERENCE_FPS = 25.0`;
- session-relative `WorldTime` derived from `CTimer`;
- real elapsed `realDeltaTime` / `DeltaT`;
- `realLegacyStep = realDeltaTime * 25`;
- capped visual step `speedNormalizer = min(realLegacyStep, 1.0)`;
- a 60 FPS target from `GetLimitFps()`;
- software pacing and VSync pacing modes;
- a 25 Hz accumulator with at most five steps per rendered frame;
- dropped-step count and accumulator alpha;
- compatibility helpers `timefac`, `timepow`, `timeNormalizer`, `standlimit`,
  `checkNormalizer` and probability scaling;
- FPS sampling plus main-scene client ping and local skill-delay decrement.

The helper semantics are not interchangeable:

- `timefac(x)` uses uncapped real elapsed legacy steps and can catch up after a long
  frame.
- `timeNormalizer(x)` and `FPS_ANIMATION_FACTOR` use the step capped at 1.0 and therefore
  deliberately avoid a visual catch-up jump.
- fixed update count can run 0..5 steps and discards excess accumulated time.
- `ShouldRunFixedVisualEmission()` converts any positive step count into one emission
  opportunity, intentionally preventing burst replay.
- `checkNormalizer` is a legacy 40 ms opportunity gate, not the fixed-step scheduler.

### Frame loop and pacing

`ZzzScene.cpp::MainScene` performs, in order:

1. capture frame start;
2. `LoadInformationFps()` and profiler begin;
3. input and one scene move (`NewMoveLogInScene`, `NewMoveCharacterScene` or
   `MoveMainScene`);
4. `g_PhysicsManager.Move(0.005f)` and notices once per render frame;
5. mixed counters: `ChatTime` uses `timefac`, `MacroTime` and scene-frame globals use raw
   per-frame increments, while `WaterTextureNumber` uses fixed-step count;
6. render, physics render, swap;
7. software sleep when VSync is not the selected authority;
8. profiler end and networking tail.

`Winmain.cpp` selects VSync only when the reported display refresh approximately matches
the 60 FPS target and swap control succeeds. Otherwise `thread_sleep()` is authoritative.
Software pacing sleeps most of the remaining interval and spin-waits the final 1.5 ms.

### Why fixed update is not authoritative

- The scheduler computes a step count but does not invoke a simulation-update entry
  point.
- `MoveMainScene`, object/character movement, input, physics, notices and most effect
  graphs still execute once per render frame.
- Only selected weather, water, UI blink and visual-emission consumers read the count.
- `GetFixedUpdateAlpha()` is unused, so rendering is not interpolating between fixed
  simulation snapshots.
- State ownership is mixed: some values use uncapped elapsed scaling, some capped visual
  scaling, some fixed opportunities and some raw frame increments.
- Moving the existing broad update graph into a `for (fixedStep...)` loop would repeat
  input, RNG, packet sends, allocations and visual emissions during catch-up frames.

## 2. FULL CONSUMER CLASSIFICATION

The table classifies important consumer families. Repeated effect/particle call sites are
grouped by their common owner and timing contract; individual special cases remain an
implementation-phase audit gate.

| Class | File / function or family | Current source | Legacy expectation / render-FPS dependency | Migration risk | Central timing / interpolation / validation |
|---|---|---|---|---|---|
| GAMEPLAY / SIMULATION CRITICAL | `ZzzScene.cpp::MainScene`, `MoveMainScene` | Once per render frame plus mixed helpers | Legacy update order is frame-driven; raw counters depend on render FPS | Critical: broad repetition changes input, RNG, packets and order | Decompose by owner before central fixed cadence; interpolation is not applied to logic; trace 25/60/120 state and packet parity |
| GAMEPLAY / SIMULATION CRITICAL | `ZzzCharacter.cpp`, `ZzzAI.cpp`, object movement | Mostly `timefac`, some `timeNormalizer`, frame counters | Client presentation/movement advances from render loop; server remains authoritative | High: collision, path, target and action transitions | Reuse central elapsed/fixed APIs only after separating prediction/presentation; interpolate render transforms, not decisions |
| GAMEPLAY / SIMULATION CRITICAL | `BMD::PlayAnimation` | `AnimationFrame += timefac(PlaySpeed)` | Authored around 25 FPS but elapsed-scaled; frame/key crossings trigger actions/effects | High: skipped or repeated key events, attack visuals | Central elapsed legacy step may remain initially; snapshot interpolation only after key-event ownership is separated |
| GAMEPLAY / SIMULATION CRITICAL | Client `SkillManager::CalcSkillDelay` | real frame milliseconds from `LoadInformationFps` | Local visual cooldown decreases by elapsed ms | Medium/high: client responsiveness and UI; server decides legality | Keep elapsed milliseconds; expose through central monotonic frame delta, never fixed render count; compare against server results |
| NETWORK / PROTOCOL | `wsclientinline.h::SendRequestMagic` | `GetTickCount` 300 ms throttle | Client-side send suppression; server remains authoritative | High: packet cadence and responsiveness | Do not tie to simulation steps; eventually use a wrap-safe monotonic deadline API; replay/rapid-cast tests |
| GAMEPLAY / SIMULATION CRITICAL | GameServer `SkillManager`, `Attack`, `HackSkillCheck` | predominantly `GetTickCount` deadlines | Server-authoritative cooldown, speed and anti-abuse behavior | Critical, protocol/gameplay security | Do not centralize into client clock. A future server monotonic wrapper is a separate server audit; interpolation never applies |
| GAMEPLAY / SIMULATION CRITICAL | GameServer `ObjectManager` movement/path, recovery, death/regen | `GetTickCount` plus server process cadence | Authoritative movement and gameplay deadlines | Critical | Remain server-owned; validate wrap behavior and server cadence separately |
| GAMEPLAY / SIMULATION CRITICAL | GameServer `EffectManager::MainProc` and effect durations | server process cadence, effect counters/deadlines | One-second/tick-count effect semantics are gameplay data | Critical | Preserve server owner and cadence; no client interpolation of authority |
| VISUAL / INTERPOLATABLE | `ZzzEffect.cpp`, `ZzzEffectParticle.cpp`, `ZzzEffectJoint.cpp` | mostly `timefac`; selected emission gates | Motion/lifetime authored in legacy frame units; creation/RNG may be frame-sensitive | High: density, lifetime, RNG order and event attachment | Central visual delta is suitable for continuous motion; emissions need fixed/non-burst policies; validate counts and screenshots |
| VISUAL / INTERPOLATABLE | `ZzzEffectFireLeave.cpp::MoveLeaves` weather | fixed-step count plus `WorldTime` | Rain target/position update at 25 Hz; trigonometric values use elapsed wall animation | Low/medium | Existing fixed use is appropriate; render-only interpolation of rain position may be safe after wrap-aware snapshot test |
| VISUAL / INTERPOLATABLE | Pet/boid/environment emission families | `ShouldRunFixedVisualEmission`, RNG, some `timefac` | At most one legacy emission opportunity per render frame | Medium | Keep non-burst scheduler semantic; interpolate existing objects, never synthesize missed RNG emissions without density tests |
| VISUAL / INTERPOLATABLE | Camera `CGMCameraWorld`, `CameraMove` | capped `FPS_ANIMATION_FACTOR` / `timeNormalizer` | Smooth convergence without catch-up jumps | Low/medium | Strong candidate for central visual delta/smoothing API; test camera arrival time, overshoot and focus stalls |
| VISUAL / INTERPOLATABLE | Head-chat lifetimes | capped `FPS_ANIMATION_FACTOR` | Visual lifetime avoids stall catch-up | Low | Central visual elapsed policy; interpolation unnecessary beyond continuous alpha/position |
| VISUAL / INTERPOLATABLE | Water and terrain animation | fixed `WaterTextureNumber`, `WorldTime`, per-map globals | Mixed 25 Hz texture phase and continuous absolute-time waves | Medium: map-specific visuals | Keep explicit phase versus continuous-time domains; interpolate phase only if texture sequence parity is preserved |
| VISUAL / INTERPOLATABLE | BMD texture/chrome/effect UV animation | extensive `timeGetTime` / `WorldTime` | Wall-time visual phase independent of frame count | Medium: wrap/precision and pause semantics | Prefer session-relative central visual time where equivalent; no fixed simulation dependency; screenshot/long-uptime tests |
| VISUAL / INTERPOLATABLE | Cloth physics `CPhysicsManager`, `CPhysicsCloth` | called once/frame with constant `0.005f`, internally also `timefac` | Integration cadence is render-driven and combines constant and scaled inputs | High: numerical stability, RNG wind and collision | Dangerous first migration. Instrument steps/energy/stretch; choose fixed physics cadence only after parity; interpolate rendered cloth vertices if snapshots exist |
| UI / WALL-CLOCK | `UIControls` caret/notice blink | fixed-step count | Deterministic legacy 25 Hz counters | Low | Existing central fixed count is appropriate; interpolation not useful; verify blink period at 25/60/120 |
| UI / WALL-CLOCK | `NewUIStyleFX` hover fades | `GetFrameDeltaSeconds` | Continuous real-time UI transition | Low | Correct central frame-delta consumer; clamp/pause policy should be explicit |
| UI / WALL-CLOCK | message boxes, map-name UI, event displays, popup/button debounce | `timeGetTime` / `GetTickCount` | Human-time deadlines independent of simulation/render FPS | Low/medium | Do not move to fixed simulation. A shared wrap-safe monotonic UI/deadline API may replace direct OS reads incrementally |
| UI / WALL-CLOCK | event countdown presentation | server values plus OS elapsed timers | Display should track wall time/server sync | Medium | Keep wall-clock domain; validate resync, focus loss and reconnect, never infer gameplay authority from UI |
| UI / WALL-CLOCK | `ChatTime` | legacy counter decremented by `timefac` | Approximate legacy-duration UI state | Medium | Candidate for explicit elapsed duration after behavior capture; interpolation not relevant |
| UNKNOWN / REQUIRES RUNTIME VALIDATION | `MacroTime`, `MoveSceneFrame`, `frame_scene_desplace` | raw increment once per render frame | Many modulo/age consumers may assume 25 FPS | High | Inventory every reader before changing; likely split into simulation tick, visual frame ID and wall duration rather than one replacement |
| NETWORK / PROTOCOL | reconnect state machine | `GetTickCount` deadlines | Real timeout/retry windows independent of FPS | High | Keep network monotonic domain; use wrap-safe elapsed/deadline helpers only with packet/retry parity tests |
| NETWORK / PROTOCOL | zone-move grace and packet tick fields | `GetTickCount` | Protocol/security/compatibility timestamps | Critical | Preserve field widths and semantics; do not substitute simulation time or change wire values |
| NETWORK / PROTOCOL | `csteady_clock` ping map | `high_resolution_clock`; send once per FPS sample second | Diagnostics latency, not simulation | Low/medium | Keep separate monotonic diagnostics/network measurement; guard outstanding IDs/lifetime; no interpolation |
| NETWORK / PROTOCOL | service connection/session timers | service-local `GetTickCount` | Login, reconnect, timeout and cleanup deadlines | Critical | Remain process/domain-owned; audit wrap and retry semantics separately |
| DIAGNOSTICS / PROFILING | `RenderProfiler` | QPC scopes, `timeGetTime` report window | Measures actual CPU wall time | Low | Must not use simulation time; retain high-resolution monotonic measurement and aggregate off hot paths |
| DIAGNOSTICS / PROFILING | `Time/Timer.cpp::CTimer` | QPC with multimedia fallback | Underlies session-relative client elapsed time | High infrastructure risk | Reuse behind `csteady_clock`; validate frequency/fallback/precision before any replacement |
| DIAGNOSTICS / PROFILING | FPS counter | rendered-frame count per one-second checker | Displayed render throughput | Low | Keep render-FPS metric distinct from simulation TPS and presentation rate |
| UNKNOWN / REQUIRES RUNTIME VALIDATION | Lua-exposed `timeGetTime` | raw OS time | Script contract and consumers are not mapped | High | Do not change until scripts/assets and value expectations are audited; interpolation unknown |
| UNKNOWN / REQUIRES RUNTIME VALIDATION | blocking `Sleep` call sites | thread-specific waits | Some are I/O/background pacing, not frame pacing | High | Classify thread and shutdown ownership individually; never route all sleeps through frame clock |

## 3. CRITICAL 25-FPS DEPENDENCIES

1. `REFERENCE_FPS` defines the conversion from seconds to legacy update units.
2. `BMD::PlayAnimation` advances authored animation keys using legacy-step scaling;
   integer key crossings drive prior-frame/action behavior.
3. Effect, particle and joint lifetimes/motion contain a very large legacy-unit surface.
4. Weather, water, UI blink and selected emitters explicitly consume the 25 Hz scheduler.
5. `checkNormalizer` still gates several minimap/effect/blur emission paths at a 40 ms
   opportunity cadence.
6. Raw `MoveSceneFrame` / `frame_scene_desplace` readers may encode elapsed age or modulo
   cadence and require a reader-by-reader audit.
7. Probability scaling (`rand_calc_check`) and non-burst emission policies preserve
   density statistically rather than replaying every missed fixed step.
8. Physics uses a constant per-frame argument and additional legacy scaling, so its
   effective integration is coupled to both render cadence and compensation.

## 4. CONFIRMED RENDER-FPS-DEPENDENT BEHAVIOR

- `MoveMainScene`, physics and notices execute once per rendered frame.
- `MacroTime`, `MoveSceneFrame` and `frame_scene_desplace` advance once per render frame.
- Input is sampled once per render frame.
- RNG-bearing update graphs may make more decisions at higher render FPS even when
  probability is normalized.
- `timefac` preserves average continuous speed but does not create separate fixed
  simulation states.
- capped visual scaling deliberately slows rather than catches up after a frame longer
  than 40 ms.
- fixed scheduler consumers may process multiple deterministic steps after a moderate
  stall, while non-burst emitters process at most one opportunity.

## 5. SAFE INTERPOLATION CANDIDATES

These are candidates, not implementation approval:

- camera position/zoom/orientation between two verified states;
- visual object/character transforms after gameplay/path/action state is fixed;
- continuous effect, particle and joint position/alpha/scale that has no discrete event
  or RNG side effect;
- terrain/water phase values where interpolation respects cyclic wrap;
- weather particle positions after the 25 Hz creation/update owner is preserved;
- cloth render vertices only after a stable fixed physics step and two snapshots exist;
- UI hover/fade presentation already driven by elapsed render delta.

`GetFixedUpdateAlpha()` should eventually represent
`remaining fixed accumulator / fixed step` and be consumed only by rendering between a
previous and current simulation snapshot. It must not advance gameplay, trigger animation
events, emit particles, send packets or mutate the authoritative state.

## 6. DANGEROUS MIGRATION CANDIDATES

- the whole `MoveMainScene` graph;
- character path/collision and action transition logic;
- attack/skill animation key events and packet-send gates;
- GameServer cooldown, movement, effect, anti-speed and recovery deadlines;
- physics before its effective current step is measured;
- effect creation, RNG, lifetime expiry and owner-linked effects as one bulk conversion;
- raw global frame counters before all readers are classified;
- reconnect, zone-move and protocol tick fields;
- Lua timer behavior and threaded `Sleep` calls without runtime entry-path evidence.

## 7. PROPOSED FINAL TIMING OWNERSHIP MODEL

Evolve `csteady_clock` as the single client frame/simulation timing authority, preserving
its public compatibility surface during migration. Its eventual explicit domains should
be:

- **Monotonic real time:** session-relative time and wrap-safe elapsed/deadline values.
- **Render frame:** raw/clamped render delta, render frame ID and render FPS.
- **Simulation:** fixed 25 Hz step duration, bounded step count, dropped-time diagnostics
  and simulation tick ID.
- **Interpolation:** alpha plus read-only previous/current snapshot contract.
- **Legacy adapters:** `timefac`, `timepow`, `timeNormalizer`, `standlimit` and
  `checkNormalizer`, retained until every consumer migrates and removal is approved.
- **Frame pacing:** exactly one selected authority, VSync or software deadline pacing.
- **Timing diagnostics:** counters exposed to the existing profiler, without making
  profiler measurement depend on simulation time.

Wall-clock UI deadlines and client networking should consume a shared monotonic clock or
deadline utility from the existing timing infrastructure where this preserves semantics,
but they remain separate domains. QPC profiling remains independent. GameServer and other
services retain process-local authoritative gameplay/network timing; any server timer
wrapper is a separate migration, not a client FPS feature.

## 8. PHASED IMPLEMENTATION PLAN

No phase is authorized by this audit.

1. **T0 — Runtime baseline instrumentation:** add opt-in aggregate counters only; no
   timing behavior change.
2. **T1 — Contract the existing clock:** document units, clamp/stall policy, frame versus
   fixed domains, wrap semantics and startup/focus behavior; add tests/invariants without
   changing consumers.
3. **T2 — Raw counter inventory:** map every reader of `MoveSceneFrame`,
   `frame_scene_desplace`, `MacroTime`, `checkNormalizer` and timer globals; assign an
   owner and expected cadence.
4. **T3 — Low-risk elapsed consumers:** migrate UI fades/deadlines and continuous camera
   presentation to explicit existing-clock APIs one family at a time.
5. **T4 — Visual simulation families:** migrate deterministic weather/effect motion in
   bounded groups, keeping non-burst emissions and RNG order explicit.
6. **T5 — Simulation seam:** extract a narrow fixed-update boundary only for consumers
   whose input/RNG/packet/ownership contracts are understood. Do not loop the whole scene.
7. **T6 — Snapshot interpolation:** add previous/current snapshots and apply alpha to
   proven render-only transforms.
8. **T7 — Physics timing:** after instrumentation, choose and validate a stable fixed
   cloth step with render interpolation if evidence supports it.
9. **T8 — Legacy adapter retirement:** migrate remaining consumers; remove an adapter or
   old path only after repository-wide proof, Release/x86 runtime parity and specific
   user approval under the architecture removal gates.

## 9. RUNTIME INSTRUMENTATION REQUIRED

Extend the existing opt-in profiler/diagnostics rather than creating another profiler:

- raw and clamped frame delta histogram: median, p95, p99 and maximum;
- render frames, simulation steps, zero-step frames, multi-step frames and dropped steps;
- accumulator and interpolation alpha distribution;
- update time split for input, scene/object/character/effect/physics/UI/network tail;
- counts of animation key crossings and action transitions;
- object path steps, local movement distance and server correction/snap events;
- skill requests, client throttle rejects, server accepts/rejects and cooldown deltas;
- effect/particle/joint create, expire and live counts by representative family;
- RNG/emission opportunities versus actual emissions;
- physics step count, maximum displacement/stretch and non-finite-state detection;
- packet count/bytes/cadence by relevant head/subhead without logging secrets;
- focus loss, stall, map transition and reconnect markers;
- build, configuration, hardware, driver, resolution, VSync/pacer mode and scene metadata.

Hot paths must aggregate into fixed storage and report periodically. Logging each particle,
object, packet or timing call would change the behavior being measured.

## 10. RUNTIME VALIDATION MATRIX

Use the same Release/x86 client/server build, Data, configuration, map population, camera,
resolution and graphics settings for each comparison.

| Dimension | Required cases | Required evidence |
|---|---|---|
| Frame rate/pacing | 25 baseline where available, 60 software, 60 VSync, experimental 120 only after configurable audit harness exists | render FPS/TPS, frame-time distribution, step/dropped-step counts |
| Scene load | login, character select, empty Lorencia, crowded town, monster/skill-heavy scene | update/render sections, live objects/effects, visual capture |
| Gameplay | walk/run/path, stop/turn, teleport, normal attack, representative physical/magic/master skills | server position/state, packet cadence, damage/cooldown results, client action timing |
| Animation | idle, walk/run, attack, hit, death, wings/pets/equipment | action duration, key crossings, attachments, screenshot/video parity |
| Effects | dense particles, joints, blur, weather, water, map-special effects | creation/expiry/live counts, density and lifetime parity |
| Physics | representative cloth/wings during idle, movement, attack and stall | displacement/stretch metrics, collision and visual parity |
| UI | caret, notices, chat timeout, hover fades, event countdowns | measured period/duration and focus-loss behavior |
| Network | rapid skill request, reconnect, zone move, ping, delayed/lost response | packet order/rate, retries, timeouts, server accept/reject parity |
| Stress | 100/250/500 ms injected stall, repeated map change, focus loss/restore | catch-up/drop policy, no burst storm, no gameplay leap |
| Long run | at least timer-wrap/precision-oriented soak appropriate to changed API | monotonic behavior, stable visual phases, memory and timer maps |

Parity verdicts must use GameServer state for gameplay and packets, not visual smoothness
alone. Acceptance requires unchanged gameplay speed, movement/path results, attack and
skill legality, animation/event order, effect lifetime/density, packet cadence, input feel
and reconnect behavior, with any intentional visual-only improvement separately recorded.

## What should remain unchanged

- `REFERENCE_FPS = 25.0` until every authored legacy dependency is understood.
- GameServer authority over movement, combat, effects, cooldowns and anti-speed checks.
- packet layouts, tick fields and reconnect/zone timing semantics.
- the current VSync/software single-authority pacing policy unless separately measured.
- QPC-based profiler measurement independent of simulation time.
- legacy adapters and fallbacks until consumer migration, runtime validation and specific
  removal approval are complete.

## Open validation debt

- No runtime 25/60/120 parity run was performed in this documentation-only phase.
- Direct OS timer call counts include inactive/conditional code until runtime paths are
  exercised.
- Every reader of the broad frame globals still needs the T2 reader-level inventory.
- Effect/particle grouping is architectural; special-case event triggers require
  family-by-family implementation audits.
- Server timer wrap safety and scheduler cadence deserve a separate server timing audit
  before any server-wide wrapper is proposed.

## T0 — ACTIVE MIGRATION: opt-in baseline instrumentation

Implemented on 2026-08-30 by extending `RenderProfiler`; no parallel profiler or timing
authority was added. The existing `MU_RENDER_PROFILER=1`, `-renderprofiler`, and
`renderprofiler.enable` gates remain the only enable paths, and diagnostics remain off by
default.

CONFIRMED CURRENT STATE instrumentation now reports fixed-capacity distributions for raw
frame delta, legacy-clamped visual delta, render FPS, fixed accumulator and interpolation
alpha. It also reports fixed steps, zero-step/multi-step frames, dropped steps, and coarse
QPC costs for input, scene movement, object/item update, character update, effects,
physics, UI/notices, rendering and the network tail. Aggregate counters cover animation
key crossings, fixed-visual-emission checks/opportunities, non-finite physics state, and
packet counts/bytes. Packet payloads are neither copied nor logged by T0 diagnostics.

UNVERIFIED / REQUIRES RUNTIME VALIDATION: Release/x86 compilation proves integration but
does not prove representative runtime cost or 25/60/120 parity. T0 intentionally does not
provide exact effect/particle/joint expiration counts: expiration is distributed through
many subtype branches, and instrumenting those branches would be too invasive. Creation,
live-population and per-family expiration precision remains validation debt for a later,
separately approved instrumentation refinement. Action-transition and centralized
effect/particle/joint creation counters also remain deferred because their legacy source
files require byte-preserving edits before safe hot-path instrumentation; no encoding-wide
rewrite was accepted for T0.

## T0.1 — ACTIVE MIGRATION: packet coverage and render-cost split

Repository tracing confirmed that active classic packet builders call
`CStreamPacketEngine::Send`, which forwards to `wsclientinline.h::SendPacket` and then to
the socket owner's `sSend`. The earlier `WSclient.cpp::DataSend` counter observed only a
separate wrapper and therefore remained zero during the first baseline. T0.1 records a
packet only when the central `SendPacket` boundary receives a successful result from the
existing final socket send, using only the already-known transmitted byte count. The
payload, encryption, buffering, retries and send order are unchanged. The compiled Asio
`CProtocolSend` path has no connection call site in the current source and is not treated
as an active parallel owner.

Main-scene render diagnostics now divide owner-level CPU submission into four mutually
exclusive child sections: world/terrain, objects/models/characters, effects/particles and
UI. They remain nested inside the existing whole-scene `Render` measurement, so the parent
overlaps all children by design; the four child sections do not overlap each other.
`PhysicsRender` and `Swap` remain separate sibling sections. No framebuffer postprocess
owner is present in the current source: item glow is part of model/material rendering and
`RenderAfterEffects` belongs to the effects section. Login and character-select rendering
remain covered by the parent render measurement rather than the main-scene-only split.
