import * as THREE from "three";
import { LEDSystem } from "../core/ledSystem";
import { cfg } from "../config";

function starVertex(index: number): THREE.Vector3 {
  const isOuter = index % 2 === 0;
  const r = isOuter ? cfg.R_OUTER : cfg.R_INNER;
  const angle = Math.PI / 2 - (index * Math.PI * 2) / cfg.NUM_EDGES;
  return new THREE.Vector3(Math.cos(angle) * r, Math.sin(angle) * r, 0);
}

export class Star {
  constructor(private ledSystem: LEDSystem) {
    this.build();
  }

  private build() {
    for (let edge = 0; edge < cfg.NUM_EDGES; edge++) {
      const vStart = starVertex(edge);
      const vEnd = starVertex((edge + 1) % cfg.NUM_EDGES);

      for (let j = 0; j < cfg.LEDS_PER_EDGE; j++) {
        const frac = j / cfg.LEDS_PER_EDGE;
        const pos = new THREE.Vector3().lerpVectors(vStart, vEnd, frac);
        this.ledSystem.addLED(pos);
      }
    }
  }

  toGroup(): THREE.Group {
    const group = new THREE.Group();

    // Faint star outline so you can see the shape
    const points: THREE.Vector3[] = [];
    for (let i = 0; i <= cfg.NUM_EDGES; i++) {
      points.push(starVertex(i % cfg.NUM_EDGES));
    }
    const geom = new THREE.BufferGeometry().setFromPoints(points);
    const line = new THREE.Line(
      geom,
      new THREE.LineBasicMaterial({ color: 0x222244, transparent: true, opacity: 0.35 })
    );
    group.add(line);

    return group;
  }
}
