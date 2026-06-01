
  if(!localStorage.getItem('owi_current_user')) location.href='/#login';
  const c=document.getElementById('c'),ctx=c.getContext('2d',{willReadFrequently:true}),file=document.getElementById('file'),img=document.getElementById('img'),v=document.getElementById('v'),st=document.getElementById('status'),threshold=document.getElementById('threshold'),thresholdValue=document.getElementById('thresholdValue'),invert=document.getElementById('invert'),cropFill=document.getElementById('cropFill'),bitmapName=document.getElementById('bitmapName'),bitmapOutput=document.getElementById('bitmapOutput'),reminderList=document.getElementById('reminderList'),audioTrack=document.getElementById('audioTrack'),audioVolume=document.getElementById('audioVolume'),audioVolumeValue=document.getElementById('audioVolumeValue'),owiHost=document.getElementById('owiHost'),audioStatus=document.getElementById('audioStatus'),tiltXBar=document.getElementById('tiltXBar'),tiltYBar=document.getElementById('tiltYBar'),shakeBar=document.getElementById('shakeBar'),tiltXVal=document.getElementById('tiltXVal'),tiltYVal=document.getElementById('tiltYVal'),shakeVal=document.getElementById('shakeVal'),sensorNumbers=document.getElementById('sensorNumbers'),sensorStatus=document.getElementById('sensorStatus');let source=null,timer=null;function setStatus(t,b){st.textContent=t;st.className=b?'status danger':'status'}function setAudioStatus(t,b){audioStatus.textContent=t;audioStatus.className=b?'status danger':'status'}function setSensorStatus(t,b){sensorStatus.textContent=t;sensorStatus.className=b?'status danger':'status'}function fitDraw(el){ctx.fillStyle='#000';ctx.fillRect(0,0,128,64);const sw=el.videoWidth||el.naturalWidth,sh=el.videoHeight||el.naturalHeight;if(!sw||!sh)return;const scale=cropFill.checked?Math.max(128/sw,64/sh):Math.min(128/sw,64/sh),w=sw*scale,h=sh*scale;ctx.drawImage(el,(128-w)/2,(64-h)/2,w,h)}function makeFrame(){if(source)fitDraw(source);const data=ctx.getImageData(0,0,128,64).data,out=new Uint8Array(1024),limit=Number(threshold.value),inv=invert.checked;for(let y=0;y<64;y++)for(let x=0;x<128;x++){const i=(y*128+x)*4,lum=(data[i]*30+data[i+1]*59+data[i+2]*11)/100;if(inv?lum<limit:lum>limit)out[y*16+(x>>3)]|=128>>(x&7)}return out}function cleanName(n){return(n||'owi_look').replace(/[^a-zA-Z0-9_]/g,'_').replace(/^[0-9]/,'_$&')||'owi_look'}function bitmapCode(bytes){const name=cleanName(bitmapName.value),lines=['#include <Arduino.h>','','const uint8_t '+name+'[] PROGMEM = {'];for(let i=0;i<bytes.length;i+=16)lines.push('  '+Array.from(bytes.slice(i,i+16)).map(b=>'0x'+b.toString(16).padStart(2,'0').toUpperCase()).join(', ')+(i+16<bytes.length?',':''));lines.push('};');return lines.join('\n')}function updateBitmapOutput(){thresholdValue.textContent=threshold.value;bitmapOutput.value=bitmapCode(makeFrame())}function addReminderRow(time='07:30',text='enroll lagi ya deck'){if(reminderList.children.length>=5){setStatus('Maksimal 5 reminder.',true);return}const row=document.createElement('div');row.className='row reminderRow';row.innerHTML='<input class="reminderTime" type="time" value="'+time+'"><input class="reminderText" maxlength="32" value="'+text.replace(/"/g,'&quot;')+'"><button type="button" class="removeReminder">Hapus</button>';row.querySelector('.removeReminder').onclick=()=>{if(reminderList.children.length>1)row.remove()};reminderList.appendChild(row)}function collectReminders(){return Array.from(reminderList.querySelectorAll('.reminderRow')).slice(0,5).map(row=>({time:row.querySelector('.reminderTime').value,text:row.querySelector('.reminderText').value}))}async function sendFrame(){try{const frame=makeFrame();updateBitmapOutput();setStatus('Mengirim tampilan ke Owi...');const res=await fetch('/frame',{method:'POST',headers:{'Content-Type':'application/octet-stream'},body:frame});await res.text();setStatus('Tampilan baru sudah muncul di Owi.')}catch(e){setStatus(e.message,true)}}async function loadAudioTracks(){try{const data=await (await fetch('/api/audio/tracks')).json();owiHost.value=data.defaultHost||owiHost.value;audioTrack.innerHTML='';for(const t of data.tracks){const opt=document.createElement('option');opt.value=t;opt.textContent=t;audioTrack.appendChild(opt)}audioTrack.value=data.tracks.includes('lovestory.mp3')?'lovestory.mp3':data.tracks[0]||''}catch(e){setAudioStatus(e.message,true)}}function audioPayload(){return{track:audioTrack.value,volume:Number(audioVolume.value)/100,host:owiHost.value.trim()}}async function playAudioTrack(track){if(track)audioTrack.value=track;audioVolumeValue.textContent=(Number(audioVolume.value)/100).toFixed(2);setAudioStatus('Menghubungkan audio WiFi...');try{const r=await fetch('/api/audio/play',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(audioPayload())});const text=await r.text();if(!r.ok)throw new Error(text);setAudioStatus(text)}catch(e){setAudioStatus(e.message,true)}}async function stopAudioTrack(){try{const r=await fetch('/api/audio/stop',{method:'POST'});setAudioStatus(await r.text())}catch(e){setAudioStatus(e.message,true)}}async function refreshAudioStatus(){try{const s=await (await fetch('/api/audio/status')).json();if(s.playing)setAudioStatus('Playing '+s.track+' • '+s.seconds+'s • vol '+Number(s.volume).toFixed(2));}catch(e){}}async function refreshSensors(){try{const s=await (await fetch('/api/sensors')).json();const term=document.getElementById('terminal');if(term){let lines=term.value.split('\n').filter(Boolean);lines.push(JSON.stringify(s));if(lines.length>50)lines.shift();term.value=lines.join('\n')+'\n';term.scrollTop=term.scrollHeight;}if(!s.seen||s.ageMs>1600){setSensorStatus('Belum ada telemetry dari Owi',true);return}tiltXBar.value=Math.round(Number(s.tiltX)*100);tiltYBar.value=Math.round(Number(s.tiltY)*100);shakeBar.value=Math.round(Number(s.shakeMeter)*100);tiltXVal.textContent=Number(s.tiltX).toFixed(2);tiltYVal.textContent=Number(s.tiltY).toFixed(2);shakeVal.textContent=Number(s.shakeMeter).toFixed(2);if(document.getElementById('bTouch')) document.getElementById('bTouch').className='badge '+(s.touch?'on':'');if(document.getElementById('bNod')) document.getElementById('bNod').className='badge '+(s.nod?'on':'');if(document.getElementById('bShake')) document.getElementById('bShake').className='badge '+(s.headShake?'on':'');if(document.getElementById('bSurprise')) document.getElementById('bSurprise').className='badge '+(s.surprised?'on':'');if(document.getElementById('bCurious')) document.getElementById('bCurious').className='badge '+(s.curious?'on':'');if(document.getElementById('bUp')) document.getElementById('bUp').className='badge '+(s.faceUp?'on':'');if(document.getElementById('bDown')) document.getElementById('bDown').className='badge '+(s.faceDown?'on':'');if(document.getElementById('bAngry')) document.getElementById('bAngry').className='badge '+(s.angry?'on':'');sensorNumbers.textContent='IP '+s.ip+' • ax '+Number(s.ax).toFixed(2)+' ay '+Number(s.ay).toFixed(2)+' az '+Number(s.az).toFixed(2)+' • gyro '+Number(s.gx).toFixed(2)+','+Number(s.gy).toFixed(2)+','+Number(s.gz).toFixed(2)+' • temp '+Number(s.temp).toFixed(1)+'C hum '+Number(s.hum).toFixed(0)+'%';const faceEl=document.querySelector('.dashboard .face');if(faceEl){const lx=Number(s.tiltX)*40||0,ly=Number(s.tiltY)*30||0;faceEl.style.transform='translate('+lx+'px, '+ly+'px)';let eh='74px',mt='70px',mtx='';if(s.surprised||s.angry){eh='84px';mt='80px';if(s.angry)mtx='rotate(180deg)';}else if(s.sleepy||s.faceDown||s.nod||s.headShake){eh='18px';}const le=faceEl.querySelector('.eye.left'),re=faceEl.querySelector('.eye.right'),mo=faceEl.querySelector('.mouth');if(le)le.style.height=eh;if(re)re.style.height=eh;if(mo){mo.style.top=mt;mo.style.transform=mtx;}}setSensorStatus('Live • '+s.ageMs+'ms');
  const gamePhaseNum = Number(s.game || 0);
  const pHP = Number(s.playerHP ?? 100), eHP = Number(s.enemyHP ?? 80);
  const gamePanel = document.getElementById('gamePanel'), gameIdle = document.getElementById('gameIdle');
  if (gamePanel && gameIdle) {
    if (gamePhaseNum > 0) {
      gamePanel.style.display = ''; gameIdle.style.display = 'none';
      const phBar = document.getElementById('playerHPBar'), ehBar = document.getElementById('enemyHPBar');
      if (phBar) phBar.style.width = Math.max(0, pHP) + '%';
      if (ehBar) ehBar.style.width = Math.max(0, Math.round(eHP / 80 * 100)) + '%';
      const phVal = document.getElementById('playerHPVal'), ehVal = document.getElementById('enemyHPVal');
      if (phVal) phVal.textContent = pHP; if (ehVal) ehVal.textContent = eHP;
      const msg = document.getElementById('gameMsg');
      if (msg) {
        if (gamePhaseNum === 4) msg.textContent = '🏆 Victory!!';
        else if (gamePhaseNum === 5) msg.textContent = '💀 Game Over';
        else if (gamePhaseNum === 6) msg.textContent = 'Ready! Tap Owi untuk Serang';
        else if (s.crit == 1) msg.textContent = '⚡ Critical Hit!!';
        else msg.textContent = '';
      }
    } else {
      gamePanel.style.display = 'none'; gameIdle.style.display = '';
    }
  }}catch(e){setSensorStatus(e.message,true)}}file.onchange=()=>{clearInterval(timer);timer=null;const f=file.files[0];if(!f)return;const url=URL.createObjectURL(f),done=()=>{fitDraw(source);updateBitmapOutput()};if(f.type.startsWith('video/')){source=v;v.src=url;v.play();v.onloadeddata=done}else{source=img;img.src=url;img.onload=done}};document.getElementById('send').onclick=sendFrame;document.getElementById('play').onclick=()=>{if(!source)return;clearInterval(timer);timer=setInterval(sendFrame,180)};document.getElementById('stop').onclick=()=>{clearInterval(timer);timer=null;setStatus('Berhenti')};document.getElementById('clear').onclick=async()=>{clearInterval(timer);timer=null;const r=await fetch('/clear',{method:'POST'});setStatus(await r.text())};document.getElementById('addReminder').onclick=()=>addReminderRow('12:00','enroll lagi ya deck');document.getElementById('sendReminder').onclick=async()=>{clearInterval(timer);timer=null;try{const r=await fetch('/reminder',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({reminders:collectReminders()})});setStatus(await r.text())}catch(e){setStatus(message,true)}};document.getElementById('sendReminderText').onclick=async()=>{clearInterval(timer);timer=null;try{const list=collectReminders();const r=await fetch('/reminder',{method:'POST',headers:{'Content-Type':'text/plain'},body:(list[0]&&list[0].text)||'enroll lagi ya deck'});setStatus(await r.text())}catch(e){setStatus(e.message,true)}};document.getElementById('refreshBitmap').onclick=updateBitmapOutput;document.getElementById('copyBitmap').onclick=async()=>{updateBitmapOutput();await navigator.clipboard.writeText(bitmapOutput.value);setStatus('Look tersalin.')};document.getElementById('downloadBitmap').onclick=()=>{updateBitmapOutput();const blob=new Blob([bitmapOutput.value],{type:'text/plain'}),a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download=cleanName(bitmapName.value)+'.h';a.click();URL.revokeObjectURL(a.href)};[threshold,invert,cropFill,bitmapName].forEach(el=>el.addEventListener('input',updateBitmapOutput));audioVolume.oninput=()=>audioVolumeValue.textContent=(Number(audioVolume.value)/100).toFixed(2);document.getElementById('playAudio').onclick=()=>playAudioTrack();document.getElementById('stopAudio').onclick=stopAudioTrack;document.getElementById('playLove').onclick=()=>playAudioTrack('lovestory.mp3');document.querySelectorAll('[data-cmd]').forEach(btn=>btn.onclick=async()=>{clearInterval(timer);timer=null;try{const r=await fetch('/cmd/'+btn.dataset.cmd,{method:'POST'});setStatus(await r.text())}catch(e){setStatus(e.message,true)}});document.getElementById('logoutBtn').onclick=()=>{localStorage.removeItem('owi_current_user');location.href='/'};addReminderRow();updateBitmapOutput();loadAudioTracks();setInterval(()=>{refreshSensors();refreshAudioStatus();refreshLog()},500);
  const joyBox=document.getElementById('joyBox'),joyStick=document.getElementById('joyStick');let joyTimer=null,isDragging=false;function updateJoy(e){const rect=joyBox.getBoundingClientRect();let x=e.clientX-rect.left-75,y=e.clientY-rect.top-75;x=Math.max(-30,Math.min(30,x));y=Math.max(-30,Math.min(30,y));joyStick.style.transform='translate('+x+'px,'+y+'px)';if(!joyTimer){joyTimer=setTimeout(()=>{joyTimer=null},100);fetch('/raw',{method:'POST',body:'J'+Math.round(x)+','+Math.round(y)+'\n'}).catch(()=>{})}}if(joyBox){joyBox.addEventListener('pointerdown',e=>{isDragging=true;joyBox.setPointerCapture(e.pointerId);updateJoy(e)});joyBox.addEventListener('pointermove',e=>{if(isDragging)updateJoy(e)});const resetJoy=()=>{isDragging=false;joyStick.style.transform='translate(0px,0px)';fetch('/raw',{method:'POST',body:'J0,0\n'}).catch(()=>{})};joyBox.addEventListener('pointerup',resetJoy);joyBox.addEventListener('pointercancel',resetJoy)}
  async function refreshLog(){try{const r=await fetch('/logs');document.getElementById('webLog').value=await r.text();}catch(e){}}
  const ws = new WebSocket('ws://' + location.host);
  ws.binaryType = "arraybuffer";
  let micCtx;
  let micStartTime;
  document.getElementById('listenMic').onclick = () => {
    if(!micCtx) {
      micCtx = new (window.AudioContext || window.webkitAudioContext)({sampleRate: 16000});
    }
    micCtx.resume();
    document.getElementById('aiStatus').textContent = 'Mendengarkan...';
  };
  document.getElementById('stopMic').onclick = () => {
    if(micCtx) micCtx.suspend();
    document.getElementById('aiStatus').textContent = 'AI Idle';
  };
  const micCanvas = document.getElementById('micCanvas');
  const micCanvasCtx = micCanvas ? micCanvas.getContext('2d') : null;
  ws.onmessage = (event) => {
    if (typeof event.data !== 'string') {
      if(micCtx && micCtx.state === 'running') {
        const pcm16 = new Int16Array(event.data);
        const audioBuffer = micCtx.createBuffer(1, pcm16.length, 16000);
        const float32 = audioBuffer.getChannelData(0);
        let sumSq = 0;
        for(let i=0; i<pcm16.length; i++) {
          float32[i] = pcm16[i] / 32768.0;
          sumSq += float32[i] * float32[i];
        }
        if (micCanvasCtx) {
          let rms = Math.sqrt(sumSq / pcm16.length);
          let barWidth = Math.min(200, rms * 1500);
          micCanvasCtx.fillStyle = '#000';
          micCanvasCtx.fillRect(0, 0, 200, 40);
          micCanvasCtx.fillStyle = '#00ff00';
          micCanvasCtx.fillRect(0, 0, barWidth, 40);
        }
        const src = micCtx.createBufferSource();
        src.buffer = audioBuffer;
        src.connect(micCtx.destination);
        if(!micStartTime || micStartTime < micCtx.currentTime) micStartTime = micCtx.currentTime;
        src.start(micStartTime);
        micStartTime += audioBuffer.duration;
      }
      return;
    }
  };
  setInterval(refreshLog,1000);setInterval(refreshAudioStatus,1200);setInterval(refreshSensors,250);refreshLog();refreshAudioStatus();refreshSensors();
  