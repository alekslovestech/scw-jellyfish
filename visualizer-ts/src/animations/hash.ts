// Deterministic hash of (a, b, salt) → float in [0,1). Integer math only, so it
// is stable frame-to-frame, reproducible, and portable to the Arduino firmware.
export function hash01(a: number, b: number, salt: number): number {
  let h = (Math.imul(a, 73856093) ^ Math.imul(b, 19349663) ^ Math.imul(salt, 83492791)) >>> 0;
  h = Math.imul(h ^ (h >>> 15), 0x2c1b3c6d);
  h ^= h >>> 12;
  h = Math.imul(h ^ (h >>> 4), 0x297a2d39);
  h ^= h >>> 15;
  return (h >>> 0) / 4294967296;
}
