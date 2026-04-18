import * as THREE from "three";
import type { Config } from "../config";
import { makeBell, makeTentacle, makeCentralTentacle } from "./shapes";
import { getTentaclePoints, getCentralPoints, getBellPoints } from "./lights";
import { LEDSystem } from "../core/ledSystem";

export class Jellyfish {
  readonly relative_size: number;
  readonly position: THREE.Vector3;

  private readonly cfg: Config;
  private readonly leds: LEDSystem;

  private readonly bellGeo: THREE.BufferGeometry;
  private readonly tentacleGeos: THREE.BufferGeometry[];
  private readonly centralGeos: THREE.BufferGeometry[];

  constructor(
    cfg: Config,
    relative_size: number,
    x: number,
    y: number,
    z: number,
    leds: LEDSystem
  ) {
    this.cfg = cfg;
    this.relative_size = relative_size;
    this.position = new THREE.Vector3(x, y, z);
    this.leds = leds;

    // ─────────────────────────────
    // Geometry
    // ─────────────────────────────
    this.bellGeo = makeBell(cfg);

    this.tentacleGeos = Array.from({ length: cfg.tentacles.count }, (_, k) =>
      makeTentacle(cfg, (k * 2 * Math.PI) / cfg.tentacles.count)
    );

    this.centralGeos = Array.from({ length: cfg.central.count }, (_, k) =>
      makeCentralTentacle(cfg, (k * 2 * Math.PI) / cfg.central.count)
    );

    // ─────────────────────────────
    // LED REGISTRATION (CLEAN VERSION)
    // ─────────────────────────────

    // Bell LEDs
    for (const p of getBellPoints(cfg)) {
      this.leds.addLED({
        group: "bell",
        position: p.position.clone().multiplyScalar(relative_size).add(this.position),
      });
    }

    // Tentacle LEDs
    for (let k = 0; k < cfg.tentacles.count; k++) {
      const angle = (k * 2 * Math.PI) / cfg.tentacles.count;

      for (const p of getTentaclePoints(cfg, angle)) {
        this.leds.addLED({
          group: "tentacle",
          position: p.position.clone().multiplyScalar(relative_size).add(this.position),
        });
      }
    }

    // Central LEDs
    for (let k = 0; k < cfg.central.count; k++) {
      const angle = (k * 2 * Math.PI) / cfg.central.count;

      for (const p of getCentralPoints(cfg, angle)) {
        this.leds.addLED({
          group: "central",
          position: p.position.clone().multiplyScalar(relative_size).add(this.position),
        });
      }
    }
  }

  // ─────────────────────────────
  // Scene geometry only
  // ─────────────────────────────
  toGroup(
    bellMat: THREE.Material,
    tentMat: THREE.Material,
    centralMat: THREE.Material
  ): THREE.Group {
    const group = new THREE.Group();

    group.scale.setScalar(this.relative_size);
    group.position.copy(this.position);

    group.add(new THREE.Mesh(this.bellGeo, bellMat));

    for (const geo of this.tentacleGeos) {
      group.add(new THREE.Mesh(geo, tentMat));
    }

    for (const geo of this.centralGeos) {
      group.add(new THREE.Mesh(geo, centralMat));
    }

    return group;
  }

  dispose(): void {
    this.bellGeo.dispose();
    this.tentacleGeos.forEach((g) => g.dispose());
    this.centralGeos.forEach((g) => g.dispose());
  }
}