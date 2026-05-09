# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

```bash
yarn dev          # Start dev server (Vite, hot reload)
yarn build        # Type-check (tsc) then bundle (vite build)
yarn start        # Preview the production build
```

There are no tests. Type checking is the verification step: `yarn build` runs `tsc` first.

## Architecture

This is a Three.js + TypeScript browser app that visualizes a physical jellyfish LED art installation. The visualizer is a faithful 3-D preview of what the real hardware will show — animations written here translate directly to Arduino/C++.

### Data flow

```
config.ts  ──►  Jellyfish (structure/)  ──►  LEDSystem  ──►  LEDRenderer  ──►  Three.js Points
                                                  ▲
                                         AnimationManager
                                                  ▲
                                         animations/*.ts
```

1. `main.ts` creates the scene, materials, and all systems.
2. `Jellyfish` registers its LEDs into `LEDSystem` (assigning global monotonic IDs) and returns a `THREE.Group` for the mesh.
3. `LEDRenderer.update()` is called every frame: it asks `AnimationManager` to run the current animation (which mutates `led.color` and `led.intensity`), then writes positions and modulated colours into GPU buffer attributes.
4. `panel.ts` (Tweakpane) lets the user switch animations and tweak `cfg` values; changes call `redraw()` which disposes old jellies, resets `LEDSystem`, and rebuilds.

### Hardware model

There are 13 jellyfish and 5 600 total LEDs:

| Jelly | Type | Strips | LEDs/strip | IDs |
|---|---|---|---|---|
| 0 | Hero | 16 | 50 | 0 – 799 |
| 1–12 | Standard | 8 | 50 | 800 – 5599 |

Hero jelly strips 0–7 are inner-only; strips 8–15 carry bell + outer.  
Standard jelly strips each carry inner (pos 0–29) → bell (30–39) → outer (40–49).

`config.ts` exports `cfg.hardware` (standard layout) and `cfg.jelly0` (hero layout). Always use these constants — never hard-code segment sizes.

### LED lookup

`ledMap.ts` exports `getLEDDescriptor(ledId)` which returns a `LEDDescriptor`:

```ts
{ jellyId, stripIndex, angle_deg, posInStrip, segment, posInSegment, t }
```

`t` is the key field for animations: it is always 0→1 within the segment in wiring order. For inner tentacles `t=0` is the tip (bottom); for outer tentacles `t=0` is the root (top), so use `1-t` to get bottom-to-top motion.

### Writing an animation

Every animation is an object matching `LEDAnimation` from `src/animations/types.ts`:

```ts
export type LEDAnimation = {
  name: string;
  update: (leds: LED[], time: number) => void;  // time = Date.now()/1000
};
```

Pattern:
1. Compute a phase from `time`.
2. Loop over `leds`; call `getLEDDescriptor(led.id)` for spatial context.
3. Set `led.color.setRGB(r, g, b)` (0–1) and `led.intensity` (0–1).

After creating an animation file, add it to the `animations` map in `panel.ts` to make it selectable in the UI.

See `ANIMATION_GUIDE.md` for detailed recipes (waves, cascades, per-segment colouring, direction tests).
