const fs = require('fs');
const txt = fs.readFileSync('web_serial_server.js', 'utf8');
const lines = txt.split('\n');
for (let i = 0; i < lines.length; i++) {
    if (lines[i].includes('"/test_max"')) {
        console.log(lines.slice(i, i+20).join('\n'));
        break;
    }
}
