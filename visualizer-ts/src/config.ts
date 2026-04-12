export interface BellConfig {
  radius: number;
}

export interface TentacleConfig {
  count:          number;
  radius:         number;
  length:         number;
  wave_amplitude: number;
  wave_frequency: number;
}

export interface CentralConfig {
  count:          number;
  radius:         number;
  length:         number;
  wave_amplitude: number;
  wave_frequency: number;
  ring_radius:    number;
}

export interface Config {
  bell:      BellConfig;
  tentacles: TentacleConfig;
  central:   CentralConfig;
}

export const cfg: Config = {
  bell: {
    radius: 2.5,
  },
  tentacles: {
    count:          16,
    radius:         0.03,
    length:         2.5,
    wave_amplitude: 0.1,
    wave_frequency: 1.0,
  },
  central: {
    count:          8,
    radius:         0.1,
    length:         5.0,
    wave_amplitude: 0.1,
    wave_frequency: 2.0,
    ring_radius:    0.7,
  },
};
