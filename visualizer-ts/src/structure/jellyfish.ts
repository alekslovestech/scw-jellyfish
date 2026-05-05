import * as THREE from "three";
import type { Config } from "../config";
import { makeBell, makeTentacle, makeCentralTentacle } from "./shapes";
import { getStripPoints, getInnerOnlyStripPoints, getBellOuterStripPoints } from "./lights";
import { LEDSystem } from "../core/ledSystem";

export class Jellyfish {
  readonly id: number;
  readonly relative_size: number;
  readonly position: THREE.Vector3;

  private readonly cfg: Config;
  private readonly leds: LEDSystem;

  private readonly bellGeo: THREE.BufferGeometry;
  private readonly outerGeos: THREE.BufferGeometry[];
  private readonly innerGeos: THREE.BufferGeometry[];

  constructor(
    id: number,
    cfg: Config,
    relative_size: number,
    x: number,
    y: number,
    z: number,
    leds: LEDSystem
  ) {
    this.id = id;
    this.cfg = cfg;
    this.relative_size = relative_size;
    this.position = new THREE.Vector3(x, y, z);
    this.leds = leds;

    // ─────────────────────────────
    // Geometry (visual mesh only)
    // ─────────────────────────────
    this.bellGeo = makeBell(cfg);

    this.outerGeos = Array.from(
      { length: cfg.hardware.strips_per_jelly },
      (_, k) => makeTentacle(cfg, (k * 2 * Math.PI) / cfg.hardware.strips_per_jelly)
    );

    this.innerGeos = Array.from(
      { length: cfg.hardware.strips_per_jelly },
      (_, k) => makeCentralTentacle(cfg, (k * 2 * Math.PI) / cfg.hardware.strips_per_jelly)
    );

    // ─────────────────────────────
    // LED registration
    // ─────────────────────────────
    if (id === 0) {
      // Hero jelly: 16 strips (8 inner-only + 8 bell+outer)
      for (let s = 0; s < cfg.jelly0.inner_strips; s++) {
        for (const p of getInnerOnlyStripPoints(cfg, s)) {
          this.leds.addLED({
            group: p.segment,
            jellyId: this.id,
            t: p.t,
            position: p.position.clone().multiplyScalar(relative_size).add(this.position),
          });
        }
      }
      for (let s = 0; s < cfg.jelly0.bell_outer_strips; s++) {
        for (const p of getBellOuterStripPoints(cfg, s)) {
          this.leds.addLED({
            group: p.segment,
            jellyId: this.id,
            t: p.t,
            position: p.position.clone().multiplyScalar(relative_size).add(this.position),
          });
        }
      }
    } else {
      // Standard jelly: 8 strips × 50 LEDs (inner tip → bell → outer tip)
      for (let s = 0; s < cfg.hardware.strips_per_jelly; s++) {
        for (const p of getStripPoints(cfg, s)) {
          this.leds.addLED({
            group: p.segment,
            jellyId: this.id,
            t: p.t,
            position: p.position.clone().multiplyScalar(relative_size).add(this.position),
          });
        }
      }
    }
  }

  // ─────────────────────────────
  // Scene geometry only
  // ─────────────────────────────
  toGroup(
    bellMat: THREE.Material,
    outerMat: THREE.Material,
    innerMat: THREE.Material
  ): THREE.Group {
    const group = new THREE.Group();

    group.scale.setScalar(this.relative_size);
    group.position.copy(this.position);

    group.add(new THREE.Mesh(this.bellGeo, bellMat));

    for (const geo of this.outerGeos) {
      group.add(new THREE.Mesh(geo, outerMat));
    }

    for (const geo of this.innerGeos) {
      group.add(new THREE.Mesh(geo, innerMat));
    }

    return group;
  }

  dispose(): void {
    this.bellGeo.dispose();
    this.outerGeos.forEach((g) => g.dispose());
    this.innerGeos.forEach((g) => g.dispose());
  }
}
