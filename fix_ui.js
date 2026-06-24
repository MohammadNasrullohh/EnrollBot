const fs = require('fs');
let txt = fs.readFileSync('web_serial_server_new.js', 'utf8');

// Find the first 'function pageHtml' (Brutalist) and remove it up to 'function appShellStyles'
const firstPageHtmlIdx = txt.indexOf('function pageHtml');
const appShellStylesIdx = txt.indexOf('function appShellStyles');

if (firstPageHtmlIdx !== -1 && appShellStylesIdx !== -1 && firstPageHtmlIdx < appShellStylesIdx) {
    console.log("Found Brutalist UI block. Removing...");
    const before = txt.substring(0, firstPageHtmlIdx);
    const after = txt.substring(appShellStylesIdx);
    txt = before + after;
    fs.writeFileSync('web_serial_server.js', txt);
    console.log("Saved to web_serial_server.js");
} else {
    console.log("Could not find blocks cleanly. Please check manually.");
}
