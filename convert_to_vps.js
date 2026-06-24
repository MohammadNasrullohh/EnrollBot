const fs = require('fs');

let code = fs.readFileSync('vps_server.js', 'utf8');

// 1. Add WebSocket import
code = code.replace(/const http = require\("http"\);/, 'const http = require("http");\nconst WebSocket = require("ws");');

// 2. Remove Serialport imports/variables
code = code.replace(/let serial = null;/g, 'let serial = null; let owiSocket = null;');

// 3. Replace sendCommand
code = code.replace(/async function sendCommand\(command\) \{[\s\S]*?logEvent\(\`cmd \$\{command\}\`\);[\s\S]*?openDelay: 40 \}\);\n\}/, `async function sendCommand(command) {
  const allowed = new Set(["C", "M", "R", "T", "G", "F", "P", "O", "D", "E", "L", "W", "1", "2"]);
  if (!allowed.has(command)) throw new Error("Command tidak valid");
  logEvent(\`cmd \${command}\`);
  if (owiSocket) owiSocket.send("CMD:" + command);
  else throw new Error("Owi belum konek ke WebSocket VPS");
}`);

// 4. Replace sendToSerial
code = code.replace(/async function sendToSerial\(buffer\) \{[\s\S]*?openDelay: 180 \}\);\n  \}\n\}/, `async function sendToSerial(buffer) {
  logEvent(\`frame start \${buffer.length} bytes\`);
  if (owiSocket) owiSocket.send(Buffer.concat([Buffer.from("FRAME:"), buffer]));
  else throw new Error("Owi belum konek ke WebSocket VPS");
}`);

// 5. Replace Reminder functions
code = code.replace(/async function sendReminderText\(text\) \{[\s\S]*?openDelay: 40 \}\);\n\}/, `async function sendReminderText(text) {
  const clean = String(text || "").replace(/[^\\x20-\\x7E]/g, "").trim().slice(0, 32) || "enroll lagi ya deck";
  logEvent(\`reminder "\${clean}"\`);
  if (owiSocket) owiSocket.send("REMINDER:TEXT:" + clean);
}`);

code = code.replace(/async function sendReminderSchedule\(time, text\) \{[\s\S]*?openDelay: 40 \}\);\n\}/, `async function sendReminderSchedule(time, text) {
  const safeTime = /^([01]\\d|2[0-3]):[0-5]\\d$/.test(String(time || "")) ? String(time) : "07:30";
  const clean = String(text || "").replace(/[^\\x20-\\x7E]/g, "").trim().slice(0, 32) || "enroll lagi ya deck";
  const payload = \`\${safeTime}|\${clean}\`;
  logEvent(\`reminder \${payload}\`);
  if (owiSocket) owiSocket.send("REMINDER:SCHED:" + payload);
}`);

code = code.replace(/async function sendReminderSchedules\(reminders\) \{[\s\S]*?openDelay: 40 \}\);\n\}/, `async function sendReminderSchedules(reminders) {
  const items = Array.isArray(reminders) ? reminders.slice(0, 5) : [];
  const payloadItems = items.map((item) => {
    const safeTime = /^([01]\\d|2[0-3]):[0-5]\\d$/.test(String(item.time || "")) ? String(item.time) : "07:30";
    const clean = String(item.text || "").replace(/[^\\x20-\\x7E]/g, "").trim().slice(0, 32) || "enroll lagi ya deck";
    return \`\${safeTime}|\${clean}\`;
  });
  if (payloadItems.length === 0) payloadItems.push("07:30|enroll lagi ya deck");
  const payload = \`A:\${payloadItems.join(";")}\`;
  logEvent(\`reminders \${payloadItems.length}\`);
  if (owiSocket) owiSocket.send("REMINDER:LIST:" + payload);
}`);

