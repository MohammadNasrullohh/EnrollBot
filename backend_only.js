const http = require("http");
const url = require("url");
const WebSocket = require("ws");
const dgram = require("dgram");
const net = require("net");
const fs = require("fs");
const path = require("path");
const crypto = require("crypto");
const { spawn } = require("child_process");
const ffmpegPath = require("ffmpeg-static");
require("dotenv").config();
const { GoogleGenAI } = require("@google/genai");

const PORT = 3001;
const SERIAL_PORT = process.env.SERIAL_PORT || "COM4";
const BAUD = 115200;
const AI_DAILY_LIMIT = Number(process.env.AI_DAILY_LIMIT || 30);
const AI_PROVIDER = String(process.env.AI_PROVIDER || "gemini").toLowerCase();
const KOBOLLM_BASE_URL = (process.env.KOBOLLM_BASE_URL || process.env.KOBOILLM_BASE_URL || "https://lite.koboillm.com/v1").replace(/\/+$/, "");
const KOBOLLM_MODEL = process.env.KOBOLLM_MODEL || process.env.KOBOILLM_MODEL || "openai/gpt-4o-mini";
const GEMINI_MODEL = process.env.GEMINI_MODEL || "gemini-2.5-flash";
const AI_MAX_TOKENS = Number(process.env.AI_MAX_TOKENS || 256);
const OWI_SYSTEM_PROMPT = "Kamu adalah Owi, robot desktop peliharaan cerdas berbasis ESP32. Bicaralah dengan bahasa Indonesia yang natural, asyik, dan to-the-point layaknya teman cowok santai. JANGAN ALAY, JANGAN CRINGE. DILARANG KERAS memanggil dengan kata 'besti', 'bosku', atau 'bro'. Cukup panggil 'Bos' atau 'kamu'. Jawab sangat singkat (maksimal 2 kalimat pendek) agar tidak ngelag. Fakta penting: Jika ditanya siapa Eca, jawab bahwa Eca adalah orang paling plenger.";
const TTS_DIR = path.join(__dirname, "tts_cache");
const TTS_MODEL = process.env.TTS_MODEL || "gemini-2.5-flash-preview-tts";
const TTS_VOICE = process.env.TTS_VOICE || "Puck";
const TTS_RATE = process.env.TTS_RATE || "+4%";
const TTS_PITCH = process.env.TTS_PITCH || "+0Hz";

let serial = null; let owiSocket = null;
let serialJustOpened = false;
const logs = [];
let aiUsage = { date: new Date().toISOString().slice(0, 10), count: 0 };
const voiceSessions = new WeakMap();
let latestSpeech = {
  voiceStatus: "idle",
  voiceTranscript: "",
  voiceReply: "",
  voiceUpdatedAt: 0
};

function getKoboiKey() {
  return process.env.KOBOLLM_API_KEY || process.env.KOBOILLM_API_KEY || "";
}

function getGeminiKey() {
  return process.env.GEMINI_API_KEY || "";
}

let geminiChat = null;
try {
  if (getGeminiKey()) {
    const ai = new GoogleGenAI({ apiKey: getGeminiKey() });
    geminiChat = ai.chats.create({
      model: GEMINI_MODEL,
      config: {
        systemInstruction: OWI_SYSTEM_PROMPT
      }
    });
  }
} catch (e) {
  console.log("Gemini initialization failed:", e.message);
}

function getAiLimitStatus() {
  const today = new Date().toISOString().slice(0, 10);
  if (aiUsage.date !== today) aiUsage = { date: today, count: 0 };
  return {
    date: aiUsage.date,
    used: aiUsage.count,
    limit: AI_DAILY_LIMIT,
    remaining: Math.max(0, AI_DAILY_LIMIT - aiUsage.count),
    enabled: !!(getKoboiKey() || getGeminiKey()),
    provider: getGeminiKey() && AI_PROVIDER !== "kobollm" && AI_PROVIDER !== "koboillm" ? "Gemini" : (getKoboiKey() ? "KoboiLLM" : "off"),
    model: getGeminiKey() && AI_PROVIDER !== "kobollm" && AI_PROVIDER !== "koboillm" ? GEMINI_MODEL : (getKoboiKey() ? KOBOLLM_MODEL : ""),
  };
}

