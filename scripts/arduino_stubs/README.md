# Arduino host syntax stubs

These headers are deliberately minimal compile-time stand-ins for Arduino-ESP32
and the firmware libraries. They let `scripts/check_firmware_gnu11.sh` compile
all firmware translation units with desktop `g++ -std=gnu++11` and catch C++
language/API-shape regressions such as invalid brace construction or accidental
`const` calls.

They do not emulate hardware, networking, timing, memory limits, library
implementations, or linking on an ESP32. A PlatformIO or Arduino IDE build is
still the authoritative firmware build.
