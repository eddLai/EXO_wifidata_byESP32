#include <Arduino.h>
HardwareSerial MySerial(2);  // second port

void setup(){
    Serial.begin(115200);
    MySerial.begin(115200, SERIAL_8N1, 16, 17);
}

void loop(){
    if (MySerial.available()) {
        char c = MySerial.read();
        Serial.print(c);
    }
}