# Renderer Phase 1 — Compatibility Lifetime Stabilization (გეგმა განსახილველად)

თარიღი: 2026-07-16

სტატუსი: **გეგმა / განსახილველი** — არც ერთი production-ფაილი ჯერ არ შეცვლილა. ეს დოკუმენტი
აღწერს Phase 1-ის სამუშაოს, დამყარებულს ლოკალური worktree-ის რეალურ სიმბოლოებზე.
ხაზების ნომრები worktree-ს ცვლილებისას შეიძლება წაინაცვლოს.

წინაპირობა: Phase 0 (diagnostics + baseline instrumentation) უკვე დანერგილია და commit-შია
(`CreateOpenglWindow`, `RenderProfiler`, wiring ZzzBMD/CShaderScene/CShaderGL/ZzzScene-ში).
Phase 1 იყენებს Phase 0-ის live-resource counter-ებს ვერიფიკაციისთვის.

---

## 1. მიზანი

სამი დადასტურებული **P1** დეფექტის root-cause გასწორება, additive და compatibility-safe
გზით, legacy fallback-ის სრული შენარჩუნებით:

- **P1-A** — GL context ნადგურდება renderer-ის GL რესურსების გათავისუფლებამდე (shutdown რიგი).
- **P1-B** — BMD mesh-ის VAO/VBO/EBO-ს არ აქვს წაშლის გზა (leak reload-ზე და shutdown-ზე).
- **P1-C** — VBO path-ს არ აქვს validated entry-point/capability gate.

არა-მიზნები (ამ ფაზაში არ ვაკეთებთ): CShaderScene/CShaderGL-ის კონსოლიდაცია, shader
წყაროების ცვლილება, VBO eligibility/draw order/output-ის ცვლილება, pixel-format ან
ახალი GL context-ის მოთხოვნა, CPU/GPU skinning ან bone transport-ის ცვლილება, terrain/UI.

---

## 2. დადასტურებული მიმდინარე მდგომარეობა (რეალური კოდი)

### Shutdown თანმიმდევრობა (ასეა ახლა)

1. `WM_DESTROY` → `WINHANDLE.cpp:619` იძახებს `KillGLWindow()`-ს.
2. `KillGLWindow` (`Winmain.cpp:208–232`): `wglMakeCurrent(NULL, NULL)` →
   `wglDeleteContext(g_hRC)` → `g_hRC = NULL` → `ReleaseDC`. **Context აქ ნადგურდება.**
3. `winLoop()` ბრუნდება → `Winmain.cpp:1518` `gwinhandle->Destroyer()`.
4. `Destroyer` (`WINHANDLE.cpp:165–178`): `DestroyImGuiWindow()` [ImGui GL2 shutdown] +
   `DestroyWindow()`.
5. `DestroyWindow` (`Winmain.cpp:411+`): `ReleaseCharacters()`, `DeleteWaterTerrain()`,
   `gMapManager->DeleteObjects()`, `gmClientModels->GetModel(i)->Release()` [BMD::Release],
   `Bitmaps.UnloadAllImages()`.
6. Static/global dtor-ები process-exit-ზე: `CShaderScene::~CShaderScene()→Release()`
   (`CShaderScene.cpp:32,218`), `CShaderGL::~CShaderGL()` (`CShaderGL.cpp:15`).

➡️ ImGui GL2 shutdown, BMD::Release-ის GL ნაწილი და shader-პროგრამების წაშლა — ყველა
სრულდება ნაბიჯ 2-ის **შემდეg**, ე.ი. current context-ის გარეშე. GL deletion current
context-ის გარეშე = invalid/driver-dependent ქცევა (**P1-A**).

### BMD GL ბუფერების მფლობელობა

- `Mesh_t::VAO, VBO_Vertices, VBO_Normals, VBO_TexCoords, VBO_Colors, EBO` (`ZzzBMD.h:153`);
  ctor ანულებს handle-ებს (`ZzzBMD.h:176–182`).
