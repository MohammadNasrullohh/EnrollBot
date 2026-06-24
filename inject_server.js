const fs = require('fs');
let f = fs.readFileSync('frontend_mod1.js', 'utf8');

const srv = `const server = http.createServer((req, res) => {
  const parsedUrl = url.parse(req.url, true);
  if (req.method === 'GET') {
    if (parsedUrl.pathname === '/') {
      res.writeHead(200, { 'Content-Type': 'text/html' });
      res.end(pageHtml());
    } else if (parsedUrl.pathname === '/control') {
      res.writeHead(200, { 'Content-Type': 'text/html' });
      res.end(controlPageHtml());
    } else {
      res.writeHead(404);
      res.end();
    }
  } else {
    res.writeHead(404);
    res.end();
  }
});

const wss = new WebSocket.Server({ server });`;

f = f.replace('const wss = new WebSocket.Server({ server });', srv);
fs.writeFileSync('frontend_mod2.js', f);
console.log('injected!');
