#include <Arduino.h>
#include <HX711.h>

#include "Geometry.h"
#include "InteractionEngine.h"
#include "LogBuffer.h"
#include "NetworkManager.h"
#include "PatternEngine.h"
#include "SensorModel.h"
#include "Settings.h"
#include "ShowClock.h"
#include "WebApi.h"
#include "version.h"

using namespace jelly;


SettingsStore gSettingsStore;
Geometry gGeometry(gSettingsStore.data());
HX711 gHx711;
SensorModel gSensor(gHx711);
InteractionEngine gInteraction;
ShowClock gShowClock;
PatternEngine gRenderer(gSettingsStore.data(), gGeometry, gInteraction);
NetworkManager gNetwork;
WebApi gWebApi(
    gSettingsStore,
    gGeometry,
    gSensor,
    gInteraction,
    gRenderer,
    gNetwork,
    gShowClock);

void setup() {
  Log.begin(115200);
  Log.println("\nJellyfish Forest booting...");
  Log.printf("Firmware %s, protocol %d\n", JELLY_FW_VERSION, JELLY_PROTOCOL_VERSION);

  gSettingsStore.load();
  gGeometry.recalculate();
  gRenderer.begin();
  gSensor.begin(gSettingsStore.data());
  gNetwork.begin(gSettingsStore.data());
  gWebApi.begin();
}

void loop() {
  const uint64_t localNowMs = monotonicMillis();
  DeviceSettings& settings = gSettingsStore.data();

  gNetwork.receive(
      localNowMs,
      settings,
      gShowClock,
      gInteraction,
      gRenderer);

  gSensor.tick(localNowMs, settings);
  PlatformState localPlatform = gSensor.state();
  localPlatform.senderShowTimeMs = gShowClock.now(localNowMs);
  gInteraction.setLocalPlatform(localPlatform, localNowMs);

  if (gSensor.consumeActivation()) {
    ActivationEvent event;
    event.eventId = esp_random() | 1U;
    event.platformId = settings.deviceId;
    event.origin = settings.position;
    event.weightKg = localPlatform.weightKg;
    event.agitation = localPlatform.agitation;
    event.startShowTimeMs = gShowClock.now(localNowMs);
    gRenderer.startActivation(event, event.startShowTimeMs);
    gNetwork.broadcastActivation(event);
    Log.printf("Platform %u activated at %.2f kg\n", event.platformId, event.weightKg);
  }

  const uint64_t showNowMs = gShowClock.now(localNowMs);
  gInteraction.tick(localNowMs, showNowMs, settings.interaction);
  gNetwork.publish(localNowMs, showNowMs, settings, gInteraction);
  gRenderer.tick(
      localNowMs,
      showNowMs,
      gInteraction.installation(localNowMs),
      gNetwork.audio());
  gWebApi.tick(localNowMs);

  delay(1);
}
