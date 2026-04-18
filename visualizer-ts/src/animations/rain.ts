import { LED } from "../core/ledSystem";

export const rain = {
  name: "rain",

  update(leds: LED[], time: number) {
    const speed = 1.5;

    for (const led of leds) {
      // use Y position as natural phase
      const phase = led.position.y * 0.5;

      const offset = (phase + time * speed) % 1;

      led.intensity = Math.max(0, 1 - offset);
      console.log(leds[0]?.intensity);

    }
  },
  
};