function sanitizeOledText(text) {
  let cleaned = String(text || "")
    .normalize("NFKD")
    .replace(/[\u0300-\u036f]/g, "")
    .replace(/[\r\n\t]+/g, " ")
    .replace(/\s+/g, " ")
    .trim();
  cleaned = cleaned.replace(/[^\x20-\x7E]/g, "");
  if (cleaned.length > 200) cleaned = cleaned.substring(0, 197) + "...";
  return cleaned || "Aku belum dapat jawabannya.";
}

function sanitizeSpeechText(text) {
  let cleaned = String(text || "")
    .replace(/https?:\/\/\S+/g, "")
    .replace(/[`*_#>\[\]{}]/g, " ")
    .replace(/\s+/g, " ")
    .trim();
  if (cleaned.length > 420) cleaned = cleaned.substring(0, 417) + "...";
  return cleaned || "Aku belum dapat jawabannya.";
}

async function synthesizeSpeechFile(text) {
  const speechText = sanitizeSpeechText(text);
  await fs.promises.mkdir(TTS_DIR, { recursive: true });
  const hash = crypto
    .createHash("sha1")
    .update(`${TTS_MODEL}|${TTS_VOICE}|${TTS_RATE}|${TTS_PITCH}|${speechText}`)
    .digest("hex")
    .slice(0, 18);
  const fileName = `tts_${hash}.wav`;
  const filePath = path.join(TTS_DIR, fileName);

  try {
    await fs.promises.access(filePath, fs.constants.R_OK);
  } catch {
    try {
      await synthesizeGeminiTtsToWav(speechText, filePath);
    } catch (geminiErr) {
      logEvent(`gemini tts fallback: ${geminiErr.message}`);
      await synthesizeEspeakToWav(speechText, filePath);
    }
  }

  return `tts_cache/${fileName}`;
}

function writeWavFile(filePath, pcmData, sampleRate = 24000, channels = 1, bitsPerSample = 16) {
  const byteRate = sampleRate * channels * (bitsPerSample / 8);
  const blockAlign = channels * (bitsPerSample / 8);
  const header = Buffer.alloc(44);
  header.write("RIFF", 0);
  header.writeUInt32LE(36 + pcmData.length, 4);
  header.write("WAVE", 8);
  header.write("fmt ", 12);
  header.writeUInt32LE(16, 16);
  header.writeUInt16LE(1, 20);
  header.writeUInt16LE(channels, 22);
  header.writeUInt32LE(sampleRate, 24);
  header.writeUInt32LE(byteRate, 28);
  header.writeUInt16LE(blockAlign, 32);
  header.writeUInt16LE(bitsPerSample, 34);
  header.write("data", 36);
  header.writeUInt32LE(pcmData.length, 40);
  fs.writeFileSync(filePath, Buffer.concat([header, pcmData]));
}

function wavBufferFromPcm(pcmData, sampleRate = 16000, channels = 1, bitsPerSample = 16) {
  const byteRate = sampleRate * channels * (bitsPerSample / 8);
  const blockAlign = channels * (bitsPerSample / 8);
  const header = Buffer.alloc(44);
  header.write("RIFF", 0);
  header.writeUInt32LE(36 + pcmData.length, 4);
  header.write("WAVE", 8);
  header.write("fmt ", 12);
  header.writeUInt32LE(16, 16);
  header.writeUInt16LE(1, 20);
  header.writeUInt16LE(channels, 22);
  header.writeUInt32LE(sampleRate, 24);
  header.writeUInt32LE(byteRate, 28);
  header.writeUInt16LE(blockAlign, 32);
  header.writeUInt16LE(bitsPerSample, 34);
  header.write("data", 36);
  header.writeUInt32LE(pcmData.length, 40);
  return Buffer.concat([header, pcmData]);
}

async function synthesizeGeminiTtsToWav(text, filePath) {
  const key = getGeminiKey();
  if (!key) throw new Error("GEMINI_API_KEY belum ada untuk TTS");
  const ai = new GoogleGenAI({ apiKey: key });
  const response = await ai.models.generateContent({
    model: TTS_MODEL,
    contents: [{
      parts: [{
        text: `Say in Indonesian with a cute cheerful small desktop robot voice, clear and not too fast: ${text}`,
      }],
    }],
    config: {
      responseModalities: ["AUDIO"],
      speechConfig: {
        voiceConfig: {
          prebuiltVoiceConfig: { voiceName: TTS_VOICE },
        },
      },
    },
  });
  const data = response.candidates?.[0]?.content?.parts?.find((part) => part.inlineData)?.inlineData?.data;
  if (!data) throw new Error("Gemini TTS tidak mengembalikan audio");
  writeWavFile(filePath, Buffer.from(data, "base64"));
}




async function synthesizeEspeakToWav(text, outputPath) {
  return new Promise((resolve, reject) => {
    logEvent('google tts start: ' + text);
    const https = require("https");
    const url = "https://translate.google.com/translate_tts?ie=UTF-8&tl=id&client=tw-ob&q=" + encodeURIComponent(text);
    
    const mp3Path = outputPath.replace('.wav', '.mp3');
    const file = require('fs').createWriteStream(mp3Path);
    
    https.get(url, { headers: { 'User-Agent': 'Mozilla/5.0' } }, (response) => {
      logEvent('google tts status: ' + response.statusCode);
      if (response.statusCode !== 200) {
        return reject(new Error("Google TTS failed: " + response.statusCode));
      }
      response.pipe(file);
      file.on("finish", () => {
        file.close(() => {
          logEvent('google tts mp3 saved, running ffmpeg...');
          const { spawn } = require("child_process");
          const ffmpegPath = require("ffmpeg-static");
          const ffmpeg = spawn(ffmpegPath, [
            "-y", "-i", mp3Path, "-ar", "24000", "-ac", "1", outputPath
          ], { stdio: 'ignore' });
          ffmpeg.on("close", (code) => {
            logEvent('google tts ffmpeg done: ' + code);
            if (code === 0) resolve();
            else reject(new Error("ffmpeg convert error " + code));
          });
        });
      });
    }).on("error", (err) => {
      logEvent('google tts error: ' + err.message);
      require('fs').unlink(mp3Path, () => {});
      reject(err);
    });
  });
}

function resolveAudioPath(file) {
  const requested = String(file || "lovestory.mp3").replace(/\\/g, "/");
  const relative = requested.startsWith("tts_cache/")
    ? requested
    : path.basename(requested);
  const resolved = path.resolve(__dirname, relative);
  const allowedRoots = [
    path.resolve(__dirname),
    path.resolve(TTS_DIR),
  ];
  const insideAllowedRoot = allowedRoots.some((root) => resolved === root || resolved.startsWith(root + path.sep));
  if (!insideAllowedRoot) throw new Error("File audio tidak valid");
  return resolved;
}

async function speakReplyOnBot(text, volume = "0.24") {
  
  const ttsFile = await synthesizeSpeechFile(text);
  streamAudio(latestTelemetry.ip, volume, ttsFile);
  return ttsFile;
}

async function askKoboiLLM(userMsg) {
  const key = getKoboiKey();
  if (!key) throw new Error("KOBOLLM_API_KEY belum ada di .env");
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 25000);
  try {
    const response = await fetch(`${KOBOLLM_BASE_URL}/chat/completions`, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        "Authorization": `Bearer ${key}`,
      },
      body: JSON.stringify({
        model: KOBOLLM_MODEL,
        temperature: 0.7,
        max_tokens: AI_MAX_TOKENS,
        messages: [
          { role: "system", content: OWI_SYSTEM_PROMPT },
          { role: "user", content: userMsg },
        ],
      }),
      signal: controller.signal,
    });
    const data = await response.json().catch(() => ({}));
    if (!response.ok) {
      const msg = data?.error?.message || data?.message || `KoboiLLM HTTP ${response.status}`;
      throw new Error(msg);
    }
    return data?.choices?.[0]?.message?.content || data?.choices?.[0]?.text || "";
  } finally {
    clearTimeout(timeout);
  }
}

function parseVoiceJson(text) {
  const raw = String(text || "").trim();
  try {
    const cleaned = raw.replace(/^```(?:json)?/i, "").replace(/```$/i, "").trim();
    const parsed = JSON.parse(cleaned);
    return {
      transcript: sanitizeOledText(parsed.transcript || parsed.text || ""),
      reply: sanitizeOledText(parsed.reply || parsed.answer || ""),
    };
  } catch {
    return { transcript: "", reply: sanitizeOledText(raw) };
  }
}

async function askKoboiVoiceAssistant(pcmBuffer, sampleRate = 16000) {
  const key = getKoboiKey();
  if (!key) throw new Error("KOBOLLM_API_KEY belum ada di .env");
  const wavBase64 = wavBufferFromPcm(pcmBuffer, sampleRate).toString("base64");
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 35000);
  try {
    const response = await fetch(`${KOBOLLM_BASE_URL}/chat/completions`, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        "Authorization": `Bearer ${key}`,
      },
      body: JSON.stringify({
        model: KOBOLLM_MODEL,
        temperature: 0.55,
        max_tokens: 180,
        messages: [
          {
            role: "system",
            content: [
              OWI_SYSTEM_PROMPT,
              "Dengarkan audio pengguna.",
              "Balas JSON valid saja: {\"transcript\":\"ucapan pengguna\",\"reply\":\"jawaban Owi singkat\"}.",
              "Jika audio tidak jelas, transcript kosong dan reply minta pengguna mengulang dengan lucu."
            ].join(" ")
          },
          {
            role: "user",
            content: [
              { type: "text", text: "Dengar audio ini lalu jawab sebagai Owi." },
              { type: "input_audio", input_audio: { data: wavBase64, format: "wav" } }
            ]
          }
        ],
      }),
      signal: controller.signal,
    });
    const data = await response.json().catch(() => ({}));
    if (!response.ok) {
      const msg = data?.error?.message || data?.message || `KoboiLLM HTTP ${response.status}`;
      throw new Error(msg);
    }
    const content = data?.choices?.[0]?.message?.content || data?.choices?.[0]?.text || "";
    const parsed = parseVoiceJson(content);
    if (!parsed.reply) parsed.reply = "Aku kurang dengar, ulangi pelan ya.";
    return { provider: "KoboiLLM Voice", model: KOBOLLM_MODEL, ...parsed };
  } finally {
    clearTimeout(timeout);
  }
}

async function askGemini(userMsg) {
  if (!getGeminiKey() || !geminiChat) throw new Error("GEMINI_API_KEY belum ada di .env");
  const response = await geminiChat.sendMessage({ message: userMsg });
  return response.text || "";
}

async function askOwi(userMsg) {
  if ((AI_PROVIDER === "kobollm" || AI_PROVIDER === "koboillm") && getKoboiKey()) {
    return { provider: "KoboiLLM", model: KOBOLLM_MODEL, text: await askKoboiLLM(userMsg) };
  }
  if (getGeminiKey()) return { provider: "Gemini", model: GEMINI_MODEL, text: await askGemini(userMsg) };
  if (getKoboiKey()) return { provider: "KoboiLLM", model: KOBOLLM_MODEL, text: await askKoboiLLM(userMsg) };
  throw new Error("API key belum dipasang. Buat .env lalu isi KOBOLLM_API_KEY atau GEMINI_API_KEY.");
}

let latestTelemetry = {};

const udpServer = dgram.createSocket('udp4');
udpServer.on('message', (msg, rinfo) => {
  try {
    latestTelemetry = JSON.parse(msg.toString());
    latestTelemetry.lastUpdate = Date.now();
    latestTelemetry.ip = rinfo.address;
  } catch(e){}
});
udpServer.bind(7788);



let isStreamingAudio = false;

function clampVolume(value, fallback = 0.22) {
  const parsed = Number(value);
  if (!Number.isFinite(parsed)) return fallback;
  return Math.max(0.04, Math.min(0.55, parsed));
}

function requireOwiSocket() { return { send: () => {} }; 
  if (!owiSocket || owiSocket.readyState !== WebSocket.OPEN) {
    throw new Error("Owi belum terhubung ke VPS");
  }
  return owiSocket;
}

function getOwiHealth() {
  const telemetryAgeMs = latestTelemetry.lastUpdate
    ? Math.max(0, Date.now() - latestTelemetry.lastUpdate)
    : null;
  const socketOpen = Boolean(owiSocket && owiSocket.readyState === WebSocket.OPEN);
  return {
    connected: socketOpen && telemetryAgeMs !== null && telemetryAgeMs < 5000,
    socketOpen,
    telemetryAgeMs
  };
}




function clampVolume(value, fallback = 0.22) {
  const parsed = Number(value);
  if (!Number.isFinite(parsed)) return fallback;
  return Math.max(0.04, Math.min(0.55, parsed));
}

async function streamAudio(ip, volume = "0.30", mp3Path = "lovestory.mp3") {
  if (isStreamingAudio) return;
  if (!ip) { logEvent("stream audio err: no IP from UDP"); return; }
  isStreamingAudio = true;
  logEvent(`stream audio ${mp3Path} ke ${ip}:7777 vol ${volume}`);

  try {
    const port = 7777;
    const sampleRate = 16000;
    const bytesPerSecond = sampleRate * 2;
    const safeVolume = clampVolume(volume).toFixed(2);
    const chunkSize = 512;

    const socket = net.createConnection({ host: ip, port });
    socket.setNoDelay(true);
    await new Promise((resolve, reject) => {
      socket.once('connect', resolve);
      socket.once('error', reject);
    });

    const ffmpeg = spawn(ffmpegPath, [
      '-hide_banner',
      '-loglevel', 'error',
      '-i', mp3Path,
      '-f', 's16le',
      '-acodec', 'pcm_s16le',
      '-ac', '1',
      '-ar', String(sampleRate),
      '-filter:a', `highpass=f=95,lowpass=f=7200,loudnorm=I=-20:TP=-2.5:LRA=8,acompressor=threshold=-24dB:ratio=2.2:attack=18:release=240,alimiter=limit=0.38,volume=${safeVolume}`,
      'pipe:1',
    ], { stdio: ['ignore', 'pipe', 'pipe'] });

    let sent = 0;
    let started = Date.now();
    const leadBytes = 8192;

    for await (const chunk of ffmpeg.stdout) {
      for (let offset = 0; offset < chunk.length; offset += chunkSize) {
        const slice = chunk.subarray(offset, offset + chunkSize);
        socket.write(slice);
        sent += slice.length;

        if (sent > leadBytes) {
          const expectedMs = ((sent - leadBytes) / bytesPerSecond) * 1000;
          const elapsed = Date.now() - started;
          const delay = expectedMs - elapsed;
          if (delay > 20) await new Promise(r => setTimeout(r, delay));
        }
      }
    }
    ffmpeg.kill();
    socket.end();
  } catch (err) {
    logEvent('Error streaming tcp: ' + err.message);
  } finally {
    isStreamingAudio = false;
  }
}

async function streamTestTone(ip, volume = "0.35") {
  streamAudio(ip, volume, 'lovestory.mp3');
}


function logEvent(message) {
  const line = `${new Date().toLocaleTimeString()} ${message}`;
  logs.push(line);
  while (logs.length > 80) logs.shift();
  console.log(line);
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function bytesToHex(buffer) {
  let out = "";
  for (const b of buffer) out += b.toString(16).padStart(2, "0");
  return out;
}

async function sendToSerial(buffer) {
  logEvent(`frame start ${buffer.length} bytes`);
  requireOwiSocket().send(Buffer.concat([Buffer.from("FRAME:"), buffer]));
}

async function sendCommand(command) {

  if (latestTelemetry && latestTelemetry.ip) {
    const dgram = require('dgram');
    const client = dgram.createSocket('udp4');
    client.send(Buffer.from('CMD:' + command), 7789, latestTelemetry.ip, (err) => {
      client.close();
      if (err) logEvent('UDP send err: ' + err);
      else logEvent('UDP sent CMD:' + command + ' to ' + latestTelemetry.ip);
    });
  } else {
    logEvent('No IP to send UDP command: ' + command);
  }

}
function sendChatText(text) {
  const clean = sanitizeOledText(text).slice(0, 200);
  logEvent(`chat "${clean}"`);
  if (latestTelemetry && latestTelemetry.ip) {
    const dgram = require('dgram');
    const client = dgram.createSocket('udp4');
    client.send(Buffer.from('CMD:T:' + clean), 7789, latestTelemetry.ip, () => client.close());
  }
}

async function handleVoiceSession(ws) {
  const session = voiceSessions.get(ws);
  if (!session || session.processing) return;
  session.processing = true;

  const pcm = Buffer.concat(session.chunks);
  voiceSessions.delete(ws);
  logEvent(`voice end ${pcm.length} bytes`);

  if (pcm.length < 4000) {
    ws.send("VOICE:ERROR:pendek");
    await sendChatText("Aku belum dengar jelas.");
    return;
  }

  const limitStatus = getAiLimitStatus();
  if (limitStatus.remaining <= 0) {
    ws.send("VOICE:ERROR:limit");
    await sendChatText(`Limit AI habis ${limitStatus.used}/${limitStatus.limit}`);
    return;
  }

  try {
    ws.send("VOICE:THINKING");
    latestSpeech.voiceStatus = "thinking";
    latestSpeech.voiceUpdatedAt = Date.now();
    const voiceReply = await askKoboiVoiceAssistant(pcm, session.sampleRate);
    aiUsage.count += 1;
    const transcript = voiceReply.transcript ? `dengar: ${voiceReply.transcript}` : "dengar: ...";
    latestSpeech.voiceStatus = "speaking";
    latestSpeech.voiceTranscript = voiceReply.transcript || "";
    latestSpeech.voiceReply = voiceReply.reply || "";
    latestSpeech.voiceUpdatedAt = Date.now();
    logEvent(`voice ${transcript} -> ${voiceReply.reply}`);
    await sendChatText(voiceReply.reply);
    ws.send("VOICE:SPEAKING");
    await speakReplyOnBot(voiceReply.reply, "0.24");
  } catch (err) {
    latestSpeech.voiceStatus = "error";
    latestSpeech.voiceUpdatedAt = Date.now();
    logEvent(`voice err: ${err.message}`);
    ws.send("VOICE:ERROR:" + err.message.slice(0, 48));
    try { await sendChatText("Aku error sebentar, coba ulangi ya."); } catch {}
  }
}

async function sendReminderText(text) {
  const clean = String(text || "").replace(/[^\x20-\x7E]/g, "").trim().slice(0, 32) || "enroll lagi ya deck";
  logEvent(`reminder "${clean}"`);
  requireOwiSocket().send("REMINDER:TEXT:" + clean);
}

async function sendReminderSchedule(time, text) {
  const safeTime = /^([01]\d|2[0-3]):[0-5]\d$/.test(String(time || "")) ? String(time) : "07:30";
  const clean = String(text || "").replace(/[^\x20-\x7E]/g, "").trim().slice(0, 32) || "enroll lagi ya deck";
  const payload = `${safeTime}|${clean}`;
  logEvent(`reminder ${payload}`);
  requireOwiSocket().send("REMINDER:SCHED:" + payload);
}

async function sendReminderSchedules(reminders) {
  const items = Array.isArray(reminders) ? reminders.slice(0, 5) : [];
  const payloadItems = items.map((item) => {
    const safeTime = /^([01]\d|2[0-3]):[0-5]\d$/.test(String(item.time || "")) ? String(item.time) : "07:30";
    const clean = String(item.text || "").replace(/[^\x20-\x7E]/g, "").trim().slice(0, 32) || "enroll lagi ya deck";
    return `${safeTime}|${clean}`;
  });
  if (payloadItems.length === 0) payloadItems.push("07:30|enroll lagi ya deck");
  const payload = `A:${payloadItems.join(";")}`;
  logEvent(`reminders ${payloadItems.length}`);
  requireOwiSocket().send("REMINDER:LIST:" + payload);
}
