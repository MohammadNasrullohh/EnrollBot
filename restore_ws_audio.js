const fs = require('fs');
let txt = fs.readFileSync('web_serial_server.js', 'utf8');

txt = txt.replace(
  'if(requireOwiSocket()) requireOwiSocket().send(`AUDIO:${ttsFile}:${volume}`);',
  'if(requireOwiSocket()) await streamAudioToWS(requireOwiSocket(), ttsFile, volume);'
);

txt = txt.replace(
  /if \(owiSocket && owiSocket\.readyState === WebSocket\.OPEN\) \{\s*owiSocket\.send\(`AUDIO:\$\{file\}:\$\{vol\}`\);\s*\}/g,
  'await streamAudioToWS(owiSocket, file, vol);'
);

txt = txt.replace(
  /if \(owiSocket && owiSocket\.readyState === WebSocket\.OPEN\) \{\s*owiSocket\.send\(`AUDIO:TEST:\$\{vol\}`\);\s*\}/g,
  'await streamTestToneWS(owiSocket, vol);'
);

const testToneWS = `
async function streamTestToneWS(ws, volume = "0.35") {
  if (isStreamingAudio) return;
  isStreamingAudio = true;
  logEvent('stream test tone via WS vol ' + volume);
  try {
    const sampleRate = 16000;
    const durationMs = 1800;
    const frequency = 880;
    const frames = Math.floor(sampleRate * durationMs / 1000);
    const safeVolume = clampVolume(volume, 0.35);
    const chunkFrames = 256;
    for (let frame = 0; frame < frames; frame += chunkFrames) {
      if (!ws || ws.readyState !== WebSocket.OPEN) break;
      const n = Math.min(chunkFrames, frames - frame);
      const chunk = Buffer.alloc(n * 2);
      for (let i = 0; i < n; i++) {
        const pos = frame + i;
        const t = pos / sampleRate;
        const envelope = Math.min(1, Math.min(pos / 1200, (frames - pos) / 1200));
        const sample = Math.round(Math.sin(2 * Math.PI * frequency * t) * 26000 * safeVolume * envelope);
        chunk.writeInt16LE(sample, i * 2);
      }
      ws.send(chunk);
      await sleep(n * 1000 / sampleRate);
    }
  } catch(e) {}
  isStreamingAudio = false;
}
`;

txt = txt.replace('async function streamTestTone', testToneWS + '\nasync function streamTestTone');

fs.writeFileSync('web_serial_server.js', txt);
console.log("Restored websocket streaming!");
