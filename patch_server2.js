const fs = require('fs');
let code = fs.readFileSync('web_serial_server.js', 'utf8');

// 1. Add WebSocket requirement at top
code = code.replace(/const dgram = require\('dgram'\);/, "const dgram = require('dgram');\nconst WebSocket = require('ws');\nlet owiSocket = null;");

// 2. Replace UDP initialization with WebSocket Server initialization at the bottom
const udpServerRegex = /const udpServer = dgram\.createSocket\('udp4'\);[\s\S]*?udpServer\.bind\(7788\);/;
const wsServerCode = `
const wss = new WebSocket.Server({ server });
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
code = code.replace(udpServerRegex, wsServerCode);

// 3. Update streamAudio to use WebSocket
const streamAudioRegex = /async function streamAudio[\s\S]*?async function streamTestTone/m;
code = code.replace(streamAudioRegex, `async function streamAudioToWS(ws, mp3Path, volume = '0.30') {
  if (isStreamingAudio) return;
  isStreamingAudio = true;
  logEvent('stream audio ' + mp3Path + ' via WS vol ' + volume);

  try {
    const sampleRate = 16000;
    const safeVolume = clampVolume(volume).toFixed(2);
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

// Replace the TCP streamAudio call inside speakReplyOnBot
code = code.replace(/streamAudio\(latestTelemetry\.ip, volume, ttsFile\);/, 'if(owiSocket && owiSocket.readyState === WebSocket.OPEN) streamAudioToWS(owiSocket, ttsFile, volume);');

// 4. Update sendPacket to send via WebSocket
const sendPacketRegex = /function sendPacket\(buffer, opts = \{\}\) \{[\s\S]*?return new Promise\(\(resolve\) => \{[\s\S]*?\}\);[\s\S]*?\}/m;
code = code.replace(sendPacketRegex, `function sendPacket(buffer, opts = {}) {
  return new Promise((resolve) => {
    if (owiSocket && owiSocket.readyState === WebSocket.OPEN) {
      // Decode the buffer to text because CMD: is text based
      const text = buffer.toString('ascii').trim();
      owiSocket.send("CMD:" + text);
    }
    resolve();
  });
}`);

// 5. Connect UI POST events to streamAudioToWS
// Instead of rewriting the POST handlers, I will just intercept them in the router:
code = code.replace(/if \(req\.method === "POST" && req\.url === "\/play_audio"\) \{[\s\S]*?res\.end\("ok"\);[\s\S]*?\}/, 
`if (req.method === "POST" && req.url === "/play_audio") {
    let body = "";
    req.on("data", chunk => body += chunk.toString());
    req.on("end", () => {
      try {
        const payload = JSON.parse(body);
        if (owiSocket) streamAudioToWS(owiSocket, payload.file, payload.volume);
        res.writeHead(200); res.end("ok");
      } catch(e) {
        res.writeHead(500); res.end(e.message);
      }
    });
    return;
  }`);

fs.writeFileSync('web_serial_server_patched.js', code);
console.log('Patched 2!');
