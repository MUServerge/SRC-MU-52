# MU Main 5.2 — Desktop Codex Handoff

**თარიღი:** 2026-07-15  
**პრიორიტეტული client:** `MUServerge/main-5.2`  
**server:** `MUServerge/SRCMainGS`  
**reference client:** `0kju0/MuOnline-Main-5.2`  
**პლატფორმა:** Windows · Win32/x86 · C++17 · Visual Studio · OpenGL/GLEW

## რეალური Desktop workspace

Desktop-ზე პროექტი GitHub repository-ის ერთი root-ის სახით არ არის აწყობილი. სამუშაო root არის `SRC 5.2 BASE`, რომელიც სურათის მიხედვით შეიცავს:

```text
SRC 5.2 BASE/
├── .claude/
├── Client/
├── Client_2/
├── MainInfo/
├── MuServer/
├── MuServerTK/
├── SRCMainGS/
├── CHANGELOG.txt
└── CLAUDE.md
```

`MUServerge/main-5.2` და `MUServerge/SRCMainGS` არის საჯარო reference/baseline და არა Desktop layout-ის ზუსტი ასლი. Desktop Codex-მა არ უნდა ივარაუდოს, რომ `Client`, `Client_2`, `MuServer`, `MuServerTK`, `MainInfo` ან `SRCMainGS` ავტომატურად ემთხვევა GitHub-ის შესაბამის საქაღალდეს ან branch-ს.

პირველ რიგში უნდა დადგინდეს:

- რომელ საქაღალდეშია აქტიური client source და `Main.sln`/`Main.vcxproj`;
- `Client` და `Client_2` source, runtime, staging თუ build output-ია;
- რომელი server deployment უკავშირდება აქტიურ `SRCMainGS` build-ს;
- `MainInfo`-ს რეალური generator/config როლი;
- თითო საქაღალდეში დამოუკიდებელი `.git` root, branch და uncommitted ცვლილებები;
- runtime `Data`, `Shaders`, `Data/Effect/VBO`, DLL/EXE და INI/script წყარო;
- `CLAUDE.md`/`.claude`-ში არსებული პროექტის წესები და მათი შესაბამისობა მიმდინარე კოდთან.

არ წაიშალოს, არ გადაიტანოს და არ გაიწმინდოს არცერთი runtime/build საქაღალდე მხოლოდ სახელის საფუძველზე.

## Desktop Codex-ის საწყისი ინსტრუქცია

ამ ფაილის წაკითხვის შემდეგ გამოიყენე `$mu-main52-engineering` და მხოლოდ შესაბამისი სპეციალისტი სკილები. გახსენი `SRC 5.2 BASE` როგორც workspace root და ჯერ შეადგინე ზემოთ ჩამოთვლილი საქაღალდეების responsibility/repository/runtime map. შეამოწმე ყველა აღმოჩენილი worktree და desktop branch, რადგან მათში არის ცვლილებები, რომლებიც საჯარო GitHub branch-ში არ ჩანს. არ გადაწერო ან არ გააქრო მომხმარებლის ცვლილებები. ცვლილებამდე მოძებნე არსებული ფუნქცია/manager/helper და გამოიყენე reuse-first მიდგომა. შეინარჩუნე MU Online-ის gameplay, ვიზუალი, packet/data compatibility და legacy fallback. იმუშავე მცირე, შექცევადი ეტაპებით და ყოველი ეტაპი გადაამოწმე Win32 build-ითა და თამაშში.

## პროექტის საბოლოო მიზანი

- პროექტის სრული client/server არქიტექტურის ათვისება;
- ცუდად დაწერილი, დუბლირებული და unsafe კოდის ეტაპობრივი გასწორება მხოლოდ შეხებულ საზღვარზე;
- CPU/GPU, memory, loading, networking და rendering ოპტიმიზაცია გაზომვადი მტკიცებულებით;
- ახალი ფუნქციის/პაკეტის/manager-ის არ შექმნა, თუ შესაბამისი ოპტიმიზირებული გზა უკვე არსებობს;
- gameplay-ის, timing-ის, visuals-ის, packet-ების, BMD/Data ფორმატებისა და ძირეული MU პრინციპების შენარჩუნება;
- client/server parity: character, items/options, excellent/socket/ancient, monsters, maps/worlds, events/rewards, skills/combat, economy, social systems და persistence.

