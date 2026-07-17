// NOTE: config.ts is a low-level module imported by almost everything (ledMap,
// every animation). It must NOT import an animation, or it forms a cycle
// (config → animation → ledMap → config) that reads `cfg` before it's defined.
// The default/base animation is chosen in main.ts instead.

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

// Two-colour palette for the movementSimulation base (inner tips → outer tips).
// Values are normalised 0–1 RGB (so 255 = 1.0), used directly by the animation.
// The panel binds a colour picker in float mode. Editable live — no rebuild.
export interface RGB01 {
  r: number;
  g: number;
  b: number;
}

export interface PaletteConfig {
  inner: RGB01; // colour at the inner tentacle tips
  outer: RGB01; // colour at the outer tentacle tips
}

// Three-colour gradient for the fireSteady base (over a fixed white hot core):
//   c1 near the base → c2 mid → c3 at the tips. Default yellow→orange→red = fire;
//   retune toward green or blue to match fireSpread's later palettes.
export interface FirePaletteConfig {
  c1: RGB01;
  c2: RGB01;
  c3: RGB01;
}

// Master per-segment brightness (0–1), applied to every animation as the final
// step after the base + effects. `inner` scales the inner tentacles; `outer`
// scales the outer tentacles and the bell together.
export interface BrightnessConfig {
  inner: number;
  outer: number;
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
  hardware: HardwareConfig;
  jelly0: Jelly0HardwareConfig;
  bell: BellConfig;
  tentacle: TentacleConfig;  // outer tentacle geometry
  inner: InnerConfig;        // inner tentacle geometry
  colors: ColorsConfig;
  palette: PaletteConfig;
  firePalette: FirePaletteConfig;
  brightness: BrightnessConfig;
  lighting: LightingConfig;
  size_ratio: number; // scale factor per level (e.g. 0.6 → each level is 60% of previous)
  z_offset: number; // Z step between levels; level N sits at N * z_offset
  orbital_radius: number; // orbital radius for smaller fish
  orbital_random: number; // random offset for orbital radius
  height_random: number; // random offset for height (z position)
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
  palette: {
    inner: { r: 1, g: 0.5, b: 0 }, // orange — inner tips
    outer: { r: 0, g: 1, b: 0 },   // green — outer tips
  },
  firePalette: {
    c1: { r: 1, g: 1, b: 0 },   // yellow — near the base
    c2: { r: 1, g: 0.2, b: 0 }, // orange — mid
    c3: { r: 1, g: 0, b: 0 },   // red — tips
  },
  brightness: {
    inner: 1, // full brightness by default
    outer: 1,
  },
  lighting: {
    ambient_color: 0x334455,
    ambient_intensity: 8.0,
    key_color: 0x88bbff,
    key_intensity: 200,
    fill_color: 0x223366,
    fill_intensity: 60,
  },
  size_ratio: 0.6,
  z_offset: -3.0,
  orbital_radius: 8.0,
  orbital_random: 1.5,
  height_random: 1.0,
  sizes: [
    { level: 0, count: 1 },
    { level: 1, count: 8 },
  ],
};
