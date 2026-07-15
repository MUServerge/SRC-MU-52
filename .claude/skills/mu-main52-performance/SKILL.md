---
name: mu-main52-performance
description: Evidence-based CPU, GPU, memory, loading, frame-time, and network optimization workflow for MUServerge/main-5.2. Use for FPS drops, stutter, crowded-scene slowdown, high CPU/GPU/VRAM/RAM use, leaks, excessive draw calls or allocations, slow loading, timing problems, and any request to optimize the MU Online client without changing gameplay or visual behavior.
---

# MU Main 5.2 Performance

Read [references/measurement-plan.md](references/measurement-plan.md) before performance work.

Optimize measured bottlenecks, not code appearance.

## Required loop

1. Define the visible problem and representative scene.
2. Capture a reproducible baseline.
3. Profile or instrument the CPU/GPU/memory/loading path.
4. Form one falsifiable hypothesis.
5. Search for existing caches, batching, pools, limits, culling, and optimized paths before adding another.
6. Change one primary variable.
7. Repeat the same benchmark and compare.
8. Check visual/gameplay equivalence and regressions.
9. Keep, revise, or revert based on evidence.

## Prohibited shortcuts

- Do not equate fewer source lines with faster code.
- Do not remove waits, synchronization, bounds checks, or fallbacks without proving safety.
- Do not cache mutable data without invalidation and lifetime rules.
- Do not add threads to timing-sensitive or OpenGL work without ownership/context design.
- Do not change animation or simulation timing to inflate FPS.
- Do not reduce visual quality unless the user requests a configurable tradeoff.
- Do not claim success from average FPS alone.

## Optimization priority

Prefer, in order:

1. remove duplicated work;
2. fix leaks and lifetime churn;
3. reduce per-frame allocation/conversion;
4. cull work that cannot affect output;
5. batch compatible work and reduce state changes;
6. cache stable results with explicit invalidation;
7. improve locality and algorithmic complexity;
8. move work to GPU only when it replaces CPU work;
9. micro-optimize only after profiling confirms a hot loop.

## Correctness envelope

Preserve simulation/animation rate, packet cadence/order, render order/blend semantics, collision/selection/attachments, deterministic data interpretation, and unsupported-hardware fallback.

## Report

State baseline, change, measured result, variance, hardware/context, equivalence checks, and unresolved bottleneck. If runtime measurement is unavailable, provide instrumentation instead of claiming optimization.
