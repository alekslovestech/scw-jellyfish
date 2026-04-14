import * as THREE from 'three';
import type { Config } from './config';
import { makeBell, makeTentacle, makeCentralTentacle } from './shapes';
import { getTentaclePoints, getCentralPoints, getBellPoints, createLightMesh } from './lights';

export class Jellyfish {
  readonly relative_size: number;
  readonly position: THREE.Vector3;

  // Store the config so it's accessible in toGroup
  private readonly cfg: Config;

  private readonly bellGeo:      THREE.BufferGeometry;
  private readonly tentacleGeos: THREE.BufferGeometry[];
  private readonly centralGeos:  THREE.BufferGeometry[];

  constructor(cfg: Config, relative_size: number, x: number, y: number, z: number) {
    this.cfg = cfg;
    this.relative_size = relative_size;
    this.position      = new THREE.Vector3(x, y, z);

    // Build Geometries
    this.bellGeo = makeBell(cfg);
    this.tentacleGeos = Array.from({ length: cfg.tentacles.count }, (_, k) =>
      makeTentacle(cfg, k * 2 * Math.PI / cfg.tentacles.count),
    );
    this.centralGeos = Array.from({ length: cfg.central.count }, (_, k) =>
      makeCentralTentacle(cfg, k * 2 * Math.PI / cfg.central.count),
    );
  }

  toGroup(
    bellMat:    THREE.Material,
    tentMat:    THREE.Material,
    centralMat: THREE.Material,
  ): THREE.Group {
    const group = new THREE.Group();
    group.scale.setScalar(this.relative_size);
    group.position.copy(this.position);

    // 1. Add Main Body Meshes
    group.add(new THREE.Mesh(this.bellGeo, bellMat));
    for (const geo of this.tentacleGeos) group.add(new THREE.Mesh(geo, tentMat));
    for (const geo of this.centralGeos)  group.add(new THREE.Mesh(geo, centralMat));

    // 2. Add Light Dots (using this.cfg)
    
    // Bell Lights
    getBellPoints(this.cfg).forEach(p => group.add(createLightMesh(p)));

    // Outer Tentacle Lights
    for (let k = 0; k < this.cfg.tentacles.count; k++) {
      const angle = k * 2 * Math.PI / this.cfg.tentacles.count;
      getTentaclePoints(this.cfg, angle).forEach(p => group.add(createLightMesh(p)));
    }

    // Central Tentacle Lights
    for (let k = 0; k < this.cfg.central.count; k++) {
      const angle = k * 2 * Math.PI / this.cfg.central.count;
      getCentralPoints(this.cfg, angle).forEach(p => group.add(createLightMesh(p)));
    }

    return group;
  }

  dispose(): void {
    this.bellGeo.dispose();
    this.tentacleGeos.forEach(g => g.dispose());
    this.centralGeos.forEach(g => g.dispose());
  }
}