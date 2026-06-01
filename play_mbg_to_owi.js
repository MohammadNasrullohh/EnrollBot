const fs = require("fs");
const { spawn } = require("child_process");
const { SerialPort } = require("serialport");
const ffmpegPath = require("ffmpeg-static");

const portName = process.argv[2] || "COM4";
const mp3Path = process.argv[3] || "mbg.mp3";
const baudRate = 921600;
const sampleRate = 22050;
const bytesPerSecond = sampleRate * 1;
const chunkSize = 768;

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
  if (!fs.existsSync(mp3Path)) throw new Error(`MP3 tidak ketemu: ${mp3Path}`);
  if (!ffmpegPath || !fs.existsSync(ffmpegPath)) throw new Error("ffmpeg-static tidak siap");

  const port = new SerialPort({ path: portName, baudRate, autoOpen: false });
  await new Promise((resolve, reject) => port.open((err) => err ? reject(err) : resolve()));
  console.log(`Owi serial open ${portName} @ ${baudRate}`);
  await sleep(1300);

  const ffmpeg = spawn(ffmpegPath, [
    "-hide_banner",
    "-loglevel", "error",
    "-i", mp3Path,
    "-f", "u8",
    "-acodec", "pcm_u8",
    "-ac", "1",
    "-ar", String(sampleRate),
    "-filter:a", "highpass=f=80,lowpass=f=11000,acompressor=threshold=-18dB:ratio=2.5:attack=12:release=180,alimiter=limit=0.55,volume=0.42",
    "pipe:1",
  ], { stdio: ["ignore", "pipe", "pipe"] });

  let sent = 0;
  const started = Date.now();
  let ffmpegError = "";

  ffmpeg.stderr.on("data", (data) => { ffmpegError += data.toString(); });

  try {
    for await (const chunk of ffmpeg.stdout) {
      for (let offset = 0; offset < chunk.length; offset += chunkSize) {
        const slice = chunk.subarray(offset, Math.min(offset + chunkSize, chunk.length));
        await writeDrain(port, slice);
        sent += slice.length;

        const targetMs = (sent / bytesPerSecond) * 1000;
        const elapsedMs = Date.now() - started;
        const waitMs = targetMs - elapsedMs;
        if (waitMs > 1) await sleep(Math.min(waitMs, 18));
      }
      if (sent % (bytesPerSecond * 5) < chunkSize) {
        console.log(`${Math.floor(sent / bytesPerSecond)}s streamed`);
      }
    }
  } catch (err) {
    ffmpeg.kill("SIGKILL");
    port.close();
    console.error(err.message);
    process.exit(1);
  }

  await new Promise((resolve, reject) => {
    if (ffmpeg.killed || ffmpeg.exitCode !== null) {
      if (ffmpeg.exitCode === 0 || ffmpeg.exitCode === null) resolve();
      else reject(new Error(ffmpegError || `ffmpeg exit ${ffmpeg.exitCode}`));
      return;
    }
    ffmpeg.on("error", reject);
    ffmpeg.on("close", (code) => code === 0 ? resolve() : reject(new Error(ffmpegError || `ffmpeg exit ${code}`)));
  });

  await sleep(250);
  await new Promise((resolve) => port.close(() => resolve()));
  console.log("Selesai stream mbg.mp3 ke Owi");
}

main().catch((err) => {
  console.error(err.message);
  process.exit(1);
});
