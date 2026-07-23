#include "NetworkManager.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <esp_wifi.h>
#include "AppConfig.h"
#include "LogBuffer.h"
#include "MathUtils.h"
#include "version.h"

namespace jelly {

namespace {

// Kept separate from the installation show bus (42120) so Soundguy can run
// the minimal sender without touching conductor/platform traffic.
constexpr uint16_t kMusicVolumeUdpPort = 4210;

}  // namespace

String NetworkManager::chipId() const {
  const uint64_t chip = ESP.getEfuseMac();
  char buffer[13];
  snprintf(buffer, sizeof(buffer), "%04X%08X",
           static_cast<uint16_t>(chip >> 32),
           static_cast<uint32_t>(chip));
  return String(buffer);
}

void NetworkManager::begin(DeviceSettings& settings) {
  WiFi.persistent(false);
  startNextWifiAttempt(monotonicMillis(), settings);
}

IPAddress NetworkManager::configuredAddress(const DeviceSettings& settings) const {
  const uint8_t finalOctet = settings.deviceId > 0
      ? static_cast<uint8_t>(config::kLegacyIpBase + settings.deviceId)
      : config::kUnassignedIpOctet;
  return IPAddress(
      config::kLegacyIpPrefixA,
      config::kLegacyIpPrefixB,
      config::kLegacyIpPrefixC,
      finalOctet);
}

bool NetworkManager::configureStation(const DeviceSettings& settings) {
  // Reproduce the known-good sequence from the original installation firmware.
  // disconnect(true, true) turns the radio off and clears stale SDK state; mode()
  // then brings it back before static addressing is applied.
  WiFi.disconnect(true, true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname(settings.name.c_str());
  wifiPowerSaveDisabled_ = false;

  if (!config::kUseLegacyStaticIp) return true;

  const IPAddress ip = configuredAddress(settings);
  const IPAddress gateway(
      config::kLegacyIpPrefixA,
      config::kLegacyIpPrefixB,
      config::kLegacyIpPrefixC,
      config::kLegacyGatewayOctet);
  const IPAddress subnet(255, 255, 255, 0);
  const IPAddress noDns(0, 0, 0, 0);

  if (!WiFi.config(ip, gateway, subnet, noDns, noDns)) {
    Log.printf("Static IP configuration failed for %s\n", ip.toString().c_str());
    return false;
  }

  Log.printf(
      "Device ID %u, using static IP %s (gateway %s)\n",
      settings.deviceId,
      ip.toString().c_str(),
      gateway.toString().c_str());
  return true;
}

void NetworkManager::startNextWifiAttempt(
    uint64_t localNowMs,
    const DeviceSettings& settings) {
  constexpr size_t credentialCount =
      sizeof(config::kWifiCredentials) / sizeof(config::kWifiCredentials[0]);

  for (size_t scanned = 0; scanned < credentialCount; ++scanned) {
    const auto& credential = config::kWifiCredentials[wifiCredentialIndex_];
    wifiCredentialIndex_ = (wifiCredentialIndex_ + 1) % credentialCount;
    if (credential.ssid == nullptr || credential.ssid[0] == '\0') continue;

    if (!configureStation(settings)) {
      wifiAttemptActive_ = false;
      lastWifiActionLocalMs_ = localNowMs;
      return;
    }

    Log.printf("Connecting to Wi-Fi %s\n", credential.ssid);
    WiFi.begin(credential.ssid, credential.password);
    wifiAttemptActive_ = true;
    wifiAttemptStartedLocalMs_ = localNowMs;
    lastWifiActionLocalMs_ = localNowMs;
    return;
  }

  wifiAttemptActive_ = false;
  lastWifiActionLocalMs_ = localNowMs;
  Log.println("No Wi-Fi credentials configured; running autonomously.");
}

void NetworkManager::tickWifi(uint64_t localNowMs, const DeviceSettings& settings) {
  if (connected()) {
    wifiAttemptActive_ = false;
    if (!wifiPowerSaveDisabled_) {
      esp_wifi_set_ps(WIFI_PS_NONE);
      wifiPowerSaveDisabled_ = true;
    }
    if (!servicesStarted_) startNetworkServices(settings);
    return;
  }

  if (servicesStarted_) stopNetworkServices();

  if (wifiAttemptActive_) {
    if (localNowMs - wifiAttemptStartedLocalMs_ > 7000ULL) {
      Log.printf("Wi-Fi attempt timed out (status %d).\n", static_cast<int>(WiFi.status()));
      WiFi.disconnect();
      wifiAttemptActive_ = false;
      startNextWifiAttempt(localNowMs, settings);
    }
    return;
  }

  if (localNowMs - lastWifiActionLocalMs_ >= config::kWifiRetryMs) {
    startNextWifiAttempt(localNowMs, settings);
  }
}

void NetworkManager::startNetworkServices(const DeviceSettings& settings) {
  udp_.stop();
  volumeUdp_.stop();

  servicesStarted_ = udp_.begin(config::kShowUdpPort) == 1;
  if (!servicesStarted_) {
    Log.println("Could not bind show UDP port.");
    return;
  }

  if (volumeUdp_.begin(kMusicVolumeUdpPort) != 1) {
    Log.printf("Could not bind music-volume UDP port %u.\n",
               kMusicVolumeUdpPort);
    udp_.stop();
    servicesStarted_ = false;
    return;
  }

  refreshMdns(settings);
  Log.printf("Network ready: %s (%s)\n", WiFi.localIP().toString().c_str(), WiFi.SSID().c_str());
}

void NetworkManager::stopNetworkServices() {
  udp_.stop();
  volumeUdp_.stop();
  MDNS.end();
  servicesStarted_ = false;
}

void NetworkManager::refreshMdns(const DeviceSettings& settings) {
  if (!connected()) return;
  MDNS.end();
  if (!MDNS.begin(settings.name.c_str())) {
    Log.println("mDNS start failed.");
    return;
  }
  MDNS.addService("esp32art", "tcp", config::kHttpPort);
  MDNS.addServiceTxt("esp32art", "tcp", "name", settings.name);
  MDNS.addServiceTxt("esp32art", "tcp", "chip", chipId());
  MDNS.addServiceTxt("esp32art", "tcp", "mac", WiFi.macAddress());
  MDNS.addServiceTxt("esp32art", "tcp", "fw", JELLY_FW_VERSION);
  MDNS.addServiceTxt("esp32art", "tcp", "ssid", WiFi.SSID());
  MDNS.addServiceTxt("esp32art", "tcp", "id", String(settings.deviceId));
}

IPAddress NetworkManager::broadcastAddress() const {
  const IPAddress ip = WiFi.localIP();
  const IPAddress mask = WiFi.subnetMask();
  IPAddress result;
  for (uint8_t i = 0; i < 4; ++i) {
    result[i] = ip[i] | static_cast<uint8_t>(~mask[i]);
  }
  return result;
}

void NetworkManager::receive(
    uint64_t localNowMs,
    DeviceSettings& settings,
    ShowClock& clock,
    InteractionEngine& interaction,
    PatternEngine& renderer) {
  tickWifi(localNowMs, settings);
  if (!servicesStarted_) return;

  for (uint8_t packetIndex = 0; packetIndex < 10; ++packetIndex) {
    const int packetSize = udp_.parsePacket();
    if (packetSize <= 0) break;
    char buffer[1100];
    const int length = udp_.read(buffer, min(packetSize, static_cast<int>(sizeof(buffer) - 1)));
    if (length <= 0) continue;
    buffer[length] = '\0';
    parsePacket(buffer, length, localNowMs, settings, clock, interaction, renderer);
  }

  receiveVolumePackets(localNowMs);

  if (audio_.lastPacketLocalMs != 0 && localNowMs - audio_.lastPacketLocalMs > 1000ULL) {
    audio_.level *= 0.96f;
    audio_.bass *= 0.96f;
    audio_.mid *= 0.96f;
    audio_.high *= 0.96f;
    audio_.beat = false;
  }
}


void NetworkManager::receiveVolumePackets(uint64_t localNowMs) {
  // Drain several queued datagrams so a temporary render or Wi-Fi delay does
  // not leave the lights reacting to stale volume values.
  for (uint8_t packetIndex = 0; packetIndex < 12; ++packetIndex) {
    const int packetSize = volumeUdp_.parsePacket();
    if (packetSize <= 0) break;

    char buffer[64];
    const int length = volumeUdp_.read(
        buffer,
        min(packetSize, static_cast<int>(sizeof(buffer) - 1)));
    if (length <= 0) continue;

    buffer[length] = '\0';
    parseVolumePacket(buffer, localNowMs);
  }
}

void NetworkManager::parseVolumePacket(
    const char* packet,
    uint64_t localNowMs) {
  float receivedVolume = 0.0f;
  char trailing = '\0';

  // The extra %c rejects malformed packets such as "volume,0.5,junk" while
  // still accepting the sender's normal newline-free ASCII datagram.
  const int matched = sscanf(
      packet,
      "volume,%f%c",
      &receivedVolume,
      &trailing);

  if (matched != 1) return;

  audio_.level = clamp01(receivedVolume);
  audio_.lastPacketLocalMs = localNowMs;
}

void NetworkManager::parsePacket(
    const char* packet,
    size_t length,
    uint64_t localNowMs,
    DeviceSettings& settings,
    ShowClock& clock,
    InteractionEngine& interaction,
    PatternEngine& renderer) {
  StaticJsonDocument<1200> document;
  const DeserializationError error = deserializeJson(document, packet, length);
  if (error) return;
  if (document.containsKey("protocol") &&
      (document["protocol"] | 0) != JELLY_PROTOCOL_VERSION) {
    return;
  }
  const char* type = document["type"] | "";

  if (strcmp(type, "platform") == 0) {
    PlatformState state;
    state.valid = true;
    state.id = document["id"] | 0;
    state.sequence = document["sequence"] | 0;
    state.position = {
        document["x"] | 0.0f,
        document["y"] | 0.0f,
        document["z"] | 0.0f,
    };
    state.weightKg = document["weightKg"] | 0.0f;
    state.agitation = clamp01(document["agitation"] | 0.0f);
    state.calmness = clamp01(document["calmness"] | 0.0f);
    state.occupied = document["occupied"] | false;
    state.senderShowTimeMs = document["showTimeMs"].as<uint64_t>();
    interaction.ingestPlatform(state, localNowMs);
    return;
  }

  if (strcmp(type, "activation") == 0) {
    ActivationEvent event;
    event.eventId = document["eventId"] | 0;
    event.platformId = document["platformId"] | 0;
    event.origin = {
        document["x"] | 0.0f,
        document["y"] | 0.0f,
        document["z"] | 0.0f,
    };
    event.weightKg = document["weightKg"] | 0.0f;
    event.agitation = document["agitation"] | 0.0f;
    event.startShowTimeMs = document["showTimeMs"].as<uint64_t>();
    renderer.startActivation(event, clock.now(localNowMs));
    return;
  }

  if (strcmp(type, "clock") == 0) {
    const uint64_t remoteTime = document["showTimeMs"].as<uint64_t>();
    const uint8_t priority = document["priority"] | 10;
    if (remoteTime != 0) clock.ingest(remoteTime, localNowMs, priority);
    return;
  }

  if (strcmp(type, "installation") == 0) {
    InstallationState state;
    state.seenPlatforms = document["seenPlatforms"] | 0;
    state.occupiedPlatforms = document["occupiedPlatforms"] | 0;
    state.meanCalmness = document["meanCalmness"] | 0.0f;
    state.minimumCalmness = document["minimumCalmness"] | 0.0f;
    state.maximumAgitation = document["maximumAgitation"] | 0.0f;
    state.chorus = document["chorus"] | 0.0f;
    state.allCalmLatched = document["allCalm"] | false;
    state.chorusStartedShowTimeMs = document["chorusStartedShowTimeMs"].as<uint64_t>();
    state.sequence = document["sequence"] | 0;
    interaction.applyCanonicalInstallation(
        state, localNowMs, document["priority"] | 10);
    return;
  }

  if (strcmp(type, "show") == 0) {
    ShowMode mode;
    if (document.containsKey("mode") &&
        parseShowMode(String(document["mode"].as<const char*>()), mode)) {
      settings.showMode = mode;
    }
    if (document.containsKey("masterBrightness")) {
      settings.masterBrightness = clamp01(
          document["masterBrightness"] | settings.masterBrightness);
    }
    if (document.containsKey("expectedPlatformCount")) {
      settings.interaction.expectedPlatformCount = constrain(
          document["expectedPlatformCount"] | settings.interaction.expectedPlatformCount,
          1,
          config::kMaxPlatforms);
    }
    if (document.containsKey("influenceRadiusMeters")) {
      settings.interaction.influenceRadiusMeters = max(
          0.25f,
          document["influenceRadiusMeters"] | settings.interaction.influenceRadiusMeters);
    }
    if (document.containsKey("calmThreshold")) {
      settings.interaction.calmThreshold = clamp01(
          document["calmThreshold"] | settings.interaction.calmThreshold);
    }
    if (document.containsKey("calmReleaseThreshold")) {
      settings.interaction.calmReleaseThreshold = min(
          settings.interaction.calmThreshold,
          clamp01(document["calmReleaseThreshold"] |
                  settings.interaction.calmReleaseThreshold));
    }
    if (document.containsKey("maxAgitationForChorus")) {
      settings.interaction.maxAgitationForChorus = clamp01(
          document["maxAgitationForChorus"] |
          settings.interaction.maxAgitationForChorus);
    }
    if (document.containsKey("allCalmHoldSeconds")) {
      settings.interaction.allCalmHoldSeconds = max(
          0.0f,
          document["allCalmHoldSeconds"] | settings.interaction.allCalmHoldSeconds);
    }
    if (document.containsKey("chorusFadeSeconds")) {
      settings.interaction.chorusFadeSeconds = max(
          0.1f,
          document["chorusFadeSeconds"] | settings.interaction.chorusFadeSeconds);
    }
    if (document.containsKey("activationWaveSpeedMetersPerSecond")) {
      settings.interaction.activationWaveSpeedMetersPerSecond = max(
          0.1f,
          document["activationWaveSpeedMetersPerSecond"] |
          settings.interaction.activationWaveSpeedMetersPerSecond);
    }
    if (document.containsKey("activationWaveWidthMeters")) {
      settings.interaction.activationWaveWidthMeters = max(
          0.05f,
          document["activationWaveWidthMeters"] |
          settings.interaction.activationWaveWidthMeters);
    }
    if (document.containsKey("activationWaveDurationSeconds")) {
      settings.interaction.activationWaveDurationSeconds = max(
          0.5f,
          document["activationWaveDurationSeconds"] |
          settings.interaction.activationWaveDurationSeconds);
    }
    return;
  }

  if (strcmp(type, "audio") == 0) {
    audio_.level = clamp01(document["level"] | 0.0f);
    audio_.bass = clamp01(document["bass"] | 0.0f);
    audio_.mid = clamp01(document["mid"] | 0.0f);
    audio_.high = clamp01(document["high"] | 0.0f);
    audio_.beat = document["beat"] | false;
    audio_.lastPacketLocalMs = localNowMs;
  }
}

void NetworkManager::publish(
    uint64_t localNowMs,
    uint64_t showNowMs,
    const DeviceSettings& settings,
    const InteractionEngine& interaction) {
  if (!servicesStarted_) return;

  const PlatformState& localPlatform = interaction.localPlatform();
  if (localPlatform.valid &&
      localNowMs - lastPlatformBroadcastLocalMs_ >= config::kPlatformBroadcastMs) {
    lastPlatformBroadcastLocalMs_ = localNowMs;
    sendPlatform(showNowMs, localPlatform);
  }

  if (settings.deviceId == config::kConductorDeviceId) {
    if (localNowMs - lastClockBroadcastLocalMs_ >= config::kClockBroadcastMs) {
      lastClockBroadcastLocalMs_ = localNowMs;
      sendClock(showNowMs, settings);
    }
    if (localNowMs - lastInstallationBroadcastLocalMs_ >= config::kInstallationBroadcastMs) {
      lastInstallationBroadcastLocalMs_ = localNowMs;
      sendInstallation(showNowMs, settings, interaction.localInstallation());
    }
  }
}

void NetworkManager::sendPlatform(uint64_t showNowMs, const PlatformState& platform) {
  StaticJsonDocument<384> document;
  document["protocol"] = JELLY_PROTOCOL_VERSION;
  document["type"] = "platform";
  document["id"] = platform.id;
  document["sequence"] = platform.sequence;
  document["x"] = platform.position.x;
  document["y"] = platform.position.y;
  document["z"] = platform.position.z;
  document["weightKg"] = platform.weightKg;
  document["agitation"] = platform.agitation;
  document["calmness"] = platform.calmness;
  document["occupied"] = platform.occupied;
  document["showTimeMs"] = showNowMs;
  sendDocument(document);
}

void NetworkManager::sendClock(uint64_t showNowMs, const DeviceSettings& settings) {
  StaticJsonDocument<256> document;
  document["protocol"] = JELLY_PROTOCOL_VERSION;
  document["type"] = "clock";
  document["source"] = "device";
  document["sourceId"] = settings.deviceId;
  document["priority"] = 10;
  document["showTimeMs"] = showNowMs;
  sendDocument(document);
}

void NetworkManager::sendInstallation(
    uint64_t showNowMs,
    const DeviceSettings& settings,
    const InstallationState& installation) {
  StaticJsonDocument<512> document;
  document["protocol"] = JELLY_PROTOCOL_VERSION;
  document["type"] = "installation";
  document["source"] = "device";
  document["sourceId"] = settings.deviceId;
  document["priority"] = 10;
  document["showTimeMs"] = showNowMs;
  document["seenPlatforms"] = installation.seenPlatforms;
  document["occupiedPlatforms"] = installation.occupiedPlatforms;
  document["meanCalmness"] = installation.meanCalmness;
  document["minimumCalmness"] = installation.minimumCalmness;
  document["maximumAgitation"] = installation.maximumAgitation;
  document["chorus"] = installation.chorus;
  document["allCalm"] = installation.allCalmLatched;
  document["chorusStartedShowTimeMs"] = installation.chorusStartedShowTimeMs;
  document["sequence"] = installation.sequence;
  sendDocument(document);
}

void NetworkManager::broadcastActivation(const ActivationEvent& event) {
  if (!servicesStarted_ || event.eventId == 0 || event.eventId == lastBroadcastEventId_) return;
  lastBroadcastEventId_ = event.eventId;
  StaticJsonDocument<384> document;
  document["protocol"] = JELLY_PROTOCOL_VERSION;
  document["type"] = "activation";
  document["eventId"] = event.eventId;
  document["platformId"] = event.platformId;
  document["x"] = event.origin.x;
  document["y"] = event.origin.y;
  document["z"] = event.origin.z;
  document["weightKg"] = event.weightKg;
  document["agitation"] = event.agitation;
  document["showTimeMs"] = event.startShowTimeMs;
  sendDocument(document);
}

}  // namespace jelly
