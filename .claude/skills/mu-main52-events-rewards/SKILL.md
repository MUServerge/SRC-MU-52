---
name: mu-main52-events-rewards
description: Event and reward architecture workflow for the MUServerge/main-5.2 MU Online Season 5.2 Win32 C++ client. Use for Blood Castle, Devil Square, Chaos Castle, Cursed Temple, DoppelGanger, Empire Guardian, Kanturu, Raklion, seasonal events, event phases, timers, entry UI, score, match results, reward presentation, packets, maps, and event monsters.
---

# MU Main 5.2 Events and Rewards

## Model the event as a state machine

1. Read `references/events-rewards-map.md` before changing event behavior.
2. Trace packet/server time -> event phase/state -> map and monster behavior -> timer/score UI -> result/reward presentation.
3. Write the current phases, transitions, triggers, timeout source, reconnect behavior, and failure states before editing.
4. Search shared match/event systems and the specific event family before creating a timer, counter, result model, or UI flow.

## Enforce authority boundaries

- Treat entry eligibility, phase timing, score, success/failure, ranking, reward selection, item/currency grant, and drops as server-authoritative.
- The client may request entry and display results; it must not grant rewards or become the source of truth.
- Use server-synchronized timing where the protocol provides it. Avoid independent clocks that drift or duplicate an existing countdown.
- Validate packet length, counts, enum ranges, and state transitions before consuming result data; fail closed on malformed data.
- Preserve event IDs, map IDs, packet layouts, transition order, timing, and original gameplay.

## Change safely

- Reuse shared state/result UI in `CSEventMatch` and existing event systems where behavior matches.
- Keep event rules, presentation, monster specialization, and reward display separated even if legacy files mix them.
- Treat actual reward tables or grant logic as a server-repository dependency when absent from this client.
- Test entry/denial, countdown, active phase, success, failure, timeout, reconnect, map exit, repeated events, ranking bounds, and missing/late packets.
- Refactor only the touched event path and prove transition parity before consolidating events.

## Report evidence

- Provide an event matrix: phase owner, packet, timer source, map module, monster path, UI, result, and reward authority.
- Clearly label client-only presentation, verified protocol behavior, and server-side unknowns.
- Add verified mappings to `references/events-rewards-map.md`.
