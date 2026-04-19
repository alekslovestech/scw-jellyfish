import * as THREE from "three";
import { LEDSystem } from "./ledSystem";
import { AnimationManager } from "./animationManager";

export class LEDRenderer {
  private geometry: THREE.BufferGeometry;
  private material: THREE.PointsMaterial;
  private points: THREE.Points;

  private positionArray!: Float32Array;
  private colorArray!: Float32Array;
  private lastCount: number = 0;

  constructor(private ledSystem: LEDSystem, private animationManager: AnimationManager) {
    this.geometry = new THREE.BufferGeometry();

   this.material = new THREE.PointsMaterial({
      size: 0.15,
      vertexColors: true,
      transparent: true,
      opacity: 1,
      depthWrite: false,
      depthTest: false,
      blending: THREE.AdditiveBlending,
    });

    this.points = new THREE.Points(this.geometry, this.material);
    this.points.renderOrder = 999;

    this.rebuild();
  }

  // ─────────────────────────────────────────────
  // Build buffers
  // ─────────────────────────────────────────────
  rebuild(): void {
    const leds = this.ledSystem.getAll();
    const count = leds.length;

    this.positionArray = new Float32Array(count * 3);
    this.colorArray = new Float32Array(count * 3);

    this.geometry.setAttribute(
      "position",
      new THREE.BufferAttribute(this.positionArray, 3)
    );

    this.geometry.setAttribute(
      "color",
      new THREE.BufferAttribute(this.colorArray, 3)
    );
  }

  // ─────────────────────────────────────────────
  // Update every frame
  // ─────────────────────────────────────────────
  update(): void {
    const leds = this.ledSystem.getAll();

    if (leds.length === 0) return;

    if (leds.length !== this.lastCount) {
      this.rebuild();
      this.lastCount = leds.length;
      return;
    }

    // ── Run animation (sets led.color and led.intensity)
    this.animationManager.update(leds, Date.now() / 1000);

    // ── Write to buffers
    for (let i = 0; i < leds.length; i++) {
      const led = leds[i];

      this.positionArray[i * 3 + 0] = led.position.x;
      this.positionArray[i * 3 + 1] = led.position.y;
      this.positionArray[i * 3 + 2] = led.position.z;

      this.colorArray[i * 3 + 0] = led.color.r * led.intensity;
      this.colorArray[i * 3 + 1] = led.color.g * led.intensity;
      this.colorArray[i * 3 + 2] = led.color.b * led.intensity;
    }

    (this.geometry.getAttribute("position") as THREE.BufferAttribute).needsUpdate = true;
    (this.geometry.getAttribute("color") as THREE.BufferAttribute).needsUpdate = true;
  }

  // ─────────────────────────────────────────────
  getObject(): THREE.Points {
    return this.points;
  }

  dispose(): void {
    this.geometry.dispose();
    this.material.dispose();
  }
}