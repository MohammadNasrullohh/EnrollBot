const fs = require('fs');
let web = fs.readFileSync('web_serial_server.js', 'utf8');

const oldEndpoint = `const { synthesizeSpeechFile } = require('./web_serial_server.js'); 
        // actually synthesizeSpeechFile is a global inside the same file.
        // Let's just call it.
        const wavPath = await synthesizeSpeechFile(text);`;

const fix = `const wavPath = await synthesizeSpeechFile(text);`;

web = web.replace(oldEndpoint, fix);

fs.writeFileSync('web_serial_server.js', web);
console.log('Fixed /api/speak');
