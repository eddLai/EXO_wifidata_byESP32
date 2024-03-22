#include <Arduino.h>
#include <WiFi.h>

const char *ssid = "DaDESKTOP"; // connect to the same domain
const char *password = "12345666";
const char* host = "169.254.248.204";
// const char* host = "0";
const uint16_t port = 8080;

WiFiClient client;
HardwareSerial MySerial(2);  // second port
void getHip_INFO(WiFiClient client);

void setup() {
  Serial.begin(115200);
  MySerial.begin(115200, SERIAL_8N1, 16, 17);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
}

void loop() {
    if (client.connect(host, port)) {
    Serial.println("Connected to server");
  } else {
    Serial.println("Connection to server failed");
  }

  delay(5000);  // 5秒后尝试再次连接
//   if (client.connected()) {
//     Serial.println("Connected to server");
//     // while (MySerial.available()) {
//     //   getHip_INFO(client);
//     // }
//   } else {
//     Serial.println("Server denied connection");
//   }
}

void getHip_INFO(WiFiClient client) {
    int bufferSize = 50;  // 初始緩衝區大小
  char* buffer = (char*) malloc(bufferSize);  // 動態分配記憶體
  if (buffer == NULL) {
    Serial.println("Failed to allocate memory!");
    return;
  }

  int index = 0;
  while (MySerial.available()) {
    char c = MySerial.read();
    if (c == '\n') {
      buffer[index] = '\0';  // 結束字符
      break;  // 跳出循環
    }

    // 檢查是否需要更多記憶體
    if (index >= bufferSize - 1) {
      bufferSize *= 2;  // 雙倍緩衝區大小
      char* newBuffer = (char*) realloc(buffer, bufferSize);
      if (newBuffer == NULL) {
        Serial.println("Failed to allocate more memory!");
        free(buffer);
        return;
      }
      buffer = newBuffer;
    }

    buffer[index++] = c;
  }
  MySerial.flush();
  Serial.println("From HIP: ");
  Serial.println(buffer);  // 直接打印整個字符串
  Serial.println();
  client.print(buffer);
  client.println();
  free(buffer);  // 釋放動態分配的記憶體
}
