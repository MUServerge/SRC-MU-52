# LookAndFeel5 responsive resolution: stage 1

Date: 2026-07-15

## 1. Root cause

The active desktop branch selected LookAndFeel5 before the normal `ScreenType` branches in
`source/WINHANDLE.cpp::CWINHANDLE::InitSize` and forced both `g_fScreenRate_x` and
`g_fScreenRate_y` to `1.8f`. `WindowWidth / rate` and `WindowHeight / rate` therefore changed
the logical extent while the physical size of a logical UI unit stayed fixed. The HUD and every
other scaled Look5 caller consequently retained effectively fixed physical dimensions as the
drawable size changed. At 800x600 the 640-unit HUD also exceeded the 444.44-unit logical width.

The defect was not in texture UV calculation. `NewUICommon.cpp::RenderImageF` already converts
source-pixel atlas rectangles to normalized UVs independently of rendered geometry.

## 2. Active coordinate system and design canvas

The active Look5 design canvas is 640x480 logical units.

Evidence:

- `NewUIMainFrameWindow.cpp` defines `LOOK5_EXP_W = 640` and centers the HUD through
  `pos_center` / `PositionX_In_The_Mid`, whose base canvas is 640.
- HUD bottom/center macros in `CGMFrame.h` use a 640x480 authored canvas.
- The Look5 base atlas is 1224x180 source pixels (`Client/Data/Interface/HUD/Look-5/UI_HUD_Base.png`)
  but is rendered as 558x82 logical geometry. The atlas size is therefore a source rectangle, not
  the design canvas.
- AG/SD source textures are 346x11 and are rendered with independent logical dimensions derived
  from the HUD frame. Orb sheets are 912x1520 with 152x152 animation cells. These remain UV/source
  data and are not resolution-scaled as UVs.

The selected policy is a uniformly scaled 640x480 safe canvas with an extended logical extent in
the unused aspect-ratio dimension. Existing `CGMFrame.h` anchors continue to place left/right and
top/bottom elements against the extended drawable edges; centered elements use the centered safe
canvas. This preserves the active widescreen behavior without independently stretching X and Y.

## 3. Coordinate pipeline

### Before

1. `Resolutions.xml` supplies the configured drawable width, height, and font size.
2. `CWINHANDLE::SetDisplayIndex` calls `InitSize`.
3. Look5 overwrites both rates with `1.8f`.
4. `GetScreenX/Y` expose `WindowWidth/1.8` and `WindowHeight/1.8`.
5. `RenderImageF` passes logical geometry to `RenderBitmap`; `RenderBitmap` multiplies geometry by
   the rates while UVs stay normalized.
6. `CUIRenderTextOriginal::RenderText` multiplies logical position/box dimensions by the rates.
7. `WM_MOUSEMOVE` divides physical mouse coordinates by the rates; `CheckMouseIn` compares those
   logical coordinates with logical control rectangles.

### After

1. Configured/client drawable dimensions still enter through the same owner.
2. Look5 computes one uniform scale:

   `uiScale = min(drawableWidth / 640, drawableHeight / 480)`

3. Both legacy screen-rate adapters receive that same scale. Look1-Look4 retain their old branches.
4. Logical visible extent remains:

   `virtualWidth = drawableWidth / uiScale`

   `virtualHeight = drawableHeight / uiScale`

5. Centered safe-canvas offsets are explicit:

   `safeOffsetX = (drawableWidth - 640 * uiScale) / 2`

   `safeOffsetY = (drawableHeight - 480 * uiScale) / 2`

6. `Look5DesignToScreenX/Y` and `Look5ScreenToDesignX/Y` expose exact forward/inverse centered
   transforms. Existing render/text/mouse APIs remain adapters over the same uniform rate.
7. A non-minimized `WM_SIZE` recomputes the transform, refreshes logical mouse coordinates, invokes
   the established `RenderFrameUpdate` cache/layout invalidation, and resizes the existing UI 3D
   cameras. The existing option-window resolution path continues to perform the same layout and
   3D-camera refresh after `SetDisplayIndex`.

Text/thin-line pixel snapping is unchanged in stage 1. Existing integer text origins remain the
pixel-snapping boundary after logical-to-physical conversion.

## 4. Exact active owners

