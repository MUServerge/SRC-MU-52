---
name: mu-main52-maps-worlds
description: Map and world architecture workflow for the MUServerge/main-5.2 client and MUServerge/SRCMainGS server. Use for world and map IDs, terrain, collision and attribute maps, gates, warps, spawn regions, safe zones, map access rules, minimaps, camera, weather, lighting, music, map-specific models and effects, monsters, drops, events, view range, and client-server map consistency.
---

# MU Main 5.2 Maps and Worlds

## Trace the world end to end

1. Read `references/maps-worlds-map.md` before changing a map.
2. Trace map ID -> server registration/rules -> gates and spawn coordinates -> attribute/path data -> client world loading -> terrain/models/effects -> minimap/UI -> monsters/events/drops.
3. Build a client/server parity table for every affected ID, coordinate, gate, attribute, and rule.
4. Search existing generic map managers, base-map hooks, and explicit map specializations before adding another world branch.

## Preserve contracts

- Treat map IDs, gate IDs, coordinates, attribute bit meanings, event map ranges, and packet fields as shared compatibility contracts.
- Keep movement, collision, access, safe-zone, spawn, drop, experience, PK, and event eligibility authoritative on the server.
- Keep terrain, camera, lighting, weather, sound, model/effect loading, minimap, and visual ambience on the client.
- Never fix a server/client mismatch by silently duplicating the other side's logic. Establish one owner and synchronize the contract.
- Preserve original MU geography, progression, event behavior, and visual identity unless explicitly asked to change them.

## Change safely

- Reuse `MapManager`, `Map`, `MapPath`, gate/warp, base map, and map-process abstractions where applicable.
- Isolate map-specific behavior behind an existing hook or a narrow registered specialization; avoid scattered numeric world checks.
- Validate coordinate bounds and attribute access before indexing fixed map grids.
- Load shared resources once and release them at the correct world lifetime; measure map-load time, memory, frame time, and draw/state cost.
- Test login spawn, gate travel, death gate, teleport, reconnect, party movement, edge tiles, blocked/safe tiles, map exit/re-entry, event transitions, and unknown IDs.

## Report evidence

- Provide a parity matrix: ID/name, server manager, client loader, gates, attributes, spawn/monsters, event owner, drop/rate rules, UI/minimap, resources.
- Label verified behavior, historical client code, configuration dependencies, and unresolved data files separately.
- Add verified mappings and mismatches to `references/maps-worlds-map.md`.
