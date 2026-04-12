import matplotlib.widgets as widgets

from config import cfg

PANEL = [
    ('bell.radius',                0.5,  5.0,  0.1  ),

    ('tentacles.count',            1,    32,   1    ),
    ('tentacles.radius',           0.01, 0.3,  0.005),
    ('tentacles.length',           0.5,  8.0,  0.1  ),
    ('tentacles.wave_amplitude',   0.0,  0.5,  0.01 ),
    ('tentacles.wave_frequency',   0.0,  5.0,  0.25 ),

    ('central.count',              1,    16,   1    ),
    ('central.radius',             0.01, 0.5,  0.01 ),
    ('central.length',             0.5,  10.0, 0.1  ),
    ('central.wave_amplitude',     0.0,  0.5,  0.01 ),
    ('central.wave_frequency',     0.0,  5.0,  0.25 ),
    ('central.ring_radius',        0.0,  2.0,  0.05 ),
]

SECTION_LABELS = {'bell': 'Bell', 'tentacles': 'Outer tentacles', 'central': 'Central tentacles'}

SL_LEFT  = 0.63
SL_WIDTH = 0.32
SL_H     = 0.042
SL_GAP   = 0.008
HDR_H    = 0.050


def _resolve(path):
    section, field = path.split('.')
    return getattr(cfg, section), field

def _getter(path):
    obj, field = _resolve(path)
    return getattr(obj, field)

def _setter(path, v):
    obj, field = _resolve(path)
    setattr(obj, field, int(v) if isinstance(getattr(obj, field), int) else v)

def _label(field):
    return field.replace('_', ' ').capitalize()


def build_panel(fig, draw):
    sliders = {}
    y = 0.96
    prev_section = None

    for path, lo, hi, step in PANEL:
        section, field = path.split('.')

        if section != prev_section:
            y -= HDR_H
            fig.text(SL_LEFT, y, SECTION_LABELS[section].upper(),
                     color='#88bbdd', fontsize=8, fontweight='bold',
                     transform=fig.transFigure)
            prev_section = section

        y -= SL_H
        ax_s = fig.add_axes([SL_LEFT, y, SL_WIDTH, SL_H - 0.004], facecolor='#0e1e2e')
        sl = widgets.Slider(ax_s, _label(field), lo, hi,
                            valinit=_getter(path), valstep=step,
                            color='#3a6a99', initcolor='none')
        sl.label.set_color('#aaccee')
        sl.label.set_fontsize(8)
        sl.valtext.set_color('#aaccee')
        sl.valtext.set_fontsize(8)
        sliders[path] = sl
        y -= SL_GAP

    _timer = [None]

    def on_change(_val):
        if _timer[0] is not None:
            _timer[0].stop()
        _timer[0] = fig.canvas.new_timer(interval=150)
        _timer[0].single_shot = True
        _timer[0].add_callback(redraw)
        _timer[0].start()

    def redraw():
        for path, sl in sliders.items():
            _setter(path, sl.val)
        draw()

    for sl in sliders.values():
        sl.on_changed(on_change)
