---
name: mu-main52-skills-combat
description: Skills and combat architecture workflow for the MUServerge/main-5.2 client and MUServerge/SRCMainGS GameServer. Use for normal or master skills, skill learning and lists, input and hotkeys, attack packets, target/range/area selection, cooldown and speed checks, mana/BP consumption, physical/magic/curse/elemental damage, hit/miss, critical/excellent/double/combo damage, PvP/PvE rules, shields, reflect, buffs/debuffs/DOT, combat animation/effects, monster or automated attacks, and combat persistence or balance changes.
---

# MU Main 5.2 Skills & Combat

## Core rule

Keep GameServer authoritative for skill ownership, eligibility, targets, timing, resources, hit results, damage, effects, death, and rewards. Treat the client as input, prediction, animation, audio, effects, and UI. Preserve original MU behavior unless the user explicitly approves a balance or gameplay change.

Apply `$mu-main52-engineering` for project invariants, `$mu-main52-protocol-persistence` for packet and save boundaries, `$mu-main52-character` for stats/equipment/state, and the relevant item, monster, map, or event skill for domain rules.

## Establish the active combat contract

1. Record the selected client and GameServer Visual Studio configurations and their effective macros. Do not use `stdafx.h` fallback values as build evidence.
2. Read [combat-topology.md](references/combat-topology.md) for verified owners and entry points.
3. Build a parity row for the affected skill from [combat-audit.md](references/combat-audit.md).
4. Search for the skill ID, base/master replacement chain, effect ID/group, packets, animation/action, sound, item grant, monster caller, helper/automation caller, and map/event overrides.
5. Reuse the existing `SkillManager`, `Attack`, `EffectManager`, `MasterSkillTree`, and client presentation path. Do not add a parallel damage, cooldown, buff, or targeting system.

## Trace one cast end to end

Trace:

`input/AI -> client eligibility and visual cooldown -> encrypted packet -> ProtocolCore -> structural validation -> skill ownership -> delay/speed validation -> target/range/map/PvP validation -> mana/BP/ammo -> RunningSkill -> Attack -> effects/DOT -> HP/SD/death -> result packets -> client animation/UI -> persistence when applicable`.

For area and multi-hit skills, also trace center/angle, viewport candidates, server-selected versus client-supplied targets, maximum targets, per-target deduplication, repeated-hit counters, and combo state.

## Validate before execution

- Validate the complete packet size before casting. For target arrays require an overflow-safe exact relation between fixed bytes, count, and entry size; clamping count alone is insufficient.
- Validate skill ID against the active build's range before indexing delay, attribute, master-value, animation, or effect arrays.
- Resolve the skill from the server-owned character list. Never execute a client-supplied skill merely because its ID exists in configuration.
- Validate live/playing/teleport state, target object type, map equality, safe-zone attributes, event phase, duel/guild/party/Gens relationship, PvP flags, range/radius/frustum, line/path rules when required, equipment, class, change-up, level, energy, leadership, kill points, guild status, ammo, mana, and BP.
- Validate master-skill prerequisites, replacement chain, rank/group, point cost, maximum level, stored level, and array bounds on both load and upgrade.
- Deduct resources only after a cast is accepted at the intended commit point. A rejected, duplicated, or malformed cast must not consume twice or produce effects.
- Use wrap-safe elapsed-time comparisons. Keep anti-speed heuristics separate from canonical per-skill cooldown validation.

## Preserve calculation order

Treat order as gameplay data. Record intermediate values before modifying a formula:

1. base character and equipment ranges;
2. skill and master-skill contribution;
3. effect/item flat and percentage modifiers;
4. weapon/durability modifiers;
5. random roll and critical/excellent selection;
6. PvP/PvE and skill-specific rates;
7. defense, ignore-defense, resistances, immunity, reduction, wings/helpers;
8. double/combo/reflect and shield split;
9. HP/SD mutation, durability, applied effect/DOT, death and packets.

Do not “clean up” integer division, signedness, rounding, random-call order, inclusive/exclusive ranges, critical-versus-excellent precedence, or percentage placement without a golden comparison. Fix a confirmed formula defect in a separate balance-aware patch with representative class/gear/PvP/PvE vectors.

## Maintain skill-data parity

Server data is authoritative for gameplay; client data must still match presentation and eligibility expectations.

- Pair client `Skill.bmd` with server `Skill/Skill.txt` and `Skill/SkillDamage.txt`.
- Pair client master-tree data/tooltips with server `Skill/MasterSkillTree.txt` and persisted master-skill layout.
- Pair client `BuffEffect.bmd` and buff enums with server `Effect.txt`, effect IDs/groups, flags, durations, values, and save policy.
- Pair skill IDs, base/master brand/replacement IDs, range/radius, delay, mana/BP, class requirements, effect IDs, action/animation, and protocol layout.
- Treat localized names/tooltips as presentation, never authority.
- Validate BMD file length, record count, checksum, struct layout, and index bounds before loading.

## Handle effects and persistence

- Use `EffectManager` as the single server owner for stacking/group replacement, stat insertion/removal, duration, periodic damage, visibility, party/view packets, and save policy.
- Distinguish tick-count durations from absolute-expiry effects. Preserve the one-second effect processing cadence unless intentionally migrated.
- Define refresh, replace, weaker/stronger comparison, coexistence, dispel, death, map change, disconnect, and reconnect behavior for every effect.
- Recalculate derived character attributes exactly once at the correct insertion/removal boundary.
- Persist only effects whose configuration and serialization contract mark them saveable; reject expired or invalid saved effects on load.
- Treat DOT source identity, shield split, absorb/heal, kill credit, death, and reward attribution as server-owned.
- Master-skill upgrades mutate live state and are saved through DataServer. Verify disconnect/crash behavior and never acknowledge a durable guarantee that the current path does not provide.

## Verification matrix

Test representative classes, base/master variants, weapon states, buffs, and maps across:

- normal hit, miss, critical, excellent, double, combo, reflect, immunity, defense ignore, HP/SD split, and death;
- PvE, PvP, duel, party/guild/Gens, safe zone, event map/phase, summoned monster, and invalid target;
- exact range edge, outside range, coordinate spoof, duplicate target, oversized count, truncated target list, replay, rapid repeat, tick wrap, and reconnect;
- insufficient mana/BP/ammo, invalid equipment/class/level, unknown skill, unlearned skill, invalid master level, and corrupted persisted skill/effect;
- buff add, weaker/stronger refresh, group replacement, expiry, dispel, death, map move, logout/login, periodic damage, and source disconnect;
- client/server configuration mismatch, skill/effect data drift, packet size/layout drift, and maximum IDs.

Capture server-side intermediate values and final packets. Compare client animation/UI against server results, but use GameServer state and persisted reload as the verdict.

## Change discipline

Separate presentation fixes, validation hardening, formula corrections, data changes, and protocol migrations. Make one behavior change at a time, preserve a rollback path, and document confirmed behavior, chosen configuration, formula vectors, packet parity, tests, and unresolved Windows/runtime checks.
