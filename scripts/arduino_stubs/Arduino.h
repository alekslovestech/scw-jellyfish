#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>

using std::size_t;
using byte = uint8_t;
constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 6.28318530717958647692f;
constexpr float DEG_TO_RAD = PI / 180.0f;

class String {
 public:
  String() = default;
  String(const char* value) : value_(value ? value : "") {}
  String(const std::string& value) : value_(value) {}
  String(char value) : value_(1, value) {}
  String(int value) : value_(std::to_string(value)) {}
  String(unsigned int value) : value_(std::to_string(value)) {}
  String(long value) : value_(std::to_string(value)) {}
  String(unsigned long value) : value_(std::to_string(value)) {}
  String(long long value) : value_(std::to_string(value)) {}
  String(unsigned long long value) : value_(std::to_string(value)) {}
  String(float value) : value_(std::to_string(value)) {}
  String(double value) : value_(std::to_string(value)) {}
  size_t length() const { return value_.size(); }
  const char* c_str() const { return value_.c_str(); }
  void reserve(size_t value) { value_.reserve(value); }
  void trim() {}
  void toLowerCase() {}
  int indexOf(const char*) const { return -1; }
  void replace(const char*, const char*) {}
  void remove(size_t, size_t = std::string::npos) {}
  bool equalsIgnoreCase(const char* value) const { return value_ == (value ? value : ""); }
  bool equalsIgnoreCase(const String& value) const { return value_ == value.value_; }
  float toFloat() const { return std::strtof(value_.c_str(), nullptr); }
  long toInt() const { return std::strtol(value_.c_str(), nullptr, 10); }
  char operator[](size_t index) const { return value_[index]; }
  String& operator+=(const String& other) { value_ += other.value_; return *this; }
  String& operator+=(const char* other) { value_ += other ? other : ""; return *this; }
  String& operator+=(char other) { value_ += other; return *this; }
  String& operator=(const char* other) { value_ = other ? other : ""; return *this; }
  bool operator==(const char* other) const { return value_ == (other ? other : ""); }
  bool operator!=(const char* other) const { return !(*this == other); }
  operator std::string() const { return value_; }
 private:
  std::string value_;
};
inline String operator+(const String& a, const String& b) { return String(std::string(a) + std::string(b)); }
inline String operator+(const String& a, const char* b) { return String(std::string(a) + (b ? b : "")); }
inline String operator+(const char* a, const String& b) { return String((a ? a : "") + std::string(b)); }

template <typename A, typename B>
constexpr auto min(A a, B b) -> typename std::common_type<A, B>::type { using R=typename std::common_type<A,B>::type; return std::min<R>(a,b); }
template <typename A, typename B>
constexpr auto max(A a, B b) -> typename std::common_type<A, B>::type { using R=typename std::common_type<A,B>::type; return std::max<R>(a,b); }
template <typename T, typename A, typename B>
constexpr T constrain(T value, A low, B high) { return value < low ? static_cast<T>(low) : (value > high ? static_cast<T>(high) : value); }

inline void delay(unsigned long) {}
inline unsigned long millis() { return 0; }
inline uint32_t esp_random() { return 1; }

class SerialClass {
 public:
  void begin(unsigned long) {}
  template <typename T> void print(const T&) {}
  template <typename T> void println(const T&) {}
};
extern SerialClass Serial;

class ESPClass {
 public:
  uint64_t getEfuseMac() const { return 0; }
  void restart() {}
};
extern ESPClass ESP;
