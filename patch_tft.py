import re

with open('web_serial_server.js', 'r', encoding='utf-8') as f:
    content = f.read()

bad_html = '''<button id="clearDraw" class="primary" style="height:46px">Clear Canvas</button><div style="grid-column:1/-1;text-align:center">'''

good_html = '''<button id="clearDraw" class="primary" style="height:46px">Clear Canvas</button>
<div style="grid-column:1/-1; display:flex; gap:10px; margin-top:10px;">
  <label style="flex:1;font-family:'Roboto Mono',monospace;font-size:0.75rem;font-weight:800;color:var(--text-light)">BRUSH
    <input type="range" id="brushSize" min="1" max="15" value="3" style="width:100%;margin-top:0.4rem;">
  </label>
  <label style="flex:1;font-family:'Roboto Mono',monospace;font-size:0.75rem;font-weight:800;color:var(--text-light)">WARNA
    <input type="color" id="drawColor" value="#ffffff" style="width:100%;height:24px;border:none;padding:0;cursor:pointer;margin-top:0.2rem;border-radius:4px;">
  </label>
  <label style="flex:1;font-family:'Roboto Mono',monospace;font-size:0.75rem;font-weight:800;color:var(--text-light)">BG CANVAS
    <input type="color" id="canvasColor" value="#000000" style="width:100%;height:24px;border:none;padding:0;cursor:pointer;margin-top:0.2rem;border-radius:4px;">
  </label>
</div>
<div style="grid-column:1/-1;text-align:center">'''

content = content.replace(bad_html, good_html)

# Also rename the second canvas ID so it doesn't conflict, wait no, 
# The javascript uses const c=drawCanvas which will pick the FIRST one if not careful.
# But since I already injected const c=document.querySelectorAll('.drawCanvas')[1] or something?
# Let's check the JS for the second canvas:
# We used const c=drawCanvas in both scripts. The first script is inline. The second script is at the bottom.
# If $('drawCanvas') returns the first one, then the second canvas never got the event listeners!!
# Oh my god. 

bad_js_c = '''const c=drawCanvas,ctx=c.getContext('2d',{willReadFrequently:true});'''
good_js_c = '''const c=document.querySelectorAll('canvas')[1],ctx=c.getContext('2d',{willReadFrequently:true});'''

content = content.replace(bad_js_c, good_js_c)

with open('web_serial_server.js', 'w', encoding='utf-8') as f:
    f.write(content)
print("Done")
