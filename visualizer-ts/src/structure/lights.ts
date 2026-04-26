import * as THREE from "three";
import type { Config } from "../config";
import { bellProfile } from "./shapes";

/**
 * LED data model (core concept)
 */
export type LED = {
  position: THREE.Vector3;

  // semantic grouping
  group: "bell" | "tentacle" | "central";

  // index within its group
  index: number;

  // normalized position along a strip (0 → 1), useful for waves
  t: number;

  jellyId?: number; // <--- Add this line here

  // optional metadata for future animation use
  angle?: number;
  ring?: number;
};

/* ──────────────────────────────────────────────────────────────
   Bell LEDs
────────────────────────────────────────────────────────────── */
export function getBellPoints(cfg: Config): LED[] {
  const leds: LED[] = [];

  const rings = 3;
  const lightsPerRing = 8;

  let globalIndex = 0;

  for (let r_idx = 1; r_idx <= rings; r_idx++) {
    const r = (r_idx / rings) * cfg.bell.radius * 0.8;

    for (let i = 0; i < lightsPerRing; i++) {
      const th = (i / lightsPerRing) * Math.PI * 2;

      leds.push({
        position: new THREE.Vector3(
          r * Math.cos(th),
          r * Math.sin(th),
          bellProfile(r) + 0.05
        ),
        group: "bell",
        index: globalIndex++,
        t: r_idx / rings,
        angle: th,
        ring: r_idx,
      });
    }
  }

  return leds;
}

/* ──────────────────────────────────────────────────────────────
   Tentacle LEDs (outer)
────────────────────────────────────────────────────────────── */
export function getTentaclePoints(cfg: Config, angle: number): LED[] {
  const { wave_amplitude: amp, wave_frequency: freq, length } = cfg.tentacles;

  const leds: LED[] = [];
  const numLights = 12;

  for (let j = 0; j < numLights; j++) {
    const s = j / (numLights - 1);
    const wave = amp * Math.sin(2 * Math.PI * freq * s);

    leds.push({
      position: new THREE.Vector3(
        (cfg.bell.radius + wave) * Math.cos(angle),
        (cfg.bell.radius + wave) * Math.sin(angle),
        -s * length + bellProfile(cfg.bell.radius)
      ),
      group: "tentacle",
      index: j,
      t: s,
      angle,
    });
  }

  return leds;
}

/* ──────────────────────────────────────────────────────────────
   Central LEDs
────────────────────────────────────────────────────────────── */
export function getCentralPoints(cfg: Config, angle: number): LED[] {
  const { wave_amplitude: amp, wave_frequency: freq, length, ring_radius } =
    cfg.central;

  const leds: LED[] = [];
  const numLights = 8;

  for (let j = 0; j < numLights; j++) {
    const s = j / (numLights - 1);
    const wave = amp * Math.sin(2 * Math.PI * freq * s);

    leds.push({
      position: new THREE.Vector3(
        (ring_radius + wave) * Math.cos(angle),
        (ring_radius + wave) * Math.sin(angle),
        -s * length
      ),
      group: "central",
      index: j,
      t: s,
      angle,
    });
  }

  return leds;
}