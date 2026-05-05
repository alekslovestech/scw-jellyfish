import * as THREE from "three";
import type { Config } from "../config";
import { bellProfile } from "./shapes";

export type StripLEDPoint = {
  position: THREE.Vector3;
  segment: "inner" | "bell" | "outer";
  posInStrip: number;
  posInSegment: number;
  t: number; // normalized 0→1 within the segment
};

// ─────────────────────────────────────────────────────────────────────────────
// getStripPoints
//
// Returns all 50 LED positions for one physical strip, in wiring order:
//   inner tentacle tip (pos 0) → bell (pos 30) → outer tentacle tip (pos 49)
//
// stripIndex 0–7, mapped to angles 0°, 45°, … 315°.
// ─────────────────────────────────────────────────────────────────────────────
export function getStripPoints(cfg: Config, stripIndex: number): StripLEDPoint[] {
  const { inner_leds, bell_leds, outer_leds, strips_per_jelly } = cfg.hardware;
  const { radius: bell_radius } = cfg.bell;
  const { ring_radius, length: inner_length } = cfg.inner;
  const { length: outer_length } = cfg.tentacle;

  const angle = (stripIndex / strips_per_jelly) * Math.PI * 2;
  const cosA = Math.cos(angle);
  const sinA = Math.sin(angle);

  const points: StripLEDPoint[] = [];

  // ── Inner tentacle (pos 0 → inner_leds-1) ─────────────────────────────────
  // Hangs from center ring. pos=0 is the tip (bottom), pos=29 is the root (bell).
  for (let j = 0; j < inner_leds; j++) {
    const s = j / (inner_leds - 1); // 0 = tip, 1 = root
    const z = -inner_length * (1 - s); // tip at -inner_length, root at 0

    points.push({
      position: new THREE.Vector3(ring_radius * cosA, ring_radius * sinA, z),
      segment: "inner",
      posInStrip: j,
      posInSegment: j,
      t: s,
    });
  }

  // ── Bell (pos inner_leds → inner_leds+bell_leds-1) ────────────────────────
  // Sweeps radially from ring_radius (inner edge) to bell_radius (outer rim).
  // Follows bellProfile curvature.
  for (let j = 0; j < bell_leds; j++) {
    const s = j / (bell_leds - 1); // 0 = inner edge, 1 = outer rim
    const r = ring_radius + (bell_radius - ring_radius) * s;
    const z = bellProfile(r);

    points.push({
      position: new THREE.Vector3(r * cosA, r * sinA, z),
      segment: "bell",
      posInStrip: inner_leds + j,
      posInSegment: j,
      t: s,
    });
  }

  // ── Outer tentacle (pos inner_leds+bell_leds → leds_per_strip-1) ──────────
  // Hangs from the bell rim. pos=0 is the root (bell rim), pos=9 is the tip.
  for (let j = 0; j < outer_leds; j++) {
    const s = j / (outer_leds - 1); // 0 = bell rim, 1 = tip
    const z = bellProfile(bell_radius) - s * outer_length;

    points.push({
      position: new THREE.Vector3(bell_radius * cosA, bell_radius * sinA, z),
      segment: "outer",
      posInStrip: inner_leds + bell_leds + j,
      posInSegment: j,
      t: s,
    });
  }

  return points;
}

// ─────────────────────────────────────────────────────────────────────────────
// getInnerOnlyStripPoints  (jelly 0 — strips 0–7)
//
// All 50 LEDs are inner tentacle.
// pos 0 = tip (bottom), pos 49 = root (at bell).
// ─────────────────────────────────────────────────────────────────────────────
export function getInnerOnlyStripPoints(cfg: Config, stripIndex: number): StripLEDPoint[] {
  const { inner_strips, inner_leds } = cfg.jelly0;
  const { ring_radius, length: inner_length } = cfg.inner;

  const angle = (stripIndex / inner_strips) * Math.PI * 2;
  const cosA  = Math.cos(angle);
  const sinA  = Math.sin(angle);

  const points: StripLEDPoint[] = [];

  for (let j = 0; j < inner_leds; j++) {
    const s = j / (inner_leds - 1); // 0 = tip, 1 = root
    const z = -inner_length * (1 - s);

    points.push({
      position: new THREE.Vector3(ring_radius * cosA, ring_radius * sinA, z),
      segment: "inner",
      posInStrip: j,
      posInSegment: j,
      t: s,
    });
  }

  return points;
}

// ─────────────────────────────────────────────────────────────────────────────
// getBellOuterStripPoints  (jelly 0 — strips 8–15)
//
// 50 LEDs split as bell (25) + outer (25).
// pos  0 = bell inner edge, pos 24 = bell outer rim,
// pos 25 = outer root (bell rim), pos 49 = outer tip (bottom).
// ─────────────────────────────────────────────────────────────────────────────
export function getBellOuterStripPoints(cfg: Config, armIndex: number): StripLEDPoint[] {
  const { bell_outer_strips, bell_leds, outer_leds } = cfg.jelly0;
  const { radius: bell_radius }                       = cfg.bell;
  const { ring_radius }                               = cfg.inner;
  const { length: outer_length }                      = cfg.tentacle;

  const angle = (armIndex / bell_outer_strips) * Math.PI * 2;
  const cosA  = Math.cos(angle);
  const sinA  = Math.sin(angle);

  const points: StripLEDPoint[] = [];

  // Bell: pos 0 (inner edge) → pos bell_leds-1 (outer rim)
  for (let j = 0; j < bell_leds; j++) {
    const s = j / (bell_leds - 1);
    const r = ring_radius + (bell_radius - ring_radius) * s;
    const z = bellProfile(r);

    points.push({
      position: new THREE.Vector3(r * cosA, r * sinA, z),
      segment: "bell",
      posInStrip: j,
      posInSegment: j,
      t: s,
    });
  }

  // Outer: pos bell_leds (root) → pos bell_leds+outer_leds-1 (tip)
  for (let j = 0; j < outer_leds; j++) {
    const s = j / (outer_leds - 1);
    const z = bellProfile(bell_radius) - s * outer_length;

    points.push({
      position: new THREE.Vector3(bell_radius * cosA, bell_radius * sinA, z),
      segment: "outer",
      posInStrip: bell_leds + j,
      posInSegment: j,
      t: s,
    });
  }

  return points;
}