- შექმნა/ატვირთვა: `BMD::CreateVertexBuffer` (`ZzzBMD.cpp:3442`, GL ბუფერები `:3515–3548`).
- `BMD::Release` (`ZzzBMD.cpp:2413–2482`) ათავისუფლებს CPU მასივებს + bitmap-ებს, მაგრამ
  **არც ერთ `glDeleteBuffers`/`glDeleteVertexArrays`-ს არ იძახებს** → leak realloc/reload-ზე
  (Open2) და shutdown-ზე (**P1-B**).

### VBO capability gate

- `glewInit`-ის შედეგი ახლა ლოგდება (Phase 0, `Winmain.cpp:802–804`), მაგრამ
  `CreateVertexBuffer`/`RenderMeshVBO` VAO/integer-attrib entry-point-ებს
  **capability-შემოწმების გარეშე** იყენებს. არსებული fallback მხოლოდ program-level-ია,
  არა context/capability-level (**P1-C**).

### გამოსაყენებელი არსებული pattern-ები (reuse-first)

- `CShaderScene::Release` (`CShaderScene.cpp:218–233`) უკვე იცავს `if (glDeleteProgram != NULL)`
  — იმავე defensive guard-ს გავიმეორებთ BMD ბუფერების წაშლისას.
- Phase 0 profiler `Gen`-wrapper-ები (`RenderProfilerGenVertexArrays/GenBuffers`) — deletion
  wrapper-ებს იმავე ფორმით დავამატებთ.
- Phase 0 capability-შემოწმების ლოგიკა (`WriteOpenGLCapabilityDiagnostics`,
  `Winmain.cpp:677–735`) — gate-ისთვის იმავე წყაროდან.

---

## 3. ცვლილებების თანმიმდევრობა (რიგი კრიტიკულია)

> **პრინციპი:** ჯერ **რიგი** გავასწოროთ (GL რესურსები დავშალოთ სანამ context მიმდინარეა),
> მხოლოდ **მერე** დავამატოთ deletion. deletion-ის რიგის გასწორებამდე დამატება ნიშნავს
> `glDeleteBuffers`-ს context-ის გარეშე — უარესი. ეს investigation §1 გაფრთხილების და
> Rule #11-ის პირდაპირი დაცვაა.

### ნაბიჯი 1 — მოწესრიგებული GL teardown context-ის განადგურებამდე

- ერთი მფლობელი helper (მაგ. `ReleaseRendererGLResources()`), რომელიც სრულდება სანამ
  `g_hRC` მიმდინარეა, თანმიმდევრობით:
  1. ImGui GL2 backend shutdown (`ImGui_ImplOpenGL2_Shutdown` GL ნაწილი);
  2. BMD GL ბუფერების წაშლა ყველა live მოდელისთვის (ნაბიჯი 2);
  3. shader პროგრამები (`CShaderScene::Release`, `CShaderGL` release);
  4. საჭირო ტექსტურები.
