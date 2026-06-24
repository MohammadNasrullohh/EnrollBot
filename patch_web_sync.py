import re

with open('web_serial_server.js', 'r', encoding='utf-8') as f:
    content = f.read()

# Modify sync, clearDraw, and enterDraw
bad_js = '''async function enterDraw(){await fetch('/cmd/W',{method:'POST'})}'''
good_js = '''async function enterDraw(){
        const bgCol = document.getElementById('canvasColor').value || '#000000';
        await fetch('/cmd/W:' + hexToRGB565(bgCol),{method:'POST'});
      }'''

content = content.replace(bad_js, good_js)

bad_clear = '''clearDraw.onclick=()=>{ctx.fillStyle='#000';ctx.fillRect(0,0,240,320);sync();fetch('/cmd/W',{method:'POST'})};'''
good_clear = '''clearDraw.onclick=()=>{
        const bgCol = document.getElementById('canvasColor').value || '#000000';
        ctx.fillStyle=bgCol;
        ctx.fillRect(0,0,240,320);
        pendingDrawCmds=[]; // clear pending
        sync();
      };'''

content = content.replace(bad_clear, good_clear)

# Sync with system menu
bad_sys = '''document.querySelectorAll('.sysBtn').forEach(b=>b.onclick=async()=>{try{const r=await fetch('/cmd/'+b.dataset.cmd,{method:'POST'});setStatus(await r.text(),!r.ok)}catch(e){setStatus(e.message,true)}});'''
good_sys = '''document.querySelectorAll('.sysBtn').forEach(b=>b.onclick=async()=>{
        try{
            if (b.dataset.cmd === 'C' || b.dataset.cmd === 'CLEAR') {
                drawSyncState.textContent = 'KELUAR DRAW MODE';
                drawSyncState.style.color = 'var(--text-muted)';
            }
            const r=await fetch('/cmd/'+b.dataset.cmd,{method:'POST'});
            setStatus(await r.text(),!r.ok)
        }catch(e){setStatus(e.message,true)}
      });'''

content = content.replace(bad_sys, good_sys)

with open('web_serial_server.js', 'w', encoding='utf-8') as f:
    f.write(content)
