---
name: mu-main52-protocol-persistence
description: End-to-end network protocol, session, authentication, reconnect, service routing, and database persistence workflow for the MUServerge/main-5.2 client and MUServerge/SRCMainGS server suite. Use for C1/C2/C3/C4 packets, head/subhead handlers, WSclient and TranslateProtocol, GameServer ProtocolCore, DataServer/JoinServer/ConnectServer links, packet encryption and serials, login or map-server movement, reconnect, character or inventory saves, warehouses, SQL, schema or packet migrations, duplicate/replay prevention, and client-server packet parity.
---

# MU Main 5.2 Protocol & Persistence

## Core rule

Treat every packet and persisted field as a compatibility contract. Trace the complete path before changing one endpoint. Keep GameServer authoritative, preserve original MU behavior, and reject untrusted client input before casting or using it.

Apply `$mu-main52-engineering` for project-wide invariants and `$mu-main52-refactor` when restructuring touched legacy code. Load the relevant domain skill for character, item, reward, map, monster, or event semantics.

## Establish the active system

1. Record the exact client and service build configurations, update macros, architecture, and enabled feature flags.
2. Prove which transport is compiled and used at runtime. Do not infer it from unused source.
3. Identify the authoritative owner of the state: client presentation, GameServer gameplay/session, JoinServer account session, DataServer persistence, or ConnectServer routing/update.
4. Read [protocol-topology.md](references/protocol-topology.md) for verified entry points and known parallel paths.
5. Read [persistence-map.md](references/persistence-map.md) for save ownership, service boundaries, and known consistency risks.

Do not introduce a third transport. `NEW_PROTOCOL_SYSTEM` is an incomplete ASIO wrapper around legacy payloads in the inspected client and is disabled in the current project definition. Harden and prove compatibility before enabling or extending it.

## Trace one operation end to end

For each operation, build a parity row containing:

- direction and lifecycle state;
- C1/C2/C3/C4 framing, encryption, XOR, serial, head, and subhead;
- exact, minimum, or variable packet size;
- count field, entry size, and maximum count for variable arrays;
- client struct, sender/receiver, and state mutation;
- GameServer struct, dispatcher, validation, authority check, and domain handler;
- DataServer, JoinServer, or ConnectServer request/response and database operation;
- version guards and packing assumptions;
- retry, duplicate, timeout, disconnect, and rollback behavior.

Trace request, frame/encrypt, receive/decrypt, dispatch, authorization, domain mutation, persistence, response, and client presentation. Never approve a one-sided packet change.

## Validate before interpretation

At every trust boundary:

1. Require enough bytes for the relevant header before reading it.
2. Validate protocol code and declared size against available bytes and configured maximums.
3. Validate head/subhead before selecting a concrete type.
4. Require `size >= sizeof(FixedPart)` before any cast or copy.
5. Validate variable payloads with overflow-safe arithmetic: `fixed + count * entry == declared_size`, plus an explicit maximum count.
6. Reject truncated, oversized, trailing, malformed, or impossible packets consistently.
7. Validate object index, connection state, account/character ownership, map, slot, item, currency, and permissions on the authoritative server.
8. Validate expected encryption, serial, ordering, rate, and replay behavior. Rate checks do not replace structural validation.

Prefer a checked packet view or decode adapter over direct `BYTE*`-to-struct casts. Keep raw wire structs trivially copyable and standard layout. Preserve packing, signedness, field order, field widths, endianness, enum values, and x86 ABI; add targeted `static_assert(sizeof(...))` and layout checks where safe.

## Protect session and reconnect state

Model transitions explicitly: ConnectServer selection, GameServer socket, JoinServer authentication, character list, character load, world entry, map-server move, reconnect, and final disconnect.

- Bind every transition to the expected account, character, server, connection generation, and short-lived authorization material.
- Make map-move and reconnect credentials single-use or replay-resistant where protocol compatibility allows.
- Expire stale reconnect records and prevent restoration into a different account, character, server, or duplicate live session.
- Define cleanup ownership for partial login, failed map move, service timeout, and socket loss.
- Never log passwords, auth codes, personal data, or full sensitive packets. Minimize credential lifetime and clear temporary copies when practical.

## Protect persistence and economy

- Map each mutable field to one authoritative in-memory owner and one persistence owner.
- Treat item, currency, reward, trade, warehouse, mix, shop, mail, and event claims as duplication-sensitive operations.
- Make retries idempotent with an operation identity or a proven state precondition. A repeated response or reconnect must not repeat the mutation.
- Define transaction boundaries for coupled writes. If legacy code performs multiple independent updates, document the crash window and recovery invariant instead of claiming atomicity.
- For save fan-out, record which sub-save can fail and how load reconciles partial state.
- Use bound SQL parameters for identifiers and values. Do not interpolate account, character, guild, or user-controlled text into SQL.
- Make warehouse and character ownership locks valid across every DataServer instance that can serve the same identity; a process-local container is not a distributed lock.
- Do not change packet layouts or database schemas without a version/handshake plan, migration order, backward-compatibility window, and rollback path.

## Verification matrix

Test the normal path and at least:

- zero-length, truncated, oversized, bad header, wrong declared size, count overflow, and extra trailing bytes;
- unknown head/subhead, wrong encryption, wrong serial, duplicate, replayed, delayed, and out-of-order packets;
- disconnect during login, character load/save, map move, trade, warehouse, mix, reward claim, and service response;
- duplicate login, stale reconnect, expired map auth, service restart, and repeated save/request;
- client/server update-macro mismatch and raw-struct size mismatch;
- database timeout, partial multi-write failure, and retry without item/currency duplication.

Build both client and affected services. Compare packet bytes or structured traces across both endpoints. Verify reconnect and persistence by reloading from storage, not only by observing current memory.

## Change discipline

Keep changes small and paired across endpoints. Separate framing/validation hardening from packet-layout or schema migrations. Preserve a compatibility fallback until the new path is verified. Report confirmed facts, unresolved build/runtime assumptions, risks, and the exact verification performed.