| Stage | Active owner |
|---|---|
| Configured resolutions | `Client/Data/Resolutions.xml`; `source/Resolutions.cpp` |
| Physical drawable dimensions | `source/ZzzOpenglUtil.cpp` `WindowWidth/WindowHeight` |
| Look5 scale and logical extent | `source/WINHANDLE.cpp::InitSize` |
| Safe offset / forward / inverse transform | `source/WINHANDLE.cpp` Look5 conversion methods |
| Startup and selected mode | `source/Winmain.cpp`; `source/WINHANDLE.cpp::SetDisplayIndex` |
| Runtime option change | `source/NewUIOptionWindow.cpp::change_resolution` |
| Resize invalidation | `source/WINHANDLE.cpp::WndProc(WM_SIZE)` |
| Window anchors/layout | `source/CGMFrame.h`; individual `EventOrderWindows` handlers |
| Texture geometry and UV conversion | `source/NewUICommon.cpp::RenderImageF`; `source/ZzzOpenglUtil.cpp::RenderBitmap` |
| Text geometry | `source/UIControls.cpp::CUIRenderTextOriginal::RenderText` |
| Mouse inverse / hit tests | `source/WINHANDLE.cpp::WndProc(WM_MOUSEMOVE)`; `source/NewUICommon.cpp::CheckMouseIn` |
| OpenGL UI viewport/projection | `source/ZzzOpenglUtil.cpp::BeginBitmap` |
| Scissor | direct callers, currently notably `source/NewUIMiniMap.cpp` |
| Embedded 3D UI | `source/NewUI3DRenderMng.cpp::CNewUI3DCamera::Render` |

`GetScreenWidth()` in `ZzzInventory.cpp` is not the drawable width. It subtracts occupied right-side
NewUI windows and must not be used as a transform input.

## 5. Local reference comparison

Reference: `C:/Users/hatim/Desktop/Takumi12 - 5.2/Source/Source/Main5.2/source`.

| Area | Reference | Active branch | Classification |
|---|---|---|---|
| Resolution initialization | Central `CWINHANDLE::InitSize` | Same owner | Reusable boundary |
| Scale calculation | `ScreenType 0`: independent 640/480 scale; `ScreenType 1`: uniform minimum | Look5 bypassed all modes with a fixed value | Uniform-minimum math reusable; independent X/Y is incompatible due distortion |
| Virtual extent | Drawable divided by current rates | Same | Reusable |
| Viewport/projection | Full drawable `BeginBitmap`; physical `glViewport` | Same, plus unrelated local renderer diagnostics | Reusable; renderer edits preserved |
| Bitmap conversion | Logical geometry scaled in `RenderBitmap`; normalized UVs | Same | Reusable |
| Text conversion | Logical text positions/boxes multiplied by screen rates | Same with local antialias/cache changes | Reusable; local changes preserved |
| Mouse conversion | Physical mouse divided by rates | Same | Reusable adapter |
| Window reflow | Option change calls `RenderFrameUpdate` | Same | Reusable |
| Window resize | No Look5-specific responsive invalidation | Added only for active Look5 | Reference obsolete/incomplete for this requirement |
| Look5 branch/assets | No equivalent fixed-scale Look5 policy | Later local Look5 HUD and assets | Reference is incompatible as a source replacement |

No reference source or asset was copied.

## 6. Files changed

- `source/WINHANDLE.cpp`: dynamic Look5 scale, safe-area conversion implementation, resize
  recomputation and existing cache invalidation.
- `source/WINHANDLE.h`: named 640x480 Look5 design constants and conversion API. No data member,
  vtable order, packet, packed structure, or serialized layout changed.
- `docs/LOOKANDFEEL5_RESPONSIVE_RESOLUTION_STAGE1_2026-07-15.md`: this report.
- Workspace `CHANGELOG.txt`: delivery record.

## 7. Affected Look5 surface

Because `g_fScreenRate_x/y` are shared legacy adapters, the scale correction affects all Look5
callers that use scaled bitmap/color/text helpers and logical mouse coordinates. The representative
verified source path is the main HUD:

- main frame/base and decorations;
- life/mana orb geometry;
- AG/SD tracks and fills;
- normal/Master EXP layout only (no EXP calculation or animation logic change);
- Look5 helper/shop buttons and their common button hit rectangles;
- item/skill hotkey geometry that uses the same logical helpers;
- HUD text rendered through `g_pRenderText`.

The expanded regression scope also includes common NewUI windows, chat, notifications, inventory
families, tooltips, modal windows, native edit controls, minimap clipping, and 3D previews whenever
they use the shared rates. Look1-Look4 do not enter the new scale or resize branch.

## 8. Remaining fixed or mixed-coordinate islands

These are not migrated in stage 1:

- Newer Look5 additions in `CGMInvasionManager.cpp`, `HudTooltip.cpp`, and `ScaleForm.cpp` manually
  multiply some text coordinates by `g_fScreenRate_x/y` before calling `g_pRenderText`, whose
  implementation already applies those rates. They require focused render/input parity work.
