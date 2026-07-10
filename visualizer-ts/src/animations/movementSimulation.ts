import { LED } from "../core/ledSystem";
import { getLEDDescriptor } from "../ledMap";
import { cfg } from "../config";
import { isLit } from "./movementReach";

// Continuous position 0→1 along the full inner→bell→outer path, so colour can
// diffuse smoothly instead of switching at the segment boundary.
//   inner tip (0.0) → inner root (0.6) → bell rim (0.8) → outer tip (1.0)
function pathPos(segment: string, t: number): number {
  if (segment === "inner") return t * 0.6;        // t=0 tip → t=1 root
  if (segment === "bell")  return 0.6 + t * 0.2;  // t=0 inner edge → t=1 outer rim
  return 0.8 + t * 0.2;                            // t=0 root → t=1 outer tip
}

// Smoothstep 0→1 between edges. Used to place the inner→outer colour crossover
// on the bell, so the inner tentacles read as the inner colour and the outer
// tentacles as the outer colour, with the transition happening across the bell.
function smoothstep(e0: number, e1: number, x: number): number {
  const t = Math.min(1, Math.max(0, (x - e0) / (e1 - e0)));
  return t * t * (3 - 2 * t);
}

export const movementSimulation = {
  name: "movementSimulation",

  update(leds: LED[], time: number) {
    // Two-colour palette (inner tips → outer tips), editable live from the panel.
    // Stored as normalised 0–1 RGB, used directly below.
    const inner = cfg.palette.inner;
    const outer = cfg.palette.outer;

    for (const led of leds) {
      const desc = getLEDDescriptor(led.id);

      // Inner tentacles show the inner colour, outer tentacles the outer colour,
      // and the bell (pos 0.6–0.8) is the smooth crossover between them.
      const pos = pathPos(desc.segment, desc.t);
      const mix = smoothstep(0.6, 0.8, pos);
      led.color.setRGB(
        inner.r + (outer.r - inner.r) * mix,
        inner.g + (outer.g - inner.g) * mix,
        inner.b + (outer.b - inner.b) * mix,
      );

      // Tentacle motion + intense-reaction reach come from the shared engine.
      led.intensity = isLit(desc, time) ? 1.0 : 0.0;
    }
  },
};
