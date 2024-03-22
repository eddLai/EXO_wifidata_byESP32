#include <Arduino.h>
#include <WiFi.h>

const char* ssid = "MyESP32AP";
const char* password = "12345678";
WiFiServer server(8080);
HardwareSerial MySerial(2); // second port
const int bufSize = 30;
char buf[bufSize];
int buf_index = 0;
String cmd_str;
WiFiClient client;

void command();
void getHip_INFO();
void handleCommandsTask(void* pvParameters);
void getAndSendHipInfoTask(void* pvParameters);

void setup() {
  // Init
  Serial.begin(115200);
  delay(1000);
  IPAddress IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(IP, gateway, subnet);
  //Connecting
  WiFi.softAP(ssid, password);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  server.begin();
  MySerial.begin(115200, SERIAL_8N1, 16, 17);
  Serial.println("Serial at pin is ready!");
  
  xTaskCreatePinnedToCore(handleCommandsTask, "HandleCommands", 10000, NULL, 1, NULL, 0); // 核心0
  xTaskCreatePinnedToCore(getAndSendHipInfoTask, "GetAndSendHipInfo", 10000, NULL, 1, NULL, 1); // 核心1
  Serial.println("Multi-core ready!");
}

void loop() {
  if (!client || !client.connected()) {
    client = server.available();
    if (client) {
      Serial.println("Client connected!");
    }
  }
  vTaskDelay(pdMS_TO_TICKS(1000));
}

void handleCommandsTask(void* pvParameters) {
  while (true) {
    if (client && client.connected()) {
      command();
    }
    delay(10); // 避免CPU佔用率過高
  }
}

void getAndSendHipInfoTask(void* pvParameters) {
  while (true) {
    if (client && client.connected()) {
      getHip_INFO();
    }
    delay(10); // 避免CPU佔用率過高
  }
}

void command() {
    while (client.available()) {
        char c = client.read();
        if (c == '\0') {
            if (cmd_str.length() > 0) {
                Serial.print("received data from PC: ");
                MySerial.print(cmd_str);
                Serial.println(cmd_str);
                client.write((const uint8_t *)cmd_str.c_str(), cmd_str.length());
                cmd_str = "";
            }
            }
        else {
            cmd_str += c;
        }
    }
}

void getHip_INFO() {
  const int bufferSize = 1024;
  static char buffer[bufferSize];
  static int index = 0;

  while (MySerial.available()) {
    buffer[index] = MySerial.read();
    if (buffer[index] == '\n' || index == bufferSize - 2) {
      buffer[index + 1] = '\0';
      // Serial.println("From HIP: ");
      Serial.println(buffer);
      client.println(buffer);
      index = 0;
      break;
    } else {
      index++;
    }
  }
}
