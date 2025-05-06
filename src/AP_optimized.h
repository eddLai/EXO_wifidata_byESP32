/**
 * ESP32 ODC bridge
 * --------------------------------------------------------------
 * - TCP 8080  :  收命令 (0x01) → 轉發給 UART2
 * - UDP 5000  :  把 UART2 每一行 (以 '\n' 結尾) 廣播到子網
 * - mDNS      :  esp32odc.local
 * --------------------------------------------------------------
 * UART2  :  RX = 16, TX = 17, 115 200-8-N-1
 * Wi-Fi  :  STA mode，IP 固定 10.154.48.200
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_wifi.h"

// ---------------- Wi-Fi & 網路參數 ----------------
const char* ssid     = "網路人我的超人";
const char* password = "12345666";

IPAddress local_IP (10, 154, 48, 200);
IPAddress gateway  (10, 154, 48, 238);   // ipconfig 的「預設閘道」
IPAddress subnet   (255, 255, 255, 0);

WiFiServer server(8080);                 // TCP 指令端口
WiFiClient client;

WiFiUDP     udp;                         // UDP 廣播
const uint16_t UDP_PORT = 5000;
IPAddress   bcastIP;

// ---------------- UART2 --------------------------
HardwareSerial MySerial(2);
const int UART_RX_PIN = 16;
const int UART_TX_PIN = 17;

// ---------------- Buffer -------------------------
const int   BUF_SIZE = 256;
uint8_t     recvBuf[BUF_SIZE];           // TCP→UART 封包暫存
int         recvIdx = 0;

char  uartBuf[BUF_SIZE];                 // UART→Wi-Fi 行暫存
int   uartIdx = 0;
bool  isLEDOn = false;

// ---------------- 任務宣告 ------------------------
void handleWiFiTask (void* pv);
void handleUARTTask(void* pv);
void processPacket ();
uint8_t calculateCRC8(const uint8_t* data, int len);
void sendResponse  (uint8_t cmdID, const uint8_t* payload, int len);

// =================================================
//                         S E T U P
// =================================================
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);   // 關掉 brown-out reset
  Serial.begin(115200);
  delay(500);

  // ── Wi-Fi STA 固定 IP ──
  WiFi.mode(WIFI_STA);
  if (!WiFi.config(local_IP, gateway, subnet))
    Serial.println("STA Failed to configure");

  WiFi.begin(ssid, password);
  Serial.print("Connecting Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) { Serial.print('.'); delay(500); }
  Serial.printf("\nConnected! IP = %s\n", WiFi.localIP().toString().c_str());

  // 廣播位址：同網段末尾 .255
  bcastIP = gateway;
  bcastIP[3] = 255;
  udp.begin(UDP_PORT);
  Serial.printf("UDP broadcast  %s:%u\n", bcastIP.toString().c_str(), UDP_PORT);

  // TCP Server
  server.begin();

  // 省電模式
  WiFi.setSleep(true);
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

  // mDNS
  if (MDNS.begin("esp32odc"))
    Serial.println("mDNS responder: esp32odc.local");

  // UART2
  MySerial.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  Serial.println("UART2 ready");
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // 任務
  xTaskCreatePinnedToCore(handleWiFiTask , "WiFiTask" , 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(handleUARTTask , "UARTTask" , 4096, NULL, 1, NULL, 1);

  Serial.println("Setup complete.");
}

// =================================================
//                       L O O P
// =================================================
void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }   // 主要邏輯都在任務

// =================================================
//              Wi-Fi (TCP)  →  UART2
// =================================================
void handleWiFiTask(void* pv) {
  for (;;) {
  if (!client.connected()) {
    if (client) {
      Serial.println("TCP client disconnected");
      digitalWrite(LED_BUILTIN, LOW);  // 關燈
    }
    client.stop();             // 釋放舊 client
    client = server.available();  // 等待新 client 連線
    if (client) {
      Serial.println("TCP client connected");
      digitalWrite(LED_BUILTIN, HIGH);  // 開燈
    }
    }

    while (client && client.connected() && client.available()) {
      if (recvIdx < BUF_SIZE)
        recvBuf[recvIdx++] = client.read();
      else
        recvIdx = 0;                    // overflow reset

      processPacket();
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// =================================================
//              UART2  →  Wi-Fi (UDP & TCP 回應)
// =================================================
void handleUARTTask(void* pv) {
  for (;;) {
    while (MySerial.available()) {
      char c = MySerial.read();
      uartBuf[uartIdx++] = c;

      if (c == '\n' || uartIdx >= BUF_SIZE - 1) {
        // 結束符號
        uartBuf[uartIdx] = '\0';

        // ① UDP 廣播
        udp.beginPacket(bcastIP, UDP_PORT);
        udp.write((uint8_t*)uartBuf, uartIdx);
        udp.endPacket();

        // ② (可選) TCP client 回傳
        if (client && client.connected())
          sendResponse(0x02, (uint8_t*)uartBuf, uartIdx);

        uartIdx = 0;
        break;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// =================================================
//     Parse TCP 封包，CmdID 0x01 → 發到 UART2
// =================================================
void processPacket() {
  while (recvIdx >= 5) {
    if (recvBuf[0]!=0xAA || recvBuf[1]!=0x55) {
      memmove(recvBuf, recvBuf+1, --recvIdx);
      continue;
    }
    uint8_t cmdID  = recvBuf[2];
    uint8_t length = recvBuf[3];

    if (recvIdx < 5 + length) break;          // 尚未完整收到

    uint8_t crc = recvBuf[4 + length];
    if (calculateCRC8(recvBuf, 4 + length) != crc) {
      Serial.println("CRC fail → resync");
      memmove(recvBuf, recvBuf+1, --recvIdx);
      continue;
    }

    // ---------- 指令處理 ----------
    if (cmdID == 0x01) {                      // PC→UART 指令
      MySerial.write(&recvBuf[4], length);
      sendResponse(0x03, (const uint8_t*)"ACK", 3);   // 即時 ACK
      
    }
    else if (cmdID == 0x03) {
      sendResponse(0x03, (const uint8_t*)"ACK", 3);
    }
    // --------------------------------
    else if (cmdID == 0x10) {  // 反轉 LED 狀態
    isLEDOn = !isLEDOn;                              // 狀態反轉
    digitalWrite(LED_BUILTIN, isLEDOn ? HIGH : LOW); // Toggle LED based on current state
    Serial.printf("💡 Command received: LED turned %s\n", isLEDOn ? "ON" : "OFF");
    sendResponse(0x10, (const uint8_t*)"OK", 2);      // Send ACK response
    }    

    int consumed = 5 + length;
    memmove(recvBuf, recvBuf + consumed, recvIdx - consumed);
    recvIdx -= consumed;
  }
}

// =================================================
//                      工具函式
// =================================================
uint8_t calculateCRC8(const uint8_t* data, int len) {
  uint8_t crc = 0;
  for (int i = 0; i < len; i++) crc ^= data[i];
  return crc;
}

void sendResponse(uint8_t cmdID, const uint8_t* payload, int len) {
  uint8_t pkt[BUF_SIZE];
  int idx = 0;
  pkt[idx++] = 0xAA; pkt[idx++] = 0x55;
  pkt[idx++] = cmdID;
  pkt[idx++] = len;
  memcpy(pkt + idx, payload, len);
  idx += len;
  pkt[idx++] = calculateCRC8(pkt, idx);

  if (client && client.connected())
    client.write(pkt, idx);
}
