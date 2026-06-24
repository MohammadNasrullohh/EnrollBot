const fs = require('fs');
const txt = fs.readFileSync('web_serial_server.js', 'utf8');
const match = txt.indexOf('"/api/chat"');
if (match !== -1) {
    console.log(txt.slice(match - 100, match + 1500));
} else {
    console.log("Not found.");
}
