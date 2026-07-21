#pragma once

#include <Arduino.h>

#if __has_include("Secrets.h")
#include "Secrets.h"
#endif

#ifndef JELLY_WIFI_SSID_1
#define JELLY_WIFI_SSID_1 "SCW"
#endif
#ifndef JELLY_WIFI_PASSWORD_1
#define JELLY_WIFI_PASSWORD_1 "Jellyfish"
#endif
#ifndef JELLY_WIFI_SSID_2
#define JELLY_WIFI_SSID_2 ""
#endif
#ifndef JELLY_WIFI_PASSWORD_2
#define JELLY_WIFI_PASSWORD_2 ""
#endif

// These defaults are compile-safe examples, not verified wiring for the installation.
// Override them with build flags or edit them before flashing real hardware.
#ifndef JELLY_LED_PIN_0
#define JELLY_LED_PIN_0 4
#endif
#ifndef JELLY_LED_PIN_1
#define JELLY_LED_PIN_1 5
#endif
#ifndef JELLY_LED_PIN_2
#define JELLY_LED_PIN_2 6
#endif
#ifndef JELLY_LED_PIN_3
#define JELLY_LED_PIN_3 7
#endif
#ifndef JELLY_LED_PIN_4
#define JELLY_LED_PIN_4 9
#endif
#ifndef JELLY_LED_PIN_5
#define JELLY_LED_PIN_5 10
#endif
#ifndef JELLY_LED_PIN_6
#define JELLY_LED_PIN_6 11
#endif
#ifndef JELLY_LED_PIN_7
#define JELLY_LED_PIN_7 12
#endif

#ifndef JELLY_USE_STATIC_IP
#define JELLY_USE_STATIC_IP 1
#endif
#ifndef JELLY_STATIC_IP_PREFIX_A
#define JELLY_STATIC_IP_PREFIX_A 192
#endif
#ifndef JELLY_STATIC_IP_PREFIX_B
#define JELLY_STATIC_IP_PREFIX_B 168
#endif
#ifndef JELLY_STATIC_IP_PREFIX_C
#define JELLY_STATIC_IP_PREFIX_C 0
#endif
#ifndef JELLY_STATIC_IP_BASE
#define JELLY_STATIC_IP_BASE 100
#endif
#ifndef JELLY_STATIC_IP_GATEWAY
#define JELLY_STATIC_IP_GATEWAY 1
#endif
#ifndef JELLY_UNASSIGNED_IP_OCTET
#define JELLY_UNASSIGNED_IP_OCTET 199
#endif

#ifndef JELLY_HX711_DOUT
#define JELLY_HX711_DOUT 14
#endif
#ifndef JELLY_HX711_SCK
#define JELLY_HX711_SCK 13
#endif

namespace jelly {
namespace config {

struct WifiCredential {
  const char* ssid;
  const char* password;
};

static const WifiCredential kWifiCredentials[] = {
    {JELLY_WIFI_SSID_1, JELLY_WIFI_PASSWORD_1},
    {JELLY_WIFI_SSID_2, JELLY_WIFI_PASSWORD_2},
};

static const uint8_t kLedPins[8] = {
    JELLY_LED_PIN_0,
    JELLY_LED_PIN_1,
    JELLY_LED_PIN_2,
    JELLY_LED_PIN_3,
    JELLY_LED_PIN_4,
    JELLY_LED_PIN_5,
    JELLY_LED_PIN_6,
    JELLY_LED_PIN_7,
};

constexpr uint8_t kStripCount = 8;
constexpr uint16_t kSmallLedsPerStrip = 50;
constexpr uint16_t kBigLedsPerStrip = 150;
constexpr uint16_t kMaxLedsPerStrip = kBigLedsPerStrip;
constexpr uint8_t kMaxPlatforms = 12;
constexpr uint8_t kMaxActivationWaves = 8;

constexpr uint8_t kHx711DoutPin = JELLY_HX711_DOUT;
constexpr uint8_t kHx711SckPin = JELLY_HX711_SCK;
constexpr uint32_t kHx711ReadyTimeoutMs = 2500;

constexpr uint16_t kShowUdpPort = 42120;
constexpr uint16_t kHttpPort = 80;
constexpr uint32_t kWifiRetryMs = 10000;
constexpr uint32_t kPlatformBroadcastMs = 200;
constexpr uint32_t kClockBroadcastMs = 250;
constexpr uint32_t kInstallationBroadcastMs = 250;
constexpr uint32_t kPeerTimeoutMs = 3000;
constexpr uint32_t kCanonicalTimeoutMs = 1500;
constexpr uint32_t kRenderIntervalMs = 20;
constexpr uint32_t kLogCapacity = 12000;
constexpr uint8_t kConductorDeviceId = 1;

constexpr const char* kMdnsService = "_esp32art";
constexpr const char* kMdnsProtocol = "_tcp";

// The installation access point does not provide DHCP. Match the original
// firmware: device N uses 192.168.0.(100 + N). An unassigned controller uses
// a temporary address so it can still be commissioned.
constexpr bool kUseLegacyStaticIp = JELLY_USE_STATIC_IP != 0;
constexpr uint8_t kLegacyIpPrefixA = JELLY_STATIC_IP_PREFIX_A;
constexpr uint8_t kLegacyIpPrefixB = JELLY_STATIC_IP_PREFIX_B;
constexpr uint8_t kLegacyIpPrefixC = JELLY_STATIC_IP_PREFIX_C;
constexpr uint8_t kLegacyIpBase = JELLY_STATIC_IP_BASE;
constexpr uint8_t kLegacyGatewayOctet = JELLY_STATIC_IP_GATEWAY;
constexpr uint8_t kUnassignedIpOctet = JELLY_UNASSIGNED_IP_OCTET;

}  // namespace config
}  // namespace jelly
