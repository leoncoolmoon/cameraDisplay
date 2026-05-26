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
// 路由注册：在 setup() 里提前调用，不依赖 WiFi
void wifiServerRegisterRoutes() {
  server.on("/",       handleRoot);
  server.on("/stream", handleStream);
  server.on("/upload", HTTP_POST,
            []() { /* 响应在 handleFileUpload UPLOAD_FILE_END 里发送 */ },
            handleFileUpload);
  server.on("/delete", HTTP_POST, handleFileDelete);
  server.on("/update", HTTP_POST, handleOTAResult, handleOTAUpload);
  server.onNotFound(handleNotFound);
  Serial.println("[WiFi] Routes registered");
}

// ------------------------------------------------------------
// AP 启动：WiFi up → DNS up → server.begin()
// 每次短按开启时调用
static bool serverStarted = false;

void wifiApStart() {
  int channel = random(1, 14);
  WiFi.mode(WIFI_AP);
  delay(100);  // 等待 WiFi modem 稳定，防止立即调用 softAPConfig 导致重启
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, channel, WIFI_HIDDEN);
  delay(100);  // 等待 AP 完全起来再启动 DNS
  dnsServer.start(53, "*", apIP);
  Serial.printf("[WiFi] AP started  ch=%d  hidden=%d  IP=%s\n",
                channel, WIFI_HIDDEN, apIP.toString().c_str());

  if (!serverStarted) {
    server.begin();
    serverStarted = true;
    Serial.println("[WiFi] HTTP server started");
  }
}

// ------------------------------------------------------------
void wifiApStop() {
  WiFi.softAPdisconnect(true);
  dnsServer.stop();
  // server 本身保持运行，重新开启 WiFi 后客户端自然能连上
  Serial.println("[WiFi] AP stopped");
}

// ------------------------------------------------------------
// 在 loop() 中调用
void wifiProcess() {
  dnsServer.processNextRequest();
  server.handleClient();
}