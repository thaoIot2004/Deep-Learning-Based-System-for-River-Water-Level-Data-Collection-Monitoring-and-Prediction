#include <Arduino.h>
#include "esp_sleep.h"
#include "WiFi.h"

#define SIM_BAUD    115200
#define DEBUG_BAUD  115200

// URL Google Apps Script
const char* googleScriptURL = "https://script.google.com/macros/s/AKfycbwjMLXnkeyfFNMAN5S4EB0bgG3uk5bzd6Ot2G0M5Y6ruJ0OFY8cv1fW609MLL2p0xkW/exec";

// HC-SR04
const int trig = 14;
const int echo = 12;

// Lưu biến giữa các lần deep-sleep

// Gửi AT command
String sendATCommand(String cmd, unsigned long timeout = 2000) {
  String response = "";
  Serial2.println(cmd);
  unsigned long startTime = millis();
  while (millis() - startTime < timeout) {
    while (Serial2.available()) {
      char c = Serial2.read();
      response += c;
    }
  }
  Serial.println("CMD: " + cmd);
  Serial.println("RESP: " + response);
  return response;
}

// Thiết lập PDP
bool setupPDP() {
  sendATCommand("AT+QICSGP=1,1,\"m-wap\",\"mms\",\"mms\",1", 3000);
  String resp = sendATCommand("AT+QIACT=1", 8000);
  return (resp.indexOf("OK") != -1);
}

// Khởi tạo module SIM
bool initSIM() {
  String resp = sendATCommand("AT", 1000);
  if (resp.indexOf("OK") == -1) {
    Serial.println("Module SIM không phản hồi!");
    return false;
  }

  // Tắt PDP cũ nếu còn
  sendATCommand("AT+QIDEACT=1", 5000);

  if (!setupPDP()) {
    Serial.println("Kết nối PDP thất bại!");
    return false;
  }
  return true;
}

// Gửi dữ liệu lên Google Sheet
bool pushDataToGoogleSheet(String jsonData) {
  sendATCommand("AT+QHTTPCFG=\"contextid\",1", 2000);

  String url = String(googleScriptURL);
  int urlLen = url.length();

  String cmd = "AT+QHTTPURL=" + String(urlLen) + ",80";
  String resp = sendATCommand(cmd, 2000);
  if (resp.indexOf("CONNECT") != -1) {
    Serial2.println(url);
    delay(1000);
  } else {
    Serial.println("Lỗi cấu hình URL");
    return false;
  }

  int dataLen = jsonData.length();
  cmd = "AT+QHTTPPOST=" + String(dataLen) + ",80,80";
  resp = sendATCommand(cmd, 2000);
  if (resp.indexOf("CONNECT") != -1) {
    Serial2.print(jsonData);
    delay(2000);
    resp = "";
    unsigned long start = millis();
    while (millis() - start < 5000) {
      while (Serial2.available()) {
        resp += char(Serial2.read());
      }
    }
    Serial.println("HTTP POST resp: " + resp);
    bool ok = (resp.indexOf("200") != -1 || resp.indexOf("OK") != -1);

    // Đóng PDP sau khi gửi
    sendATCommand("AT+QIDEACT=1", 5000);
    return ok;
  }
  return false;
}

// Đọc khoảng cách
int readDistance() {
  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);

  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(5);
  digitalWrite(trig, LOW);

  unsigned long duration = pulseIn(echo, HIGH);
  return int(duration / 2 / 29.412);
}

void setup() {
  Serial.begin(DEBUG_BAUD);

  // Tắt WiFi & Bluetooth
  WiFi.mode(WIFI_OFF);
  btStop();

  Serial2.begin(SIM_BAUD, SERIAL_8N1, 16, 17);
  delay(2000);

  Serial.println("Khởi động...");

  if (!initSIM()) {
    esp_sleep_enable_timer_wakeup(10 * 1000000ULL);
    esp_deep_sleep_start();
  }

  int distance = readDistance();
  Serial.println("Khoảng cách: " + String(distance) + " cm");

  String jsonData = String("{\"num1\":") + String(distance)
                     + String(",\"num2\":0,\"num3\":0,\"num4\":0}");
  Serial.println("Dữ liệu gửi: " + jsonData);

  if (pushDataToGoogleSheet(jsonData)) {
    Serial.println("Gửi dữ liệu thành công!");
  } else {
    Serial.println("Gửi dữ liệu thất bại!");
  }

  Serial.println("Đã gửi dữ liệu " );

  
  Serial.println("Ngủ 30p...");
  esp_sleep_enable_timer_wakeup(1800ULL * 1000000ULL); // 30 phút
  esp_deep_sleep_start();
}

void loop() {}
