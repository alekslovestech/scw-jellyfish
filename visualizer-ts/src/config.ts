import { LEDAnimation } from "./animations/types";
import { colorCycle } from "./animations/colorCycle";

// Physical LED strip layout — standard jellyfish (jellies 1–12)
export interface HardwareConfig {
  strips_per_jelly: number; // 8 strips per jellyfish
  leds_per_strip: number;   // 50 LEDs per strip
  inner_leds: number;       // 30 — positions  0–29 (inner tentacle tip → bell root)
  bell_leds: number;        // 10 — positions 30–39 (bell inner edge → outer rim)
  outer_leds: number;       // 10 — positions 40–49 (bell rim → outer tentacle tip)
}

// Physical LED strip layout — hero jellyfish (jelly 0 only)
// Has 16 strips: 8 dedicated inner strips + 8 dedicated bell+outer strips.
export interface Jelly0HardwareConfig {
  inner_strips: number;      // 8 — strips dedicated entirely to inner tentacle
  bell_outer_strips: number; // 8 — strips split between bell and outer tentacle
  leds_per_strip: number;    // 50 — same strip length as standard
  inner_leds: number;        // 50 — full strip is inner (tip=pos 0 → root=pos 49)
  bell_leds: number;         // 25 — first half of bell+outer strip (inner edge → outer rim)
  outer_leds: number;        // 25 — second half of bell+outer strip (root → tip)
}

export interface BellConfig {
  radius: number;
}

export interface TentacleConfig {
  radius: number;
  length: number;
  wave_amplitude: number;
  wave_frequency: number;
}

export interface InnerConfig {
  radius: number;
  length: number;
  wave_amplitude: number;
  wave_frequency: number;
  ring_radius: number;
}

export interface JellyfishSizeConfig {
  level: number; // 0 = biggest; relative size = size_ratio ^ level
  count: number;
}

export interface ColorsConfig {
  background: number;
  bell: number;
  outer: number;
  inner: number;
  dots: number;
}

export interface LightingConfig {
  ambient_color: number;
  ambient_intensity: number;
  key_color: number;
  key_intensity: number;
  fill_color: number;
  fill_intensity: number;
}

export interface Config {
  animation: LEDAnimation;
  hardware: HardwareConfig;
  jelly0: Jelly0HardwareConfig;
  bell: BellConfig;
  tentacle: TentacleConfig;  // outer tentacle geometry
  inner: InnerConfig;        // inner tentacle geometry
  colors: ColorsConfig;
  lighting: LightingConfig;
  size_ratio: number;
  z_offset: number;
  sizes: JellyfishSizeConfig[];
}

export const cfg: Config = {
  hardware: {
    strips_per_jelly: 8,
    leds_per_strip: 50,
    inner_leds: 30,
    bell_leds: 10,
    outer_leds: 10,
  },
  jelly0: {
    inner_strips: 8,
    bell_outer_strips: 8,
    leds_per_strip: 50,
    inner_leds: 50,
    bell_leds: 25,
    outer_leds: 25,
  },
  bell: {
    radius: 2.5,
  },
  tentacle: {
    radius: 0.03,
    length: 2.5,
    wave_amplitude: 0.1,
    wave_frequency: 1.0,
  },
  inner: {
    radius: 0.1,
    length: 5.0,
    wave_amplitude: 0.1,
    wave_frequency: 2.0,
    ring_radius: 0.7,
  },
  colors: {
    background: 0x081c38,
    bell: 0x55aaee,
    outer: 0x3377cc,
    inner: 0x88ccff,
    dots: 0x9900ff,
  },
  lighting: {
    ambient_color: 0x334455,
    ambient_intensity: 8.0,
    key_color: 0x88bbff,
    key_intensity: 200,
    fill_color: 0x223366,
    fill_intensity: 60,
  },
  animation: colorCycle,
  size_ratio: 0.6,
  z_offset: -3.0,
  sizes: [
    { level: 0, count: 1 },
    { level: 1, count: 4 },
    { level: 2, count: 8 },
  ],
};
