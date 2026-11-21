#pragma once

#include <stdint.h>

namespace Obd {

enum Gauge : uint8_t { Rpm, Voltage, Coolant, Fuel, GaugeCount };
enum class Link : uint8_t { Searching, Connecting, WaitingForEcu, Online, Error };

struct Reading {
  float value;
  uint32_t timestamp;
  bool valid;
};

struct State {
  Reading readings[GaugeCount];
  Link link;
  bool fuelUnsupported;
};

constexpr uint32_t readingLifetimeMs = 3500;

bool start();
State snapshot();
const char *linkText(Link link);

}  // namespace Obd
