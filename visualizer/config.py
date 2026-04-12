from dataclasses import dataclass


@dataclass
class BellConfig:
    radius: float = 2.5


@dataclass
class TentacleConfig:
    count:          int   = 16
    radius:         float = 0.03
    length:         float = 2.5
    wave_amplitude: float = 0.1
    wave_frequency: float = 1.0


@dataclass
class CentralConfig:
    count:          int   = 8
    radius:         float = 0.1
    length:         float = 5.0
    wave_amplitude: float = 0.1
    wave_frequency: float = 2.0
    ring_radius:    float = 0.7


@dataclass
class Config:
    bell:      BellConfig      = None
    tentacles: TentacleConfig  = None
    central:   CentralConfig   = None

    def __post_init__(self):
        self.bell      = self.bell      or BellConfig()
        self.tentacles = self.tentacles or TentacleConfig()
        self.central   = self.central   or CentralConfig()


cfg = Config()
