import re

with open('web_serial_server.js', 'r', encoding='utf-8') as f:
    content = f.read()

# Add sendDfPlayer if not exists
if 'async function sendDfPlayer' not in content:
    send_df_func = '''
async function sendDfPlayer(action, track = 1, volume = 22) {
  const safeAction = String(action || "").toUpperCase();
  let payload = "";
  if (safeAction === "PLAY") payload = DFP:PLAY:;
  else if (safeAction === "STOP") payload = "DFP:STOP";
  else if (safeAction === "PAUSE") payload = "DFP:PAUSE";
  else if (safeAction === "RESUME") payload = "DFP:RESUME";
  else if (safeAction === "VOL") payload = DFP:VOL:;
  else throw new Error("Action DFPlayer tidak valid");
  logEvent(dfplayer );
  await sendCommand(payload);
}
'''
    content = content.replace('async function sendCommand', send_df_func + '\nasync function sendCommand')

# Replace the playlist
old_playlist = '''<div class="song" data-file="mbg.mp3"><span>1</span><strong>MBG</strong><small>MBG Anthem 2:40</small></div><div class="song" data-file="hai_owi.wav"><span>2</span><strong>Save Your Tears</strong><small>The Weeknd 2:04</small></div><div class="song active" data-file="lovestory.mp3"><span>3</span><strong>Love Story</strong><small>Taylor Swift 3:01</small></div><div class="song" data-file="DFP"><span>4</span><strong>Tarot</strong><small>Hindia 3:05</small></div><div class="song" data-file="DFP2"><span>5</span><strong>Kasih Aba-aba</strong><small>Tenxi, Naykilla 3:09</small></div>'''

new_playlist = '''<div class="song" data-file="DFP:2"><span>1</span><strong>MBG</strong><small>MBG Anthem 2:40</small></div><div class="song" data-file="hai_owi.wav"><span>2</span><strong>Save Your Tears</strong><small>The Weeknd 2:04</small></div><div class="song active" data-file="DFP:1"><span>3</span><strong>Love Story</strong><small>Taylor Swift 3:01</small></div><div class="song" data-file="DFP:4"><span>4</span><strong>Tarot</strong><small>Hindia 3:05</small></div><div class="song" data-file="DFP:5"><span>5</span><strong>Kasih Aba-aba</strong><small>Tenxi, Naykilla 3:09</small></div>'''

content = content.replace(old_playlist, new_playlist)

# Replace btnPlaySong onclick
old_onclick = "btnPlaySong.onclick=playCurrent;"
new_onclick = '''
    let isPlaying = false;
    const playIcon = '<svg width="18" height="18" viewBox="0 0 24 24" fill="currentColor"><path d="M6 4h4v16H6V4zm8 0h4v16h-4V4z"/></svg>';
    const pauseIcon = '<svg width="18" height="18" viewBox="0 0 24 24" fill="currentColor"><rect x="6" y="4" width="4" height="16"/><rect x="14" y="4" width="4" height="16"/></svg>';
    
    btnPlaySong.onclick = async () => {
      if (currentFile.startsWith('DFP:')) {
        if (isPlaying) {
          await fetch('/dfplayer',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'PAUSE'})});
          isPlaying = false;
          btnPlaySong.innerHTML = playIcon;
        } else {
          // If we haven't played yet, we might need to send PLAY. But RESUME works if it was paused.
          // Let's just send playCurrent if we want to play from start, but we need to track if it's paused.
          // For simplicity, let's just always PLAY the track if it's not playing, or RESUME.
          // Let's just always call playCurrent() because myDFPlayer.start() works for resume.
          await playCurrent();
          isPlaying = true;
          btnPlaySong.innerHTML = pauseIcon;
        }
      } else {
        await playCurrent();
      }
    };
    
    // When clicking a song, reset play state and play it
    document.querySelectorAll('.song').forEach(el => {
      el.addEventListener('click', () => {
        isPlaying = true;
        btnPlaySong.innerHTML = pauseIcon;
      });
    });
'''
content = content.replace(old_onclick, new_onclick)

# Replace playCurrent
old_playcurrent = "    async function playCurrent(){try{if(currentFile==='DFP'){const r=await fetch('/dfplayer',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'PLAY',track:1,volume:22})});setStatus(await r.text(),!r.ok);return}const vol=(volLoveStory.value/100).toFixed(2);const r=await fetch('/play_audio',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({file:currentFile,volume:vol})});setStatus(await r.text(),!r.ok)}catch(e){setStatus(e.message,true)}}"

new_playcurrent = '''    async function playCurrent(){try{if(currentFile.startsWith('DFP:')){const track = parseInt(currentFile.split(':')[1]) || 1; const r=await fetch('/dfplayer',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'PLAY',track:track,volume:22})});setStatus(await r.text(),!r.ok);return}const vol=(volLoveStory.value/100).toFixed(2);const r=await fetch('/play_audio',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({file:currentFile,volume:vol})});setStatus(await r.text(),!r.ok)}catch(e){setStatus(e.message,true)}}'''

content = content.replace(old_playcurrent, new_playcurrent)

with open('web_serial_server.js', 'w', encoding='utf-8') as f:
    f.write(content)
print("web_serial_server.js patched!")
