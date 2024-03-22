// LED_bulitin test program

#include "Arduino.h"

void setup() {
    pinMode(BUILTIN_LED, OUTPUT);
    Serial.begin(115200);
}

void loop() {
    digitalWrite(BUILTIN_LED, HIGH);
    Serial.write("LED ON\n");
    delay(1000);
    digitalWrite(BUILTIN_LED, LOW);
    Serial.write("LED LOW\n");
    delay(1000);
}