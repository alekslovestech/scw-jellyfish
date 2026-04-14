import * as THREE from "three";
import type { Config } from "./config";
import { bellProfile } from "./shapes"; // Import the curve math

// ── Point Generators ─────────────────────────────────────────────────────────

export function getTentaclePoints(cfg: Config, angle: number): THREE.Vector3[] {
  const { wave_amplitude: amp, wave_frequency: freq, length } = cfg.tentacles;
  const points: THREE.Vector3[] = [];
  const numLights = 12; 

  for (let j = 0; j < numLights; j++) {
    const s = j / (numLights - 1);
    const wave = amp * Math.sin(2 * Math.PI * freq * s);
    points.push(new THREE.Vector3(
      (cfg.bell.radius + wave) * Math.cos(angle),
      (cfg.bell.radius + wave) * Math.sin(angle),
      -s * length + bellProfile(cfg.bell.radius)
    ));
  }
  return points;
}

export function getCentralPoints(cfg: Config, angle: number): THREE.Vector3[] {
  const { wave_amplitude: amp, wave_frequency: freq, length, ring_radius } = cfg.central;
  const points: THREE.Vector3[] = [];
  const numLights = 8;

  for (let j = 0; j < numLights; j++) {
    const s = j / (numLights - 1);
    const wave = amp * Math.sin(2 * Math.PI * freq * s);
    points.push(new THREE.Vector3(
      (ring_radius + wave) * Math.cos(angle),
      (ring_radius + wave) * Math.sin(angle),
      -s * length
    ));
  }
  return points;
}

export function getBellPoints(cfg: Config): THREE.Vector3[] {
  const points: THREE.Vector3[] = [];
  const rings = 3;
  const lightsPerRing = 8;

  for (let r_idx = 1; r_idx <= rings; r_idx++) {
    const r = (r_idx / rings) * cfg.bell.radius * 0.8;
    for (let i = 0; i < lightsPerRing; i++) {
      const th = (i / lightsPerRing) * Math.PI * 2;
      points.push(new THREE.Vector3(
        r * Math.cos(th),
        r * Math.sin(th),
        bellProfile(r) + 0.05
      ));
    }
  }
  return points;
}

// ── Mesh Helper ──────────────────────────────────────────────────────────────

export function createLightMesh(pos: THREE.Vector3, color: number = 0x00ff00): THREE.Mesh {
  const geo = new THREE.SphereGeometry(0.04, 6, 6);
  const mat = new THREE.MeshBasicMaterial({ color });
  const mesh = new THREE.Mesh(geo, mat);
  mesh.position.copy(pos);
  return mesh;
}