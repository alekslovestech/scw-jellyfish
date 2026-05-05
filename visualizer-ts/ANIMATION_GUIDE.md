# Animation Guide — LED Mapping & How to Write Animations

This document explains how the LED mapping works and how to write animations
using it. It is the reference for translating any animation to real hardware (Arduino / C++).

---

## 1. The Physical Hardware

There are **13 jellyfish** in the scene:

| Jelly | Type | Strips | LEDs per strip | Total LEDs |
|---|---|---|---|---|
| 0 | Hero | 16 | 50 | 800 |
| 1–12 | Standard | 8 | 50 | 400 each |

**Hero jelly (jelly 0) — 16 strips split into two groups:**

```
Strips  0– 7 → INNER ONLY
  [ inner tip ——————————————————— inner root ]
    pos 0                               pos 49
    (bottom)                             (top)

Strips  8–15 → BELL + OUTER
  [ bell inner edge ——— bell outer rim | outer root ——— outer tip ]
    pos 0                   pos 24       pos 25           pos 49
    (top of bell)         (rim)         (rim)            (bottom)
```

**Standard jelly (jellies 1–12) — 8 strips, each strip contains all three sections:**

```
[ inner tip —— inner root | bell inner — bell outer rim | outer root — outer tip ]
  pos 0            pos 29   pos 30             pos 39     pos 40          pos 49
  (bottom)          (top)   (top)               (rim)      (rim)         (bottom)
```

The strips are evenly spaced angularly: 8 strips → one every 45°.

---

## 2. LED ID Assignment

Every LED in the entire scene has a unique integer ID that counts up from 0.

```
Jelly 0  →  IDs    0 –  799   (800 LEDs)
Jelly 1  →  IDs  800 – 1199   (400 LEDs)
Jelly 2  →  IDs 1200 – 1599   (400 LEDs)
...
Jelly 12 →  IDs 5200 – 5599   (400 LEDs)

Total: 5600 LEDs across all 13 jellies
```

Within each jelly, IDs count through strips in order, then LEDs within each strip:

```
Jelly 1 example (starts at ID 800):
  Strip 0:  IDs  800 –  849   (pos 0–49)
  Strip 1:  IDs  850 –  899
  ...
  Strip 7:  IDs 1150 – 1199
```

---

## 3. The LED Descriptor — What You Get for Every LED

Call `getLEDDescriptor(led.id)` and you receive a descriptor with these fields:

```
jellyId       Which jellyfish this LED belongs to (0–12)
stripIndex    Which physical strip within that jellyfish (0–7 standard, 0–15 hero)
angle_deg     Rotational angle of that strip (0°, 45°, 90° … 315°)
posInStrip    Raw position inside the strip (0–49)
segment       Which section: "inner" | "bell" | "outer"
posInSegment  Position within that section
              → inner:  0 = tip (bottom),  29 or 49 = root (top)
              → bell:   0 = inner edge,    9 or 24 = outer rim
              → outer:  0 = root (top),    9 or 24 = tip (bottom)
t             Normalized 0→1 within the segment (always 0 at one end, 1 at the other)
              → inner:  0 = tip (bottom),  1 = root (top)
              → bell:   0 = inner edge,    1 = outer rim
              → outer:  0 = root (top),    1 = tip (bottom)
```

### Segment sizes

| Segment | Hero (jelly 0) | Standard (jellies 1–12) |
|---|---|---|
| inner | 50 LEDs (posInSegment 0–49) | 30 LEDs (posInSegment 0–29) |
| bell  | 25 LEDs (posInSegment 0–24) | 10 LEDs (posInSegment 0–9)  |
| outer | 25 LEDs (posInSegment 0–24) | 10 LEDs (posInSegment 0–9)  |

---

## 4. The t Value — The Most Important Field for Animations

`t` is a number from **0.0 to 1.0** that tells you where along its segment a LED sits.
It is the same regardless of jellyfish size or type.

```
INNER TENTACLE
  t = 0.0 ——— tip (very bottom, furthest from bell)
  t = 0.5 ——— middle of the tentacle
  t = 1.0 ——— root (top, connects to bell)

BELL
  t = 0.0 ——— inner edge (closest to centre axis)
  t = 0.5 ——— middle of the bell dome
  t = 1.0 ——— outer rim (where tentacles attach)

OUTER TENTACLE
  t = 0.0 ——— root (top, connects to bell rim)
  t = 0.5 ——— middle of the tentacle
  t = 1.0 ——— tip (very bottom, furthest from bell)
```

