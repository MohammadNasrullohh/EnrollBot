const fs = require('fs');

let vps = fs.readFileSync('vps_server.js', 'utf8');

const udpCode = `
const udpServer = dgram.createSocket('udp4');
udpServer.on('message', (msg, rinfo) => {
  try {
    latestTelemetry = JSON.parse(msg.toString());
    latestTelemetry.lastUpdate = Date.now();
    latestTelemetry.ip = rinfo.address;
  } catch(e){}
});
udpServer.bind(7788);
`;

const streamCode = `


function clampVolume(value, fallback = 0.22) {
  const parsed = Number(value);
  if (!Number.isFinite(parsed)) return fallback;
  return Math.max(0.04, Math.min(0.55, parsed));
}

async function streamAudio(ip, volume = "0.30", mp3Path = "lovestory.mp3") {
  if (isStreamingAudio) return;
  if (!ip) return;
  isStreamingAudio = true;
  logEvent(\`stream audio \${mp3Path} ke \${ip}:7777 vol \${volume}\`);

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
      '-filter:a', \`highpass=f=95,lowpass=f=7200,loudnorm=I=-20:TP=-2.5:LRA=8,acompressor=threshold=-24dB:ratio=2.2:attack=18:release=240,alimiter=limit=0.38,volume=\${safeVolume}\`,
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
`;

vps = vps.replace('let latestTelemetry = {};', 'let latestTelemetry = {};\n' + udpCode);
vps = vps.replace(/async function streamAudio[\s\S]*?async function streamTestTone[\s\S]*?\n}/, streamCode);

fs.writeFileSync('web_serial_server.js', vps);
console.log('Successfully created local GemBot server!');
