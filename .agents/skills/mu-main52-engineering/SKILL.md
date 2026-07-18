---
name: mu-main52-engineering
description: "Engineering guide for the MUServerge/main-5.2 MU Online Season 5.2 Win32 C++ client. Use for any task in this project: feature work, bug fixes, code review, architecture decisions, build changes, UI, rendering, networking, data loading, effects, models, maps, or project-wide analysis. Enforce reuse-first discovery, preservation of original game behavior, incremental modernization, x86/ABI safety, and verified changes."
---

# MU Main 5.2 Engineering

Treat `MUServerge/main-5.2` as a legacy game client being modernized incrementally, not rewritten.

Read [references/project-context.md](references/project-context.md) at the start of substantial work. Read only the relevant specialist skill for deeper refactor, performance, or renderer work.

## Non-negotiable rules

1. Preserve gameplay, timing, visuals, packet behavior, data compatibility, and scene flow unless the user explicitly requests a behavior change.
2. Search before creating. Use `rg` locally or repository search to find existing symbols, wrappers, managers, utilities, packages, flags, and call sites.
3. Reuse or extend a proven existing implementation when it satisfies the requirement. Do not create parallel managers, duplicated state, helper families, loaders, or rendering paths.
4. Introduce a dependency only after proving existing code and dependencies cannot solve the problem cleanly. Check Win32/x86, VS v143, static runtime, licensing, binary size, and deployment impact.
5. Keep each change bounded and reversible. Avoid big-bang rewrites.
6. Improve bad code at the touched seam, but do not silently expand a feature into an unrelated project-wide refactor.
7. Preserve struct layout, packing, protocol sizes, BMD/data formats, hardcoded offsets, global initialization order, and ownership assumptions unless audited end-to-end.
8. Never claim an optimization without a before/after measurement or a clearly labeled unverified hypothesis.
9. Keep a safe fallback for risky renderer, loader, network, or compatibility changes.
10. Never delete code merely because search found no caller; account for callbacks, exports, scripts, resources, macros, function pointers, and runtime name lookup.

## Required workflow

### Establish scope

- Identify requested behavior and explicit non-goals.
- Locate entry points, ownership, callers, callees, flags, config, assets, and teardown.
- Check the worktree and preserve unrelated user changes.

### Run reuse-first inventory

Search for similar functions, existing owners, common helpers, linked packages, disabled features, unfinished implementations, and teardown/fallback paths. Prefer direct reuse, then extension, then an adapter, then a focused new component. Add a dependency only as a last justified option.

### Map invariants and risk

Classify gameplay, rendering state/order, ABI/data layout, protocol, resource lifetime, threading/timing, and build/deployment effects. Choose the smallest design respecting them.

### Implement with local cleanup

- Fix misleading names, duplicated branches, unsafe ownership, missing cleanup, and unclear boundaries directly required by the change.
- Add a seam before replacing a large legacy block.
- Keep behavior changes distinguishable from mechanical cleanup.
- Defer unrelated debt as an explicit follow-up instead of hiding it in the patch.

### Verify proportionally

- Build affected Win32 Debug/Release configurations when available.
- Test success, fallback, failure, initialization, shutdown, reload, and scene transitions.
- Compare visuals, logs, packets, timings, or file output as appropriate.
- State what was verified and what still needs the Windows game client.

## Architecture direction

Move gradually toward explicit ownership and one source of truth across platform, rendering, game domain, UI, network/protocol, data/loaders, and small dependency-light utilities. Do not force the target structure in one patch; create boundaries where work already touches code.

## Handoff

Report behavior changed or preserved, existing implementation reused, touched debt improved, files changed, tests performed, remaining Windows checks, and the safest next step.
