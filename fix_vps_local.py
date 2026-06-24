import re

with open('vps_server_local.js', 'r', encoding='utf-8') as f:
    content = f.read()

old_func_pattern = re.compile(r'async function synthesizeEspeakToWav.*?\}\n\n', re.DOTALL)

new_func = '''async function synthesizeEspeakToWav(text, outputPath) {
  const https = require("https");
  const fs = require("fs");
  const path = require("path");
  const mp3Path = outputPath.replace('.wav', '.mp3');
  
  const words = text.split(' ');
  const chunks = [];
  let currentChunk = '';
  for (const word of words) {
    if (currentChunk.length + word.length + 1 > 180) {
      if (currentChunk) chunks.push(currentChunk);
      currentChunk = word;
    } else {
      currentChunk += (currentChunk ? ' ' : '') + word;
    }
  }
  if (currentChunk) chunks.push(currentChunk);

  logEvent('google tts start: ' + chunks.length + ' chunks');
  
  const downloadChunk = (chunkText) => {
    return new Promise((res, rej) => {
      const url = "https://translate.google.com/translate_tts?ie=UTF-8&tl=id&client=tw-ob&q=" + encodeURIComponent(chunkText);
      https.get(url, { headers: { 'User-Agent': 'Mozilla/5.0' } }, (response) => {
        if (response.statusCode !== 200) return rej(new Error('Google TTS HTTP ' + response.statusCode));
        const bufs = [];
        response.on('data', d => bufs.push(d));
        response.on('end', () => res(Buffer.concat(bufs)));
      }).on('error', rej);
    });
  };

  try {
    let allMp3Bufs = [];
    for (let i = 0; i < chunks.length; i++) {
      allMp3Bufs.push(await downloadChunk(chunks[i]));
    }
    fs.writeFileSync(mp3Path, Buffer.concat(allMp3Bufs));
    logEvent('google tts mp3 saved, running ffmpeg...');
    
    await new Promise((res, rej) => {
      const { spawn } = require("child_process");
      const ffmpeg = spawn("ffmpeg", ["-y", "-i", mp3Path, "-ar", "16000", "-ac", "1", "-c:a", "pcm_s16le", outputPath]);
      ffmpeg.on("close", (code) => {
        logEvent('google tts ffmpeg done: ' + code);
        if (code === 0) res(); else rej(new Error("ffmpeg failed"));
      });
    });
  } catch (err) {
    logEvent('chat tts err: Google TTS failed: ' + err.message);
    throw err;
  }
}

'''

content = old_func_pattern.sub(new_func, content)

with open('vps_server_local.js', 'w', encoding='utf-8') as f:
    f.write(content)

print("Replaced!")