- გამოძახება `KillGLWindow`-ში, `wglDeleteContext`-ის **წინ** (რეკომენდაცია: ერთი owner,
  სიმეტრიული `CreateOpenglWindow`-სთან — იხ. გადაწyვეტილება #1).
- Non-GL teardown (fonts, registry, sockets, CPU მასივები, UI) რჩება უცვლელი `DestroyWindow`-ში.
- **Double-teardown guard** (bool flag) — რადგან `Destroyer()→DestroyWindow()` მაინც შემდეg
  გაეშვება; handle-zeroing უზრუნველყოფს რომ მოგვიანო CPU `Release` no-op იყოს GL მხრივ.

### ნაბიჯი 2 — BMD VAO/VBO/EBO სიმეტრიული წაშლა (root-cause, Rule #11)

- ახალი `BMD::ReleaseVertexBuffers()`: guard-ით (`glDeleteBuffers != NULL &&
  glDeleteVertexArrays != NULL`, context მიმდინარე), შლის თითო mesh-ის VAO/VBO×4/EBO-ს,
  აღრიცხავს `ResourceDeleted`-ს profiler wrapper-ით, და **ანულებს handle-ებს**
  (ემთხვევა `Mesh_t` ctor zeroing-ს `ZzzBMD.h:176–182`).
- გამოძახება: (ა) `Open2`-ში realloc-ზე, handle-ების გადაწერამდე → reload leak fix
  (აქ context მიმდინარეა); (ბ) ნაბიჯ 1-ის shutdown pass-იდან.
- **CPU-only `BMD::Release`-ს deletion არ ვამატებ** — investigation §1 გაფრთხილების დაცვა
  (Release ამჟამად current context-ის გარეშე შეიძლება გაეშვას).
- ახალი profiler wrapper-ები `RenderProfilerDeleteBuffers`/`RenderProfilerDeleteVertexArrays`
  (არსებული `Gen`-wrapper-ების სარკე, `ResourceDeleted(RPR_BUFFER/RPR_VAO)`). Phase 0-ში
  deletion counter-ის მიზნით მათი დამატება აკრძალული იყო; Phase 1-ში deletion **რეალური
  feature-ია**, ამიტომ ლეგიტიმურია.

### ნაბიჯი 3 — VBO path-ის capability gate

- Phase 0 capability-შემოწმება გავიტანოთ პატარა reusable helper-ში (ერთხელ დათვლილი
  struct: log + gate ერთი წყაროდან, დუბლირების გარეშე), ვაქეშოთ ერთი `bool`.
- `IsVboSceneEnabled()` / `CreateVertexBuffer` / `RenderMeshVBO` ამ gate-ს კითხულობს;
  unsupported → VAO/VBO არ იქმნება, mesh რჩება **არსებულ legacy path-ზე**. program-level
  fallback → სრული capability fallback.
- Supported hardware-ზე eligibility უცვლელი → **output იდენტური**.

---

## 4. ვერიფიკაცია და rollback

- Build **Release|Win32** (x86 client) ლოკალურად; errors/warnings ცალკე ანგარიში.
- `MU_RENDER_PROFILER=1`-ით, runbook-ის სცენარები: reconnect, map-transition,
  repeated-shutdown (×10). live `VAO`/`Buffer`/`Program` counter-ები უნდა დაბრუნდეს ~0-ზე
  და **არ იზრდებოდეს** reload-ებზე.
- Screenshot parity supported hardware-ზე — ვიზუალური ცვლილება არ არის მოსალოდნელი.
- Rollback: თითო ნაბიჯი დამოუკიდებლად revert-ადია; legacy path მუდამ შენარჩუნებული.
- Definition of done (Rule #10): clean Release/x86 build, ქცევის რეალური შემოwმება,
  `CHANGELOG.txt` entry, საჭიროებისას docs/skill განახლება.

---

## 5. რისკები (Rule #8 — shutdown/engine-init არის risky)

- Shutdown-ის რიგის შეცვლა engine teardown-ს ეხება → double-free guard სავალდებულოა.
- `Mesh_t` layout **არ იცვლება** (Rule #5) — მხოლოდ method + call sites; ველები არსებობს.
- ImGui GL2 shutdown context-current-ზე უნდა გაეშვას; მერე ImGui აღარ უნდა გამოიყენებოდეს.
- Handle-zeroing აუცილებელია double-delete-ის თავიდან ასაცილებლად.

---

## 6. ღია გადაწyვეტილებები (საჭიროა დამკვეთის პასუხი)

1. **Ordered teardown-ის ადგილი:** (ა) `KillGLWindow`-ში `wglDeleteContext`-ის წინ
   [რეკომენდებული — ერთი owner], თუ (ბ) ახალი გამოძახება `WM_DESTROY`-ში `KillGLWindow`-მდე?
2. **`WGLExtensionSupported` null-guard** (P2, `ZzzOpenglUtil.cpp:2217`) — capability-safety-ს
   ეხება; Phase 1-ში ჩავრთოთ თუ გადავდოთ ცალკე?

---

## 7. შემდეgი ფაზების კავშირი

Phase 1 ასრულებს roadmap-ის „1. Stabilize compatibility lifetime" boundary-ს და ხსნის გზას
Phase 2-ისთვის (ერთი shader owner / registry + scoped binding). Phase 1 არ ცვლის shader
ownership-ს და არ ითხოვს ახალ context-ს.
