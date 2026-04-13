import * as THREE from "three";
import type { Config } from "./config";

const N_S = 60;
const N_U = 20;
const N_R = 40;
const N_TH = 60;

// ── Bell profile ──────────────────────────────────────────────────────────────
// Replace with any f(r) where f(0) is the peak and f(bell.radius) ≈ 0.
export function bellProfile(r: number): number {
  return 0.2 * r * r - 0.05 * r ** 4;
}

function buildGeometry(
  vertices: number[],
  indices: number[],
): THREE.BufferGeometry {
  const geo = new THREE.BufferGeometry();
  geo.setAttribute("position", new THREE.Float32BufferAttribute(vertices, 3));
  geo.setIndex(indices);
  geo.computeVertexNormals();
  return geo;
}

// ── Bell ──────────────────────────────────────────────────────────────────────
export function makeBell(cfg: Config): THREE.BufferGeometry {
  const vertices: number[] = [];
  const indices: number[] = [];

  for (let i = 0; i < N_TH; i++) {
    for (let j = 0; j < N_R; j++) {
      const r = (j / (N_R - 1)) * cfg.bell.radius;
      const th = (i / N_TH) * Math.PI * 2;
      vertices.push(r * Math.cos(th), r * Math.sin(th), bellProfile(r));
    }
  }

  for (let i = 0; i < N_TH; i++) {
    for (let j = 0; j < N_R - 1; j++) {
      const a = i * N_R + j;
      const b = i * N_R + j + 1;
      const c = ((i + 1) % N_TH) * N_R + j;
      const d = ((i + 1) % N_TH) * N_R + j + 1;
      indices.push(a, c, b, b, c, d);
    }
  }

  return buildGeometry(vertices, indices);
}

// ── Shared tube builder ───────────────────────────────────────────────────────
function makeTube(
  spineX: number[],
  spineY: number[],
  spineZ: number[],
  bx: number[],
  by: number[],
  bz: number[],
  nx: number,
  ny: number,
  radius: number,
): THREE.BufferGeometry {
  const vertices: number[] = [];
  const indices: number[] = [];

  for (let i = 0; i < N_U; i++) {
    const u = (i / N_U) * Math.PI * 2;
    const cosU = Math.cos(u);
    const sinU = Math.sin(u);
    for (let j = 0; j < N_S; j++) {
      vertices.push(
        spineX[j] + radius * (cosU * nx + sinU * bx[j]),
        spineY[j] + radius * (cosU * ny + sinU * by[j]),
        spineZ[j] + radius * (sinU * bz[j]),
      );
    }
  }

  for (let i = 0; i < N_U; i++) {
    for (let j = 0; j < N_S - 1; j++) {
      const a = i * N_S + j;
      const b = i * N_S + j + 1;
      const c = ((i + 1) % N_U) * N_S + j;
      const d = ((i + 1) % N_U) * N_S + j + 1;
      indices.push(a, c, b, b, c, d);
    }
  }

  return buildGeometry(vertices, indices);
}

// ── Outer tentacle ────────────────────────────────────────────────────────────
export function makeTentacle(cfg: Config, angle: number): THREE.BufferGeometry {
  const { wave_amplitude: amp, wave_frequency: freq, length } = cfg.tentacles;
  const a = angle;

  const spineX: number[] = [],
    spineY: number[] = [],
    spineZ: number[] = [];
  const bx: number[] = [],
    by: number[] = [],
    bz: number[] = [];

  for (let j = 0; j < N_S; j++) {
    const s = j / (N_S - 1);
    const wave = amp * Math.sin(2 * Math.PI * freq * s);
    const waveD = amp * 2 * Math.PI * freq * Math.cos(2 * Math.PI * freq * s);
    const mag = Math.sqrt(waveD * waveD + length * length);

    spineX.push((cfg.bell.radius + wave) * Math.cos(a));
    spineY.push((cfg.bell.radius + wave) * Math.sin(a));
    spineZ.push(-s * length + bellProfile(cfg.bell.radius));

    bx.push((length * Math.cos(a)) / mag);
    by.push((length * Math.sin(a)) / mag);
    bz.push(waveD / mag);
  }

  // N = tangential direction (-sin a, cos a, 0)
  return makeTube(
    spineX,
    spineY,
    spineZ,
    bx,
    by,
    bz,
    -Math.sin(a),
    Math.cos(a),
    cfg.tentacles.radius,
  );
}

// ── Central tentacle ──────────────────────────────────────────────────────────
export function makeCentralTentacle(
  cfg: Config,
  angle: number,
): THREE.BufferGeometry {
  const {
    wave_amplitude: amp,
    wave_frequency: freq,
    length,
    ring_radius,
  } = cfg.central;
  const a = angle;

  const spineX: number[] = [],
    spineY: number[] = [],
    spineZ: number[] = [];
  const bx: number[] = [],
    by: number[] = [],
    bz: number[] = [];

  for (let j = 0; j < N_S; j++) {
    const s = j / (N_S - 1);
    const wave = amp * Math.sin(2 * Math.PI * freq * s);
    const waveD = amp * 2 * Math.PI * freq * Math.cos(2 * Math.PI * freq * s);
    const mag = Math.sqrt(waveD * waveD + length * length);

    spineX.push((ring_radius + wave) * Math.cos(a));
    spineY.push((ring_radius + wave) * Math.sin(a));
    spineZ.push(-s * length);

    bx.push((length * Math.cos(a)) / mag);
    by.push((length * Math.sin(a)) / mag);
    bz.push(waveD / mag);
  }

  return makeTube(
    spineX,
    spineY,
    spineZ,
    bx,
    by,
    bz,
    -Math.sin(a),
    Math.cos(a),
    cfg.central.radius,
  );
}
