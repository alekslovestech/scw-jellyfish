import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D  # noqa: F401

fig = plt.figure(figsize=(6, 7), facecolor='#000d1a')
ax = fig.add_subplot(111, projection='3d', facecolor='#000d1a')

# ── Bell (half-sphere) ────────────────────────────────────────────────────────
BELL_RADIUS = 2.5

# ── Tentacles ─────────────────────────────────────────────────────────────────
TENTACLE_RADIUS    = 0.03   # tube thickness
TENTACLE_LENGTH    = 2.5    # hang distance
WAVE_AMPLITUDE     = 0.1   # sine-wave amplitude (0 = straight vertical)
WAVE_FREQUENCY     = 1.0    # number of full waves along the tentacle

# ── Central tentacles (3, evenly spaced on a small inner ring) ────────────────
CENTRAL_COUNT          = 8      # number of inner tentacles
CENTRAL_RADIUS         = 0.1    # tube thickness
CENTRAL_LENGTH         = 5.0    # hang distance
CENTRAL_WAVE_AMPLITUDE = 0.1    # 0 = straight vertical
CENTRAL_WAVE_FREQUENCY = 2.0
CENTRAL_RING_RADIUS    = 0.7    # radius of the inner ring

def bell_profile(r):
    """Height of the bell at radial distance r from the axis.
    Replace with any function f(r) where f(0) is the peak and f(BELL_RADIUS) = 0.
    Default: hemisphere  f(r) = sqrt(R² - r²)
    """
    return 0.2*r**2-0.05*r**4

r_b, th_b = np.meshgrid(np.linspace(0, BELL_RADIUS, 40), np.linspace(0, 2 * np.pi, 60))
ax.plot_surface(r_b * np.cos(th_b), r_b * np.sin(th_b), bell_profile(r_b),
                color='#55aaee', alpha=0.45, linewidth=0, antialiased=True)

# ── Closing disc (solid, at the bell opening z=0) ─────────────────────────────
#r_d, th_d = np.meshgrid(np.linspace(0, BELL_RADIUS, 30), np.linspace(0, 2 * np.pi, 60))
#ax.plot_surface(r_d * np.cos(th_d), r_d * np.sin(th_d), np.zeros_like(r_d),
 #               color='#55aaee', alpha=1.0, linewidth=0, antialiased=True)



n_s   = 60
n_u   = 20
s_arr = np.linspace(0, 1, n_s)
u_arr = np.linspace(0, 2 * np.pi, n_u, endpoint=False)
cos_u = np.cos(u_arr)   # (n_u,)
sin_u = np.sin(u_arr)   # (n_u,)

NUM_TENTACLES = 16  
for k in range(NUM_TENTACLES):
    a = k * 2 * np.pi / NUM_TENTACLES

    # Spine: sine wave in the radial direction, straight down in z
    wave   = WAVE_AMPLITUDE * np.sin(2 * np.pi * WAVE_FREQUENCY * s_arr)
    wave_d = WAVE_AMPLITUDE * 2 * np.pi * WAVE_FREQUENCY \
             * np.cos(2 * np.pi * WAVE_FREQUENCY * s_arr)

    spine_x = (BELL_RADIUS + wave) * np.cos(a)
    spine_y = (BELL_RADIUS + wave) * np.sin(a)
    spine_z = -s_arr * TENTACLE_LENGTH + bell_profile(BELL_RADIUS)  # start at the bell surface

    # Frenet frame for the tube cross-section
    # N = tangential direction around the jellyfish (-sin a, cos a, 0) — constant
    # B = T × N — lies in the radial-z plane, already unit-length
    mag = np.sqrt(wave_d ** 2 + TENTACLE_LENGTH ** 2)
    bx  =  TENTACLE_LENGTH * np.cos(a) / mag
    by  =  TENTACLE_LENGTH * np.sin(a) / mag
    bz  =  wave_d / mag

    # Broadcast: (n_u, 1) * (n_s,) → (n_u, n_s)
    X = spine_x + TENTACLE_RADIUS * (cos_u[:, None] * (-np.sin(a)) + sin_u[:, None] * bx)
    Y = spine_y + TENTACLE_RADIUS * (cos_u[:, None] *   np.cos(a)  + sin_u[:, None] * by)
    Z = spine_z + TENTACLE_RADIUS * (sin_u[:, None] * bz)

    ax.plot_surface(X, Y, Z, color="#ed1e07", alpha=0.75, linewidth=0, antialiased=True)


for k in range(CENTRAL_COUNT):
    a = k * 2 * np.pi / CENTRAL_COUNT

    wave   = CENTRAL_WAVE_AMPLITUDE * np.sin(2 * np.pi * CENTRAL_WAVE_FREQUENCY * s_arr)
    wave_d = CENTRAL_WAVE_AMPLITUDE * 2 * np.pi * CENTRAL_WAVE_FREQUENCY \
             * np.cos(2 * np.pi * CENTRAL_WAVE_FREQUENCY * s_arr)

    spine_x = (CENTRAL_RING_RADIUS + wave) * np.cos(a)
    spine_y = (CENTRAL_RING_RADIUS + wave) * np.sin(a)
    spine_z = -s_arr * CENTRAL_LENGTH

    mag = np.sqrt(wave_d ** 2 + CENTRAL_LENGTH ** 2)
    bx  =  CENTRAL_LENGTH * np.cos(a) / mag
    by  =  CENTRAL_LENGTH * np.sin(a) / mag
    bz  =  wave_d / mag

    X = spine_x + CENTRAL_RADIUS * (cos_u[:, None] * (-np.sin(a)) + sin_u[:, None] * bx)
    Y = spine_y + CENTRAL_RADIUS * (cos_u[:, None] *   np.cos(a)  + sin_u[:, None] * by)
    Z = spine_z + CENTRAL_RADIUS * (                                 sin_u[:, None] * bz)

    ax.plot_surface(X, Y, Z, color="#16f706", alpha=0.75, linewidth=0, antialiased=True)

# ── Style ─────────────────────────────────────────────────────────────────────
ax.set_xlim(-3.2, 3.2)
ax.set_ylim(-3.2, 3.2)
ax.set_zlim(-5.5, 3.0)
ax.set_axis_off()
ax.view_init(elev=20, azim=30)

plt.tight_layout()
plt.show()
