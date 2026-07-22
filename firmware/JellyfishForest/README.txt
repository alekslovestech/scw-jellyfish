Jellyfish Forest 2.0.7 - coherent deep waves drop-in
===================================================

Use this package on the CURRENT working sketch. It intentionally contains only:

  PatternEngine.cpp
  PatternEngine.h
  version.h

Copy those files into the existing JellyfishForest sketch folder, replacing the
older copies. Do not replace AppConfig.h, Secrets.h, WebApi.cpp/.h,
InteractionEngine.cpp/.h, or JellyfishForest.ino. This preserves local Wi-Fi
credentials, restored LED pins, the /state endpoint used by the sound system,
and the platform-ID 7/8/9 mapping.

What was still producing a light floor
--------------------------------------

Version 2.0.6 already removed the idle ambient wash. Two other floors remained:

* occupied pixels received a constant `occupiedFloor` before wave modulation;
* chorus intensity started at 0.62 even at the bottom of every waveform.

Both are gone. Global occupied and chorus light is now carried only by
thresholded crests, so any pixel can return to exact black between waves.
Sparse ambient blooms and rare ambient shimmers remain independent events.

Calmness as an obvious visual reward
------------------------------------

The occupied response combines two coordinated visual grammars:

* an InnerSpreadWave-like travelling crest along inner -> bell -> outer path;
* a FireSpread-like root-to-branch crest across bell and hanging branches.

As calmness builds:

* peak brightness rises strongly;
* crests become broader and cleaner while retaining black valleys;
* independent strip/device phases converge on a shared position-based phase;
* the second root-to-branch wave joins the path wave;
* aligned crests gain blue/violet/pearl colour mixing and highlights.

Nearby jellyfish use the same show clock and a phase derived from physical
position. They therefore become coordinated progressively rather than snapping
all controllers to one identical frame.

Agitation as an immediate disappointment
----------------------------------------

Agitation now applies directly, before the slower calmness value has finished
falling. It:

* cuts the available crest amplitude by as much as 72%;
* restores independent strip phases and pixel-scale fragmentation;
* accelerates the movement;
* shifts the remaining fragments toward red/orange.

The disturbance remains visible and animated, but it is deliberately dimmer
and less coherent rather than visually rewarding.

Collective chorus
-----------------

The chorus is rebuilt from the same two wave families using only shared show
time and physical jellyfish position. It is brighter than the high-calm field,
but it no longer lights every pixel continuously. Synchronized crests sweep
through the forest with sustained exact-black intervals between them.

Existing dashboard controls
---------------------------

No settings-schema or API changes are required.

  contrast   In global scenes this is now WAVE DEPTH. Higher values raise the
             crest cutoff and create longer black gaps. It remains normal
             contrast for legacy fallback patterns.
  speed      Wave travel speed. Agitation adds its own temporary acceleration.
  scale      Spatial frequency/spacing of the path and root waves.
  hue        Primary blue/cool colour.
  hue2       Secondary violet/crest colour.
  brightness Final output level, unchanged.
  sparkle    Slow ambient pixel blooms, unchanged.
  density    Rare idle jellyfish shimmer frequency, unchanged.

Suggested first test:

  contrast 0.65-0.78
  speed    0.42-0.58
  scale    0.85-1.10
  hue      0.56-0.62
  hue2     0.72-0.80

No-hardware preview
-------------------

Select `movementSimulation` from the fallback dropdown on one jellyfish. It now
runs a 52-second commissioning loop:

  0-8 s    newly occupied / low calm
  8-16 s   jostling
  16-40 s  calmness builds
  40-44 s  high calm
  44-46 s  sudden jostle while calmness is still high
  46-52 s  calmness falls and the cycle resets

This exercises the same global renderer without platform packets. Return the
controller to Follow global conductor after testing.

Unaffected fallback patterns
----------------------------

Ripple, FireSpread, and InnerSpreadWave retain the restored 2.0.5 behaviour.
Only MovementSimulation was intentionally expanded as a commissioning tool.

Host validation performed
-------------------------

* 15 Python tests passed.
* Dashboard JavaScript syntax validation passed.
* Every firmware source passed the GNU++11 host compile sweep.
* Idle ambient lit fraction remained 0.0435 at persisted defaults.
* With ambient sparkle and shimmer disabled, idle output remained exact black.
* Mean linear interaction levels over 60 seconds were:
    low-calm occupied  0.0151
    fully calm         0.2472
    calm + new jostle  0.0409
    fully agitated     0.0027
    collective chorus 0.3242
* Fully calm pixels were exactly black for 34.6% of samples.
* Chorus pixels were exactly black for 37.8% of samples.
* A representative calm outer pixel had a 3.65-second continuous black gap.
* Calm strip phase deviation was 0.0000; jostling raised it to 0.0583.

These are deterministic host-renderer measurements before master brightness,
gamma correction, physical LED variation, and room optics. A real ESP32
cross-build was not run because PlatformIO is unavailable in the build
environment; the source passes the GNU++11 mode used by Arduino-ESP32 2.x.
