import * as THREE from "three";

export type LED = {
  id: number;
  position: THREE.Vector3;
  intensity: number;
  color: THREE.Color;
};

export class LEDSystem {
  private leds: LED[] = [];
  private nextId = 0;

  addLED(position: THREE.Vector3): LED {
    const led: LED = {
      id: this.nextId++,
      position,
      intensity: 1,
      color: new THREE.Color(1, 1, 1),
    };
    this.leds.push(led);
    return led;
  }

  getAll(): LED[] {
    return this.leds;
  }

  reset(): void {
    this.leds = [];
    this.nextId = 0;
  }
}
