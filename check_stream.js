const fs = require('fs');
const txt = fs.readFileSync('web_serial_server.js', 'utf8');
const lines = txt.split('\n');
for (let i = 0; i < lines.length; i++) {
    if (lines[i].includes('"/stream"')) {
        console.log(lines.slice(i, i+30).join('\n'));
        break;
    }
}
