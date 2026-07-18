---
name: mu-main52-items
description: Item architecture workflow for the MUServerge/main-5.2 MU Online Season 5.2 Win32 C++ client. Use for ITEM layout and serialization, item IDs, inventory, equipment, durability, normal options, excellent options, ancient or set options, socket and seed options, harmony, 380 options, mixes, tooltips, prices, and item-derived character stats.
---

# MU Main 5.2 Items

## Map an item end to end

1. Read `references/item-option-map.md` before changing items or options.
2. Trace raw bytes/packet -> `ITEM` decode -> canonical option interpretation -> stat/value calculation -> tooltip/UI -> model/effect.
3. Build an option matrix for every affected family: base, level, skill/luck/additional, excellent, ancient/set, socket/seed, harmony, and 380.
4. Find and reuse existing decoding, calculation, pricing, tooltip, inventory, and mix code. Do not create parallel option logic.

## Preserve binary meaning

- Treat item type, level, durability, option bits, set fields, socket slots, and packet serialization as compatibility contracts.
- Keep one canonical interpretation of each bit and byte; presentation code must consume it rather than reinterpret raw fields independently.
- Preserve option stacking order, caps, signedness, widths, sentinel values, and equipped-slot semantics.
- Treat item generation, ownership, drop results, eligibility, and reward grants as server-authoritative.
- Maintain x86 ABI and data-file compatibility. Do not change struct layout or file formats without end-to-end proof.

## Change safely

- Use `CSItemOption` for ancient/set behavior and `SocketSystem` for socket/seed behavior where applicable.
- Trace any option change into `CHARACTER_MACHINE` recalculation and tooltip rendering; both must agree.
- Prefer a shared decoder or query API when duplicated bit logic is proven equivalent, introduced incrementally behind existing callers.
- Test empty/default items, boundaries, mixed option families, comparison tooltips, equip/unequip, durability zero, trade/storage, and packet round trips.
- Preserve original MU balance and visual semantics unless explicitly asked to change them.

## Report evidence

- Document the storage field, decoder, calculation consumer, tooltip consumer, and network source for each option.
- Mark unknown encodings as unresolved; never infer an item bit layout from UI text alone.
- Add verified mappings to `references/item-option-map.md`.
