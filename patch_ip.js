const fs = require('fs');
let web = fs.readFileSync('web_serial_server.js', 'utf8');
web = web.replace('if (!ip) return;', 'if (!ip) { logEvent("stream audio err: no IP from UDP"); return; }');
fs.writeFileSync('web_serial_server.js', web);
console.log('Patched streamAudio IP check');
