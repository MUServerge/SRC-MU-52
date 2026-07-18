---
name: mu-main52-data-assets
description: "Data and asset pipeline workflow for MUServerge/main-5.2 and SRCMainGS. Use for BMD models and animations, OZJ/OZT/OZB and JPG/TGA textures, bitmap/model IDs, loaders, scripts and configuration, localization, sounds, effects, terrain files, cache and lifetime, validation, missing or conflicting resources, startup/loading performance, or client-server data parity."
---

# MU Main 5.2 Data and Assets

Treat every asset as a typed resource with a producer, format, stable ID or path, loader, owner, consumers, lifetime, and failure policy. Read [references/data-assets-map.md](references/data-assets-map.md) before substantial work.

## Inventory before changing

1. Find every declaration, load call, path, numeric ID/range, lookup, render/use site, unload call, and fallback for the resource.
2. Trace raw source format to packaged/runtime format. Do not infer a file is unused merely because the human-readable extension is absent.
3. Determine whether loading is global, world-specific, UI-skin-specific, character/item/monster-specific, lazy, or repeated.
4. Verify the active `Data` tree from the desktop build when source alone cannot prove existence, case, dimensions, compression, or contents.
5. Search the whole project for an existing loader, decoder, cache, ID range, script parser, and error-report path before adding one.

## Non-negotiable invariants

- One runtime bitmap/model ID has one intended semantic owner at a time. Detect duplicate registration, overlapping arithmetic ranges, off-by-one boundaries, and aliases before extending enums.
- Preserve BMD mesh, bone, animation, texture-name, UV, normal, blend, and material assumptions unless all consumers are migrated together.
- Validate file signature/version, declared counts, offsets, multiplication overflow, buffer bounds, string termination, index ranges, and truncated reads before allocating or dereferencing.
- Never trust asset-provided filenames or counts to fit fixed buffers or global arrays.
- Paths are Windows-oriented and may be case-insensitive on the target runtime. Still standardize separators/case and detect case-only duplicates for tools and CI.
- Load and upload GPU resources only with a valid owning GL context. Delete them before context destruction and exactly once.
- Cache by canonical identity and load parameters when reuse is safe; do not cache resources whose wrap/filter/format or world-specific mutable state differs.
- A missing required gameplay/world resource must fail clearly. Optional cosmetic resources may use an explicit placeholder or feature disablement, never silent memory reuse.
- Do not change client-visible IDs or data layouts without checking server tables, packets, scripts, tools, and existing user data.
- Separate immutable source data from per-frame mutable state. Never freeze terrain wind, water, lighting, animation, or effect state into a static asset buffer.

## Resource workflow

For each addition or defect, record:

1. **Identity:** enum/range, logical name, canonical path, owning subsystem.
2. **Format:** extension, runtime conversion/encryption, header/version, dimensions/count limits.
3. **Load:** call site, order prerequisites, thread/context restriction, duplicate behavior.
4. **Use:** all consumers and assumptions about dimensions, alpha/components, bones, meshes, or localization index.
5. **Lifetime:** owner, reload/world-switch behavior, unload order, CPU/GPU memory.
6. **Failure:** log/user message, placeholder, rollback, and whether startup/world entry can continue.
7. **Parity:** matching client/server/config identifiers and deployment files.

## Domain rules

### Models and animations

- Trace model registration through `ZzzOpenData`, `CGMModelManager`, and `BMD` parsing/use.
- Check mesh/bone/action counts, texture references, animation keys, bone indices, and model-index arithmetic before render or GPU-skinning work.
- Route shader, VBO/VAO, GPU upload, and render-state changes through `$mu-main52-renderer`; keep format and ownership findings here.

### Textures and bitmap IDs

- Trace requested JPG/TGA names through the actual runtime extension/decoder path; this client can translate a JPG request to `OZJ`.
- Verify dimensions, components/alpha, filter, wrap, mipmap/compression behavior, and ID uniqueness.
- Centralize duplicate-load diagnostics before attempting broad deduplication; identical paths may intentionally use different sampler behavior.

### Terrain and worlds

- Treat height/light/mapping/attribute/object/camera resources as a coherent world set. Validate dimensions and map ID/name correspondence.
- Keep collision/access attributes consistent with the server. Route map rules and client-server world parity through `$mu-main52-maps-worlds`.
- Keep base terrain data separate from dynamic wind, water, lighting, weather, and event effects.

### Scripts, configuration, and localization

- Preserve parser grammar, terminators, defaults, numeric widths, encoding, and index stability.
- Reject malformed rows with file/line/key diagnostics. Detect duplicate keys/IDs and missing required references.
- For localized text, verify every referenced `GlobalText` index across supported language files, format specifiers, and destination buffer sizes.
- Treat client tables as presentation unless server code/config independently authorizes gameplay values.

### Audio and effects

- Trace sound/music IDs, load/stream policy, world/event ownership, repeated play suppression, and cleanup.
- For effect resources, check pool/lifetime limits and model/bitmap dependencies before increasing visual density.

## Optimization rules

- Measure startup phase, world-entry phase, disk reads, decode time, CPU copies, GPU uploads, peak memory, and duplicate registrations before optimizing.
- Prefer manifest/inventory diagnostics and load-once reuse when behavior is identical.
- Batch or defer optional resources only when first-use hitching and GL-thread restrictions are handled.
- Do not introduce a new archive, converter, or asset manager until compatibility with current encrypted formats, patcher/deployment, ResourceGuard, and tools is proven.

## Verification

Test clean startup, every affected world/UI skin/class/item/monster/effect, world switching, reconnect, device/context recreation where supported, missing file, corrupt header, truncated file, excessive counts, duplicate ID, case-only path difference, unsupported dimensions/components, and repeated load/unload.

Compare screenshots/animation timing, collision and map attributes, logs, load time, CPU/GPU memory, resource counts, and client-server identifiers. Mark desktop asset and runtime tests pending when the repository lacks the actual `Data` tree.

## Handoff

Report resource identity, exact source/runtime paths, format assumptions, loader and consumers, ID/range audit, lifetime, failure policy, client-server parity, confirmed collisions or missing references, measured impact, and remaining desktop-data/runtime checks.
