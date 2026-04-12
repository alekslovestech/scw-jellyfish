import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D  # noqa: F401

from config import cfg
from shapes import make_bell, make_tentacle, make_central_tentacle
from panel import build_panel

fig = plt.figure(figsize=(13, 8), facecolor='#0a1520')
ax = fig.add_axes([0.0, 0.0, 0.58, 1.0], projection='3d', facecolor='#000d1a')

def draw():
    ax.cla()
    ax.plot_surface(*make_bell(cfg),  color='#55aaee', alpha=0.45, linewidth=0, antialiased=True)
    for k in range(cfg.tentacles.count):
        a = k * 2 * np.pi / cfg.tentacles.count
        ax.plot_surface(*make_tentacle(cfg, a), color='#ed1e07', alpha=0.75, linewidth=0, antialiased=True)
    for k in range(cfg.central.count):
        a = k * 2 * np.pi / cfg.central.count
        ax.plot_surface(*make_central_tentacle(cfg, a), color='#16f706', alpha=0.75, linewidth=0, antialiased=True)
    ax.set_xlim(-3.2, 3.2)
    ax.set_ylim(-3.2, 3.2)
    ax.set_zlim(-5.5, 3.0)
    ax.set_axis_off()
    fig.canvas.draw_idle()

draw()
build_panel(fig, draw)
plt.show()
