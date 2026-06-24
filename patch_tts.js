const fs = require('fs');
const { Client } = require('ssh2');

const conn = new Client();
conn.on('ready', () => {
  console.log('Client :: ready');
  conn.exec('cat /root/owibot/vps_server.js', (err, stream) => {
    if (err) throw err;
    let code = '';
    stream.on('data', (d) => code += d);
    stream.on('close', () => {
      const oldFunc = /async function synthesizeEspeakToWav[\s\S]*?\}\s*(?=\n(?:async )?function |module\.exports|return |$)/;
      
      const newFunc = 
async function synthesizeEspeakToWav(text, outputPath) {
  const https = require('https');
  const fs = require('fs');
  const path = require('path');
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
      const { spawn } = require('child_process');
      const ffmpeg = spawn('ffmpeg', ['-y', '-i', mp3Path, '-ar', '16000', '-ac', '1', '-c:a', 'pcm_s16le', outputPath]);
      ffmpeg.on('close', (code) => {
        logEvent('google tts ffmpeg done: ' + code);
        if (code === 0) res(); else rej(new Error('ffmpeg failed'));
      });
    });
  } catch (err) {
    logEvent('chat tts err: Google TTS failed: ' + err.message);
    throw err;
  }
}
;

      if (code.includes('synthesizeEspeakToWav')) {
        const newCode = code.replace(oldFunc, newFunc + '\n\n');
        conn.exec('cat > /root/owibot/vps_server.js', (err, writeStream) => {
          writeStream.write(newCode);
          writeStream.end();
          writeStream.on('close', () => {
             conn.exec('pm2 restart owi', (err, restartStream) => {
               restartStream.on('data', d => console.log(d.toString()));
               restartStream.on('close', () => conn.end());
             });
          });
        });
      } else {
        console.log("Could not find synthesizeEspeakToWav in code");
        conn.end();
      }
    });
  });
}).connect({ host: '212.2.253.247', port: 22, username: 'root', password: 'cAh2TrVUlG' });
