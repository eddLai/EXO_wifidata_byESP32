/*
 * esp32_ms_mode_ok.ino  (2025-05-07 final)
 * -------------------------------------------------
 * USB-CDC : 指令輸入／除錯輸出
 * UART2   : TX = 17, RX = 16, 115200-8-N-1 → 馬達控制板
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
 
 #define U2_BAUD     115200
 #define EPS_DIR     1e-6f     // 為 copysignf() 增加極小偏移
 #define SPEED_SCALE 100
 #define ANGLE_EPS   0.5f
 #define CHECK_PERIOD 50  // ms
 
 // ─────────── S-mode 變數 ───────────
 volatile float curA1 = 0, curA2 = 0;
 float tgtA1 = 0, tgtA2 = 0;
 float tgtS1 = 30, tgtS2 = 30;
 bool  sMode = false;
 bool  sent = false;
 bool firstCheckDone = false;
 uint32_t lastCheck = 0;
 
 // ─────────── 緩衝區 ───────────
 String usbLine;
 String u2Line;
 
 float prevD1 = NAN, prevD2 = NAN;
 float prev2D1 = NAN, prev2D2 = NAN;
 int prevSignD1 = 0, prevSignD2 = 0;  // 新增的變數，用來記錄上次的角度差符號
 
 // 設定判斷的閾值
 #define ANGLE_CONVERGENCE_THRESHOLD 0.1f  // 三次誤差都低於這個值時停止
 #define ANGLE_EPS 0.5f  // 微小誤差
 
 // 判斷是否達成目標
 bool isConverged(float d1, float d2) {
     // 判斷前三次的誤差是否都小於某個閾值
     if (fabs(d1) < ANGLE_EPS && fabs(d2) < ANGLE_EPS) {
         if (fabs(prevD1) < ANGLE_CONVERGENCE_THRESHOLD &&
             fabs(prev2D1) < ANGLE_CONVERGENCE_THRESHOLD &&
             fabs(prevD2) < ANGLE_CONVERGENCE_THRESHOLD &&
             fabs(prev2D2) < ANGLE_CONVERGENCE_THRESHOLD) {
             return true;
         }
     }
     return false;
 }
 
 // ─────────── 功能 ───────────
inline void sendSpeed(int sp1, int sp2) {
  MX.printf("X C %d C %d\r\n", sp1, sp2);  // 明確加上 CRLF
  MX.flush();
  Serial.printf("TX→U2: X C %d C %d\n", sp1, sp2);
}

inline void sendStop() {
  MX.printf("X F 0 F 0\r\n");  // 明確加上 CRLF
  MX.flush();
  Serial.println("TX→U2: X F 0 F 0");
}

// =================================================
void setup() {
  Serial.begin(115200);
  MX.begin(U2_BAUD, SERIAL_8N1, RX2, TX2);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.println(F("\n[READY] 請輸入：M S θ1 ω1 θ2 ω2"));
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
              MX.print(usbLine);  // pass-thru 指令
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
          } else {
              Serial.print(F("RX←U2(raw): "));
              Serial.print(u2Line);
          }
          u2Line = "";
      }
  }

  // ─────────── S-mode 控制邏輯 ───────────
  // ─────────── S-mode 控制邏輯 ───────────
if (sMode) {
  if (!sent) {
      int sp1 = (int)(copysignf(tgtS1 + EPS_DIR, tgtA1 - curA1) * SPEED_SCALE);
      int sp2 = (int)(copysignf(tgtS2 + EPS_DIR, tgtA2 - curA2) * SPEED_SCALE);
      sendSpeed(sp1, sp2);
      digitalWrite(LED_BUILTIN, HIGH);
      sent = true;
      lastCheck = millis();
      Serial.printf("[%lu ms] sent speed\n", millis());
      firstCheckDone = false;  // reset flag
  }

  if (millis() - lastCheck >= CHECK_PERIOD) {
      lastCheck = millis();

      // 計算角度差
      float d1 = tgtA1 - curA1;
      float d2 = tgtA2 - curA2;
      Serial.printf("[%lu ms] check position: Δ1=%.2f Δ2=%.2f\n", millis(), d1, d2);

      // 計算目前符號
      int signD1 = (d1 >= 0) ? 1 : -1;
      int signD2 = (d2 >= 0) ? 1 : -1;
      Serial.printf("[%lu ms] previous signs: signD1=%d, signD2=%d\n", millis(), prevSignD1, prevSignD2);
      Serial.printf("[%lu ms] current  signs: signD1=%d, signD2=%d\n", millis(), signD1, signD2);

      if (!firstCheckDone) {
          // 第一次進來，只儲存符號，不判斷
          prevSignD1 = signD1;
          prevSignD2 = signD2;
          firstCheckDone = true;
      } else if ((prevSignD1 != signD1) || (prevSignD2 != signD2)) {
          sendStop();
          sMode = false;
          Serial.printf("[%lu ms] [S] reached (sign change detected)\n", millis());
          digitalWrite(LED_BUILTIN, LOW);
          prevD1 = NAN; prev2D1 = NAN;
          prevD2 = NAN; prev2D2 = NAN;
          firstCheckDone = false;  // reset for next use
      } else {
          prev2D1 = prevD1;
          prevD1 = d1;
          prev2D2 = prevD2;
          prevD2 = d2;
          prevSignD1 = signD1;
          prevSignD2 = signD2;
      }
  }
}
}