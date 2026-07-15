# Data and Asset Source Map

## Client pipeline

| Area | Primary files | Notes |
|---|---|---|
| Startup registration | `source/ZzzOpenData.*` | Large centralized model/bitmap/world/UI registration surface; audit ID collisions and load order |
| Texture loading | `source/ZzzTexture.*`, `GlobalBitmap.*`, `_TextureIndex.h` | Requested image names, OZ* runtime path, decode/upload, global `Bitmaps[]` metadata |
| BMD models | `source/ZzzBMD.*`, `CGMModelManager.*`, `_enum.h`, `_define.h` | Model storage, meshes/bones/actions, lookup, global numeric ranges |
| Texture scripts | `source/TextureScript.*` | Model/material texture behavior and special cases |
| Terrain/world data | `source/ZzzLodTerrain.*`, `ZzzOpenData.*`, `MapManager.*`, `w_MapProcess.*` | `Terrain.map`, `Terrain.att`, `Terrain.obj`, light/height/mapping textures, camera data |
| Generic/script data | `source/ReadScript.*`, `ScriptItem.*`, `w_BuffScriptLoader.*`, `MoveCommandData.*` | Parsers and gameplay/presentation tables; verify server authority separately |
| Localization | `source/MultiLanguage.*` and `GlobalText` consumers | Encoding, stable numeric indices, format-string compatibility |
| Effects/audio | `source/ZzzEffect*`, `CGMEffectHandle.*`, `CGMItemEffect.*`, `SkillEffectMgr.*`, `DSplaysound.*`, `DSPlaySound.h` | Effect dependencies, pool pressure, sound lifecycle |
| Protection/download | `source/CGMProtect.*`, `ExternalObject/ResourceGuard`, `GameShop/*Downloader*` | Integrity/deployment or downloaded content; do not bypass casually |

## Confirmed static audit note

`source/ZzzOpenData.cpp` registers two different HQ skin paths to `BITMAP_HQSKIN + 8` in the inspected public branch: one near the class-408 registration and another near class-109. The later load can overwrite the earlier bitmap identity. Reconfirm against the desktop branch and intended class mapping before fixing; audit the full arithmetic range for similar collisions.

## Runtime format cautions

- `ZzzTexture.cpp` constructs an `OZJ` path from a requested image name. Search the decoder before concluding that `.jpg` references require loose JPG files.
- `ZzzLodTerrain.cpp` checks/constructs `OZJ` and `OZB` names for terrain resources.
- BMD parsing and encrypted/protected data behavior must be verified from the active client branch and deployed Data files; source-only analysis cannot prove asset contents.

## Server parity

The server does not render client assets, but it owns gameplay data that must agree with client presentation:

- world/map IDs, gates, attributes, and access;
- item/skill/monster/class identifiers and limits;
- event and reward identifiers;
- localized/display tables versus authoritative values;
- packet widths and serialized data layouts.

Use the corresponding domain skill plus `$mu-main52-protocol-persistence` for coordinated migrations.

## Asset audit checklist

1. Enumerate all registrations and produce `ID -> path -> parameters -> owner` and `path -> IDs` views.
2. Flag duplicate IDs with different paths/parameters, out-of-range IDs, unregistered use sites, and loaded-never-used candidates.
3. Treat dynamic construction, arithmetic ranges, compile flags, and world/UI variants as possible uses before deleting anything.
4. Confirm candidate unused assets against the real desktop `Data` directory and runtime instrumentation.
5. Record CPU bytes, decoded bytes, GPU bytes, load count/time, and unload behavior for optimization work.
