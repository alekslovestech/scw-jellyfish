#pragma once

#include <WiFi.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include "AppConfig.h"
#include "InteractionEngine.h"
#include "PatternEngine.h"
#include "ShowClock.h"
#include "Types.h"

namespace jelly {

class NetworkManager {
 public:
  void begin(DeviceSettings& settings);
  void receive(
      uint64_t localNowMs,
      DeviceSettings& settings,
      ShowClock& clock,
      InteractionEngine& interaction,
      PatternEngine& renderer);
  void publish(
      uint64_t localNowMs,
      uint64_t showNowMs,
      const DeviceSettings& settings,
      const InteractionEngine& interaction);

  void broadcastActivation(const ActivationEvent& event);
  void refreshMdns(const DeviceSettings& settings);

  bool connected() const { return WiFi.status() == WL_CONNECTED; }
  String ipAddress() const { return connected() ? WiFi.localIP().toString() : String(); }
  String ssid() const { return connected() ? WiFi.SSID() : String(); }
  int rssi() const { return connected() ? WiFi.RSSI() : 0; }
  const AudioState& audio() const { return audio_; }
  String macAddress() const { return WiFi.macAddress(); }
  String chipId() const;

 private:
  void tickWifi(uint64_t localNowMs, const DeviceSettings& settings);
  void startNextWifiAttempt(uint64_t localNowMs, const DeviceSettings& settings);
  void startNetworkServices(const DeviceSettings& settings);
  void stopNetworkServices();
  bool configureStation(const DeviceSettings& settings);
  IPAddress configuredAddress(const DeviceSettings& settings) const;
  IPAddress broadcastAddress() const;
  void parsePacket(
      const char* packet,
      size_t length,
      uint64_t localNowMs,
      DeviceSettings& settings,
      ShowClock& clock,
      InteractionEngine& interaction,
      PatternEngine& renderer);
  void sendPlatform(uint64_t showNowMs, const PlatformState& platform);
  void sendClock(uint64_t showNowMs, const DeviceSettings& settings);
  void sendInstallation(
      uint64_t showNowMs,
      const DeviceSettings& settings,
      const InstallationState& installation);

  template <typename TDocument>
  void sendDocument(const TDocument& document) {
    if (!servicesStarted_) return;
    char buffer[1100];
    const size_t length = serializeJson(document, buffer, sizeof(buffer));
    if (length == 0 || length >= sizeof(buffer)) return;
    udp_.beginPacket(broadcastAddress(), config::kShowUdpPort);
    udp_.write(reinterpret_cast<const uint8_t*>(buffer), length);
    udp_.endPacket();
  }

  WiFiUDP udp_;
  AudioState audio_;
  bool servicesStarted_ = false;
  bool wifiAttemptActive_ = false;
  bool wifiPowerSaveDisabled_ = false;
  uint8_t wifiCredentialIndex_ = 0;
  uint64_t wifiAttemptStartedLocalMs_ = 0;
  uint64_t lastWifiActionLocalMs_ = 0;
  uint64_t lastPlatformBroadcastLocalMs_ = 0;
  uint64_t lastClockBroadcastLocalMs_ = 0;
  uint64_t lastInstallationBroadcastLocalMs_ = 0;
  uint32_t lastBroadcastEventId_ = 0;
};

}  // namespace jelly
