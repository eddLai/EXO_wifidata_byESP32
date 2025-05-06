/*
 * esp32_ms_mode_ok.ino  (2025-05-06 updated)
 * -------------------------------------------------
 * USB-CDC : 指令輸入／除錯輸出
 * UART2   : TX = 17, RX = 16, 115200-8-N-1  → 馬達控制板
 *
 * 指令格式：
 *   M S θ1 ω1 θ2 ω2   (deg, deg/s)
 *   其他文字           → 原封直送 UART2
 */

 #include <Arduino.h>

 // ─────────── UART2 & 常數 ───────────
 HardwareSerial MX(2);
 const int RX2 = 16;
 const int TX2 = 17;
 
 #define U2_BAUD 115200
 #define U2_EOL  "\n"      // 若對端要 CRLF → 改成  "\r\n"
 #define EPS_DIR 1e-6f     // 為 copysignf() 增加極小偏移
 #define SPEED_SCALE 100
 #define ANGLE_EPS 0.5f
 #define CHECK_PERIOD 50  // ms
 
 // ─────────── S-mode 變數 ───────────
 volatile float curA1 = 0, curA2 = 0;      // 目前角度
 float tgtA1 = 0,  tgtA2 = 0;              // 目標角度
 float tgtS1 = 30, tgtS2 = 30;             // 速度上限 (deg/s)
 bool  sMode = false;
 bool  sent = false;
 uint32_t lastCheck = 0;
 
 // ─────────── 緩衝區 ───────────
 String usbLine;
 String u2Line;
 
 // ─────────── 功能 ───────────
 inline void sendSpeed(int sp1, int sp2) {
   MX.printf("X C %d C %d" U2_EOL, sp1, sp2);
   MX.flush();
   Serial.printf("TX→U2: X C %d C %d" U2_EOL, sp1, sp2);
 }
 
 inline void sendStop() {
   MX.print(F("X F 0 F 0" U2_EOL));
   MX.flush();
   Serial.println(F("TX→U2: X F 0 F 0"));
 }
 
 // =================================================
 void setup() {
   Serial.begin(115200);
   MX.begin(U2_BAUD, SERIAL_8N1, RX2, TX2);
 
   pinMode(LED_BUILTIN, OUTPUT);
   digitalWrite(LED_BUILTIN, LOW);
 
   Serial.println(F("\n[READY]  請輸入：M S θ1 ω1 θ2 ω2"));
 }
 
 // =================================================
 void loop() {
   // ───── USB 指令輸入 ─────
   while (Serial.available()) {
     char c = Serial.read();
     usbLine += c;
 
     if (c == '\n' || c == '\r' || usbLine.length() > 64) {
       float a1, w1, a2, w2;
       if (sscanf(usbLine.c_str(), "M S %f %f %f %f", &a1, &w1, &a2, &w2) == 4) {
         tgtA1 = a1;  tgtS1 = fabsf(w1);
         tgtA2 = a2;  tgtS2 = fabsf(w2);
         sMode = true;
         sent = false;
 
         Serial.printf("[%lu ms] > %s", millis(), usbLine.c_str());
         Serial.println(F("[S] start"));
       } else {
         MX.print(usbLine);
         MX.flush();
         Serial.printf("[%lu ms] > %s(pass-thru)", millis(), usbLine.c_str());
       }
       usbLine = "";
     }
   }
 
   // ───── UART2 回覆接收 ─────
   while (MX.available()) {
     char c = MX.read();
     u2Line += c;
 
     if (c == '\n' || u2Line.length() > 128) {
       float a1, s1, i1, a2, s2, i2, r, p, y;
       bool ok = (sscanf(u2Line.c_str(), "X %f %f %f %f %f %f %f %f %f",
                         &a1, &s1, &i1, &a2, &s2, &i2, &r, &p, &y) == 9);
       if (ok) {
         curA1 = a1;
         curA2 = a2;
        //  Serial.printf(
        //    "A1=%+6.2f°  S1=%+5.1f°/s  I1=%.2f A | "
        //    "A2=%+6.2f°  S2=%+5.1f°/s  I2=%.2f A | "
        //    "R=%+5.1f°  P=%+5.1f°  Y=%+5.1f°\n",
        //    a1, s1, i1 / 100.0f,
        //    a2, s2, i2 / 100.0f,
        //    r, p, y
        //  );
       } else {
         Serial.print(F("RX←U2(raw): "));
         Serial.print(u2Line);
       }
       u2Line = "";
     }
   }
 
   // ───── S-mode 控制邏輯 ─────
   if (sMode) {
     if (!sent) {
       int sp1 = (int)(copysignf(tgtS1 + EPS_DIR, tgtA1 - curA1) * SPEED_SCALE);
       int sp2 = (int)(copysignf(tgtS2 + EPS_DIR, tgtA2 - curA2) * SPEED_SCALE);
       sendSpeed(sp1, sp2);
       digitalWrite(LED_BUILTIN, HIGH);
       sent = true;
       lastCheck = millis();
       Serial.printf("[%lu ms] sent speed\n", millis());
     }
 
     if (millis() - lastCheck >= CHECK_PERIOD) {
       lastCheck = millis();
 
       float d1 = tgtA1 - curA1;
       float d2 = tgtA2 - curA2;
       Serial.printf("[%lu ms] check position: Δ1=%.2f Δ2=%.2f\n", millis(), d1, d2);
 
       if (fabsf(d1) <= ANGLE_EPS && fabsf(d2) <= ANGLE_EPS) {
         sendStop();
         sMode = false;
         Serial.printf("[%lu ms] [S] reached\n", millis());
         digitalWrite(LED_BUILTIN, LOW);
       }
     }
   }
 }
 