import * as THREE from 'three';
import type { Config } from './config';
import { makeBell, makeTentacle, makeCentralTentacle } from './shapes';

export class Jellyfish {
  readonly relative_size: number;
  readonly position: THREE.Vector3;

  private readonly bellGeo:      THREE.BufferGeometry;
  private readonly tentacleGeos: THREE.BufferGeometry[];
  private readonly centralGeos:  THREE.BufferGeometry[];

  constructor(cfg: Config, relative_size: number, x: number, y: number, z: number) {
    this.relative_size = relative_size;
    this.position      = new THREE.Vector3(x, y, z);

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

    group.add(new THREE.Mesh(this.bellGeo, bellMat));
    for (const geo of this.tentacleGeos) group.add(new THREE.Mesh(geo, tentMat));
    for (const geo of this.centralGeos)  group.add(new THREE.Mesh(geo, centralMat));

    return group;
  }

  dispose(): void {
    this.bellGeo.dispose();
    this.tentacleGeos.forEach(g => g.dispose());
    this.centralGeos.forEach(g => g.dispose());
  }
}
