const fs = require('fs');
let web = fs.readFileSync('web_serial_server.js', 'utf8');

const oldEndpoint = `streamAudio(wavPath, 1.0);`;

const fix = `streamAudio(latestTelemetry?.ip, "1.0", wavPath);`;

web = web.replace(oldEndpoint, fix);

fs.writeFileSync('web_serial_server.js', web);
console.log('Fixed /api/speak signature');