// 6. Replace streamAudio and testTone to just send WebSocket commands
code = code.replace(/async function streamAudio\(ip, volume = "0\.30", mp3Path = "lovestory\.mp3"\) \{[\s\S]*?stream audio selesai"\);\n  \}\n\}/, `async function streamAudio(ip, volume = "0.30", mp3Path = "lovestory.mp3") {
  logEvent(\`Request audio \${mp3Path} vol \${volume}\`);
  if (owiSocket) owiSocket.send("AUDIO:" + mp3Path + ":" + clampVolume(volume).toFixed(2));
}`);

code = code.replace(/async function streamTestTone\(ip, volume = "0\.35"\) \{[\s\S]*?test MAX selesai"\);\n  \}\n\}/, `async function streamTestTone(ip, volume = "0.35") {
  const safeVolume = clampVolume(volume, 0.35);
  logEvent(\`Request test tone vol \${safeVolume.toFixed(2)}\`);
  if (owiSocket) owiSocket.send("AUDIO:TEST:" + safeVolume.toFixed(2));
}`);

// 7. Add WebSocket Server attachment at the bottom
code = code.replace(/server\.listen\(PORT, \(\) => \{/, `const wss = new WebSocket.Server({ server });
wss.on('connection', (ws) => {
  logEvent("OwiBot Connected via WebSocket");
  owiSocket = ws;
  ws.on('message', (message) => {
    try {
      const text = message.toString();
      if (text.startsWith("{")) {
        latestTelemetry = JSON.parse(text);
        latestTelemetry.lastUpdate = Date.now();
        if (latestTelemetry.req_song === 1 || latestTelemetry.req_lovestory === 1) {
          if (owiSocket) owiSocket.send("AUDIO:lovestory.mp3:0.28");
        } else if (latestTelemetry.req_song === 2) {
          if (owiSocket) owiSocket.send("AUDIO:mbg.mp3:0.32");
        } else if (latestTelemetry.req_song === 3) {
          if (owiSocket) owiSocket.send("AUDIO:hai_owi.wav:0.45");
        }
      }
    } catch(e) {}
  });
  ws.on('close', () => {
    logEvent("OwiBot Disconnected");
    if (owiSocket === ws) owiSocket = null;
  });
});

// New Endpoint for Audio HTTP Streaming
const url = require('url');
const oldHandler = server.listeners('request')[0];
server.removeAllListeners('request');
server.on('request', (req, res) => {
  const parsedUrl = url.parse(req.url, true);
  if (req.method === "GET" && parsedUrl.pathname === "/stream") {
    const file = parsedUrl.query.file || "lovestory.mp3";
    const vol = parsedUrl.query.vol || "0.30";
    const isTest = file === "TEST";
    
    res.writeHead(200, {
      "Content-Type": "audio/x-raw",
      "Transfer-Encoding": "chunked"
    });
    
    if (isTest) {
      // Stream test tone 880Hz
      const sampleRate = 16000;
      const durationMs = 1800;
      const frequency = 880;
      const frames = Math.floor(sampleRate * durationMs / 1000);
      const safeVolume = clampVolume(vol, 0.35);
      const chunk = Buffer.alloc(frames * 2);
      for (let i = 0; i < frames; i++) {
        const t = i / sampleRate;
        const envelope = Math.min(1, Math.min(i / 1200, (frames - i) / 1200));
        const sample = Math.round(Math.sin(2 * Math.PI * frequency * t) * 26000 * safeVolume * envelope);
        chunk.writeInt16LE(sample, i * 2);
      }
      res.write(chunk);
      setTimeout(() => res.end(), 2000);
    } else {
      const ffmpeg = spawn(ffmpegPath, [
        "-hide_banner",
        "-loglevel", "error",
        "-i", file,
        "-f", "s16le",
        "-acodec", "pcm_s16le",
        "-ac", "1",
        "-ar", "16000",
        "-filter:a", \`highpass=f=95,lowpass=f=7200,loudnorm=I=-20:TP=-2.5:LRA=8,acompressor=threshold=-24dB:ratio=2.2:attack=18:release=240,alimiter=limit=0.38,volume=\${clampVolume(vol).toFixed(2)}\`,
        "pipe:1"
      ], { stdio: ["ignore", "pipe", "ignore"] });
      ffmpeg.stdout.pipe(res);
    }
    return;
  }
  oldHandler(req, res);
});

server.listen(PORT, () => {`);

fs.writeFileSync('vps_server.js', code);
console.log("Converted vps_server.js successfully!");