**Key rule:** `t` always goes 0→1 in the direction the strip is wired.
For "bottom to top" motion on inner tentacles use `t` directly.
For "bottom to top" motion on outer tentacles use `1 - t`.

---

## 5. Writing an Animation — The Pattern

Every animation follows the same three steps:

```
Step 1 — Compute a time value
          Use the global time to decide where you are in the cycle.

Step 2 — Loop over all LEDs
          For each LED call getLEDDescriptor(led.id).
          Use the descriptor fields to decide what colour/brightness to apply.

Step 3 — Set the LED
          led.color.setRGB(r, g, b)   — values 0.0 to 1.0
          led.intensity = value        — overall brightness 0.0 to 1.0
```

---

## 6. Animation Recipes

### Recipe A — Colour every jelly a different colour (static)

```
For each LED:
  look up jellyId  → pick a colour from your palette
  set that colour
```

Used in: `indexTest.ts`

---

### Recipe B — Colour individual segments differently

```
For each LED:
  look up jellyId  → pick a base colour
  look up segment  → if "inner", use blue
                     if "bell",  use green
                     if "outer", use red
  set that colour
```

Used in: `indexTest.ts` (SEGMENT_COLORS overrides)

---

### Recipe C — Wave from bottom to top (inner tentacles)

```
Define a wave front that moves from 0 to 1 over time.

For each inner LED:
  if waveFront > t  →  LED is ON   (wave has passed this LED)
  if waveFront < t  →  LED is OFF  (wave has not reached here yet)
```

Because `t = 0` is the tip (bottom) and `t = 1` is the root (top),
the waveFront starting at 0 lights the bottom first and moves upward.

Used in: `movementSimulation.ts`

---

### Recipe D — Wave from bottom to top (outer tentacles)

Outer tentacles have `t = 0` at the root (top) and `t = 1` at the tip (bottom).
To get bottom-to-top motion you must **invert t**:

```
bottomProgress = 1 - t

if waveFront > bottomProgress  →  LED is ON
```

Used in: `movementSimulation.ts`

---

### Recipe E — Full-jelly top-to-bottom cascade

Map every LED to a single position in a top-to-bottom sweep using this formula:

```
if segment == "bell":
    cascadePos = t * 0.15             (bell occupies the top 15%)

if segment == "inner":
    cascadePos = 0.15 + (1 - t) * 0.85   (root near top, tip at bottom)

if segment == "outer":
    cascadePos = 0.15 + t * 0.85         (root near top, tip at bottom)

Then:  if waveFront > cascadePos  →  LED is ON
```

This produces a wave that starts at the bell centre and fans outward and
downward through both tentacle types simultaneously.

Used in: `synchronizedJellyWave.ts`

---

### Recipe F — Direction verification (hard on/off sweep)

```
Compute offTime for each LED (0 = turns off first, 1 = turns off last):

  inner:  offTime = posInSegment / (segmentLength - 1)
          → 0 at tip (bottom), 1 at root (top)

  outer:  offTime = (segmentLength - 1 - posInSegment) / (segmentLength - 1)
          → 0 at tip (bottom), 1 at root (top)

  bell (combined with outer as one sweep):
          offTime = outerLength + (bellLength - 1 - posInSegment)
                    ——————————————————————————————————————————————
                         outerLength + bellLength - 1

Then:  if sweepProgress <= offTime  →  LED is ON
```

IMPORTANT: use `cfg.jelly0.*` sizes for jelly 0 and `cfg.hardware.*` for all others.

Used in: `directionTest.ts`

---

## 7. Reference — Segment sizes for C++ lookup

When you have a raw LED index (0–49 within a strip) you can find its segment
by checking these ranges:

**Standard jellyfish (jellies 1–12):**
```
pos  0 –  29  →  inner   (posInSegment = pos)
pos 30 –  39  →  bell    (posInSegment = pos - 30)
pos 40 –  49  →  outer   (posInSegment = pos - 40)
```

**Hero jellyfish (jelly 0):**
```
Strips 0–7 (inner-only strips):
  pos  0 –  49  →  inner   (posInSegment = pos)

Strips 8–15 (bell+outer strips):
  pos  0 –  24  →  bell    (posInSegment = pos)
  pos 25 –  49  →  outer   (posInSegment = pos - 25)
```

To find which strip a global LED index belongs to:
```
jellyId    = floor(id / ledsPerJelly)
withinJelly = id % ledsPerJelly
stripIndex  = floor(withinJelly / 50)
posInStrip  = withinJelly % 50
```

For jelly 0: `ledsPerJelly = 800`
For all others: `ledsPerJelly = 400`
