import re

with open('web_serial_server.js', 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Add color picker
ui_old = '''<label style="font-family:'Roboto Mono',monospace;font-size:0.75rem;font-weight:800;">BRUSH
              <input type="range" id="brushSize" min="1" max="7" value="3" style="width:100%;margin-top:0.4rem;">
              </label>'''

ui_new = '''<div style="display:flex;gap:10px;">
              <label style="flex:1;font-family:'Roboto Mono',monospace;font-size:0.75rem;font-weight:800;">BRUSH
                <input type="range" id="brushSize" min="1" max="15" value="3" style="width:100%;margin-top:0.4rem;">
              </label>
              <label style="flex:1;font-family:'Roboto Mono',monospace;font-size:0.75rem;font-weight:800;">WARNA
                <input type="color" id="drawColor" value="#ffffff" style="width:100%;height:24px;border:none;padding:0;cursor:pointer;margin-top:0.2rem;border-radius:4px;">
              </label>
            </div>'''

content = content.replace(ui_old, ui_new)

# 2. Modify drawAt and add /draw_cmds handler in frontend
js_old = '''      function drawAt(pt){
        const b=Number(document.getElementById('brushSize').value||3);
        drawCtx.lineWidth=b;
        drawCtx.strokeStyle='#fff';drawCtx.fillStyle='#fff';
        if(lastPt){drawCtx.beginPath();drawCtx.moveTo(lastPt.x,lastPt.y);drawCtx.lineTo(pt.x,pt.y);drawCtx.stroke();}
        drawCtx.beginPath();drawCtx.arc(pt.x,pt.y,Math.max(0.5,b/2),0,Math.PI*2);drawCtx.fill();
        lastPt=pt;
      }
      canvas.addEventListener('mousedown',e=>{drawing=true;lastPt=null;drawAt({x:e.offsetX,y:e.offsetY})});
      canvas.addEventListener('mousemove',e=>{if(!drawing)return;drawAt({x:e.offsetX,y:e.offsetY});scheduleDrawSync()});
      canvas.addEventListener('mouseup',()=>drawing=false);
      canvas.addEventListener('mouseleave',()=>drawing=false);
      
      let drawSyncTimer=null,drawSyncPending=false,drawSyncBusy=false;
      function scheduleDrawSync(){
        drawSyncPending=true;
        if(drawSyncTimer)return;
        drawSyncTimer=setTimeout(flushDrawSync,140);
      }
      async function flushDrawSync(){
        drawSyncTimer=null;
        if(drawSyncBusy)return;
        if(!drawSyncPending)return;
        drawSyncPending=false;
        drawSyncBusy=true;
        try {
          const bytes = getDrawBytes();
          const r=await fetch('/frame',{method:'POST',headers:{'Content-Type':'application/octet-stream'},body:bytes});
          const text=await r.text();
          if(!r.ok)throw new Error(text);
          drawSyncState.textContent='LIVE DRAW TERKIRIM';
          drawSyncState.style.color='var(--success)';
        } catch(e) {
          drawSyncState.textContent='GAGAL: '+e.message;
          drawSyncState.style.color='var(--danger)';
        }
        drawSyncBusy=false;
        if(drawSyncPending)scheduleDrawSync();
      }'''

js_new = '''      let pendingDrawCmds = [];
      function hexToRGB565(hex) {
          let r = parseInt(hex.substring(1,3), 16) >> 3;
          let g = parseInt(hex.substring(3,5), 16) >> 2;
          let b = parseInt(hex.substring(5,7), 16) >> 3;
          return (r << 11) | (g << 5) | b;
      }
      function drawAt(pt){
        const b=Number(document.getElementById('brushSize').value||3);
        const col=document.getElementById('drawColor').value||'#ffffff';
        drawCtx.lineWidth=b;
        drawCtx.strokeStyle=col;drawCtx.fillStyle=col;
        let x1 = lastPt ? lastPt.x : pt.x;
        let y1 = lastPt ? lastPt.y : pt.y;
        if(lastPt){drawCtx.beginPath();drawCtx.moveTo(lastPt.x,lastPt.y);drawCtx.lineTo(pt.x,pt.y);drawCtx.stroke();}
        drawCtx.beginPath();drawCtx.arc(pt.x,pt.y,Math.max(0.5,b/2),0,Math.PI*2);drawCtx.fill();
        
        pendingDrawCmds.push(Math.round(x1)+","+Math.round(y1)+","+Math.round(pt.x)+","+Math.round(pt.y)+","+hexToRGB565(col)+","+b);
        lastPt=pt;
      }
      canvas.addEventListener('mousedown',e=>{drawing=true;lastPt=null;drawAt({x:e.offsetX,y:e.offsetY})});
      canvas.addEventListener('mousemove',e=>{if(!drawing)return;drawAt({x:e.offsetX,y:e.offsetY});scheduleDrawSync()});
      canvas.addEventListener('mouseup',()=>drawing=false);
      canvas.addEventListener('mouseleave',()=>drawing=false);
      
      let drawSyncTimer=null,drawSyncPending=false,drawSyncBusy=false;
      function scheduleDrawSync(){
        drawSyncPending=true;
        if(drawSyncTimer)return;
        drawSyncTimer=setTimeout(flushDrawSync,80);
      }
      async function flushDrawSync(){
        drawSyncTimer=null;
        if(drawSyncBusy)return;
        if(pendingDrawCmds.length===0)return;
        drawSyncPending=false;
        drawSyncBusy=true;
        let cmdsToSend = pendingDrawCmds.join("|");
        pendingDrawCmds = [];
        try {
          const r=await fetch('/draw_cmds',{method:'POST',headers:{'Content-Type':'text/plain'},body:cmdsToSend});
          const text=await r.text();
          if(!r.ok)throw new Error(text);
          drawSyncState.textContent='LIVE DRAW TERKIRIM';
          drawSyncState.style.color='var(--success)';
        } catch(e) {
          drawSyncState.textContent='GAGAL: '+e.message;
          drawSyncState.style.color='var(--danger)';
        }
        drawSyncBusy=false;
        if(pendingDrawCmds.length>0)scheduleDrawSync();
      }'''

content = content.replace(js_old, js_new)

# 3. Add /draw_cmds to server routes
route_old = '''  if (req.method === "POST" && req.url === "/frame") {
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

route_new = route_old + '''
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

content = content.replace(route_old, route_new)

with open('web_serial_server.js', 'w', encoding='utf-8') as f:
    f.write(content)
