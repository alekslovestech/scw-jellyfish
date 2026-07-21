Jellyfish Forest 2.0.5 pattern tuning

Drop-in update:
1. Replace PatternEngine.cpp and PatternEngine.h in your current JellyfishForest sketch.
2. Leave AppConfig.h, Secrets.h, WebApi.*, and all other local changes untouched.
3. Recompile and upload.

Changes:
- InnerSpreadWave bell/outer pixels now remain fully visible in the primary
  colour between crests and crossfade into the secondary colour at the crest.
- FireSpread now transitions smoothly from blue back to red/yellow and repeats
  its colour cycle instead of remaining blue forever.
- Ripple is unchanged from 2.0.4.
