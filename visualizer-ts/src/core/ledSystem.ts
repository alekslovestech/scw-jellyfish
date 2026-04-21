import * as THREE from "three";

export type LEDGroup = "bell" | "tentacle" | "central";

export type LED = {
  id: number;
  group: LEDGroup;
  position: THREE.Vector3;

  // runtime animated values only
  intensity: number;
  color: THREE.Color;
};

export class LEDSystem {
  private leds: LED[] = [];

  private byGroup: Record<LEDGroup, LED[]> = {
    bell: [],
    tentacle: [],
    central: [],
  };

  private nextId = 0;

  // ─────────────────────────────
  // ADD LED (SAFE + SIMPLE)
  // ─────────────────────────────
  addLED(led: Omit<LED, "id" | "color" | "intensity"> & Partial<Pick<LED, "color" | "intensity">>): LED {
  const full: LED = {
    id: this.nextId++,
    group: led.group,
    position: led.position,

    // FORCE VISIBILITY
    intensity: 1,
    color: new THREE.Color(0x00ff00), // bright green
  };

  this.leds.push(full);
  this.byGroup[full.group].push(full);

  return full;
}

  addMany(leds: Array<Omit<LED, "id" | "color" | "intensity"> & Partial<Pick<LED, "color" | "intensity">>>) {
    for (const led of leds) this.addLED(led);
  }

  // ─────────────────────────────
  // ACCESS
  // ─────────────────────────────
  getAll(): LED[] {
    return this.leds;
  }

  getByGroup(group: LEDGroup): LED[] {
    return this.byGroup[group];
  }

  // ─────────────────────────────
  // RESET (IMPORTANT FIX)
  // ─────────────────────────────
  reset(): void {
    this.leds = [];
    this.byGroup = {
      bell: [],
      tentacle: [],
      central: [],
    };
    this.nextId = 0;
  }

  // ─────────────────────────────
  // ANIMATION HELPERS
  // ─────────────────────────────
  setIntensityAll(value: number): void {
    for (const led of this.leds) {
      led.intensity = value;
    }
  }
}