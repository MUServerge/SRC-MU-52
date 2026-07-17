import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import { TERRAIN_SIZE, TERRAIN_SCALE, TW } from "./formats";
import type { LoadedMap } from "./loader";

// MU world space is Z-up; three.js is Y-up. Map MU (x, y, z) -> three (x, z, y)
// so the OrbitControls "up" stays the default +Y (height).
const SIZE = TERRAIN_SIZE;

// Colors used for the attribute overlay, one per TW_* flag.
const ATT_COLORS: Array<[number, [number, number, number]]> = [
  [TW.SAFEZONE, [80, 170, 255]],
  [TW.NOMOVE, [255, 70, 70]],
  [TW.NOGROUND, [255, 150, 40]],
  [TW.WATER, [40, 90, 220]],
  [TW.CHARACTER, [180, 60, 220]],
  [TW.ACTION, [240, 220, 60]],
  [TW.HEIGHT, [120, 255, 120]],
  [TW.CAMERA_UP, [120, 120, 120]]
];

export type PaintFn = (tileX: number, tileY: number) => void;

export class Viewer {
  readonly renderer: THREE.WebGLRenderer;
  readonly scene = new THREE.Scene();
  readonly camera: THREE.PerspectiveCamera;
  private controls: OrbitControls;
  private raycaster = new THREE.Raycaster();

  private terrainMesh?: THREE.Mesh;
  private geometry?: THREE.BufferGeometry;
  private wireMesh?: THREE.LineSegments;
  private overlayMesh?: THREE.Mesh;
  private overlayTexture?: THREE.DataTexture;
  private objectsGroup = new THREE.Group();
  private gridHelper?: THREE.GridHelper;

  private height: Float32Array = new Float32Array(SIZE * SIZE);
  private useLight = true;
  private lightColors?: Float32Array;

  onPaint?: PaintFn;
  onHover?: (tileX: number, tileY: number) => void;

  constructor(private canvas: HTMLCanvasElement) {
    this.renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
    this.renderer.setPixelRatio(Math.min(devicePixelRatio, 2));

    this.camera = new THREE.PerspectiveCamera(55, 1, 10, 200000);
    const c = (SIZE * TERRAIN_SCALE) / 2;
    this.camera.position.set(c, 12000, c + 14000);

    this.controls = new OrbitControls(this.camera, canvas);
    this.controls.target.set(c, 0, c);
    this.controls.maxPolarAngle = Math.PI * 0.49;

    this.scene.background = new THREE.Color(0x0d1017);
    this.scene.add(new THREE.AmbientLight(0xffffff, 0.9));
    const sun = new THREE.DirectionalLight(0xffffff, 0.6);
    sun.position.set(1, 2, 1);
    this.scene.add(sun);
    this.scene.add(this.objectsGroup);

    this.resize();
    window.addEventListener("resize", () => this.resize());
    this.bindPointer();
    this.animate();
  }

  private resize() {
    const w = this.canvas.clientWidth || window.innerWidth;
    const h = this.canvas.clientHeight || window.innerHeight;
    this.renderer.setSize(w, h, false);
    this.camera.aspect = w / h;
    this.camera.updateProjectionMatrix();
  }

  private animate = () => {
    requestAnimationFrame(this.animate);
    this.controls.update();
    this.renderer.render(this.scene, this.camera);
  };

  // --- world building ------------------------------------------------------

  load(map: LoadedMap) {
    this.clear();
    this.height = map.height ?? new Float32Array(SIZE * SIZE);
    this.lightColors = map.light;
    this.buildTerrain();
    this.buildOverlay(map);
    this.buildObjects(map);
    this.frameCamera();
  }

  private clear() {
    if (this.terrainMesh) { this.scene.remove(this.terrainMesh); this.geometry?.dispose(); }
    if (this.wireMesh) this.scene.remove(this.wireMesh);
    if (this.overlayMesh) { this.scene.remove(this.overlayMesh); this.overlayTexture?.dispose(); }
    this.objectsGroup.clear();
    if (this.gridHelper) this.scene.remove(this.gridHelper);
  }

