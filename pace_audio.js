const fs = require('fs');
let txt = fs.readFileSync('web_serial_server.js', 'utf8');

const newStreamAudio = `
async function streamAudioToWS(ws, mp3Path, volume = '0.30') {
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
        // Pace the audio: 16000 Hz * 2 bytes = 32000 bytes per second
        const durationMs = (chunk.length / 32000) * 1000;
        // Sleep slightly less than duration to keep buffer full, but prevent overflow
        await sleep(durationMs * 0.95);
      } else {
        break;
      }
    }
    ffmpeg.kill();
  } catch (err) {
    logEvent('Error streaming WS: ' + err.message);
  } finally {
    isStreamingAudio = false;
  }
}
`;

txt = txt.replace(/async function streamAudioToWS[\s\S]*?async function streamTestToneWS/, newStreamAudio + '\n\nasync function streamTestToneWS');

fs.writeFileSync('web_serial_server.js', txt);
console.log("Added pacing to streamAudioToWS!");
