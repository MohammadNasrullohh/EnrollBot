const fs = require("fs");
const { execFileSync } = require("child_process");
const path = require("path");
const { spawn } = require("child_process");
const { SerialPort } = require("serialport");
const ffmpegPath = require("ffmpeg-static");

const portName = process.argv[2] || "COM4";
const text = process.argv.slice(3).join(" ") || "hai, aku owi";
const wavPath = path.join(__dirname, "owi_tts.wav");
const baudRate = Number(process.env.OWI_SPEAK_BAUD || 460800);
const sampleRate = 16000;
const bytesPerSecond = sampleRate * 2;
const chunkSize = 1024;

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function psEscape(value) {
  return String(value).replace(/'/g, "''");
}

function makeTtsWav() {
  const ps = `
Add-Type -AssemblyName System.Speech
$synth = New-Object System.Speech.Synthesis.SpeechSynthesizer
$synth.Rate = 1
$synth.Volume = 100
$synth.SetOutputToWaveFile('${psEscape(wavPath)}')
$synth.Speak('${psEscape(text)}')
$synth.Dispose()
`;
  execFileSync("powershell", ["-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", ps], { stdio: "inherit" });
}

function writeDrain(port, chunk) {
  return new Promise((resolve, reject) => {
    port.write(chunk, (writeErr) => {
      if (writeErr) return reject(writeErr);
      port.drain((drainErr) => drainErr ? reject(drainErr) : resolve());
    });
  });
}

async function streamAudio() {
  const port = new SerialPort({ path: portName, baudRate, autoOpen: false });
  await new Promise((resolve, reject) => port.open((err) => err ? reject(err) : resolve()));
  console.log(`Owi ngomong lewat ${portName}: "${text}"`);
  await sleep(1800);

  const ffmpeg = spawn(ffmpegPath, [
    "-hide_banner",
    "-loglevel", "error",
    "-i", wavPath,
    "-f", "s16le",
    "-acodec", "pcm_s16le",
    "-ac", "1",
    "-ar", String(sampleRate),
    "-filter:a", "afade=t=in:st=0:d=0.035,highpass=f=120,lowpass=f=6200,acompressor=threshold=-24dB:ratio=2.5:attack=20:release=240,alimiter=limit=0.30,volume=0.24",
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
}

async function main() {
  makeTtsWav();
  if (!fs.existsSync(wavPath)) throw new Error("TTS gagal membuat wav");
  await streamAudio();
  console.log("Selesai ngomong");
}

main().catch((err) => {
  console.error(err.message);
  process.exit(1);
});
