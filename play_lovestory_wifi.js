const fs = require("fs");
const net = require("net");
const { spawn } = require("child_process");
const ffmpegPath = require("ffmpeg-static");

const host = process.argv[2];
const mp3Path = process.argv[3] || "lovestory.mp3";
const port = Number(process.argv[4] || 7777);
const sampleRate = 16000;
const bytesPerSecond = sampleRate * 2;
const chunkSize = 1024;
const volume = process.env.LOVE_STORY_VOLUME || "0.20";

if (!host) {
  console.error("Pakai: node play_lovestory_wifi.js <IP_OWI> [mp3] [port]");
  process.exit(1);
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function socketWrite(socket, chunk) {
  return new Promise((resolve, reject) => {
    socket.write(chunk, (err) => err ? reject(err) : resolve());
  });
}

async function main() {
  if (!fs.existsSync(mp3Path)) throw new Error(`MP3 tidak ketemu: ${mp3Path}`);

  const socket = net.createConnection({ host, port });
  socket.setNoDelay(true);
  await new Promise((resolve, reject) => {
    socket.once("connect", resolve);
    socket.once("error", reject);
  });
  console.log(`Connected to Owi ${host}:${port}`);

  const ffmpeg = spawn(ffmpegPath, [
    "-hide_banner",
    "-loglevel", "error",
    "-i", mp3Path,
    "-f", "s16le",
    "-acodec", "pcm_s16le",
    "-ac", "1",
    "-ar", String(sampleRate),
    "-filter:a", `highpass=f=80,lowpass=f=7800,acompressor=threshold=-23dB:ratio=2.1:attack=18:release=260,alimiter=limit=0.34,volume=${volume}`,
    "pipe:1",
  ], { stdio: ["ignore", "pipe", "pipe"] });

  let sent = 0;
  let started = Date.now();
  let ffmpegError = "";
  ffmpeg.stderr.on("data", (data) => { ffmpegError += data.toString(); });

  // Send an initial cushion a little faster; ESP will prebuffer before playback.
  const leadBytes = 8192;

  for await (const chunk of ffmpeg.stdout) {
    for (let offset = 0; offset < chunk.length; offset += chunkSize) {
      const slice = chunk.subarray(offset, Math.min(offset + chunkSize, chunk.length));
      await socketWrite(socket, slice);
      sent += slice.length;

      if (sent > leadBytes) {
        const targetMs = ((sent - leadBytes) / bytesPerSecond) * 1000;
        const elapsedMs = Date.now() - started;
        const waitMs = targetMs - elapsedMs;
        if (waitMs > 1) await sleep(Math.min(waitMs, 24));
      } else {
        started = Date.now();
      }
    }
    if (sent % (bytesPerSecond * 5) < chunkSize) {
      console.log(`${Math.floor(sent / bytesPerSecond)}s sent`);
    }
  }

  await new Promise((resolve, reject) => {
    ffmpeg.on("close", (code) => code === 0 ? resolve() : reject(new Error(ffmpegError || `ffmpeg exit ${code}`)));
    ffmpeg.on("error", reject);
  });

  await sleep(500);
  socket.end();
  console.log("Selesai stream WiFi");
}

main().catch((err) => {
  console.error(err.message);
  process.exit(1);
});
