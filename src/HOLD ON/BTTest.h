#include "BluetoothSerial.h"

BluetoothSerial SerialBT; // 创建一个BluetoothSerial对象

void KneeBO_CMD(byte table, byte pa1, byte pa2) {
  byte buf[7] = {0, 0, 0, 0, 0, 0, 0};

  buf[0] = 0x4A; // 開始字節
  buf[1] = 0x01; // 裝置編號
  buf[2] = table; // 功能
  buf[3] = pa1; // 參數1
  buf[4] = pa2; // 參數2
  buf[5] = 0x44; // 結束字節

  // 計算校驗字節
  for (int i = 0; i < 6; i++) {
    buf[6] ^= buf[i];
  }

  // 通過藍牙發送數據
  SerialBT.write(buf, sizeof(buf));
}

void setup() {
  Serial.begin(115200);
  if (!SerialBT.begin("ESP32", true)) { // 启动ESP32的蓝牙功能
    Serial.println("BT master init failed!");
    while (1);
  }

  Serial.println("BT on, waiting for connection...");
  if (SerialBT.connect("YMKB_0001")) { // 尝试连接到名为"Philips BT2003"的设备
    Serial.println("connected to YMKB_0001");
  } else {
    Serial.println("failed to connect YMKB_0001");
  }
  KneeBO_CMD(0x41, 81, 0);
}

void loop() {
  if (SerialBT.connected()) {
    char recvChar = (char)SerialBT.read();
    Serial.print(recvChar);
  }
}