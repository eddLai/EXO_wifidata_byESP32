#include <Arduino.h>
#include <WiFi.h>

const char *ssid = "MyESP32AP";
const char *password = "12345678";
WiFiServer server(8080);
HardwareSerial MySerial(2);  // second port
const int bufSize = 30;
char buf[bufSize];
int buf_index = 0;
String cmd_str;

void command(WiFiClient client);
void getHip_INFO(WiFiClient client);

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
  MySerial.begin(115200, SERIAL_8N1, 16, 17);
  Serial.println("Serial at pin is ready!");
}

void loop() {
  WiFiClient client = server.available();
  if (client) {
    Serial.println("Client connected!");

    while (client.connected()) {
        command(client);
        getHip_INFO(client);
    }
    
    client.stop();
    Serial.println("Client disconnected!");
  }
}

void command(WiFiClient client){
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

void getHip_INFO(WiFiClient client) {
  int bufferSize = 35;
  char buffer[bufferSize] = {};
  int index = 0;
  while (MySerial.available() && index < bufferSize - 1) {
    buffer[index++] = MySerial.read();
  }
  buffer[index] = '\0';  // 确保字符串结束
  Serial.println("From HIP: ");
  Serial.println(buffer);
  client.write((const uint8_t *)buffer, index);
  Serial.println("finished");
}
