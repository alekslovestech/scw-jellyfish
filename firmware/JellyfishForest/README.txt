Jellyfish Forest 2.0.6 - sparse ambient drop-in
================================================

Use this package on the CURRENT working sketch. It intentionally contains only:

  PatternEngine.cpp
  PatternEngine.h
  version.h

Copy those files into the existing JellyfishForest sketch folder, replacing the
older copies. Do not replace AppConfig.h, Secrets.h, WebApi.cpp/.h,
InteractionEngine.cpp/.h, or JellyfishForest.ino. This preserves the local Wi-Fi
credentials, restored LED pins, /state endpoint, and platform-ID 7/8/9 mapping.

Ambient behaviour
-----------------

* Idle LEDs have no permanent base glow. Exact darkness is the default.
* Individual pixels receive independent 7-16 second blue/violet blooms.
* At the persisted defaults, roughly 4% of pixels are active on average,
  including rare shimmer frames; the rest remain off.
* Each jellyfish has an independent candidate schedule for a bright 3.2-5.2
  second spherical shimmer with a deterministic random local centre.
* Many candidate cycles are skipped. At density 0.45, one jellyfish averages
  roughly one accepted shimmer every two to three minutes.
* Device ID and shared show time de-correlate the shimmer timing between
  jellyfish while keeping it stable across reboots.

Controls in the existing dashboard
----------------------------------

  sparkle  Slow pixel-bloom density. 0 disables slow blooms.
  density  Rare shimmer frequency. 0 disables ambient shimmers.
  speed    Higher values shorten both bloom and shimmer duration.
  scale    Shimmer shell width.
  hue      First ambient palette colour.
  hue2     Second ambient palette colour.
  brightness / master brightness
            Final output level, unchanged.

Default hue 0.52 and hue2 0.72 span cyan-blue through violet.

Interaction safety
------------------

The platform-driven renderer remains separate. A presence-dependent brightness
floor replaces the old always-on ambient wash near occupied platforms, so calm
occupancy remains bright and agitation still adds light and turbulence.

Validation performed
--------------------

* 15 Python tests passed.
* JavaScript syntax validation passed.
* Every firmware source passed the GNU++11 host compile sweep.
* Exact C++ ambient-profile test measured a 0.0435 default lit fraction.
* With sparkle=0 and density=0, the idle output remained exactly black.
* A six-minute single-jelly test contained rare shimmer frames and was fully
  dark on 3530 of 3600 sampled frames.
* Synthetic interaction totals were occupied 170.3, calm 203.7, agitated 279.7.

A real ESP32 cross-build was not run because PlatformIO is unavailable in the
build environment. The source does pass the same GNU++11 language mode used by
Arduino-ESP32 2.x.
