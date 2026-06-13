import { cfg } from "./config";

export type StarLEDDescriptor = {
  edge: number;         // 0–9, which of the 10 edges
  posInEdge: number;    // 0–4, position within that edge
  t: number;            // 0→1 normalized along the edge
  tPerim: number;       // 0→1 normalized along the full perimeter
  starPoint: number;    // 0–4, which of the 5 outer tips this edge belongs to
  isAtOuterTip: boolean;    // LED is at an outer tip vertex
  isAtInnerCorner: boolean; // LED is at an inner corner vertex
};

export function getLEDDescriptor(ledId: number): StarLEDDescriptor {
  const edge = Math.floor(ledId / cfg.LEDS_PER_EDGE);
  const posInEdge = ledId % cfg.LEDS_PER_EDGE;
  const t = posInEdge / cfg.LEDS_PER_EDGE;
  const tPerim = ledId / (cfg.LEDS_PER_EDGE * cfg.NUM_EDGES);

  // Even edges go outer-tip → inner-corner, odd edges go inner-corner → outer-tip.
  // Both edges adjacent to a tip share the same starPoint.
  const starPoint = Math.floor((edge + 1) / 2) % cfg.NUM_POINTS;

  const isAtOuterTip = edge % 2 === 0 && posInEdge === 0;
  const isAtInnerCorner = edge % 2 === 1 && posInEdge === 0;

  return { edge, posInEdge, t, tPerim, starPoint, isAtOuterTip, isAtInnerCorner };
}
