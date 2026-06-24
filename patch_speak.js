const fs = require('fs');
let web = fs.readFileSync('web_serial_server.js', 'utf8');

const newEndpoint = `
  if (req.method === "POST" && req.url === "/api/speak") {
    let body = "";
    req.on("data", chunk => body += chunk);
    req.on("end", async () => {
      try {
        const data = JSON.parse(body);
        const text = data.text || "Halo";
        logEvent("Test speak: " + text);
        const { synthesizeSpeechFile } = require('./web_serial_server.js'); 
        // actually synthesizeSpeechFile is a global inside the same file.
        // Let's just call it.
        const wavPath = await synthesizeSpeechFile(text);
        if (wavPath && fs.existsSync(wavPath)) {
            streamAudio(wavPath, 1.0);
            res.writeHead(200, { "Content-Type": "application/json" });
            res.end(JSON.stringify({ success: true, text }));
        } else {
            res.writeHead(500);
            res.end(JSON.stringify({ error: "Failed to synthesize" }));
        }
      } catch (err) {
        res.writeHead(500);
        res.end(JSON.stringify({ error: err.message }));
      }
    });
    return;
  }
`;

web = web.replace('if (req.method === "POST" && req.url === "/api/chat") {', newEndpoint + '\n  if (req.method === "POST" && req.url === "/api/chat") {');

fs.writeFileSync('web_serial_server.js', web);
console.log('Added /api/speak endpoint');
