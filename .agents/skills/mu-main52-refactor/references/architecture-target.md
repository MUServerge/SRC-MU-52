# Incremental architecture target

## Dependency direction

Aim for stable boundaries without moving files merely to simulate architecture.

### Platform

Own Windows handles, OpenGL context, input, timing, threads, filesystem primitives, and system APIs.

### Rendering

Own GPU resources, shaders, textures, render passes, state transitions, model/terrain/effect drawing, and capability fallbacks. Gameplay may request rendering but should not own GPU handles.

### Game domain

Own characters, objects, skills, maps, animation decisions, gameplay state, and scene orchestration. Avoid dependence on UI widgets.

### UI

Own windows, layout, input presentation, tooltips, and view state. Reuse domain services rather than duplicating gameplay rules.

### Network and data

Own packet serialization, validation, file formats, loaders, config, and version boundaries. Convert raw external data at controlled boundaries.

### Utilities

Keep utilities small, dependency-light, and behavior-specific. Avoid generic helper dumping grounds.

## Ownership rules

- One authoritative owner per resource or mutable state.
- Initialization and release must be symmetric.
- Borrowed pointers must not imply ownership.
- Managers must represent a real lifecycle/domain boundary.
- Avoid new singletons; place narrow seams around unavoidable legacy globals.
- Pass explicit state at system/pass boundaries instead of repeatedly reading globals.

## Compatibility seams

Use adapters and fallbacks to modernize risky subsystems. Keep old and new paths only during migration and define removal criteria.

## Migration lifecycle

Use this sequence for an architectural replacement:

`AUDIT -> DEPENDENCY MAP -> DEFINE RESPONSIBILITY/OWNER -> TARGET ARCHITECTURE -> ADDITIVE MIGRATION -> VALIDATION -> MIGRATE ALL CONSUMERS -> REMOVE SUPERSEDED LEGACY CODE -> DOCUMENT FINAL STATE`

Additive-first is the transition strategy, not permanent duplication. Removal is a separate, explicitly approved change and is allowed only when:

1. The current owner, entry paths, callbacks, indirect consumers, assets, configuration and teardown are mapped.
2. The replacement preserves the required source, binary, packet, file-format, gameplay, timing, rendering and lifecycle contracts.
3. Every intended consumer has migrated and repository-wide searches find no unexplained dependency on the superseded path.
4. Success, fallback, failure, initialization, reload, scene-transition and shutdown behavior relevant to the subsystem are validated.
5. The affected Release/x86 solution builds and runtime equivalence is verified with recorded evidence.
6. A bounded rollback path or recoverable pre-removal state exists.
7. The removal is isolated from unrelated refactoring and its impact is documented.
8. The user specifically approves the concrete removal.
9. The migration record, final architecture documentation and `CHANGELOG.txt` are updated.

Until every gate is satisfied, retain the proven legacy path and label the migration state and remaining validation honestly.

## Decision record

For non-trivial changes, capture problem/evidence, invariants, existing solution considered, chosen boundary, rejected alternatives, validation/fallback, and remaining debt.