  private buildTerrain() {
    const geo = new THREE.BufferGeometry();
    const verts = SIZE * SIZE;
    const positions = new Float32Array(verts * 3);
    const colors = new Float32Array(verts * 3);

    for (let y = 0; y < SIZE; y++) {
      for (let x = 0; x < SIZE; x++) {
        const i = y * SIZE + x;
        positions[i * 3 + 0] = x * TERRAIN_SCALE;
        positions[i * 3 + 1] = this.height[i];
        positions[i * 3 + 2] = y * TERRAIN_SCALE;
        this.writeVertexColor(colors, i);
      }
    }

    const indices = new Uint32Array((SIZE - 1) * (SIZE - 1) * 6);
    let k = 0;
    for (let y = 0; y < SIZE - 1; y++) {
      for (let x = 0; x < SIZE - 1; x++) {
        const a = y * SIZE + x;
        const b = a + 1;
        const c = a + SIZE;
        const d = c + 1;
        indices[k++] = a; indices[k++] = c; indices[k++] = b;
        indices[k++] = b; indices[k++] = c; indices[k++] = d;
      }
    }

    geo.setAttribute("position", new THREE.BufferAttribute(positions, 3));
    geo.setAttribute("color", new THREE.BufferAttribute(colors, 3));
    geo.setIndex(new THREE.BufferAttribute(indices, 1));
    geo.computeVertexNormals();

    const mat = new THREE.MeshLambertMaterial({ vertexColors: true, side: THREE.DoubleSide });
    this.geometry = geo;
    this.terrainMesh = new THREE.Mesh(geo, mat);
    this.scene.add(this.terrainMesh);

    const wire = new THREE.WireframeGeometry(geo);
    this.wireMesh = new THREE.LineSegments(wire, new THREE.LineBasicMaterial({ color: 0x2a3550 }));
    this.wireMesh.visible = false;
    this.scene.add(this.wireMesh);
  }

  private writeVertexColor(colors: Float32Array, i: number) {
    if (this.useLight && this.lightColors) {
      colors[i * 3 + 0] = this.lightColors[i * 3 + 0];
      colors[i * 3 + 1] = this.lightColors[i * 3 + 1];
      colors[i * 3 + 2] = this.lightColors[i * 3 + 2];
    } else {
      colors[i * 3 + 0] = 0.55;
      colors[i * 3 + 1] = 0.58;
      colors[i * 3 + 2] = 0.62;
    }
  }

  refreshColors() {
    if (!this.geometry) return;
    const colors = this.geometry.getAttribute("color") as THREE.BufferAttribute;
    for (let i = 0; i < SIZE * SIZE; i++) this.writeVertexColor(colors.array as Float32Array, i);
    colors.needsUpdate = true;
  }

  // Attribute overlay drawn as a translucent plane hugging the terrain.
  private buildOverlay(map: LoadedMap) {
    const geo = (this.geometry as THREE.BufferGeometry).clone();
    const tex = new THREE.DataTexture(new Uint8Array(SIZE * SIZE * 4), SIZE, SIZE, THREE.RGBAFormat);
    tex.flipY = false;
    tex.needsUpdate = true;

    const uv = new Float32Array(SIZE * SIZE * 2);
    for (let y = 0; y < SIZE; y++) {
      for (let x = 0; x < SIZE; x++) {
        const i = y * SIZE + x;
        uv[i * 2 + 0] = x / (SIZE - 1);
        uv[i * 2 + 1] = y / (SIZE - 1);
      }
    }
    geo.setAttribute("uv", new THREE.BufferAttribute(uv, 2));

    const mat = new THREE.MeshBasicMaterial({
      map: tex, transparent: true, opacity: 0.55, depthWrite: false, side: THREE.DoubleSide
    });
    const mesh = new THREE.Mesh(geo, mat);
    mesh.position.y = 20; // lift slightly to avoid z-fighting
    mesh.visible = false;
    this.overlayMesh = mesh;
    this.overlayTexture = tex;
    this.scene.add(mesh);
    if (map.att) this.updateOverlay(map.att.wall);
  }

  updateOverlay(wall: Uint16Array) {
    if (!this.overlayTexture) return;
    const data = this.overlayTexture.image.data as unknown as Uint8Array;
    for (let i = 0; i < SIZE * SIZE; i++) {
      let r = 0, g = 0, b = 0, a = 0;
      for (const [flag, rgb] of ATT_COLORS) {
        if (wall[i] & flag) { r = rgb[0]; g = rgb[1]; b = rgb[2]; a = 255; break; }
      }
      data[i * 4 + 0] = r; data[i * 4 + 1] = g; data[i * 4 + 2] = b; data[i * 4 + 3] = a;
    }
    this.overlayTexture.needsUpdate = true;
  }