## დაყენებული სპეციალისტი სკილები

`mu-main52-engineering`, `refactor`, `performance`, `renderer`, `character`, `items`, `monsters`, `events-rewards`, `maps-worlds`, `ui-lookandfeel5`, `protocol-persistence`, `skills-combat`, `economy-transactions`, `social-systems`, `data-assets`, `build-release-qa`.

## პრიორიტეტი 1 — Shader, VBO და OpenGL 3.3

სრული roadmap არსებობს ცალკე ფაილად: `MU_Main_5.2_Shader_VBO_GL33_Roadmap.md`. ცვლილებები ჯერ არ განხორციელებულა.

### დადასტურებული არქიტექტურა

- არსებობს ორი shader სისტემა: `CShaderScene` და `CShaderGL`.
- `CShaderScene`-ში რეალურად გამოიყენება Terrain და Character; Default/Glow/Colorize იტვირთება, მაგრამ საჯარო branch-ში caller არ ჩანს.
- `CShaderGL` ფარავს ძველ საერთო `shader_id` გზას და BMD VBO material shader-ებს.
- ძველი `RenderShader()`/`RenderVertexBuffer()` გზა პრაქტიკულად მკვდარია; საბოლოო წაშლამდე desktop caller/runtime audit აუცილებელია.
- legacy renderer კვლავ იყენებს CPU transforms, fixed-function matrix stack-ს, client arrays-ს, GLU-ს და OpenGL 2 ImGui backend-ს.

### დადასტურებული პრობლემები

1. **Shader state conflict:** Character shader ჩართულია `RenderCharactersClient()` pass-ზე; `RenderMeshVBO()` რთავს material shader-ს და ბოლოს აკეთებს `glUseProgram(0)`. შედეგად manager-ის state და რეალური GL state ერთმანეთს სცილდება და render order-მა შეიძლება განათება შეცვალოს.
2. **GPU resource leak:** mesh VAO/VBO/EBO იქმნება, მაგრამ `BMD::Release()`-ში შესაბამისი `glDeleteBuffers`/`glDeleteVertexArrays` cleanup არ ჩანს.
3. **CPU და GPU skinning დუბლირება:** CPU `BMD::Transform()` კვლავ სრულდება, შემდეგ იგივე geometry GPU-ზე ხელახლა გარდაიქმნება.

### შესამოწმებელი/შემდეგი პრობლემები

- `glewInit`, GL/GLSL ვერსია, VAO/VBO entry point-ები და hardware limits სათანადოდ არ მოწმდება.
- 200 bone × 3 `vec4` = 600 `vec4` არ არის GL 3.3-ის მინიმალურ 1024-component/256-vec4 გარანტიაში, თუმცა desktop GPU-ებზე ხშირად 4096+ component არსებობს. ამიტომ ეს არის portability risk და არა ავტომატური failure; საჭიროა runtime check და UBO/TBO fallback კვლევა.
- `glGetUniformLocation()` და `glGetFloatv()` per-draw overhead ლოგიკურად ჩანს, მაგრამ `RenderMeshVBO()` desktop branch-ში ხაზ-ხაზ უნდა დადასტურდეს.
- sequential EBO რეალურ vertex reuse-ს არ იძლევა.
- GLEW/GLAD/`glprocs.lib` dependency მიმართულება გასაწმენდია build verification-ის შემდეგ.
- base PFD მოძველებული 16-bit color/depth-ია, თუმცა `InitGLMultisample()` უკვე იყენებს `wglChoosePixelFormatARB` გზას. მიზანი 32-bit color/24-bit depth/8-bit stencil-ია არსებული ARB გზის სწორად გაერთიანებით.

### მიღებული გადაწყვეტილება

- Shader და VBO საჭიროა და არ იშლება.
- legacy renderer რჩება fallback-ად.
- პირდაპირ GL 3.3 Core-ზე გადასვლა არ შეიძლება.
- ჯერ GL 3.3 Compatibility context, შემდეგ ეტაპობრივი fixed-function ჩანაცვლება და ბოლოს Core.
- საჭიროა ერთი shader manager ან მკაცრი program-state ownership/restore.

