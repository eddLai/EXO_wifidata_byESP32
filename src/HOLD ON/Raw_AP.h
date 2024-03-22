#include <Arduino.h>
#include <WiFi.h>

const char *ssid = "MyESP32AP";
const char *password = "12345678";
WiFiServer server(8080);
HardwareSerial MySerial(1);  // second port
const int bufSize = 30;  // 定義緩衝區大小
char buf[bufSize];  // 緩衝區
int buf_index = 0;  // 緩衝區索引

void getHip_INFO(WiFiClient client);
void solvPC_quest(WiFiClient client);

void setup() {
  // Init
  Serial.begin(115200);
  delay(1000);
  WiFi.mode(WIFI_AP);
  IPAddress IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(IP, gateway, subnet);
  //Connecting
  WiFi.softAP(ssid, password);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  server.begin();
  MySerial.begin(115200, SERIAL_8N1, 20, 21);
  Serial.println("Serial at pin 20, 21 is ready!");
}

void loop() {
  WiFiClient client = server.available();  // 監聽來自PC端的連接
  if (client) {
    Serial.println("Client connected!");
    // String message = "Hello, Client!\n";
    // client.print(message);
    // client.flush();

    while (client.connected()){
      getHip_INFO(client);
    }
    
    client.stop();
    Serial.println("Client disconnected!");
    }
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
  client.print(buffer);
  client.println();
  free(buffer);  // 釋放動態分配的記憶體
}

void solvPC_quest(WiFiClient client){
  if (client.available()) {
    String cmd_str = client.readStringUntil('\n');
    Serial.print("Received command: ");
    Serial.println(cmd_str);
    // 將String對象轉換為C風格字符串
    // const char* cstr = cmd_str.c_str();
    // MySerial.write(cstr, strlen(cstr)); 
    MySerial.print(cmd_str);
  }
}

// 未解之謎
// void getHip_INFO(WiFiClient client) {
//   while (MySerial.available() > 0) {
//     char c = MySerial.read();
//     buf[buf_index % bufSize] = c;
//     buf_index++;
//   }
//   Serial.print("Buffer content: ");
//   for (int i = 0; i < buf_index; i++) {
//     Serial.print(buf[i]);
//     client.print(buf[i]);
//   }
//   Serial.println();
//   client.println();
  
//   // 清空緩衝區以便下一次使用（可選）
//   memset(buf, 0, bufSize);
//   buf_index = 0;

//   delay(100);  // 延遲1秒
// }