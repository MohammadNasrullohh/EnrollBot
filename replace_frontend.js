const fs = require('fs');
let frontend = fs.readFileSync('frontend_only.js', 'utf8');

// The logic starts right after:
// const wss = new WebSocket.Server({ server });
const wsIndex = frontend.indexOf('const wss = new WebSocket.Server({ server });');

const wsLogicNew = `const wss = new WebSocket.Server({ server });
wss.on('connection', (ws) => {
  logEvent("Connected via WebSocket");
  ws.on('message', (message, isBinary) => {
    try {
      if (isBinary) {
        const buffer = Buffer.isBuffer(message) ? message : Buffer.from(message);
        // ESP32 sends raw audio
        const session = voiceSessions.get(ws);
        if (session && !session.processing) {
          session.chunks.push(buffer);
          session.bytes += buffer.length;
        }
        return;
      }
      const text = message.toString();
      let parsed;
      try {
        parsed = JSON.parse(text);
      } catch(e) {
        // Not JSON
        return;
      }

      if (parsed.type === "auth" && parsed.role === "owibot") {
         logEvent("OwiBot Authenticated");
         owiSocket = ws;
      } else if (parsed.event === "start_record") {
         voiceSessions.set(ws, { chunks: [], bytes: 0, sampleRate: 16000, processing: false });
         latestSpeech.voiceStatus = "listening";
         logEvent("Voice start record");
      } else if (parsed.event === "stop_record") {
         latestSpeech.voiceStatus = "thinking";
         logEvent("Voice stop record, thinking...");
         handleVoiceSession(ws).catch(e => logEvent(e.message));
      } else if (parsed.type === "telemetry") {
         latestTelemetry = parsed;
         latestTelemetry.lastUpdate = Date.now();
      }
    } catch(e) {
      logEvent("WS Message Error: " + e.message);
    }
  });
  ws.on('close', () => {
    logEvent("WebSocket Disconnected");
    if (owiSocket === ws) owiSocket = null;
  });
});

const oldHandler = server.listeners('request')[0];
// We keep the old stream logic for browser testing if needed
`;

frontend = frontend.replace(/const wss = new WebSocket\.Server[\s\S]*?const oldHandler = server\.listeners\('request'\)\[0\];/, wsLogicNew);

fs.writeFileSync('frontend_mod1.js', frontend);
console.log("Frontend modified");
