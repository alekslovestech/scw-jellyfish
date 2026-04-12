import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

import { cfg } from './config';
import { makeBell, makeTentacle, makeCentralTentacle } from './shapes';
import { buildPanel } from './panel';

// ── Renderer ──────────────────────────────────────────────────────────────────
const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.setSize(innerWidth, innerHeight);
renderer.setClearColor(0x041428);
document.body.appendChild(renderer.domElement);

// ── Scene & camera ────────────────────────────────────────────────────────────
const scene  = new THREE.Scene();
scene.fog    = new THREE.FogExp2(0x041428, 0.018);

const camera = new THREE.PerspectiveCamera(55, innerWidth / innerHeight, 0.1, 200);
camera.position.set(0, 2, 12);

const controls = new OrbitControls(camera, renderer.domElement);
controls.enableDamping = true;

// ── Lighting ──────────────────────────────────────────────────────────────────
scene.add(new THREE.AmbientLight(0x112233, 4.0));
const key = new THREE.PointLight(0x4499ff, 60, 40);
key.position.set(4, 7, 6);
scene.add(key);

// ── Materials ─────────────────────────────────────────────────────────────────
const bellMat = new THREE.MeshPhongMaterial({
  color: 0x55aaee, transparent: true, opacity: 0.75,
  side: THREE.DoubleSide, shininess: 60,
});
const tentMat = new THREE.MeshPhongMaterial({
  color: 0xed1e07, transparent: true, opacity: 0.75,
  side: THREE.DoubleSide, shininess: 40,
});
const centralMat = new THREE.MeshPhongMaterial({
  color: 0x16f706, transparent: true, opacity: 0.75,
  side: THREE.DoubleSide, shininess: 40,
});

// ── Scene draw ────────────────────────────────────────────────────────────────
let meshes: THREE.Mesh[] = [];

function draw(): void {
  meshes.forEach(m => { m.geometry.dispose(); scene.remove(m); });
  meshes = [];

  const add = (geo: THREE.BufferGeometry, mat: THREE.Material) => {
    const m = new THREE.Mesh(geo, mat);
    scene.add(m);
    meshes.push(m);
  };

  add(makeBell(cfg), bellMat);

  for (let k = 0; k < cfg.tentacles.count; k++) {
    add(makeTentacle(cfg, k * 2 * Math.PI / cfg.tentacles.count), tentMat);
  }

  for (let k = 0; k < cfg.central.count; k++) {
    add(makeCentralTentacle(cfg, k * 2 * Math.PI / cfg.central.count), centralMat);
  }
}

draw();
buildPanel(draw);

// ── Render loop ───────────────────────────────────────────────────────────────
function animate(): void {
  requestAnimationFrame(animate);
  controls.update();
  renderer.render(scene, camera);
}
animate();

window.addEventListener('resize', () => {
  camera.aspect = innerWidth / innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(innerWidth, innerHeight);
});
