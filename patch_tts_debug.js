const fs = require('fs');
let web = fs.readFileSync('web_serial_server.js', 'utf8');

const debugGoogleTtsCode = `
async function synthesizeEspeakToWav(text, outputPath) {
  return new Promise((resolve, reject) => {
    logEvent('google tts start: ' + text);
    const https = require("https");
    const url = "https://translate.google.com/translate_tts?ie=UTF-8&tl=id&client=tw-ob&q=" + encodeURIComponent(text);
    
    const mp3Path = outputPath.replace('.wav', '.mp3');
    const file = require('fs').createWriteStream(mp3Path);
    
    https.get(url, { headers: { 'User-Agent': 'Mozilla/5.0' } }, (response) => {
      logEvent('google tts status: ' + response.statusCode);
      if (response.statusCode !== 200) {
        return reject(new Error("Google TTS failed: " + response.statusCode));
      }
      response.pipe(file);
      file.on("finish", () => {
        file.close(() => {
          logEvent('google tts mp3 saved, running ffmpeg...');
          const { spawn } = require("child_process");
          const ffmpegPath = require("ffmpeg-static");
          const ffmpeg = spawn(ffmpegPath, [
            "-y", "-i", mp3Path, "-ar", "24000", "-ac", "1", outputPath
          ], { stdio: 'ignore' });
          ffmpeg.on("close", (code) => {
            logEvent('google tts ffmpeg done: ' + code);
            if (code === 0) resolve();
            else reject(new Error("ffmpeg convert error " + code));
          });
        });
      });
    }).on("error", (err) => {
      logEvent('google tts error: ' + err.message);
      require('fs').unlink(mp3Path, () => {});
      reject(err);
    });
  });
}
`;

web = web.replace(/async function synthesizeEspeakToWav[\s\S]*?\}\n/, debugGoogleTtsCode);
fs.writeFileSync('web_serial_server.js', web);
console.log('Patched TTS with debug logs');
