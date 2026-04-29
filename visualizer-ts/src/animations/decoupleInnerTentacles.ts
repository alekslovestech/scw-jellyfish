import { LED } from "../core/ledSystem"; 

export const decoupleInnerTentacles = {
  name: "decoupleInnerTentacles",

  update(leds: LED[], time: number) {
    for (const led of leds) {
      const id = led.jellyId ?? 0;

      // 1. BELL & OUTER TENTACLES: Blinking Blue
      if (led.group === "bell" || led.group === "tentacle") {
        // Create a blinking speed (Sine wave goes from -1 to 1)
        // We add the id to the time so they don't all blink at the exact same moment
        const blink = Math.sin(time * 3 + id) * 0.5 + 0.5; 
        
        // Set color to Blue (R=0, G=0, B=blink)
        led.color.setRGB(0, 0, blink);
      } 
      
      // 2. INNER TENTACLES: Red Worm Effect
      else if (led.group === "central") {
        // 'led.t' is the position from 0 (top) to 1 (bottom)
        // We use it to create a "wave" that travels down the strand
        const wormPosition = (led.t ?? 0);
        const speed = time * 5;
        
        // This math creates a sharp "pulse" that moves
        const pulse = Math.sin(wormPosition * 10 - speed);
        const intensity = Math.max(0, pulse); // Only show the positive part of the wave

        // Set color to Red (R=intensity, G=0, B=0)
        led.color.setRGB(intensity, 0, 0);
      }
    }
  },
};