  private buildObjects(map: LoadedMap) {
    if (!map.obj) return;
    const geo = new THREE.BoxGeometry(60, 120, 60);
    const byType = new Map<number, THREE.InstancedMesh>();
    const counts = new Map<number, number>();
    for (const o of map.obj.objects) counts.set(o.type, (counts.get(o.type) ?? 0) + 1);

    const dummy = new THREE.Object3D();
    const idx = new Map<number, number>();
    for (const [type, count] of counts) {
      const color = new THREE.Color().setHSL(((type * 47) % 360) / 360, 0.6, 0.55);
      const mat = new THREE.MeshLambertMaterial({ color });
      const inst = new THREE.InstancedMesh(geo, mat, count);
      inst.name = `obj_type_${type}`;
      byType.set(type, inst);
      idx.set(type, 0);
      this.objectsGroup.add(inst);
    }
    for (const o of map.obj.objects) {
      const inst = byType.get(o.type)!;
      const i = idx.get(o.type)!;
      dummy.position.set(o.position[0], o.position[2] + 60, o.position[1]);
      dummy.rotation.set(0, THREE.MathUtils.degToRad(o.angle[2]), 0);
      const s = o.scale || 1;
      dummy.scale.set(s, s, s);
      dummy.updateMatrix();
      inst.setMatrixAt(i, dummy.matrix);
      idx.set(o.type, i + 1);
    }
    for (const inst of byType.values()) inst.instanceMatrix.needsUpdate = true;
  }

  private frameCamera() {
    const c = (SIZE * TERRAIN_SCALE) / 2;
    let maxH = 0;
    for (let i = 0; i < this.height.length; i++) if (this.height[i] > maxH) maxH = this.height[i];
    this.controls.target.set(c, maxH * 0.3, c);
    this.camera.position.set(c, Math.max(12000, maxH + 6000), c + 16000);
  }

  // --- toggles -------------------------------------------------------------

  setLight(on: boolean) { this.useLight = on; this.refreshColors(); }
  setWireframe(on: boolean) { if (this.wireMesh) this.wireMesh.visible = on; }
  setOverlay(on: boolean) { if (this.overlayMesh) this.overlayMesh.visible = on; }
  setObjects(on: boolean) { this.objectsGroup.visible = on; }
  setGrid(on: boolean) {
    if (on && !this.gridHelper) {
      const total = SIZE * TERRAIN_SCALE;
      this.gridHelper = new THREE.GridHelper(total, SIZE, 0x334066, 0x1c2540);
      this.gridHelper.position.set(total / 2, 5, total / 2);
      this.scene.add(this.gridHelper);
    }
    if (this.gridHelper) this.gridHelper.visible = on;
  }

  // --- picking / painting --------------------------------------------------

  private bindPointer() {
    let painting = false;
    const toTile = (ev: PointerEvent): [number, number] | null => {
      if (!this.terrainMesh) return null;
      const rect = this.canvas.getBoundingClientRect();
      const nx = ((ev.clientX - rect.left) / rect.width) * 2 - 1;
      const ny = -((ev.clientY - rect.top) / rect.height) * 2 + 1;
      this.raycaster.setFromCamera(new THREE.Vector2(nx, ny), this.camera);
      const hit = this.raycaster.intersectObject(this.terrainMesh, false)[0];
      if (!hit) return null;
      const tx = Math.round(hit.point.x / TERRAIN_SCALE);
      const ty = Math.round(hit.point.z / TERRAIN_SCALE);
      if (tx < 0 || ty < 0 || tx >= SIZE || ty >= SIZE) return null;
      return [tx, ty];
    };

    this.canvas.addEventListener("pointerdown", (ev) => {
      if (!ev.shiftKey) return;
      const t = toTile(ev);
      if (t && this.onPaint) { painting = true; this.controls.enabled = false; this.onPaint(t[0], t[1]); }
    });
    this.canvas.addEventListener("pointermove", (ev) => {
      const t = toTile(ev);
      if (t && this.onHover) this.onHover(t[0], t[1]);
      if (painting && t && this.onPaint) this.onPaint(t[0], t[1]);
    });
    const stop = () => { painting = false; this.controls.enabled = true; };
    window.addEventListener("pointerup", stop);
  }
}
