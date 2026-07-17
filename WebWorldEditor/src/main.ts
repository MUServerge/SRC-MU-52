import { Viewer } from "./viewer";
import {
  assembleMap, loadFromFileList, loadFromDirectoryPicker, loadFromDataTransfer,
  LoadedMap
} from "./loader";
import { serializeAtt, serializeObj, TW } from "./formats";

const $ = <T extends HTMLElement>(id: string) => document.getElementById(id) as T;

const canvas = $("gl") as HTMLCanvasElement;
const viewer = new Viewer(canvas);

let current: LoadedMap | null = null;

// ---- brush painting on the attribute grid --------------------------------

function currentBrush(): { flag: number; radius: number; add: boolean } {
  const flag = parseInt(($("attFlag") as HTMLSelectElement).value, 10);
  const radius = parseInt(($("attBrush") as HTMLInputElement).value, 10);
  const add = (document.querySelector('input[name="attMode"]:checked') as HTMLInputElement).value === "add";
  return { flag, radius, add };
}

viewer.onPaint = (tx, ty) => {
  if (!current?.att) return;
  const { flag, radius, add } = currentBrush();
  const wall = current.att.wall;
  for (let dy = -radius; dy <= radius; dy++) {
    for (let dx = -radius; dx <= radius; dx++) {
      const x = tx + dx, y = ty + dy;
      if (x < 0 || y < 0 || x >= 256 || y >= 256) continue;
      if (dx * dx + dy * dy > radius * radius) continue;
      const i = y * 256 + x;
      if (add) wall[i] |= flag;
      else wall[i] &= ~flag & 0xffff;
    }
  }
  viewer.updateOverlay(wall);
  ($("chkAtt") as HTMLInputElement).checked = true;
  viewer.setOverlay(true);
};

viewer.onHover = (tx, ty) => {
  if (!current) return;
  const att = current.att ? current.att.wall[ty * 256 + tx] : 0;
  $("hud").textContent = `tile (${tx}, ${ty})  att=0x${att.toString(16).padStart(2, "0")}`;
};

// ---- loading --------------------------------------------------------------

async function applyMap(map: LoadedMap) {
  current = map;
  viewer.load(map);
  $("overlay").classList.add("hidden");
  $("layersPanel").hidden = false;
  $("attPanel").hidden = false;
  ($("btnSaveAtt") as HTMLButtonElement).disabled = !map.att;
  ($("btnSaveObj") as HTMLButtonElement).disabled = !map.obj;
  renderInfo(map);
}

function renderInfo(map: LoadedMap) {
  const objCount = map.obj?.objects.length ?? 0;
  const types = map.obj ? new Set(map.obj.objects.map((o) => o.type)).size : 0;
  const rows = [
    ["Source", map.name],
    [".att", map.att ? `map ${map.att.map}${map.att.ext ? " (ext/16-bit)" : ""}` : "—"],
    [".obj", map.obj ? `v${map.obj.version}, ${objCount} objects, ${types} types` : "—"],
    ["Height", map.height ? "loaded" : "flat (missing)"],
    ["Light", map.light ? "loaded" : "none"]
  ];
  let html = rows.map(([k, v]) => `<div><span>${k}</span><b>${v}</b></div>`).join("");
  if (map.warnings.length) {
    html += `<div class="warn">${map.warnings.map(escapeHtml).join("<br>")}</div>`;
  }
  $("info").innerHTML = html;
}

function escapeHtml(s: string): string {
  return s.replace(/[&<>]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;" }[c]!));
}

async function loadFiles(files: FileList | File[], label: string) {
  const raw = await loadFromFileList(files);
  if (raw.size === 0) {
    alert("No MU map files (.att / .obj / .OZB) found in the selection.");
    return;
  }
  await applyMap(assembleMap(raw, label));
  refreshFileList(raw);
}

function refreshFileList(raw: Map<string, { name: string }>) {
  const names = Array.from(raw.values()).map((f) => f.name).sort();
  $("fileList").innerHTML = names.map((n) => `<div>${escapeHtml(n)}</div>`).join("");
}

// ---- exporting ------------------------------------------------------------

function download(name: string, bytes: Uint8Array) {
  const blob = new Blob([bytes as unknown as ArrayBuffer], { type: "application/octet-stream" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = name;
  a.click();
  URL.revokeObjectURL(url);
}

// ---- UI events ------------------------------------------------------------

$("btnPickFolder").addEventListener("click", async () => {
  const res = await loadFromDirectoryPicker();
  if (res === null) {
    // Not supported or cancelled — fall back to file input.
    ($("filePick") as HTMLInputElement).click();
    return;
  }
  if (res.files.size === 0) {
    alert("That folder has no MU map files (.att / .obj / .OZB).");
    return;
  }
  await applyMap(assembleMap(res.files, res.label));
  refreshFileList(res.files);
});

$("filePick").addEventListener("change", (e) => {
  const input = e.target as HTMLInputElement;
  if (input.files && input.files.length) loadFiles(input.files, "selection");
});

const drop = $("dropZone");
["dragover", "dragenter"].forEach((t) =>
  drop.addEventListener(t, (e) => { e.preventDefault(); drop.classList.add("over"); })
);
["dragleave", "drop"].forEach((t) =>
  drop.addEventListener(t, () => drop.classList.remove("over"))
);
drop.addEventListener("drop", async (e) => {
  e.preventDefault();
  if (!e.dataTransfer) return;
  const raw = await loadFromDataTransfer(e.dataTransfer);
  if (raw.size === 0) { alert("No MU map files found in the drop."); return; }
  await applyMap(assembleMap(raw, "drop"));
  refreshFileList(raw);
});

$("chkLight").addEventListener("change", (e) => viewer.setLight((e.target as HTMLInputElement).checked));
$("chkWire").addEventListener("change", (e) => viewer.setWireframe((e.target as HTMLInputElement).checked));
$("chkAtt").addEventListener("change", (e) => viewer.setOverlay((e.target as HTMLInputElement).checked));
$("chkObjects").addEventListener("change", (e) => viewer.setObjects((e.target as HTMLInputElement).checked));
$("chkGrid").addEventListener("change", (e) => viewer.setGrid((e.target as HTMLInputElement).checked));

$("attBrush").addEventListener("input", (e) => {
  $("attBrushVal").textContent = (e.target as HTMLInputElement).value;
});

$("btnSaveAtt").addEventListener("click", () => {
  if (!current?.att) return;
  download(current.attName ?? "EncTerrain.att", serializeAtt(current.att));
});
$("btnSaveObj").addEventListener("click", () => {
  if (!current?.obj) return;
  download(current.objName ?? "EncTerrain.obj", serializeObj(current.obj));
});

// Keep TW referenced so the enum is bundled and available for debugging.
(window as any).__MU_TW = TW;
