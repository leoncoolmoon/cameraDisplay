#pragma once
#include <WebServer.h>
#include <LittleFS.h>
#include "default_page.h"

// ── 外部依赖（定义在主 .ino）────────────────────────────────
extern WebServer server;
extern bool      streamPaused;

// ── 静态文件句柄，跨上传阶段持有 ────────────────────────────
static File _uploadFile;

// ------------------------------------------------------------
void handleRoot() {
  if (LittleFS.exists("/index.html")) {
    File f = LittleFS.open("/index.html", "r");
    server.streamFile(f, "text/html");
    f.close();
  } else {
    server.send(200, "text/html", DEFAULT_INDEX_HTML);
  }
}

// ------------------------------------------------------------
void handleFileUpload() {
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    const String &name = upload.filename;
    if (name != "index.html" && name != "QRcode.jpg") {
      Serial.println("[Upload] Rejected: " + name);
      return;
    }
    streamPaused = true;
    _uploadFile = LittleFS.open("/" + name, "w");
    if (!_uploadFile) Serial.println("[Upload] Failed to open: " + name);
    else              Serial.println("[Upload] Start: " + name);

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (_uploadFile) _uploadFile.write(upload.buf, upload.currentSize);

  } else if (upload.status == UPLOAD_FILE_END) {
    if (_uploadFile) {
      _uploadFile.close();
      Serial.printf("[Upload] Done: %s (%u bytes)\n",
                    upload.filename.c_str(), upload.totalSize);
    }
    streamPaused = false;
    server.send(200, "text/plain", "上传完成: " + upload.filename);
  }
}

// ------------------------------------------------------------
void handleFileDelete() {
  String path = "/" + server.arg("filename");
  if (LittleFS.exists(path)) {
    LittleFS.remove(path);
    Serial.println("[Delete] " + path);
    server.send(200, "text/plain", "已删除: " + path);
  } else {
    server.send(404, "text/plain", "文件不存在: " + path);
  }
}

// ------------------------------------------------------------
void handleNotFound() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}
