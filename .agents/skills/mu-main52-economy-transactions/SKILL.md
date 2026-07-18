---
name: mu-main52-economy-transactions
description: "Economy and transaction workflow for the MUServerge/main-5.2 client and MUServerge/SRCMainGS server. Use for player trade, inventory transfers, warehouse and guild warehouse, NPC and personal shops, cash shop, chaos mixes, Zen, WCoin and custom coins, jewels as currency, taxes, prices, persistence, rollback, duplicate-item prevention, lost-item incidents, or any operation that changes item or currency ownership."
---

# MU Main 5.2 Economy and Transactions

Treat every ownership or balance change as a server-authoritative transaction. Read [references/economy-transaction-map.md](references/economy-transaction-map.md) before substantial work.

## Trace the complete operation

1. Identify the initiating client UI and request packet.
2. Trace GameServer dispatch, validation, lock/state flags, item and currency calculation, mutation, persistence request, response packet, and client reconciliation.
3. Include DataServer/SQL when state survives disconnects or crosses GameServer processes.
4. Record the owner of each item and balance before, during, and after success, rejection, timeout, disconnect, and partial failure.
5. Search for the existing inventory transaction, item insertion/deletion, money-limit, coin-save, item-move policy, logging, and save functions before adding logic.

## Non-negotiable invariants

- The client may preview price, tax, rate, capacity, or eligibility, but the server must recompute and authorize it from current state.
- One item serial has one authoritative owner and location. Preserve item bytes, options, durability, sockets, set data, slot maps, and inventory pointer generation together.
- Debit and credit use the same validated price, currency type, commission, and item instance. Never trust client-supplied item identity, value, quantity, target, or success rate.
- Validate every participant, interface state, range, target relation, source slot, destination capacity, ownership, trade/vault restrictions, balance, caps, and lock before destructive mutation.
- Any item or offer change invalidates both parties' confirmation. Preserve the anti-scam confirmation delay.
- Every acquired transaction or shop/vault lock is released on every exit path. Prefer scoped cleanup or one explicit cleanup path over scattered flag resets.
- Do not report success to the client before the authoritative in-memory transition is complete. Mark asynchronous persistence boundaries explicitly; do not claim atomicity across GameServer and DataServer without proof.
- Retried, duplicated, reordered, or late packets must not repeat a debit, credit, reward, mix, purchase, or item transfer.
- Use checked widths and signedness for Zen, coin, price, tax, count, and intermediate multiplication. Enforce both insufficient-funds and maximum-balance limits.
- Log stable transaction evidence: accounts/characters, operation, item serial and full option identity, source/destination, currency deltas, result, and correlation key where available.

## Review transaction ordering

For each flow, build a ledger with these phases:

1. `Validate`: packet shape, state, identities, immutable offer snapshot, limits, and capacity.
2. `Reserve`: acquire existing interface/inventory/shop/warehouse guards and snapshot rollback state.
3. `Plan`: compute all item moves and every debit/credit without mutating live ownership.
4. `Apply`: execute each planned mutation exactly once inside the proven transaction boundary.
5. `Commit`: commit inventory state and request required persistence in the established order.
6. `Reconcile`: send authoritative items, balances, UI result, preview, and logs.
7. `Release`: clear all locks, temporary offers, interface state, and rollback buffers on every outcome.

Do not introduce a generic transaction framework until the existing `gObjInventoryTransaction/Commit/Rollback`, personal-shop guard, warehouse-use guard, and DataServer callbacks have been audited. First repair the smallest unsafe seam while preserving packet and database compatibility.

## Domain rules

- **Trade:** treat two inventories and all offered currencies as one logical exchange; validate both directions before commit and reset confirmation whenever the offer changes.
- **Warehouse:** preserve warehouse page/account/guild identity, password/lock state, item map, tax rules, load/save/close ordering, and cross-server exclusivity.
- **NPC/personal shop:** use the server's live item instance and configured value. Lock the seller during purchase; insert, debit, credit, delete, save, notify, and unlock with complete failure cleanup.
- **Chaos mix:** client recipes and rates are presentation. The server owns recipe matching, ingredients, tax, RNG, consumption, result creation, failure degradation, and logs.
- **Cash/custom coins:** trace GameServer to DataServer/SQL callbacks. Separate displayed balance, pending delta, confirmed balance, gift target, periodic item storage, and failure result.
- **Jewels as currency:** count stacked and unstacked forms consistently, validate recipient capacity for change/payment items, and apply commission once.

## Verification matrix

Test success plus: invalid slot, changed serial, insufficient funds, maximum recipient balance, full destination, prohibited item, simultaneous buyers, repeated confirm/buy/mix packet, cancel during operation, disconnect at each phase, DataServer timeout/failure, reconnect, map/server change, and shutdown/save.

Compare inventory bytes/maps, item serial ownership, Zen/coin totals, DB state, packets, and logs before and after. For two-party flows, verify conservation across both parties. Mark desktop/server runtime tests as pending when only static source analysis was possible.

## Handoff

Report the complete client -> GameServer -> DataServer path, authoritative checks reused, mutation and persistence order, cleanup paths, conservation result, confirmed defects, branch/version caveats, and remaining concurrency or failure-injection tests.
