const fs = require('fs');

let cpp = fs.readFileSync('src/wifi_max_stream.cpp', 'utf8');

// 1. Add WebSocket include
cpp = cpp.replace(/#include <WiFi\.h>\n/, "#include <WiFi.h>\n#include <WebSocketsClient.h>\n#include <HTTPClient.h>\n");

// 2. Remove UDP objects and add WebSocket object
cpp = cpp.replace(/WiFiUDP telemetryUdp;\nWiFiUDP micUdp;\n/, "WebSocketsClient webSocket;\n");

// 3. Add WebSocket event handler
const eventHandler = `
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[WS] Disconnected!");
      break;
    case WStype_CONNECTED:
      Serial.printf("[WS] Connected to url: %s\\n", payload);
      break;
    case WStype_TEXT:
      {
        String text = (char*)payload;
        Serial.printf("[WS] get text: %s\\n", text.c_str());
        if (text.startsWith("CMD:")) {
          String cmd = text.substring(4);
          if (cmd == "C") { currentState = STATE_FACE; faceMode = BMP_FACE_NORMAL; gamePhase = GAME_IDLE; Serial.println("cmd: Back to Face"); }
          else if (cmd.startsWith("T:")) {
            strncpy(chatText, cmd.substring(2).c_str(), sizeof(chatText)-1);
            chatText[sizeof(chatText)-1] = '\\0';
            currentState = STATE_CHAT;
            chatDisplayUntilMs = millis() + max(3000, (int)strlen(chatText) * 150);
          }
          // handle other cmds if necessary
        }
      }
      break;
  }
}
`;
cpp = cpp.replace(/void setup\(\) \{/, eventHandler + "\nvoid setup() {");

// 4. In setup, initialize WebSocket instead of UDP
cpp = cpp.replace(/telemetryUdp\.begin\(7788\);\n\s*micUdp\.begin\(7777\);/, `webSocket.begin("212.2.253.247", 3000, "/");\n  webSocket.onEvent(webSocketEvent);\n  webSocket.setReconnectInterval(5000);`);

// 5. In loop, run webSocket.loop()
cpp = cpp.replace(/unsigned long now = millis\(\);/, "unsigned long now = millis();\n  webSocket.loop();");

// 6. Rewrite sendTelemetry
cpp = cpp.replace(/void sendTelemetry\(\) \{[\s\S]*?telemetryUdp\.endPacket\(\);\n\s*\}/, `void sendTelemetry() {
  if (millis() - lastTelemetryMs < 100) return;
  lastTelemetryMs = millis();
  char json[256];
  snprintf(json, sizeof(json), "{\\"tiltX\\":%.2f,\\"tiltY\\":%.2f,\\"shakeMeter\\":%.2f}", tiltX, tiltY, shakeMeter);
  webSocket.sendTXT(json);
}`);

fs.writeFileSync('src/wifi_max_stream.cpp', cpp);
console.log("Converted C++ to use WebSockets!");
