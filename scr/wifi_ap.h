#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "config.h"
#include "web_handlers.h"
#include "ota_handler.h"

// ── 对象实例（此处定义，其他文件 extern 引用）───────────────
WebServer  server(80);
DNSServer  dnsServer;
IPAddress  apIP(192, 168, 4, 1);

// ── 外部依赖（定义在主 .ino）────────────────────────────────
// handleStream 定义在主 .ino，因为它依赖帧缓冲全局变量
extern void handleStream();

// ------------------------------------------------------------
void wifiApStart() {
  int channel = random(1, 14);  // 随机信道
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, channel, WIFI_HIDDEN);
  dnsServer.start(53, "*", apIP);
  Serial.printf("[WiFi] AP started  ch=%d  hidden=%d  IP=%s\n",
                channel, WIFI_HIDDEN, apIP.toString().c_str());
}

// ------------------------------------------------------------
void wifiApStop() {
  WiFi.softAPdisconnect(true);
  dnsServer.stop();
  Serial.println("[WiFi] AP stopped");
}

// ------------------------------------------------------------
void wifiServerSetup() {
  server.on("/",       handleRoot);
  server.on("/stream", handleStream);
  server.on("/upload", HTTP_POST,
            []() { /* 响应在 handleFileUpload UPLOAD_FILE_END 里发送 */ },
            handleFileUpload);
  server.on("/delete", HTTP_POST, handleFileDelete);
  server.on("/update", HTTP_POST, handleOTAResult, handleOTAUpload);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("[WiFi] HTTP server started");
}

// ------------------------------------------------------------
// 在 loop() 中调用
void wifiProcess() {
  dnsServer.processNextRequest();
  server.handleClient();
}
