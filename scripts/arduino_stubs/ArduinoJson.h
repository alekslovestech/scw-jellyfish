#pragma once
#include "Arduino.h"
class JsonVariantProxy {
 public:
  template <typename T> JsonVariantProxy& operator=(const T&) { return *this; }
  template <typename T> T as() const { return T(); }
  template <typename T> T operator|(const T& fallback) const { return fallback; }
  const char* operator|(const char* fallback) const { return fallback; }
};
class JsonObject {
 public:
  JsonVariantProxy operator[](const char*) { return {}; }
};
template <size_t Capacity>
class StaticJsonDocument {
 public:
  JsonVariantProxy operator[](const char*) { return {}; }
  JsonVariantProxy operator[](const char*) const { return {}; }
  bool containsKey(const char*) const { return false; }
  JsonObject createNestedObject(const char*) { return {}; }
};
class DynamicJsonDocument {
 public:
  explicit DynamicJsonDocument(size_t) {}
  JsonVariantProxy operator[](const char*) { return {}; }
  JsonObject createNestedObject(const char*) { return {}; }
};
class DeserializationError { public: explicit operator bool() const { return false; } };
template <typename Doc> inline DeserializationError deserializeJson(Doc&, const char*, size_t) { return {}; }
template <typename Doc> inline size_t serializeJson(const Doc&, char*, size_t) { return 1; }
template <typename Doc> inline size_t serializeJson(const Doc&, String&) { return 1; }
