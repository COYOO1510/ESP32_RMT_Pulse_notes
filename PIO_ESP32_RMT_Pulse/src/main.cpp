#include <Arduino.h>
#include "RmtPulseLib.h"

const uint8_t pin = 4;
const uint32_t pulseCount = 5;
const uint32_t frequencyHz = 500000;
const float dutyPercent = 20.0f;

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("RMT pulse library demo");
}

void loop() {
    const bool ok = RmtPulseLib::send(pin, pulseCount, frequencyHz, dutyPercent);
    if (!ok) {
        Serial.print("Send failed: ");
        Serial.println(RmtPulseLib::lastError());
    } else {
        Serial.println("Pulse burst sent.");
    }
    delay(2000);
}
