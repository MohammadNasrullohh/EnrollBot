const fs = require('fs');
const txt = fs.readFileSync('web_serial_server.js', 'utf8');
const lines = txt.split('\n');
for (let i = 0; i < lines.length; i++) {
    if (lines[i].includes('async function speakReplyOnBot')) console.log('speakReplyOnBot', i);
    if (lines[i].includes('req.url === "/play_audio"')) console.log('/play_audio', i);
    if (lines[i].includes('req.url === "/test_max"')) console.log('/test_max', i);
}