- `NewUIMiniMap.cpp` calls `glScissor` directly. Its rectangle space and one-time Y inversion must be
  proven and converted through a shared scissor helper before changing it.
- `NewUIHeroPositionInfo.cpp` and `NewUIItemExplanationWindow.cpp` contain hardcoded physical
  resolution branches.
- Native Win32 edit-control placement in `UIControls.cpp` mixes logical coordinates, physical
  control sizes, and window offsets and needs a dedicated audit.
- Some world overlays and `CGMFontLayer` intentionally operate in physical pixels. They must remain
  explicit and must not be converted twice.
- Existing `BeginOpengl`/`EndOpengl` pushes and pops matrices; UI 3D camera rendering restores the
  full UI viewport by calling `BeginBitmap` afterward. A later preview-focused stage should verify
  scissor/blend/depth/camera state across every early return rather than rewriting it here.

## 9. Static resolution matrix

The real supported list comes from `Client/Data/Resolutions.xml` and spans 800x600 through
3840x2160, including 4:3, 5:4, 16:10, 16:9/near-16:9, near-2:1, and 3440x1440 ultrawide.

| Mode | Aspect case | Scale | Logical extent | Physical centered safe offset |
|---|---|---:|---:|---:|
| 800x600 | smallest, 4:3 | 1.25 | 640x480 | 0,0 |
| 1024x768 | 4:3 | 1.60 | 640x480 | 0,0 |
| 1280x1024 | 5:4/non-native | 2.00 | 640x512 | 0,32 |
| 1280x800 | 16:10 | 1.667 | 768x480 | 106.67,0 |
| 1366x768 | near 16:9 | 1.60 | 853.75x480 | 171,0 |
| 1920x1080 | 16:9 high-resolution | 2.25 | 853.33x480 | 240,0 |
| 1920x1200 | 16:10 | 2.50 | 768x480 | 160,0 |
| 2560x1440 | 16:9 | 3.00 | 853.33x480 | 320,0 |
| 3440x1440 | ultrawide | 3.00 | 1146.67x480 | 760,0 |
| 3840x2160 | largest, 16:9 | 4.50 | 853.33x480 | 480,0 |

The formulas were evaluated for every configured mode. Runtime visual/input validation remains
pending because the built `Client_2/Main.exe` was not copied into the authoritative runnable
`Client` tree.

### Manual runtime checklist

For 800x600, 1280x1024, 1280x800, 1366x768, 1920x1080, 3440x1440, and 3840x2160, test windowed and
fullscreen startup. In one session switch between at least 800x600, 1366x768, and 1920x1080.
Where the host permits a client-area resize, resize without restarting.

At each mode capture a screenshot and verify HUD/base, life/mana/AG/SD, normal and Master EXP,
button hover/click, helper gear, skill/item hotkeys, text clipping, chat, notifications, window
drag/bounds, tooltips, inventory/equipment preview, Master Tree, minimap scissor, and modal focus.
Open zero, one, two, and three right-side inventory-family windows. Confirm no stale rectangles after
each change and no OpenGL state leak around 3D previews.

## 10. Build and compatibility evidence

- Solution: `Source/Main5.2/Main.sln`
- Configuration/platform: `Release|x86` mapped to `Release|Win32`
- Successful command used `/m:1` after a parallel link attempt transiently missed
  `Release/CCRC32.obj`.
- Output: `C:/Users/hatim/Desktop/SRC 5.2 BASE/Client_2/Main.exe`
- SHA-256 at verification: `C3157B82D4046E3244AFBDAA45EE2A1496A48BBBF75E6397889EA44848B86570`
- New warnings from this change: none.
- Existing warnings observed: output-directory trailing slash (`MSB8004`), macro redefinitions
  (`C4005`), code-page characters (`C4566`), signed/unsigned comparisons (`C4018`), unused locals
  (`C4101`), and constant truncation (`C4309`).

No packet, gameplay, EXP formula, item-slot, bitmap/texture ID, atlas registration, UV identity, or
asset file changed. Existing shader/VBO/profiler diagnostics were preserved. Nothing was deployed,
committed, pushed, or copied to the runnable client.

## 11. Remaining migration stages

1. Correct manually double-scaled Look5 text/tooltip additions and derive their visual/input bounds
   from one logical rectangle.
2. Migrate chat, buffs, notifications, party/pet, and minimap scissor through explicit coordinate
   helpers.
3. Migrate inventory/character/storage/trade/shop grids, window bounds, dragging, tooltips, native
   edit controls, and item/character previews.
4. Migrate Master Tree and modal/scrollable windows, then remove only proven redundant resolution
   branches.
5. Run the full screenshot/input/DPI/windowed/fullscreen matrix and record frame-time/draw-call
   baselines before making any performance claim.
