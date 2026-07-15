# Item and option architecture map

## Verified entry points

- `source/ZzzInfomation.h`: item conversion, durability percentage, item value, equipment storage in `CHARACTER_MACHINE`.
- `source/_struct.h`: core `ITEM` fields, including socket-related storage; inspect the exact definition before changing layout.
- `source/CSItemOption.h/.cpp`: ancient/set type and option scripts, equipped-set counting, option aggregation, default option text, set UI.
- `source/SocketSystem.h/.cpp`: socket/seed categories, option IDs, equipped-set socket bonuses, tooltip/value calculation, status bonuses.
- `source/ItemTooltip.cpp`, `source/NewUIItemTooltip.cpp`: excellent and other tooltip presentation.
- `source/NewUIItemMng.*`, `source/NewUIInventoryCtrl.cpp`, `source/NewUIMyInventory.*`, `source/ZzzInventory.cpp`: item ownership, movement, equipment, and UI.
- `source/NewUIMixInventory.cpp`: mix-item path.
- `source/wsclientinline.h`: packet decoding and inventory/equipment updates.

## Option audit matrix

For every family, record: raw field/bits | decode function | value formula | character-stat consumer | price consumer | tooltip consumer | serialization source.

- Base type/index and level.
- Skill, luck, and additional option.
- Excellent option mask and individual effects.
- Ancient/set type A/B, set count, mastery/default options.
- Socket slots, seed ID/category/sphere level, socket set bonus.
- Harmony and 380 options when present.
- Durability, requirements, value, trade/storage/mix restrictions.

## Mandatory edge cases

- Empty slot/sentinel values, invalid type, maximum level/durability, all option bits, mixed excellent+ancient/socket cases.
- Equip/unequip and comparison tooltip parity with recalculated stats.
- Packet round trip and reconnect hydration.
- Integer widths in price and bonus calculations; note the conditional 64-bit socket value path.

## Verified server-side reference (`MUServerge/SRCMainGS`)

- `Source/GameServer/GameServer/Item.*`: authoritative `CItem` representation and `Convert` path, including new/excellent, set, harmony, extended, socket options, and socket bonus fields.
- `ItemManager.*`, `ItemOption.*`, `ItemOptionRate.*`: item definitions, requirements, durability, conversion, and option application.
- `SetItemType.*`, `SetItemOption.*`: ancient/set classification and authoritative bonuses.
- `SocketItemType.*`, `SocketItemOption.*`: socket item classification and authoritative socket effects.
- `JewelOfHarmonyOption.*`, `380ItemType.*`, `380ItemOption.*`: harmony and 380 families.
- `ItemBag.*`, `ItemBagManager.*`, `ItemBagEx.*`, `ItemDrop.*`, `CustomEventDrop.*`: authoritative option generation and drops.
- `ItemMove.*`, `ChaosBox.*`, `Protocol.cpp`, and DataServer item persistence paths: movement, mixes, serialization, and storage boundaries.

Pair every client field with the matching `CItem` field and packet encoding before changing bits, sentinels, or option precedence.

## Unresolved until traced

- The repository search did not expose one symbol literally named `AncientOption`; ancient behavior is verified under the set-option system and must be traced by storage fields and scripts.
- Never assign bit meaning from item color or tooltip wording alone.
