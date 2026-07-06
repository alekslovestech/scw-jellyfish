

#include "config.h"

// Global log buffer
String serialLog;

//Log support
void logPrint(const String &s) {
    Serial.print(s);
    serialLog += s;
    trimLog();
}

void logPrint(const char *s) {
    Serial.print(s);
    serialLog += s;
    trimLog();
}

void logPrint(char c) {
    Serial.print(c);
    serialLog += c;
    trimLog();
}

void logPrint(int v) {
    Serial.print(v);
    serialLog += String(v);
    trimLog();
}

void logPrint(unsigned int v) {
    Serial.print(v);
    serialLog += String(v);
    trimLog();
}

void logPrint(long v) {
    Serial.print(v);
    serialLog += String(v);
    trimLog();
}

void logPrint(unsigned long v) {
    Serial.print(v);
    serialLog += String(v);
    trimLog();
}

void logPrint(float v) {
    Serial.print(v);
    serialLog += String(v);
    trimLog();
}

void logPrint(double v) {
    Serial.print(v);
    serialLog += String(v);
    trimLog();

}

template<typename T>
void logPrintln(const T &value) {
    Serial.print(value);
    Serial.print("\r\n");

    serialLog += String(value);
    serialLog += "\r\n";

    trimLog();
}

void trimLog() {
    if (serialLog.length() > MAX_LOG_SIZE) {
        serialLog.remove(0, serialLog.length() - MAX_LOG_SIZE);
    }
}

void logPrintf(const char *format, ...) {
    char buffer[256];

    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Handle strings larger than our buffer
    if (len >= (int)sizeof(buffer)) {
        char *bigBuffer = new char[len + 1];

        va_start(args, format);
        vsnprintf(bigBuffer, len + 1, format, args);
        va_end(args);

        Serial.print(bigBuffer);
        serialLog += bigBuffer;

        delete[] bigBuffer;
    } else {
        Serial.print(buffer);
        serialLog += buffer;
    }

    trimLog();
}