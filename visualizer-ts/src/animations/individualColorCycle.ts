import { LED } from "../core/ledSystem"; 

export const individualColorCycle = {
  name: "individualColorCycle",

  update(leds: LED[], time: number) {
    for (const led of leds) {
      const id = led.jellyId ?? 0; 
      
      // 1. SMALLER ID OFFSET: 
      // Using 0.1 instead of 0.6 makes the color difference between 
      // Jelly #1 and Jelly #2 much smoother (like a gradient).
      const spatialOffset = id * 0.15;

      // 2. SLOWER TIME:
      // Multiplying time by 0.1 makes the rainbow cycle slowly.
      const temporalOffset = time * 0.1;

      // 3. COMBINE:
      // The % 1.0 ensures the value stays between 0 and 1 (the Hue circle)
      const hue = (spatialOffset + temporalOffset) % 1.0;

      // 4. SET COLOR:
      // Saturation at 0.8 and Lightness at 0.5 gives deep, rich colors 
      // that aren't too "neon" or "strobe-like".
      led.color.setHSL(hue, 0.8, 0.5);
    }
  },
};