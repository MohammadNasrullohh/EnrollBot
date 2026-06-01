const fs = require("fs");
const { spawn } = require("child_process");
const { SerialPort } = require("serialport");
const ffmpegPath = require("ffmpeg-static");

const portName = process.argv[2] || "COM4";
const mp3Path = process.argv[3] || "lovestory.mp3";
const baudRate = Number(process.env.LOVE_STORY_BAUD || 460800);
const sampleRate = 16000;
const bytesPerSecond = sampleRate * 2;
const chunkSize = 1024;
const nightVolume = process.env.LOVE_STORY_VOLUME || "0.08";

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function writeDrainOnce(port, chunk) {
  return new Promise((resolve, reject) => {
    port.write(chunk, (writeErr) => {
      if (writeErr) return reject(writeErr);
      port.drain((drainErr) => drainErr ? reject(drainErr) : resolve());
    });
  });
}

async function writeDrain(port, chunk) {
  for (let attempt = 0; attempt < 3; attempt++) {
    try {
      await writeDrainOnce(port, chunk);
      return;
    } catch (err) {
      if (attempt === 2) throw err;
      await sleep(35);
    }
  }
}

async function main() {
  if (!fs.existsSync(mp3Path)) {
    throw new Error(`MP3 tidak ketemu: ${mp3Path}`);
  }
  if (!ffmpegPath || !fs.existsSync(ffmpegPath)) {
    throw new Error("ffmpeg-static tidak siap");
  }

  let sent = 0;
  const port = new SerialPort({ path: portName, baudRate, autoOpen: false });
  let portFailed = false;
  port.on("error", (err) => {
    portFailed = true;
    console.error(`Serial error: ${err.message}`);
  });
  port.on("close", () => {
    if (sent > 0) portFailed = true;
  });
  await new Promise((resolve, reject) => port.open((err) => err ? reject(err) : resolve()));
  console.log(`Serial open ${portName} @ ${baudRate}`);
  await new Promise((resolve) => port.flush(() => resolve()));
  await sleep(1800);

  const ffmpeg = spawn(ffmpegPath, [
    "-hide_banner",
    "-loglevel", "error",
    "-i", mp3Path,
    "-f", "s16le",
    "-acodec", "pcm_s16le",
    "-ac", "1",
    "-ar", String(sampleRate),
    "-filter:a", `highpass=f=90,lowpass=f=7800,acompressor=threshold=-24dB:ratio=2.0:attack=24:release=280,alimiter=limit=0.24,volume=${nightVolume}`,
    "pipe:1",
  ], { stdio: ["ignore", "pipe", "pipe"] });

  let started = Date.now();
  let ffmpegError = "";

  ffmpeg.stderr.on("data", (data) => {
    ffmpegError += data.toString();
  });

  try {
    for await (const chunk of ffmpeg.stdout) {
      for (let offset = 0; offset < chunk.length; offset += chunkSize) {
        const slice = chunk.subarray(offset, Math.min(offset + chunkSize, chunk.length));
        if (portFailed || !port.isOpen) throw new Error("Serial terputus saat streaming");
        await writeDrain(port, slice);
        sent += slice.length;

        const targetMs = (sent / bytesPerSecond) * 1000;
        const elapsedMs = Date.now() - started;
        const waitMs = targetMs - elapsedMs;
        if (waitMs > 1) await sleep(Math.min(waitMs, 24));
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
    ffmpeg.on("close", (code) => {
      if (code === 0) resolve();
      else reject(new Error(ffmpegError || `ffmpeg exit ${code}`));
    });
  });

  await sleep(250);
  await new Promise((resolve) => port.close(() => resolve()));
  console.log("Selesai stream MP3 ke MAX");
}

main().catch((err) => {
  console.error(err.message);
  process.exit(1);
});