### განხორციელების რიგი

0. clean Release Win32 build, screenshots და FPS/frame-time baseline;
1. runtime shader assets და capability diagnostics;
2. Character/VBO shader state conflict;
3. მკვდარი/დუბლირებული shader გზების caller audit და cleanup;
4. VAO/VBO/EBO lifecycle;
5. VBO layout/index reuse;
6. uniform/matrix/program-switch overhead;
7. CPU/GPU skinning-ის რეალური გაყოფა;
8. GL 3.3 Compatibility context და pixel format;
9. ImGui OpenGL3, terrain/effects/UI modernization და მხოლოდ შემდეგ Core test.

### Terrain safety note

Terrain-ის მთლიანად static VBO-ში „ჩაყინვა“ დაუშვებელია. `ZzzLodTerrain.cpp`-ში grass wind და per-frame `colors[]`/water/light state დინამიურია. Base heightmap/immutable geometry შეიძლება გადავიდეს buffer-ში, ხოლო wind/light/water უნდა დარჩეს dynamic stream ან shader uniform/state-ად.

## პრიორიტეტი 2 — LookAndFeel5 და responsive UI

- მთავარი სამუშაო skin არის LookAndFeel5.
- desktop branch-ში უკვე არის მომხმარებლის UI ცვლილებები; ჯერ უნდა მოხდეს public branch-თან diff/inventory.
- მიმდინარე პრობლემა: resolution-ის შეცვლისას window/textures/text იგივე pixel ზომაზე რჩება და სწორად არ ერგება viewport/aspect ratio-ს.
- საჯარო ანალიზში `g_fScreenRate_x/y = 1.8f` ფიქსირებული მნიშვნელობა resolution-invariant sizing-ის ერთ-ერთი მიზეზია.
- მეორე/reference client-ში resolution/scaling გამოძახებები განსხვავებულია; გამოიყენე მხოლოდ შედარებისთვის, პირდაპირი copy არა.

საჭირო არქიტექტურა:

- design/reference coordinate system;
- resolution/aspect-aware scale;
- anchors/pivots და safe area;
- text/font scaling და measurement;
- mouse hit-test და rendered coordinates-ის ერთიანი transform;
- scissor/viewport და 3D item preview state;
- per-window override მხოლოდ იქ, სადაც legacy behavior ამას მოითხოვს.

ტესტები: ყველა მხარდაჭერილი resolution/aspect ratio, window bounds, text clipping, tooltip, inventory preview, mouse hover/click, fullscreen/windowed switch.

## პრიორიტეტი 3 — Master Skill Tree visual bug

მომხმარებლის მიერ აღწერილი რეალური ბაგი: Master Tree-ში EXP პროცენტულად არ იწერება/არ ჩანს.

საჯარო branch-ის ანალიზი:

- UI იყენებს `GlobalText[3335]`-თან დაკავშირებულ format path-ს;
- master EXP გამოთვლაში ჩანს ძველი hardcoded `+400` level/formula assumption;
- ეს ჯერ არ არის desktop branch-ზე საბოლოოდ დადასტურებული ან გასწორებული.

Desktop-ზე გასაკეთებელი:

1. იპოვე `NewUIMasterSkillTree` EXP render path და `GlobalText[3335]` რეალური format string;
2. დაადგინე მიმდინარე/შემდეგი Master EXP-ის authoritative fields და მათი widths;
3. გამოითვალე `(current - levelStart) / (nextLevel - levelStart) * 100` უსაფრთხო clamp-ით, თუ სწორედ incremental progress არის ნაჩვენები;
4. შეამოწმე format specifier, integer division, zero/negative denominator და text color/position/clip;
5. შეადარე server master-level table/formula-ს; არ დატოვო client-only hardcoded formula თუ server სხვა progression-ს იყენებს.

## დამატებითი აუდიტის აღმოჩენები

### Data & Assets

