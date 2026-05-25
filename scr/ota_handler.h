#pragma once
#include <WebServer.h>
#include <Update.h>

// ── 外部依赖（定义在主 .ino）────────────────────────────────
extern WebServer server;
extern bool      streamPaused;

// ------------------------------------------------------------
void handleOTAResult() {
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
  delay(200);
  ESP.restart();
}

// ------------------------------------------------------------
void handleOTAUpload() {
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    streamPaused = true;
    Serial.printf("[OTA] Start: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("[OTA] Success: %u bytes\n", upload.totalSize);
      // streamPaused 保持 true，重启前不再推流
    } else {
      Update.printError(Serial);
      streamPaused = false;  // 失败时恢复流
    }
  }
}
