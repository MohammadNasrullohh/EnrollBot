import re

with open('web_serial_server.js', 'r', encoding='utf-8') as f:
    content = f.read()

bad_js = '''const c=drawCanvas,ctx=c.getContext('2d',{willReadFrequently:true});ctx.fillStyle='#000';ctx.fillRect(0,0,240,320);ctx.strokeStyle='#fff';ctx.fillStyle='#fff';ctx.lineCap='round';let drawing=false,last=null,busy=false,pending=false;function pt(e){const r=c.getBoundingClientRect(),s=e.touches?.[0]||e;return{x:Math.max(0,Math.min(239,Math.floor((s.clientX-r.left)*240/r.width))),y:Math.max(0,Math.min(319,Math.floor((s.clientY-r.top)*320/r.height)))}}function draw(p){ctx.lineWidth=4;if(last){ctx.beginPath();ctx.moveTo(last.x,last.y);ctx.lineTo(p.x,p.y);ctx.stroke()}ctx.beginPath();ctx.arc(p.x,p.y,2,0,7);ctx.fill();last=p;syncSoon()}function down(e){e.preventDefault();drawing=true;last=null;draw(pt(e))}function move(e){if(!drawing)return;e.preventDefault();draw(pt(e))}function up(){drawing=false;last=null}c.onpointerdown=down;c.onpointermove=move;window.onpointerup=up;
      async function enterDraw(){
          const bgCol = document.getElementById('canvasColor').value || '#000000';
          await fetch('/cmd/W:' + hexToRGB565(bgCol),{method:'POST'});
        }function bytes(){const img=ctx.getImageData(0,0,240,320).data,out=new Uint8Array(9600);for(let y=0;y<320;y++)for(let xb=0;xb<30;xb++){let v=0;for(let bit=0;bit<8;bit++){const x=xb*8+bit,i=(y*240+x)*4;if(img[i]+img[i+1]+img[i+2]>384)v|=128>>bit}out[y*30+xb]=v}return out}async function sync(){if(busy)return pending=true;busy=true;pending=false;try{await enterDraw();await fetch('/frame',{method:'POST',headers:{'Content-Type':'application/octet-stream'},body:bytes()});setStatus('Draw tersinkron')}catch(e){setStatus(e.message,true)}busy=false;if(pending)syncSoon()}function syncSoon(){clearTimeout(window._ds);window._ds=setTimeout(sync,140)}enterDraw.onclick=sync;clearDraw.onclick=()=>{ctx.fillStyle='#000';ctx.fillRect(0,0,240,320);sync();fetch('/cmd/W',{method:'POST'})};'''

# Since regex is hard with minified code, let's use replace_file_content!
