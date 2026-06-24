const fs = require('fs');
let code = fs.readFileSync('web_serial_server.js', 'utf8');

// Replace TCP streamAudio with WS streamAudio
const streamAudioRegex = /async function streamAudio[\s\S]*?async function streamTestTone/m;
code = code.replace(streamAudioRegex, `async function streamAudioToWS(ws, mp3Path, volume = '0.30') {
  if (isStreamingAudio) return;
  isStreamingAudio = true;
  logEvent('stream audio ' + mp3Path + ' via WS vol ' + volume);

  try {
    const sampleRate = 16000;
    const safeVolume = clampVolume(volume).toFixed(2);
    const chunkSize = 1024;

    const ffmpeg = spawn(ffmpegPath, [
      '-hide_banner',
      '-loglevel', 'error',
      '-i', path.resolve(__dirname, mp3Path),
      '-f', 's16le',
      '-acodec', 'pcm_s16le',
      '-ac', '1',
      '-ar', String(sampleRate),
      '-filter:a', \`highpass=f=95,lowpass=f=7200,loudnorm=I=-20:TP=-2.5:LRA=8,acompressor=threshold=-24dB:ratio=2.2:attack=18:release=240,alimiter=limit=0.38,volume=\${safeVolume}\`,
      'pipe:1',
    ], { stdio: ['ignore', 'pipe', 'pipe'] });

    for await (const chunk of ffmpeg.stdout) {
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(chunk);
      }
    }
    ffmpeg.kill();
  } catch (err) {
    logEvent('Error streaming WS: ' + err.message);
  } finally {
    isStreamingAudio = false;
  }
}

async function streamTestTone`);

// Update speakReplyOnBot to use streamAudioToWS
code = code.replace(/streamAudio\(latestTelemetry\.ip, volume, ttsFile\);/, 'if(owiSocket && owiSocket.readyState === WebSocket.OPEN) streamAudioToWS(owiSocket, ttsFile, volume);');

// Replace sendCommand to use WS instead of UDP
code = code.replace(/async function sendCommand[\s\S]*?function sendChatText/m, `async function sendCommand(command) {
  if (owiSocket && owiSocket.readyState === WebSocket.OPEN) {
    owiSocket.send('CMD:' + command);
    logEvent('WS sent CMD:' + command);
  } else {
    logEvent('No WS to send command: ' + command);
  }
}
function sendChatText`);

// Replace sendChatText to use WS instead of UDP
code = code.replace(/function sendChatText[\s\S]*?async function handleVoiceSession/m, `function sendChatText(text) {
  const clean = sanitizeOledText(text).slice(0, 200);
  logEvent('chat \"' + clean + '\"');
  if (owiSocket && owiSocket.readyState === WebSocket.OPEN) {
    owiSocket.send('CMD:T:' + clean);
  }
}

async function handleVoiceSession`);

// Update the WebSocket Server to handle ESP32 logic (WStype_BIN and JSON)
const wsIndex = code.indexOf(`const wss = new WebSocket.Server({ server });`);
const wsEndIndex = code.indexOf(`const oldHandler = server.listeners('request')[0];`);
const wsLogicNew = `const wss = new WebSocket.Server({ server });
wss.on('connection', (ws) => {
  logEvent("Connected via WebSocket");
  ws.on('message', (message, isBinary) => {
    try {
      if (isBinary) {
        const buffer = Buffer.isBuffer(message) ? message : Buffer.from(message);
        const session = voiceSessions.get(ws);
        if (session && !session.processing) {
          session.chunks.push(buffer);
          session.bytes += buffer.length;
        }
        return;
      }
      const text = message.toString();
      
      // Handle legacy browser VOICE:START / VOICE:END
      if (text.startsWith("VOICE:START")) {
        const parts = text.split(":");
        const sampleRate = Math.max(8000, Math.min(24000, Number(parts[2]) || 16000));
        voiceSessions.set(ws, { chunks: [], bytes: 0, sampleRate, startedAt: Date.now(), processing: false });
        latestTelemetry.voice = "listening";
        latestSpeech.voiceStatus = "listening";
        latestSpeech.voiceUpdatedAt = Date.now();
        logEvent(\`voice start \${sampleRate}Hz\`);
        return;
      } else if (text === "VOICE:END") {
        latestTelemetry.voice = "thinking";
        latestSpeech.voiceStatus = "thinking";
        latestSpeech.voiceUpdatedAt = Date.now();
        handleVoiceSession(ws).catch((err) => logEvent(\`voice session err: \${err.message}\`));
        return;
      }

      let parsed;
      try {
        parsed = JSON.parse(text);
      } catch(e) {
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
      } else if (parsed.req_song === 1 || parsed.req_lovestory === 1) {
         if (owiSocket) streamAudioToWS(owiSocket, "lovestory.mp3", "0.28");
      } else if (parsed.req_song === 2) {
         if (owiSocket) streamAudioToWS(owiSocket, "mbg.mp3", "0.32");
      } else if (parsed.req_song === 3) {
         if (owiSocket) streamAudioToWS(owiSocket, "hai_owi.wav", "0.45");
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

`;
code = code.substring(0, wsIndex) + wsLogicNew + code.substring(wsEndIndex);

// Remove the UDP server
code = code.replace(/const udpServer = dgram\.createSocket\('udp4'\);[\s\S]*?udpServer\.bind\(7789\);/, '');
code = code.replace(/udpServer\.bind\(7788\);/, '');

fs.writeFileSync('web_serial_server_patched.js', code);
console.log('Patched');
