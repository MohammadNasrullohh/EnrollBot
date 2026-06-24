import re

with open('web_serial_server.js', 'r', encoding='utf-8') as f:
    content = f.read()

content = re.sub(
    r"<label[^>]*>BRUSH\s*<input type=\"range\" id=\"brushSize\"[^>]*>\s*</label>",
    '''<div style="display:flex;gap:10px;">
              <label style="flex:1;font-family:'Roboto Mono',monospace;font-size:0.75rem;font-weight:800;">BRUSH
                <input type="range" id="brushSize" min="1" max="15" value="3" style="width:100%;margin-top:0.4rem;">
              </label>
              <label style="flex:1;font-family:'Roboto Mono',monospace;font-size:0.75rem;font-weight:800;">WARNA
                <input type="color" id="drawColor" value="#ffffff" style="width:100%;height:24px;border:none;padding:0;cursor:pointer;margin-top:0.2rem;border-radius:4px;">
              </label>
              <label style="flex:1;font-family:'Roboto Mono',monospace;font-size:0.75rem;font-weight:800;">BG CANVAS
                <input type="color" id="canvasColor" value="#000000" style="width:100%;height:24px;border:none;padding:0;cursor:pointer;margin-top:0.2rem;border-radius:4px;">
              </label>
            </div>''',
    content
)

with open('web_serial_server.js', 'w', encoding='utf-8') as f:
    f.write(content)
print("Done UI")