- საჯარო `ZzzOpenData.cpp`-ში ორი განსხვავებული HQ skin path რეგისტრირდება ერთ `BITMAP_HQSKIN + 8` ID-ზე; მოგვიანო load-ს შეუძლია წინა texture overwrite. desktop mapping-ზე გადაამოწმე და სრული arithmetic range audit ჩაატარე.
- `.jpg` reference ყოველთვის loose JPG-ს არ ნიშნავს: `ZzzTexture.cpp` runtime `OZJ` path-ს აგებს.

### Social systems

- ამ server suite-ში Friends/Mail/presence/chat-room protocol და persistence DataServer-ის `CSProtocol.*`-შია, არა JoinServer-ში.

### Economy/transactions — საჯარო server branch-ის static findings

ეს საკითხები desktop/server branch-ში ხელახლა დაადასტურე ცვლილებამდე:

1. Personal Shop-ის `PShopTransaction` შეიძლება stuck დარჩეს, თუ buyer inventory insertion `0xFF`-ს აბრუნებს.
2. custom-currency personal-shop Zen branch-ში შესაძლო wrong-recipient `GCMoneySend` call.
3. trade coin persistence item/Zen commit-თან ატომური არ ჩანს.
4. guild warehouse exclusivity multi-process გარემოში დასამტკიცებელია.

## Build/Release გარემოს შენიშვნები

- client: `Main.sln`, `source/Main.vcxproj`, Win32/MultiByte/v143, `MAIN_UPDATE=603`, static CRT.
- server suite: ConnectServer, JoinServer, DataServer, GameServer და Encoder ცალკე solutions-ით.
- server project-ებში არის 603/401/803 და normal/CS variants; configuration ყოველთვის explicit უნდა იყოს.
- არის developer-specific absolute/relative OutDir-ები, GameServer-ში v142/v100 toolset mix და CRT configuration განსხვავებები.
- `SRCMainGS/Source/Main5.2` არ ჩათვალო პრიორიტეტულ standalone client branch-თან ავტომატურად იდენტურად.

## პირველი Desktop სამუშაო სესია

1. გახსენი `SRC 5.2 BASE` როგორც Codex workspace root და არა მხოლოდ `SRCMainGS`.
2. წაიკითხე ეს ფაილი, `CLAUDE.md`, `CHANGELOG.txt` და Shader/VBO roadmap.
3. ჯერ მხოლოდ read-only inventory: იპოვე solutions/projects, `.git` roots, build outputs, runtime clients/servers, Data/Shaders და configs.
4. თითო Git root-ში გაუშვი `git status` და შეადგინე desktop-only ცვლილებების inventory; არაფერი გადააწერო.
5. შექმენი `local-workspace-map` ანგარიში: folder -> role -> source repo/branch -> build output -> runtime consumer.
6. მხოლოდ mapping-ის დამტკიცების შემდეგ დაადასტურე აქტიური client-ის Release Win32 build ან აღწერე blocker.
7. მოიძიე runtime `Shaders`, `Data/Shaders`, `Data/Effect/VBO` ფაილები და დააკავშირე მათ მომხმარებელ EXE-სთან.
8. ჩაიწეროს GL vendor/renderer/version/GLSL და limits.
9. ჯერ გაასწორე shader program state ownership, შემდეგ GPU cleanup.
10. ცალკე მცირე task-ად გამოასწორე Master Tree EXP percentage.
11. LookAndFeel5 responsive migration დაიწყე საერთო coordinate/scale/hit-test seam-ით და არა თითო window-ის შემთხვევითი multiplier-ებით.

## Verification/rollback

- ყველა renderer/UI ცვლილებას ჰქონდეს reference screenshots და იგივე სცენის frame-time baseline;
- შემოწმდეს login, character select, world, terrain/water/grass, monsters, equipment materials, effects, inventory preview, UI, map change და repeated load/unload;
- shader failure-ზე legacy fallback უნდა მუშაობდეს;
- rollback თუ build ირღვევა, ვიზუალი მნიშვნელოვნად იცვლება, crash/driver error ჩნდება, CPU/GPU time ან VRAM უარესდება, ან ძველი GPU კარგავს fallback-ს;
- არ გამოიყენო destructive Git ოპერაციები მომხმარებლის აშკარა ნებართვის გარეშე.
