// Shared, transient state that effects produce and base animations consume.
//
// The effect stack runs AFTER the base each frame (see AnimationManager), so a
// base reads the value an effect wrote on the PREVIOUS frame — a ~16 ms lag that
// is imperceptible for this slow installation. When an effect is toggled off it
// no longer writes, so the panel resets the value to its resting state.
export const fxState = {
  // 0 = calm, 1 = full "intense reaction" flare. Driven by the intenseReaction
  // effect; read by movementSimulation to widen each tentacle's sweep.
  excitement: 0,
};
