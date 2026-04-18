import * as THREE from "three";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";

import { cfg } from "./config";
import { Jellyfish } from "./structure/jellyfish";
import { buildPanel } from "./panel";
import { LEDSystem } from "./core/ledSystem";
import { LEDRenderer } from "./core/ledRenderer";
import { AnimationManager } from "./core/animationManager";
import { colorCycle } from "./animations/colorCycle";

// ── Renderer ────────────────────────────────────────────────────────────────
const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.setSize(innerWidth, innerHeight);
renderer.setClearColor(cfg.colors.background);
document.body.appendChild(renderer.domElement);

// ── Scene & Camera ───────────────────────────────────────────────────────────
const scene = new THREE.Scene();
scene.fog = new THREE.FogExp2(cfg.colors.background, 0.018);

const camera = new THREE.PerspectiveCamera(
  55,
  innerWidth / innerHeight,
  0.1,
  200
);
camera.position.set(0, 2, 12);

const controls = new OrbitControls(camera, renderer.domElement);
controls.enableDamping = true;

// ── Lighting ────────────────────────────────────────────────────────────────
scene.add(
  new THREE.AmbientLight(
    cfg.lighting.ambient_color,
    cfg.lighting.ambient_intensity
  )
);

const key = new THREE.PointLight(
  cfg.lighting.key_color,
  cfg.lighting.key_intensity,
  40
);
key.position.set(4, 7, 6);
scene.add(key);

const fill = new THREE.PointLight(
  cfg.lighting.fill_color,
  cfg.lighting.fill_intensity,
  60
);
fill.position.set(-4, -5, -4);
scene.add(fill);

// ── Materials ───────────────────────────────────────────────────────────────
const bellMat = new THREE.MeshPhongMaterial({
  color: cfg.colors.bell,
  transparent: true,
  opacity: 0.75,
  side: THREE.DoubleSide,
  shininess: 60,
});

const tentMat = new THREE.MeshPhongMaterial({
  color: cfg.colors.tentacles,
  transparent: true,
  opacity: 0.75,
  side: THREE.DoubleSide,
  shininess: 40,
});

const centralMat = new THREE.MeshPhongMaterial({
  color: cfg.colors.central,
  transparent: true,
  opacity: 0.75,
  side: THREE.DoubleSide,
  shininess: 40,
});

// ── LED SYSTEM ──────────────────────────────────────────────────────────────
const ledSystem = new LEDSystem();
const animationManager = new AnimationManager();
animationManager.set(colorCycle);
const ledRenderer = new LEDRenderer(ledSystem, animationManager);
scene.add(ledRenderer.getObject());

// ── Jellyfish storage ───────────────────────────────────────────────────────
let jellies: Jellyfish[] = [];
let groups: THREE.Group[] = [];

// ── Build Scene ─────────────────────────────────────────────────────────────
function draw(): void {
  // cleanup scene objects
  jellies.forEach(j => j.dispose());
  groups.forEach(g => scene.remove(g));

  jellies = [];
  groups = [];

  // reset LED system safely
  ledSystem.reset();

  const ringStep = cfg.bell.radius * 3;

  cfg.sizes.forEach((entry, tier) => {
    const ringRadius = 2 * tier * Math.sqrt(ringStep);
    const n = entry.count;

    for (let i = 0; i < n; i++) {
      const angle = (i / n) * Math.PI * 2;

      const jelly = new Jellyfish(
        cfg,
        cfg.size_ratio ** entry.level,
        ringRadius * Math.cos(angle),
        ringRadius * Math.sin(angle),
        entry.level * cfg.z_offset,
        ledSystem
      );

      const group = jelly.toGroup(bellMat, tentMat, centralMat);

      jellies.push(jelly);
      groups.push(group);
      scene.add(group);
    }
  });
}

draw();
buildPanel(draw);

// ── Render loop ─────────────────────────────────────────────────────────────
function animate(): void {
  requestAnimationFrame(animate);

  ledRenderer.update();

  controls.update();
  renderer.render(scene, camera);
}

animate();

// ── Resize ─────────────────────────────────────────────────────────────────
window.addEventListener("resize", () => {
  camera.aspect = innerWidth / innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(innerWidth, innerHeight);
});