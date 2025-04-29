#include <Arduino.h>
#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// WiFi設定
const char* ssid = "MyESP32AP";
const char* password = "12345678";
WiFiServer server(8080);

// UART設定
HardwareSerial MySerial(2); // UART2
const int UART_RX_PIN = 16;
const int UART_TX_PIN = 17;

// Buffer設定
const int bufSize = 64; // 增大一點比較安全
char buf[bufSize];
String cmd_str;
WiFiClient client;

// 任務宣告
void command();
void getHip_INFO();
void handleCommandsTask(void* pvParameters);
void getAndSendHipInfoTask(void* pvParameters);

void setup() {
  // 關掉 Brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  delay(500); // 保險一點的延遲

  // WiFi AP模式設定
  IPAddress IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAPConfig(IP, gateway, subnet)) {
    Serial.println("AP Config Failed!");
  }
  if (!WiFi.softAP(ssid, password, 6)) { // 指定Channel 6，避開1/11常見干擾
    Serial.println("AP Start Failed!");
  }

  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  server.begin();

  // UART2初始化
  MySerial.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  Serial.println("UART2 is ready!");

  // FreeRTOS多核心任務
  xTaskCreatePinnedToCore(handleCommandsTask, "HandleCommands", 4096, NULL, 1, NULL, 0); // 核心0
  xTaskCreatePinnedToCore(getAndSendHipInfoTask, "GetAndSendHipInfo", 4096, NULL, 1, NULL, 1); // 核心1

  Serial.println("Multi-core setup complete!");
}

void loop() {
  if (!client.connected()) {
    client.stop(); // 確保舊連線清除
    client = server.available();
    if (client) {
      Serial.println("Client connected!");
    }
  }
  vTaskDelay(pdMS_TO_TICKS(500)); // 減少loop負擔
}

// 處理從client來的指令
void handleCommandsTask(void* pvParameters) {
  while (true) {
    if (client && client.connected()) {
      command();
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // 適度休息
  }
}

// 處理從HIP設備來的資料
void getAndSendHipInfoTask(void* pvParameters) {
  while (true) {
    if (client && client.connected()) {
      getHip_INFO();
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// 接收並轉送client命令到UART
void command() {
  while (client.available()) {
    char c = client.read();
    if (c == '\n') { // 當作一條指令結束
      if (cmd_str.length() > 0) {
        Serial.print("Received from PC: ");
        Serial.println(cmd_str);

        MySerial.println(cmd_str); // UART送出去，自帶換行
        cmd_str = "";
      }
    } else {
      if (cmd_str.length() < bufSize - 1) {
        cmd_str += c;
      } else {
        Serial.println("Warning: Command too long, clearing buffer.");
        cmd_str = "";
      }
    }
  }
}

// 讀取UART資料並回送到client
void getHip_INFO() {
  static const int bufferSize = 1024;
  static char buffer[bufferSize];
  static int index = 0;

  while (MySerial.available()) {
    char c = MySerial.read();
    buffer[index++] = c;

    if (c == '\n' || index >= bufferSize - 1) {
      buffer[index] = '\0'; // 字串結束
      Serial.print("From HIP: ");
      Serial.println(buffer);

      if (client && client.connected()) {
        client.println(buffer);
      }

      index = 0; // Reset
      break;     // 一次處理一行
    }
  }
}
