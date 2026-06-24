import re

with open('web_serial_server.js', 'r', encoding='utf-8') as f:
    content = f.read()

bad_js = '''const c=drawCanvas,ctx=c.getContext('2d',{willReadFrequently:true});ctx.fillStyle='#000';ctx.fillRect(0,0,240,320);ctx.strokeStyle='#fff';ctx.fillStyle='#fff';ctx.lineCap='round';let drawing=false,last=null,busy=false,pending=false;function pt(e){const r=c.getBoundingClientRect(),s=e.touches?.[0]||e;return{x:Math.max(0,Math.min(239,Math.floor((s.clientX-r.left)*240/r.width))),y:Math.max(0,Math.min(319,Math.floor((s.clientY-r.top)*320/r.height)))}}function draw(p){ctx.lineWidth=4;if(last){ctx.beginPath();ctx.moveTo(last.x,last.y);ctx.lineTo(p.x,p.y);ctx.stroke()}ctx.beginPath();ctx.arc(p.x,p.y,2,0,7);ctx.fill();last=p;syncSoon()}function down(e){e.preventDefault();drawing=true;last=null;draw(pt(e))}function move(e){if(!drawing)return;e.preventDefault();draw(pt(e))}function up(){drawing=false;last=null}c.onpointerdown=down;c.onpointermove=move;window.onpointerup=up;
      async function enterDraw(){
          const bgCol = document.getElementById('canvasColor').value || '#000000';
          await fetch('/cmd/W:' + hexToRGB565(bgCol),{method:'POST'});
        }function bytes(){const img=ctx.getImageData(0,0,240,320).data,out=new Uint8Array(9600);for(let y=0;y<320;y++)for(let xb=0;xb<30;xb++){let v=0;for(let bit=0;bit<8;bit++){const x=xb*8+bit,i=(y*240+x)*4;if(img[i]+img[i+1]+img[i+2]>384)v|=128>>bit}out[y*30+xb]=v}return out}async function sync(){if(busy)return pending=true;busy=true;pending=false;try{await enterDraw();await fetch('/frame',{method:'POST',headers:{'Content-Type':'application/octet-stream'},body:bytes()});setStatus('Draw tersinkron')}catch(e){setStatus(e.message,true)}busy=false;if(pending)syncSoon()}function syncSoon(){clearTimeout(window._ds);window._ds=setTimeout(sync,140)}enterDraw.onclick=sync;clearDraw.onclick=()=>{ctx.fillStyle='#000';ctx.fillRect(0,0,240,320);sync();fetch('/cmd/W',{method:'POST'})};'''

good_js = '''      let pendingDrawCmds = [];
      function hexToRGB565(hex) {
          let r = parseInt(hex.substring(1,3), 16) >> 3;
          let g = parseInt(hex.substring(3,5), 16) >> 2;
          let b = parseInt(hex.substring(5,7), 16) >> 3;
          return (r << 11) | (g << 5) | b;
      }
      const c=drawCanvas,ctx=c.getContext('2d',{willReadFrequently:true});ctx.fillStyle='#000';ctx.fillRect(0,0,240,320);ctx.strokeStyle='#fff';ctx.fillStyle='#fff';ctx.lineCap='round';let drawing=false,last=null,busy=false,pending=false;function pt(e){const r=c.getBoundingClientRect(),s=e.touches?.[0]||e;return{x:Math.max(0,Math.min(239,Math.floor((s.clientX-r.left)*240/r.width))),y:Math.max(0,Math.min(319,Math.floor((s.clientY-r.top)*320/r.height)))}}
      function draw(p){
        const b=Number(document.getElementById('brushSize').value||4);
        const col=document.getElementById('drawColor').value||'#ffffff';
        ctx.lineWidth=b;
        ctx.strokeStyle=col;ctx.fillStyle=col;
        let x1 = last ? last.x : p.x;
        let y1 = last ? last.y : p.y;
        if(last){ctx.beginPath();ctx.moveTo(last.x,last.y);ctx.lineTo(p.x,p.y);ctx.stroke()}
        ctx.beginPath();ctx.arc(p.x,p.y,Math.max(0.5,b/2),0,7);ctx.fill();
        pendingDrawCmds.push(Math.round(x1)+","+Math.round(y1)+","+Math.round(p.x)+","+Math.round(p.y)+","+hexToRGB565(col)+","+b);
        last=p;
        syncSoon();
      }
      function down(e){e.preventDefault();drawing=true;last=null;draw(pt(e))}
      function move(e){if(!drawing)return;e.preventDefault();draw(pt(e))}
      function up(){drawing=false;last=null}
      c.onpointerdown=down;c.onpointermove=move;window.onpointerup=up;
      c.addEventListener('touchstart',down,{passive:false});
      c.addEventListener('touchmove',move,{passive:false});
      
      let drawSyncTimer=null,drawSyncBusy=false;
      function syncSoon(){
        if(drawSyncTimer)return;
        drawSyncTimer=setTimeout(flushDrawSync,80);
      }
      async function flushDrawSync(){
        drawSyncTimer=null;
        if(drawSyncBusy)return;
        if(pendingDrawCmds.length===0)return;
        drawSyncBusy=true;
        let cmdsToSend = pendingDrawCmds.join("|");
        pendingDrawCmds = [];
        try {
          const r=await fetch('/draw_cmds',{method:'POST',headers:{'Content-Type':'text/plain'},body:cmdsToSend});
          const text=await r.text();
          if(!r.ok)throw new Error(text);
          if(drawSyncState) {
              drawSyncState.textContent='LIVE DRAW TERKIRIM';
              drawSyncState.style.color='var(--success)';
          }
        } catch(e) {
          if(drawSyncState) {
              drawSyncState.textContent='GAGAL: '+e.message;
              drawSyncState.style.color='var(--danger)';
          }
        }
        drawSyncBusy=false;
        if(pendingDrawCmds.length>0)syncSoon();
      }
      
      async function enterDraw(){
          const bgCol = document.getElementById('canvasColor').value || '#000000';
          await fetch('/cmd/W:' + hexToRGB565(bgCol),{method:'POST'});
      }
      async function sync(){
          if(busy)return pending=true;
          busy=true;pending=false;
          try{
              await enterDraw();
              if(pendingDrawCmds.length>0) await flushDrawSync();
          }catch(e){
              if(drawSyncState){ drawSyncState.textContent='GAGAL: '+e.message; drawSyncState.style.color='var(--danger)'; }
          }
          busy=false;
      }
      enterDraw.onclick=sync;
      clearDraw.onclick=()=>{
          const bgCol = document.getElementById('canvasColor').value || '#000000';
          ctx.fillStyle=bgCol;
          ctx.fillRect(0,0,240,320);
          pendingDrawCmds=[];
          sync();
      };'''

content = content.replace(bad_js, good_js)
with open('web_serial_server.js', 'w', encoding='utf-8') as f:
    f.write(content)
