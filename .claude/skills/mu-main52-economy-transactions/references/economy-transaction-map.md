# Economy and transaction architecture map

## Scope and authority

- Client: `MUServerge/main-5.2`, Season 5.2 Win32/x86 UI, local preview, packet requests, and authoritative response reconciliation.
- Server reference: `MUServerge/SRCMainGS`, especially `Source/GameServer/GameServer` and `Source/DataServer/DataServer`.
- The shared server repository is an older public reference. Re-run every finding against the user's current desktop server branch before changing production behavior.

## Primary client entry points

- Player trade: `source/NewUITrade.*`; sends trade item, money/coin, accept, and cancel requests and resets confirmation when an offer changes.
- Inventory movement: `source/NewUIInventoryCtrl.*`, `NewUIInventoryCore.*`, `NewUIMyInventory.*`, and `ZzzInventory.*`.
- Warehouse: `source/NewUIStorageInventory.*` and `NewUIStorageExpansion.*`; local fee display is not authoritative.
- Chaos mix: `source/NewUIMixInventory.*`, `MixMgr.*`, `CB_GetMixRate.*`, and mix message boxes. The client predicts recipe/rate/Zen for UI only.
- NPC shop: `source/NewUINPCShop.*`.
- Personal shop: `source/NewUIMyShopInventory.*`, `NewUIPurchaseShopInventory.*`, and `PersonalShopTitleImp.*`.
- Cash shop: `source/GameShop/InGameShopSystem.*`, `GameShop/NewUIInGameShop.*`, message boxes, and `GameShop/ShopListManager/*`.
- Lucky coin exchanges: `source/NewUIExchangeLuckyCoin.*` and `NewUIRegistrationLuckyCoin.*`.
- Packet ownership: start at `source/ProtocolSend.*`, `Protocol.*`, and `Protocol.h`; confirm structure size and conditional fields against GameServer headers.

## Primary GameServer owners

- `Trade.*`: request/response, offer money/coins, confirmation delay, two-inventory exchange, commit/rollback, result packets, and logs.
- `User.cpp`: `gObjInventoryTransaction`, `gObjInventoryCommit`, and `gObjInventoryRollback`; these switch between primary and backup inventory arrays/maps.
- `ItemManager.*`: inventory insertion/deletion, trade movement, maps, serial-preserving item packets, and persistence-facing item state.
- `ItemMove.*`: allow/drop/sell/trade/vault policy. Reuse it instead of adding UI-only prohibitions.
- `Warehouse.*`: Zen deposit/withdrawal, password/lock, tax, load/save/close, personal and guild warehouse flows.
- `PersonalShop.*`: price setup, shop state, item listing, buyer validation, seller guard, Zen/coin/jewel settlement, item transfer, save, and notifications.
- `Shop.*` and `ShopManager.*`: NPC shop inventory and configured server values.
- `ChaosBox.*`, `CustomMix.*`, `CustomWingMix.*`, `JewelMix.*`, and `CMixGoblinExpansion.*`: server recipe validation, rate, costs, RNG, consumption, result, and logs.
- `CashShop.*`: coin lookup, purchase/gift/use callbacks, inventory/gift storage, periodic items, and DataServer requests.
- `LuckyCoin.*` and `CustomExchangeCoin.*`: alternate currency registration/exchange.
- `ItemValue.*` and `ItemValueTrade.*`: item value and suspicious/limited trade checks.

## Primary DataServer owners

- `Warehouse.*`: personal/guild warehouse load, binary item blob, money/password, save, and use tracking.
- `PersonalShop.*`: persisted personal-shop values and insert/delete updates.
- `CashShop.*`: point balances, debit/credit, purchases, gifts, storage, and periodic items.
- Character and inventory saves are also part of transaction durability; trace the relevant `GD*`/`DG*` callbacks and SQL operations rather than assuming a `SaveSend` is synchronous.

## Verified legacy transaction behavior

- Trade starts inventory snapshots with `gObjInventoryTransaction` for both participants. Item moves target the transaction copies. Success calls `gObjInventoryCommit`; cancellation/failure uses `gObjInventoryRollback` through `CTrade::ResetTrade`.
- Trade money/coin offer changes clear confirmation and set a six-second re-confirmation delay. Preserve this anti-scam behavior.
- Inventory commit covers items/maps, not an atomic database commit with Zen or custom coins. `GDCharacterInfoSaveSend` and `GDSetCoinSend` are separate persistence messages.
- Warehouse Zen withdrawal includes a level/lock-derived tax; the client displays a matching estimate, but GameServer recomputation is authoritative.
- Chaos mix client readiness and rate are advisory. `CChaosBox::CGChaosMixRecv` and the selected server mix routine own the actual outcome.

## Confirmed public-reference audit findings

These are static findings in the public `SRCMainGS` branch, not yet patched and not yet revalidated against the user's newer desktop server code.

1. **Personal-shop seller guard can remain stuck.** In `GameServer/PersonalShop.cpp::CGPShopBuyItemRecv`, `lpTarget->PShopTransaction` is set to `1` before buyer inventory insertion. If `InventoryInsertItem` returns `0xFF`, the function returns without resetting the flag. Every acquired guard needs failure cleanup.
2. **Wrong balance packet recipient in the custom-currency personal-shop branch.** After default/Zen buyer debit, the code calls `GCMoneySend(bIndex, lpTarget->Money)` instead of sending the buyer's new balance to `aIndex`. A later seller update also targets `bIndex`; the buyer can retain stale displayed Zen until another sync.
3. **Trade coins are not one durable atomic commit with items.** Item inventories and Zen are finalized before asynchronous `GDSetCoinSend` calls. A DataServer failure or disconnect boundary requires explicit testing and reconciliation; do not describe this flow as database-atomic.
4. **Guild warehouse exclusivity needs deployment-level verification.** The DataServer file has SQL `InUse` updates/checks commented in places and also maintains an in-process guild-use vector. Confirm behavior with multiple DataServer/GameServer processes and abnormal disconnects before relying on it for duplicate prevention.

## Conservation ledger template

For every operation record:

| Asset | Source before | Destination before | Reserved | Source after | Destination after | Persisted owner | Evidence |
|---|---:|---:|---:|---:|---:|---|---|
| Item serial/options | | | | | | | |
| Zen | | | | | | | |
| Coin 1/2/3 | | | | | | | |
| Jewel counts/stacks | | | | | | | |
| Tax/commission sink | | | | | | | |

The expected delta must be zero except for an explicitly configured tax/commission sink, mix consumption/result, NPC source/sink, or cash-shop grant.

## Review questions

- Can two handlers act on the same source item or seller simultaneously?
- Does every early return release every acquired flag and restore the correct inventory pointer?
- Is the destination capacity checked for the exact item dimensions and any jewel change/payment items?
- Are price, item serial, slot, target name/index, currency type, commission, and balance revalidated from live server state?
- Are item and currency mutations ordered so rollback cannot duplicate one side or destroy the other?
- What happens when the client retries after a lost response?
- Which state is only in memory, which is queued to DataServer, and which is confirmed in SQL?
- Do reconnect and server-change paths reconcile inventory, balances, shop locks, and warehouse-use state?
