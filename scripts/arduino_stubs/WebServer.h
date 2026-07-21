#pragma once
#include "Arduino.h"
#include <functional>
enum HTTPMethod { HTTP_GET, HTTP_POST };
enum UploadStatus { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };
struct HTTPUpload { UploadStatus status=UPLOAD_FILE_START; uint8_t* buf=nullptr; size_t currentSize=0; };
class WebServer {
 public:
  explicit WebServer(uint16_t) {}
  template <typename Handler> void on(const char*, HTTPMethod, Handler) {}
  template <typename Handler, typename UploadHandler> void on(const char*, HTTPMethod, Handler, UploadHandler) {}
  template <typename Handler> void onNotFound(Handler) {}
  void begin() {}
  void stop() {}
  void handleClient() {}
  void send(int,const char*,const String&) {}
  bool hasArg(const char*) { return false; }
  String arg(const char*) { return {}; }
  HTTPUpload& upload() { static HTTPUpload value; return value; }
};
