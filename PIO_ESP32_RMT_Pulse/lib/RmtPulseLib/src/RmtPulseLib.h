#pragma once

#include <Arduino.h>

namespace RmtPulseLib {

// Send a finite pulse burst on the specified pin.
// frequencyHz defines the period, dutyPercent is 0.0f ~ 100.0f.
bool send(uint8_t pin, uint32_t pulseCount, uint32_t frequencyHz, float dutyPercent);

// Uninstall and release the RMT channel bound to this pin (if any).
void releasePin(uint8_t pin);

// Uninstall and release all channels allocated by this library.
void releaseAll();

// Read a message describing the latest failure.
const char* lastError();

}  // namespace RmtPulseLib
