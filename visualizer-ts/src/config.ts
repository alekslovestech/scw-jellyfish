export interface BellConfig {
  radius: number;
}

export interface TentacleConfig {
  count: number;
  radius: number;
  length: number;
  wave_amplitude: number;
  wave_frequency: number;
}

export interface CentralConfig {
  count: number;
  radius: number;
  length: number;
  wave_amplitude: number;
  wave_frequency: number;
  ring_radius: number;
}

export interface JellyfishSizeConfig {
  level: number; // 0 = biggest; relative size = size_ratio ^ level
  count: number; // how many jellyfish of this level
}

export interface ColorsConfig {
  background: number;
  bell:       number;
  tentacles:  number;
  central:    number;
  dots:       number;
}

export interface LightingConfig {
  ambient_color:      number;
  ambient_intensity:  number;
  key_color:          number;
  key_intensity:      number;
  fill_color:         number;
  fill_intensity:     number;
}

export interface Config {
  bell: BellConfig;
  tentacles: TentacleConfig;
  central: CentralConfig;
  colors: ColorsConfig;
  lighting: LightingConfig;
  size_ratio: number; // scale factor per level (e.g. 0.6 → each level is 60% of previous)
  z_offset: number; // Z step between levels; level N sits at N * z_offset
  sizes: JellyfishSizeConfig[];
}

export const cfg: Config = {
  bell: {
    radius: 2.5,
  },
  tentacles: {
    count: 16,
    radius: 0.03,
    length: 2.5,
    wave_amplitude: 0.1,
    wave_frequency: 1.0,
  },
  central: {
    count: 8,
    radius: 0.1,
    length: 5.0,
    wave_amplitude: 0.1,
    wave_frequency: 2.0,
    ring_radius: 0.7,
  },
  colors: {
    background: 0x081c38,
    bell:       0x55aaee,
    tentacles:  0x3377cc,
    central:    0x88ccff,
    dots:       0x9900ff,   // 0x00ff00  Green, 0xff0000 Red, 0x9900ff Purple, 0x00aaff  Blue
  
  },
  lighting: {
    ambient_color:     0x334455,
    ambient_intensity: 8.0,
    key_color:         0x88bbff,
    key_intensity:     200,
    fill_color:        0x223366,
    fill_intensity:    60,
  },
  size_ratio: 0.6,
  z_offset: -3.0,
  sizes: [
    { level: 0, count: 1 },
    { level: 1, count: 4 },
    { level: 2, count: 8 },
  ],
};
