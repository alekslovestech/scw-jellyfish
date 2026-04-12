import numpy as np


# ── Mesh resolution ───────────────────────────────────────────────────────────
_n_s   = 60
_n_u   = 20
_s_arr = np.linspace(0, 1, _n_s)
_u_arr = np.linspace(0, 2 * np.pi, _n_u, endpoint=False)
_cos_u = np.cos(_u_arr)
_sin_u = np.sin(_u_arr)


def bell_profile(r):
    """Height of the bell at radial distance r from the axis.
    Replace with any function f(r) where f(0) is the peak and f(bell.radius) ≈ 0.
    """
    return 0.2*r**2 - 0.05*r**4


def make_bell(cfg):
    r, th = np.meshgrid(
        np.linspace(0, cfg.bell.radius, 40),
        np.linspace(0, 2 * np.pi, 60),
    )
    return r * np.cos(th), r * np.sin(th), bell_profile(r)


def make_tentacle(cfg, angle):
    a = angle
    wave   = cfg.tentacles.wave_amplitude * np.sin(2 * np.pi * cfg.tentacles.wave_frequency * _s_arr)
    wave_d = cfg.tentacles.wave_amplitude * 2 * np.pi * cfg.tentacles.wave_frequency \
             * np.cos(2 * np.pi * cfg.tentacles.wave_frequency * _s_arr)

    spine_x = (cfg.bell.radius + wave) * np.cos(a)
    spine_y = (cfg.bell.radius + wave) * np.sin(a)
    spine_z = -_s_arr * cfg.tentacles.length + bell_profile(cfg.bell.radius)

    mag = np.sqrt(wave_d ** 2 + cfg.tentacles.length ** 2)
    bx  =  cfg.tentacles.length * np.cos(a) / mag
    by  =  cfg.tentacles.length * np.sin(a) / mag
    bz  =  wave_d / mag

    X = spine_x + cfg.tentacles.radius * (_cos_u[:, None] * (-np.sin(a)) + _sin_u[:, None] * bx)
    Y = spine_y + cfg.tentacles.radius * (_cos_u[:, None] *   np.cos(a)  + _sin_u[:, None] * by)
    Z = spine_z + cfg.tentacles.radius * (_sin_u[:, None] * bz)
    return X, Y, Z


def make_central_tentacle(cfg, angle):
    a = angle
    wave   = cfg.central.wave_amplitude * np.sin(2 * np.pi * cfg.central.wave_frequency * _s_arr)
    wave_d = cfg.central.wave_amplitude * 2 * np.pi * cfg.central.wave_frequency \
             * np.cos(2 * np.pi * cfg.central.wave_frequency * _s_arr)

    spine_x = (cfg.central.ring_radius + wave) * np.cos(a)
    spine_y = (cfg.central.ring_radius + wave) * np.sin(a)
    spine_z = -_s_arr * cfg.central.length

    mag = np.sqrt(wave_d ** 2 + cfg.central.length ** 2)
    bx  =  cfg.central.length * np.cos(a) / mag
    by  =  cfg.central.length * np.sin(a) / mag
    bz  =  wave_d / mag

    X = spine_x + cfg.central.radius * (_cos_u[:, None] * (-np.sin(a)) + _sin_u[:, None] * bx)
    Y = spine_y + cfg.central.radius * (_cos_u[:, None] *   np.cos(a)  + _sin_u[:, None] * by)
    Z = spine_z + cfg.central.radius * (                                  _sin_u[:, None] * bz)
    return X, Y, Z
