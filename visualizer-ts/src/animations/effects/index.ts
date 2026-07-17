import { Effect } from "../types";
import { colorCycle } from "./colorCycle";
import { fireColorCycle } from "./fireColorCycle";
import { intenseReaction } from "./intenseReaction";
import { innerWave } from "./innerWave";
import { innerGlow } from "./innerGlow";
import { tvStatic } from "./tvStatic";

// The effect stack, applied (in order) on top of the base animation every frame.
// Add new effects here to make them available to the manager and the panel.
// The colour-cycle effects run first so they drive the palettes the base reads
// this frame; innerWave/innerGlow modulate the inner tentacles; tvStatic speckles
// on top. colorCycle drives the 2-colour bases; fireColorCycle drives fireSteady.
export const effects: Effect[] = [
  colorCycle,
  fireColorCycle,
  intenseReaction,
  innerWave,
  innerGlow,
  tvStatic,
];
