const fs = require('fs');
let backend = fs.readFileSync('backend_only.js', 'utf8');

const streamAudioRegex = /async function streamAudio[\s\S]*?async function streamTestTone/m;
backend = backend.replace(streamAudioRegex, `async function streamAudioToWS(ws, mp3Path, volume = '0.30') {
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

backend = backend.replace(/streamAudio\(latestTelemetry\.ip, volume, ttsFile\);/, 'if(requireOwiSocket()) streamAudioToWS(requireOwiSocket(), ttsFile, volume);');

backend = backend.replace(/const udpServer = dgram\.createSocket\('udp4'\);[\s\S]*?udpServer\.bind\(7789\);/, '');
backend = backend.replace(/udpServer\.bind\(7788\);/, '');

backend = backend.replace(/async function sendCommand[\s\S]*?function sendChatText/m, `async function sendCommand(command) {
  if (owiSocket && owiSocket.readyState === WebSocket.OPEN) {
    owiSocket.send('CMD:' + command);
    logEvent('WS sent CMD:' + command);
  } else {
    logEvent('No WS to send command: ' + command);
  }
}
function sendChatText`);

backend = backend.replace(/function sendChatText[\s\S]*?async function handleVoiceSession/m, `function sendChatText(text) {
  const clean = sanitizeOledText(text).slice(0, 200);
  logEvent('chat \"' + clean + '\"');
  if (owiSocket && owiSocket.readyState === WebSocket.OPEN) {
    owiSocket.send('CMD:T:' + clean);
  }
}

async function handleVoiceSession`);

fs.writeFileSync('backend_mod1.js', backend);
console.log('Modified 1st pass');
