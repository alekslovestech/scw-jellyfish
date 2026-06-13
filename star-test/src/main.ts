import * as THREE from "three";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";
import { cfg } from "./config";
import { LEDSystem } from "./core/ledSystem";
import { LEDRenderer } from "./core/ledRenderer";
import { AnimationManager } from "./core/animationManager";
import { Star } from "./structure/star";
import { buildPanel } from "./panel";

// ── Renderer ─────────────────────────────────────────────────────────────────
const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.setSize(innerWidth, innerHeight);
renderer.setClearColor(cfg.colors.background);
document.body.appendChild(renderer.domElement);

// ── Scene & Camera ────────────────────────────────────────────────────────────
const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(55, innerWidth / innerHeight, 0.1, 100);
camera.position.set(0, 0, 7);

const controls = new OrbitControls(camera, renderer.domElement);
controls.enableDamping = true;

// ── LED System ────────────────────────────────────────────────────────────────
const ledSystem = new LEDSystem();
const animationManager = new AnimationManager();
animationManager.set(cfg.animation);
const ledRenderer = new LEDRenderer(ledSystem, animationManager);
scene.add(ledRenderer.getObject());

// ── Build star ────────────────────────────────────────────────────────────────
const star = new Star(ledSystem);
scene.add(star.toGroup());

buildPanel(animationManager);

// ── Render loop ───────────────────────────────────────────────────────────────
function animate(): void {
  requestAnimationFrame(animate);
  ledRenderer.update();
  controls.update();
  renderer.render(scene, camera);
}
animate();

// ── Resize ────────────────────────────────────────────────────────────────────
window.addEventListener("resize", () => {
  camera.aspect = innerWidth / innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(innerWidth, innerHeight);
});
