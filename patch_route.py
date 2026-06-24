with open('web_serial_server.js', 'r', encoding='utf-8') as f:
    content = f.read()

route_block = '''  if (req.method === "POST" && req.url === "/frame") {
    let body = [];
    req.on('data', chunk => body.push(chunk));
    req.on('end', async () => {
      body = Buffer.concat(body);
      try {
        await sendToSerial(body);
        res.writeHead(200);
        res.end("OK");
      } catch (e) {
        res.writeHead(500);
        res.end(e.message);
      }
    });
    return;
  }'''

new_route = route_block + '''
  if (req.method === "POST" && req.url === "/draw_cmds") {
    let body = "";
    req.on('data', chunk => body += chunk.toString());
    req.on('end', () => {
        try {
            requireOwiSocket().send("CMD:DRW:" + body);
            res.writeHead(200);
            res.end("OK");
        } catch(e) {
            res.writeHead(500);
            res.end(e.message);
        }
    });
    return;
  }'''

if route_block in content:
    content = content.replace(route_block, new_route)
    with open('web_serial_server.js', 'w', encoding='utf-8') as f:
        f.write(content)
    print("Success")
else:
    print("Route block not found!")
