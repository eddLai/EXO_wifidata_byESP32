#include <Arduino.h>
#include <WiFi.h>

const char* ssid = "MyESP32AP";
const char* password = "12345678";
WiFiServer server(8080);
HardwareSerial MySerial(2); // second port
String cmd_str;
WiFiClient client;

void command();

void setup() {
  // Init Serial port
  Serial.begin(115200);
  MySerial.begin(115200, SERIAL_8N1, 16, 17); // Initialize MySerial
  delay(1000); // Wait for Serial port to initialize
  
  // Setup WiFi AP
  WiFi.softAP(ssid, password);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  server.begin();
}

void loop() {
  // Check if a client has connected
  if (!client || !client.connected()) {
    client = server.available(); // Listen for incoming clients
    if (client) {
      Serial.println("Client connected!");
    }
  }
  
  // If a client is connected, execute the command function
  if (client && client.connected()) {
    command();
  }

  // Add a small delay to prevent the loop from running too fast
  delay(10);
}

void command() {
  while (client.available()) {
    char c = client.read();
    // Ignore NULL characters
    if (c == '\0') continue;

    // Assuming a newline character '\n' signifies the end of a command
    if (c == '\n') {
      if (cmd_str.length() > 0) {
        Serial.print("Received data from client: ");
        Serial.println(cmd_str); // Echo received command to Serial
        MySerial.print(cmd_str);
        // Optionally, send response back to client
        cmd_str = ""; // Reset command string
      }
    } else {
      cmd_str += c; // Append received character to command string
    }
  }
}
