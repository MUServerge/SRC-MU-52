---
name: mu-main52-social-systems
description: "Social-system architecture workflow for the MUServerge/main-5.2 client and MUServerge/SRCMainGS server. Use for party and party matching, guilds, alliances and unions, guild war, chat and whisper, friends, mail and chat rooms, duel and spectators, Gens membership and ranking, social UI, packets, persistence, disconnect cleanup, or cross-server social state."
---

# MU Main 5.2 Social Systems

Keep social state server-authoritative and trace every feature across UI, packet routing, GameServer state, DataServer persistence, and disconnect cleanup. Read [references/social-systems-map.md](references/social-systems-map.md) before substantial work.

## Trace the whole lifecycle

1. Find the client UI/cache, request sender, and `WSclient.cpp` receive handler.
2. Trace packet header/subcode and fixed-width fields into the GameServer dispatcher and feature owner.
3. Continue to DataServer and SQL whenever membership, application, memo, rank, relationship, or matching state survives a session.
4. Identify the authoritative identity and state at every step: account, character, guild number/name, party number, server code, connection index, and generation/session.
5. Trace success, rejection, duplicate request, timeout, target disconnect, requester disconnect, map-server move, reconnect, and service restart.
6. Search for existing membership, permission, broadcast, matching, presence, text-filter, rate-limit, persistence, and cleanup functions before adding logic.

## Non-negotiable invariants

- Client windows and caches present state; they never authorize membership, rank, relation, duel result, contribution, reward, or presence.
- A connection index is temporary. Validate the current account/character identity before acting on delayed replies or stored target indices.
- Names are fixed-width protocol fields, not universally safe C strings. Enforce termination, encoding, legal length, canonical comparison, and rename implications.
- Membership and leadership changes are single authoritative transitions. Reject stale, repeated, conflicting, full-capacity, self-targeted, or unauthorized operations.
- Validate requester role, target identity and state, limits, guild/party/Gens relationship, map/event restriction, proximity where required, and interface state on the server.
- Every invite, application, pending relation, duel, spectator, chat-room, and matching entry has explicit expiry/cancel/disconnect cleanup.
- Broadcast the resulting authoritative state only after the transition succeeds. Reconcile all affected members, not only the requester.
- Persistent social changes must define GameServer memory, DataServer/SQL, acknowledgement, retry, and reconnect behavior. Do not claim atomicity without proof.
- Text input must be bounded before copy/formatting, filtered by the established rules, and protected against spam, impersonation, control codes, and unintended channel escalation.
- Preserve packet layout, packing, field widths, x86 ABI, legacy encodings, and client-server version parity unless a coordinated migration is intentional.

## Domain rules

### Party and matching

- Preserve slot limits, leader identity, member uniqueness, level/class/Gens restrictions, password and automatic matching semantics.
- Treat party membership as GameServer authority; treat DataServer matching entries as discovery/application state, not live membership.
- Recalculate list, life/viewport presentation, leader changes, loot/combat/event consequences, and cleanup when a member leaves, disconnects, dies, or changes server.

### Guild, alliance, union, and war

- Keep guild number, name, master, member status, emblem, notice, alliance/hostility, and war state consistent across caches, GameServers, DataServer, and SQL.
- Require the correct rank for invite, accept, kick, status change, relation, war, notice, and disband operations.
- Treat guild matching as an application workflow distinct from authoritative membership.
- Route guild warehouse and ownership-changing operations through `$mu-main52-economy-transactions`; route Castle Siege/event rules through the relevant event workflow.

### Chat, whisper, friends, memo, and chat rooms

- Resolve the intended channel and recipients on the server. Verify block state, visibility, online server, guild/party membership, moderation, and rate rules.
- Preserve client text measurement/UI behavior separately from server message validation.
- In this source tree, friend lists, pending friends, presence routing, memo persistence, and related chat-room protocol live in DataServer `CSProtocol`, not JoinServer.
- Keep unread counts, memo ownership, read/delete state, sender identity, subject/body/photo lengths, and recipient notifications consistent after reconnect.

### Duel and spectators

- Model duel as an explicit state machine: idle, requested, accepted/start, rounds, result/cancel, cleanup.
- Validate both fighters, arena, score, death/respawn, timeout, disconnect, and map changes. Prevent stale requests and double completion.
- Spectators observe an active duel but cannot become fighters or receive private state accidentally; remove them when the arena or session ends.
- Route damage, skill, buff, and PvP-rule changes through `$mu-main52-skills-combat`.

### Gens

- Keep family, rank, contribution, reward eligibility/status, victim anti-farm tracking, PvP rules, and map restrictions server-owned.
- Coordinate GameServer live state with DataServer `Gens_Rank` and `Gens_Reward` operations; handle late callbacks and reconnects safely.
- Recompute rank and reward policy from configured/server data. Never grant a reward based only on a client request or displayed rank.

## Cross-domain boundary

- Use `$mu-main52-protocol-persistence` for packet migrations, reconnect/session routing, SQL/schema changes, replay prevention, and service-level persistence.
- Use `$mu-main52-events-rewards` when party/guild/Gens state affects event entry, score, phases, or rewards.
- Use `$mu-main52-ui-lookandfeel5` for social window layout, resolution scaling, fonts, hit testing, and LookAndFeel5 presentation.
- Use `$mu-main52-economy-transactions` for guild warehouse, paid social actions, item/currency rewards, and any ownership transfer.
- Use `$mu-main52-refactor` when cleaning directly touched legacy code; do not redesign all social systems during one feature change.

## Verification matrix

Test success plus: self-target, nonexistent/renamed target, duplicate invite/application, crossed invitations, full group, insufficient role, leader/master leaves, target index reused by another session, requester/target disconnect at every phase, map/server transfer, reconnect, DataServer timeout/restart, repeated/late reply, malformed or unterminated text, maximum-length multilingual text, spam burst, blocked user, and simultaneous relation changes.

For each test compare authoritative GameServer state, DataServer/SQL rows, all affected client caches/windows, broadcasts, logs, and cleanup flags. Mark desktop/server runtime tests pending when only static analysis was possible.

## Handoff

Report the complete client -> GameServer -> DataServer path, authoritative owner, identities and permissions checked, state machine transition, persistence boundary, broadcast recipients, cleanup paths, confirmed defects, compatibility constraints, and remaining concurrency or failure-injection tests.
