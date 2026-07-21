#pragma once

// Copy this file to Secrets.h and replace the values. Secrets.h is ignored by git.
#define JELLY_WIFI_SSID_1 "installation-network"
#define JELLY_WIFI_PASSWORD_1 "replace-me"
#define JELLY_WIFI_SSID_2 ""
#define JELLY_WIFI_PASSWORD_2 ""

// The SCW installation AP has no DHCP server, so static addressing is enabled.
// Device ID 16 becomes 192.168.0.116.
#define JELLY_USE_STATIC_IP 1
#define JELLY_STATIC_IP_PREFIX_A 192
#define JELLY_STATIC_IP_PREFIX_B 168
#define JELLY_STATIC_IP_PREFIX_C 0
#define JELLY_STATIC_IP_BASE 100
#define JELLY_STATIC_IP_GATEWAY 1
#define JELLY_UNASSIGNED_IP_OCTET 199
