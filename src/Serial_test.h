#include <Arduino.h>

// 使用 UART2，TX=17, RX=16
HardwareSerial MySerial(2);

void setup() {
  MySerial.begin(115200, SERIAL_8N1, 16, 17);  // RX=16, TX=17
}

void loop() {
  MySerial.println("X E 0 E 0\r\n");
  delay(1000);  // 每秒發一次
}
