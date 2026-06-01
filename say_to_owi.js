const fs = require("fs");
const { spawn } = require("child_process");
const { SerialPort } = require("serialport");
const ffmpegPath = require("ffmpeg-static");

const portName = process.argv[2] || "COM4";
const audioPath = process.argv[3] || "hai_owi.wav";
const baudRate = Number(process.env.OWI_SPEAK_BAUD || 460800);
const sampleRate = 16000;
const bytesPerSecond = sampleRate * 2;
const chunkSize = 1024;

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function writeDrain(port, chunk) {
  return new Promise((resolve, reject) => {
    port.write(chunk, (writeErr) => {
      if (writeErr) return reject(writeErr);
      port.drain((drainErr) => drainErr ? reject(drainErr) : resolve());
    });
  });
}

async function main() {
  if (!fs.existsSync(audioPath)) throw new Error(`Audio tidak ketemu: ${audioPath}`);

  const port = new SerialPort({ path: portName, baudRate, autoOpen: false });
  await new Promise((resolve, reject) => port.open((err) => err ? reject(err) : resolve()));
  console.log(`Owi serial open ${portName}`);
  await sleep(1800);

  const ffmpeg = spawn(ffmpegPath, [
    "-hide_banner",
    "-loglevel", "error",
    "-i", audioPath,
    "-f", "s16le",
    "-acodec", "pcm_s16le",
    "-ac", "1",
    "-ar", String(sampleRate),
    "-filter:a", "highpass=f=120,lowpass=f=6800,alimiter=limit=0.30,volume=0.22",
    "pipe:1",
  ], { stdio: ["ignore", "pipe", "pipe"] });

  let sent = 0;
  const started = Date.now();
  let ffmpegError = "";
  ffmpeg.stderr.on("data", (data) => { ffmpegError += data.toString(); });

  for await (const chunk of ffmpeg.stdout) {
    for (let offset = 0; offset < chunk.length; offset += chunkSize) {
      const slice = chunk.subarray(offset, Math.min(offset + chunkSize, chunk.length));
      await writeDrain(port, slice);
      sent += slice.length;
      const waitMs = (sent / bytesPerSecond) * 1000 - (Date.now() - started);
      if (waitMs > 1) await sleep(Math.min(waitMs, 16));
    }
  }

  await new Promise((resolve, reject) => {
    ffmpeg.on("close", (code) => code === 0 ? resolve() : reject(new Error(ffmpegError || `ffmpeg exit ${code}`)));
    ffmpeg.on("error", reject);
  });

  await sleep(1600);
  await new Promise((resolve) => port.close(() => resolve()));
  console.log("Selesai ngomong");
}

main().catch((err) => {
  console.error(err.message);
  process.exit(1);
});
