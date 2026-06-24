<script>
    if(!localStorage.getItem('owi_current_user')) location.href='/#login';
    const st=document.getElementById('status');
    const reminderList=document.getElementById('reminderList');
    function setStatus(t,bad){st.textContent=t;st.className=bad?'status-bar err':'status-bar';}

    function addReminderRow(time,text){
      time=time||'07:30';text=text||'enroll lagi ya deck';
      if(reminderList.children.length>=5){setStatus('MAX 5 REMINDERS.',true);return;}
      const row=document.createElement('div');row.className='reminderRow';
      row.innerHTML='<input class="reminderTime" type="time" value="'+time+'"><input class="reminderText" maxlength="32" value="'+text.replace(/"/g,'&quot;')+'"><button type="button" class="sm" style="padding:0.5rem">X</button>';
      row.querySelector('button').onclick=()=>{if(reminderList.children.length>1)row.remove();};
      reminderList.appendChild(row);
    }
    function collectReminders(){
      return Array.from(reminderList.querySelectorAll('.reminderRow')).slice(0,5).map(r=>({
        time:r.querySelector('.reminderTime').value,
        text:r.querySelector('.reminderText').value
      }));
    }
    document.getElementById('addReminder').onclick=()=>addReminderRow('12:00','enroll lagi ya deck');
    document.getElementById('sendReminder').onclick=async()=>{
      try{const r=await fetch('/reminder',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({reminders:collectReminders()})});setStatus(await r.text());}
      catch(e){setStatus(e.message,true);}
    };
    document.getElementById('sendReminderText').onclick=async()=>{
      try{const list=collectReminders();const r=await fetch('/reminder',{method:'POST',headers:{'Content-Type':'text/plain'},body:(list[0]&&list[0].text)||'enroll lagi ya deck'});setStatus(await r.text());}
      catch(e){setStatus(e.message,true);}
    };
    addReminderRow();

    async function playMusicClick(ev, file) {
      ev.preventDefault();
      ev.stopPropagation();
      try {
        const vol = document.getElementById('volLoveStory').value;
        const r = await fetch('/play_audio', { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({ volume: (vol/100).toFixed(2), file }) });
        setStatus(await r.text());
      } catch(e) { setStatus(e.message, true); }
    }
    document.getElementById('btnLoveStory').onclick = (ev) => playMusicClick(ev, 'lovestory.mp3');
    document.getElementById('btnMbg').onclick = (ev) => playMusicClick(ev, 'mbg.mp3');
    async function dfPlayerControl(action){
      try{
        const volume=Number(document.getElementById('volDf').value||22);
        const r=await fetch('/dfplayer',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action,track:1,volume})});
        setStatus(await r.text(),!r.ok);
      }catch(e){setStatus(e.message,true);}
    }
    document.getElementById('btnDfPlay').onclick=()=>dfPlayerControl('PLAY');
    document.getElementById('btnDfStop').onclick=()=>dfPlayerControl('STOP');
    document.getElementById('volDf').addEventListener('change',()=>dfPlayerControl('VOL'));
    document.getElementById('btnTestMax').onclick = async (ev) => {
      ev.preventDefault();
      ev.stopPropagation();
      try {
        const vol = document.getElementById('volLoveStory').value;
        const r = await fetch('/test_max', { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({ volume: (vol/100).toFixed(2) }) });
        setStatus(await r.text());
      } catch(e) { setStatus(e.message, true); }
    };
    document.getElementById('btnStopAudio').onclick = async (ev) => {
      ev.preventDefault();
      ev.stopPropagation();
      try {
        const r = await fetch('/stop_audio', { method:'POST' });
        setStatus(await r.text());
      } catch(e) { setStatus(e.message, true); }
    };

    const drawCanvas=document.getElementById('drawCanvas');
    const drawCtx=drawCanvas.getContext('2d',{willReadFrequently:true});
    const drawSyncState=document.getElementById('drawSyncState');
    drawCtx.fillStyle='#000';drawCtx.fillRect(0,0,128,64);
    drawCtx.strokeStyle='#fff';drawCtx.fillStyle='#fff';drawCtx.lineCap='round';drawCtx.lineJoin='round';
    let drawing=false,lastPt=null,drawModeReady=false,drawSyncTimer=null,drawSyncBusy=false,drawSyncPending=false;
    function setDrawSyncState(text,bad){
      drawSyncState.textContent=text;
      drawSyncState.style.color=bad?'var(--danger)':'var(--success)';
    }
    async function enterDrawMode(){
      if(drawModeReady)return;
      const r=await fetch('/cmd/W',{method:'POST'});
      const text=await r.text();
      if(!r.ok)throw new Error(text||'Gagal masuk draw');
      drawModeReady=true;
      setDrawSyncState('LIVE DRAW AKTIF',false);
    }
    function canvasPoint(ev){
      const r=drawCanvas.getBoundingClientRect();
      const src=ev.touches&&ev.touches[0]?ev.touches[0]:ev;
      return {x:Math.max(0,Math.min(127,Math.floor((src.clientX-r.left)*128/r.width))),y:Math.max(0,Math.min(63,Math.floor((src.clientY-r.top)*64/r.height)))};
    }
    function drawAt(pt){
      const b=Number(document.getElementById('brushSize').value||3);
      drawCtx.lineWidth=b;
      drawCtx.strokeStyle='#fff';drawCtx.fillStyle='#fff';
      if(lastPt){drawCtx.beginPath();drawCtx.moveTo(lastPt.x,lastPt.y);drawCtx.lineTo(pt.x,pt.y);drawCtx.stroke();}
      drawCtx.beginPath();drawCtx.arc(pt.x,pt.y,Math.max(0.5,b/2),0,Math.PI*2);drawCtx.fill();
      lastPt=pt;
      scheduleDrawSync();
    }
    function down(ev){ev.preventDefault();drawing=true;lastPt=null;drawAt(canvasPoint(ev));}
    function move(ev){if(!drawing)return;ev.preventDefault();drawAt(canvasPoint(ev));}
    function up(){drawing=false;lastPt=null;}
    drawCanvas.addEventListener('pointerdown',down);
    drawCanvas.addEventListener('pointermove',move);
    window.addEventListener('pointerup',up);
    drawCanvas.addEventListener('touchstart',down,{passive:false});
    drawCanvas.addEventListener('touchmove',move,{passive:false});
    window.addEventListener('touchend',up);
    function canvasToOledBytes(){
      const img=drawCtx.getImageData(0,0,128,64).data;
      const out=new Uint8Array(1024);
      for(let y=0;y<64;y++){
        for(let xb=0;xb<16;xb++){
          let v=0;
          for(let bit=0;bit<8;bit++){
            const x=xb*8+bit;
            const idx=(y*128+x)*4;
            const on=img[idx]+img[idx+1]+img[idx+2]>384;
            if(on)v|=(0x80>>bit);
          }
          out[y*16+xb]=v;
        }
      }
      return out;
    }
    async function sendDrawFrame(showStatus){
      await enterDrawMode();
      const bytes=canvasToOledBytes();
      const r=await fetch('/frame',{method:'POST',headers:{'Content-Type':'application/octet-stream'},body:bytes});
      const text=await r.text();
      if(!r.ok)throw new Error(text||'Frame gagal');
      if(showStatus)setStatus(text,false);
      setDrawSyncState('LIVE SYNC '+new Date().toLocaleTimeString('id-ID',{hour12:false}),false);
    }
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
      try{await sendDrawFrame(false);}
      catch(e){setDrawSyncState(e.message,true);}
      finally{
        drawSyncBusy=false;
        if(drawSyncPending)scheduleDrawSync();
      }
    }
    document.getElementById('enterDraw').onclick=async()=>{try{drawModeReady=false;await enterDrawMode();await sendDrawFrame(true);}catch(e){setStatus(e.message,true);setDrawSyncState(e.message,true);}};
    document.getElementById('clearDraw').onclick=async()=>{drawCtx.fillStyle='#000';drawCtx.fillRect(0,0,128,64);try{await sendDrawFrame(true);}catch(e){setStatus(e.message,true);setDrawSyncState(e.message,true);}};

    document.querySelectorAll('[data-cmd]').forEach(btn=>btn.onclick=async()=>{
      try{
        let r = await fetch('/cmd/'+btn.dataset.cmd,{method:'POST'});
        setStatus(await r.text());
        if(btn.dataset.cmd==='C'){
          drawModeReady=false;
          setDrawSyncState('LIVE DRAW SIAP',false);
        }
      }catch(e){setStatus(e.message,true);}
    });
    document.getElementById('logoutBtn').onclick=()=>{localStorage.removeItem('owi_current_user');location.href='/';};

    async function refreshSensors(){
      try{
        const r=await fetch('/api/sensors');const s=await r.json();
        if(!s.lastUpdate)return;
        const bMpu=document.getElementById('badgeMpu');
        bMpu.textContent='MPU: '+(s.mpu==1?'OK':'ERR');
        bMpu.className='badge '+(s.mpu==1?'ok':'err');
        const bInmp=document.getElementById('badgeInmp');
        const inmpPct=s.inmp||0;
        bInmp.textContent='INMP: '+inmpPct+'%';
        bInmp.className='badge '+(inmpPct>0?'ok':'');
        const inmpPeak=s.inmpPeak||0;
        document.getElementById('inmpLevelBar').style.width=Math.max(0,Math.min(100,inmpPct))+'%';
        document.getElementById('inmpLevelText').textContent=inmpPct+'%';
        const inmpActive=document.getElementById('inmpActiveBadge');
        inmpActive.textContent=s.micActive?'MENDENGAR':'IDLE';
        inmpActive.classList.toggle('on',!!s.micActive);
        const inmpPeakEl=document.getElementById('inmpPeakBadge');
        inmpPeakEl.textContent='PEAK '+inmpPeak+'%';
        inmpPeakEl.classList.toggle('on',inmpPeak>25);
        const bMax=document.getElementById('badgeMax');
        bMax.textContent=(s.max==1?'🔊 MAX: PLAY':'🔈 MAX: IDLE');
        bMax.className='badge '+(s.max==1?'active':'');
        const bDf=document.getElementById('badgeDf');
        bDf.textContent=s.df==1?(s.dfPlaying==1?'DF: PLAY '+String(s.dfTrack||1).padStart(4,'0'):'DF: OK'):'DF: ERR';
        bDf.className='badge '+(s.df==1?(s.dfPlaying==1?'active':'ok'):'err');

        const gMap={touch:s.touch,nod:s.nod,headShake:s.headShake,surprised:s.surprised,curious:s.curious,angry:s.angry,laugh:s.laugh,sleep:s.sleep,dizzy:s.dizzy,sad:s.sad,love:s.love,cry:s.cry,pant:s.pant};
        document.querySelectorAll('.gesture-badge').forEach(el=>{el.classList.toggle('on',!!gMap[el.dataset.g]);});

        const temp=s.temp;
        document.getElementById('valTemp').textContent=(temp&&temp>-90)?temp.toFixed(1):'--';
        document.getElementById('valHum').textContent=(s.hum&&s.hum>=0)?s.hum.toFixed(0):'--';
        document.getElementById('valShake').textContent=Number(s.shakeMeter||0).toFixed(1);

        // Expression
        const exprMap = ["TIDAK DIKETAHUI", "NORMAL", "SENANG", "MARAH", "KAGET", "SEDIH", "TIDUR", "CINTA", "MENGUAP", "KEDIP", "BERKEDIP CEPAT", "MENANGIS", "PUSING", "GELENG", "MENGANGGUK"];
        if(s.expr !== undefined) {
          const eStr = exprMap[s.expr] || s.expr;
          document.getElementById('valExpr').textContent=eStr;
          document.getElementById('faceLabel').textContent=eStr;
        }

        const stateMap = ["WAJAH NORMAL", "MENU UTAMA", "GAMES PINGPONG", "SENSOR SUHU", "REMINDER ALARM", "DRAW OLED", "PILIH LAGU"];
        if(s.state !== undefined && s.state >= 0 && s.state < stateMap.length) {
          document.getElementById('menuStateLabel').textContent = stateMap[s.state];
        }

        if(s.scoreP!==undefined)document.getElementById('scoreP').textContent=s.scoreP;
        if(s.scoreA!==undefined)document.getElementById('scoreA').textContent=s.scoreA;

        const faceEl=document.querySelector('.face');
        if(faceEl)faceEl.style.transform='translate('+(s.tiltX*40||0)+'px, '+(s.tiltY*30||0)+'px)';
        if(s.ip)document.getElementById('ipLabel').textContent='IP: '+s.ip;
        setStatus('TILT X:'+Number(s.tiltX||0).toFixed(2)+' Y:'+Number(s.tiltY||0).toFixed(2)+' | SHAKE:'+Number(s.shakeMeter||0).toFixed(2));
      }catch(e){}
    }
    setInterval(refreshSensors,250);

    async function refreshAiLimit(){
      try{
        const r=await fetch('/api/ai-limit');const s=await r.json();
        const b=document.getElementById('aiLimitBadge');
        b.textContent='AI: '+s.used+'/'+s.limit+' SISA '+s.remaining;
        b.className='badge '+(s.remaining<=3?'err':s.remaining<=8?'active':'ok');
        const k=document.getElementById('aiKeyBadge');
        k.textContent=s.enabled?'KEY: SIAP':'KEY: BELUM';
        k.className='badge '+(s.enabled?'ok':'err');
      }catch(e){}
    }
    refreshAiLimit();
    setInterval(refreshAiLimit,5000);

    // ─── SPEECH RECOGNITION (Web Speech API - id-ID) ───
    let recognition = null;
    let isListening = false;
    const speechLive = document.getElementById('speechLive');
    const speechLog = document.getElementById('speechLog');
    const speechStatus = document.getElementById('speechStatus');

    function initSpeech() {
      const SR = window.SpeechRecognition || window.webkitSpeechRecognition;
      if (!SR) { speechStatus.textContent = 'TIDAK DIDUKUNG'; return null; }
      const r = new SR();
      r.lang = 'id-ID';
      r.continuous = true;
      r.interimResults = true;
      r.maxAlternatives = 1;
      r.onstart = () => { isListening = true; speechStatus.textContent = 'MENDENGAR...'; speechStatus.style.color = 'var(--accent)'; };
      r.onend = () => { if (isListening) { try { r.start(); } catch(e){} } else { speechStatus.textContent = 'IDLE'; speechStatus.style.color = '#999'; } };
      r.onerror = (e) => { if (e.error !== 'no-speech' && e.error !== 'aborted') { speechStatus.textContent = 'ERR: ' + e.error; } };
      r.onresult = (e) => {
        let interim = '';
        for (let i = e.resultIndex; i < e.results.length; i++) {
          const t = e.results[i][0].transcript;
          if (e.results[i].isFinal) {
            const ts = new Date().toLocaleTimeString('id-ID',{hour:'2-digit',minute:'2-digit',second:'2-digit'});
            const line = document.createElement('div');
            line.textContent = '[' + ts + '] ' + t;
            speechLog.prepend(line);
            speechLive.textContent = t;
            speechLive.style.color = 'var(--success)';
            // DENGAR -> PAHAM -> JAWAB: transcript final masuk ke chatbot, bukan reminder.
            if (chatInput && sendChatBtn) {
              chatInput.value = t.trim();
              sendChatBtn.click();
            }
          } else {
            interim += t;
          }
        }
        if (interim) { speechLive.textContent = interim; speechLive.style.color = '#ffff00'; }
      };
      return r;
    }

    document.getElementById('startSpeech').onclick = () => {
      if (!recognition) recognition = initSpeech();
      if (!recognition) return;
      isListening = true;
      try { recognition.start(); } catch(e) {}
    };
    document.getElementById('stopSpeech').onclick = () => {
      isListening = false;
      if (recognition) try { recognition.stop(); } catch(e) {}
      speechStatus.textContent = 'IDLE';
      speechLive.textContent = '...';
    };

    // Chatbot UI Logic
    const chatInput = document.getElementById('chatInput');
    const sendChatBtn = document.getElementById('sendChatBtn');
    const chatHistory = document.getElementById('chatHistory');
    const chatSpeak = document.getElementById('chatSpeak');
    const chatVoiceVol = document.getElementById('chatVoiceVol');

    function appendChat(sender, msg, color, bg) {
      const bubble = document.createElement('div');
      bubble.style.padding = '0.5rem 0.8rem';
      bubble.style.borderRadius = '8px';
      bubble.style.maxWidth = '85%';
      bubble.style.background = bg;
      bubble.style.color = color;
      bubble.style.alignSelf = sender === 'User' ? 'flex-end' : 'flex-start';
      bubble.style.boxShadow = '1px 1px 0 #000';
      const strong = document.createElement('strong');
      strong.textContent = sender;
      bubble.appendChild(strong);
      bubble.appendChild(document.createElement('br'));
      bubble.appendChild(document.createTextNode(msg));
      chatHistory.appendChild(bubble);
      chatHistory.scrollTop = chatHistory.scrollHeight;
    }

    sendChatBtn.onclick = () => {
      const msg = chatInput.value.trim();
      if (!msg) return;
      chatInput.value = '';
      sendChatBtn.disabled = true;
      appendChat('Kamu', msg, '#fff', 'var(--accent)');
      
      fetch('/api/chat', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          message: msg,
          speak: !!(chatSpeak && chatSpeak.checked),
          voiceVolume: chatVoiceVol ? (Number(chatVoiceVol.value) / 100).toFixed(2) : '0.24'
        })
      })
      .then(r => r.json())
      .then(res => {
        sendChatBtn.disabled = false;
        if (res.error) {
          appendChat('Error', res.error, '#fff', 'var(--error)');
        } else {
          appendChat('Owi (' + (res.model || res.provider || 'AI') + ')', res.response, '#000', '#f1f1f1');
          if (res.oledSent === false) appendChat('OLED', 'Belum terkirim ke OLED: ' + (res.oledError || 'serial error'), '#fff', 'var(--error)');
          if (res.speechError) appendChat('VOICE', 'Suara belum keluar: ' + res.speechError, '#fff', 'var(--error)');
          refreshAiLimit();
        }
      })
      .catch(e => {
        sendChatBtn.disabled = false;
        appendChat('Error', 'Gagal memanggil API', '#fff', 'var(--error)');
      });
    };
    
    chatInput.addEventListener('keypress', function (e) {
      if (e.key === 'Enter') sendChatBtn.onclick();
    });

  </script>