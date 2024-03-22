#include <Arduino.h>

void setup() {
  Serial.begin(115200);
}

void loop() {
  Serial.println("HelloWorld!");
  Serial.print("使用核心編號：");
  Serial.println(xPortGetCoreID());//xPortGetCoreID()顯示執行核心編號
delay(1000);
}