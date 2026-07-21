#pragma once
constexpr int WIFI_PS_NONE = 0;
inline int esp_wifi_set_ps(int) { return 0; }
