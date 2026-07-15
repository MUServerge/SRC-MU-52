---
name: mu-main52-refactor
description: Safe incremental refactoring and architecture cleanup for MUServerge/main-5.2. Use when changing poorly structured, duplicated, unsafe, tightly coupled, global-state-heavy, or difficult-to-test C++ code; when modernizing ownership or module boundaries; or when feature work should also improve the directly touched legacy code without altering MU Online behavior.
---

# MU Main 5.2 Refactor

Read [references/architecture-target.md](references/architecture-target.md) before architectural work.

Refactor by preserving observable behavior first. Modern syntax alone is not architecture.

## Refactor gate

Before changing structure:

1. Identify callers, callbacks, macros, scripts, function pointers, and raw data consumers.
2. Write down current observable behavior and invariants.
3. Separate confirmed defects from style preferences.
4. Find an existing project pattern that can be reused.
5. Define the smallest safe seam and rollback point.

Do not refactor code whose ownership or runtime entry path is still unknown.

## Touched-code rule

While implementing a feature or fix:

- clean code directly required to understand and safely change that behavior;
- remove duplication introduced or exposed by the patch;
- repair ownership/lifetime defects on the same path;
- avoid formatting or renaming unrelated files;
- record larger adjacent debt for a dedicated follow-up.

Keep behavioral and mechanical changes separate when practical.

## Preferred transformations

- duplicated logic → one existing or extracted implementation;
- scattered global state → one clear owner with narrow access;
- manual lifetime → scoped owner/RAII when ABI permits;
- long mixed-purpose function → named stages preserving call order;
- magic numbers → existing enums/constants, then a named constant;
- raw arrays → bounded views/containers only when layout is not shared;
- hidden initialization → explicit initialize/release symmetry;
- compile-time experiment → capability/config with safe fallback when appropriate.

## Legacy safety

- Do not casually change public structure size/layout.
- Do not place non-trivial members into POD/memset-sensitive objects without auditing every allocation and copy.
- Do not change packet/file structs without migration.
- Do not reorder global/static initialization without lifecycle evidence.
- Do not introduce exceptions across old C/library boundaries.
- Do not replace raw pointers until ownership and aliasing are proven.

## Validation

Prove equivalence using builds, focused tests, logs, packets, file bytes, screenshots, render sequence, timing, and initialization/reload/shutdown smoke tests. If only static analysis is possible, mark runtime equivalence unverified.
