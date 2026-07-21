#include "LogBuffer.h"
#include "AppConfig.h"

namespace jelly {

LogBuffer Log;

void LogBuffer::begin(unsigned long baud) {
  Serial.begin(baud);
  delay(100);
}

void LogBuffer::append(const String& value) {
  data_ += value;
  trim();
}

void LogBuffer::trim() {
  if (data_.length() > config::kLogCapacity) {
    data_.remove(0, data_.length() - config::kLogCapacity);
  }
}

void LogBuffer::print(const String& value) {
  Serial.print(value);
  append(value);
}

void LogBuffer::println(const String& value) {
  Serial.println(value);
  append(value + "\r\n");
}

void LogBuffer::printf(const char* format, ...) {
  va_list args;
  va_start(args, format);
  va_list copy;
  va_copy(copy, args);
  const int required = vsnprintf(nullptr, 0, format, copy);
  va_end(copy);

  if (required <= 0) {
    va_end(args);
    return;
  }

  String output;
  output.reserve(required + 1);
  char* buffer = static_cast<char*>(malloc(required + 1));
  if (buffer != nullptr) {
    vsnprintf(buffer, required + 1, format, args);
    output = buffer;
    free(buffer);
    Serial.print(output);
    append(output);
  }
  va_end(args);
}

String LogBuffer::snapshot() const {
  return data_;
}

}  // namespace jelly
