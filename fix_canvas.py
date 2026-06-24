import re

with open('web_serial_server.js', 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Fix Game Editor Canvas HTML
content = content.replace(
    '<canvas id="drawCanvas" width="128"', 
    '<canvas id="gameCanvas" width="128"'
)

# 2. Fix Game Editor JS
# It's inside a script tag right after the canvas.
# But since we replaced const c=drawCanvas with const c=document.querySelectorAll('canvas')[1]
# we need to change it back to const c=gameCanvas
content = content.replace(
    "const c=document.querySelectorAll('canvas')[1],ctx=c.getContext('2d'",
    "const c=gameCanvas,ctx=c.getContext('2d'"
)

# 3. Fix TFT Draw Canvas HTML
content = content.replace(
    '<canvas id="drawCanvas" class="drawCanvas"',
    '<canvas id="tftCanvas" class="drawCanvas"'
)

# 4. Fix TFT Draw JS
# It currently has const c=drawCanvas,ctx=c.getContext('2d',{willReadFrequently:true});
content = content.replace(
    "const c=drawCanvas,ctx=c.getContext('2d',{willReadFrequently:true});",
    "const c=tftCanvas,ctx=c.getContext('2d',{willReadFrequently:true});"
)

# 5. Fix duplicate IDs for inputs
# The TFT panel has <input type="range" id="brushSize" ... Let's replace the TFT ones.
# Actually, the FIRST one (Game Editor) has:
# <input type="range" id="brushSize" min="1" max="15" value="3" style="width:100%;margin-top:0.4rem;">
# <input type="color" id="drawColor" ...
# <input type="color" id="canvasColor" ...
# But wait, does the Game Editor NEED a canvas color or brush size? 
# The Game Editor is black and white (pixel art)! It doesn't need color pickers!
# I accidentally added the color pickers to the Game Editor in patch_ui_py.py!
# Let's REMOVE the color pickers from the Game Editor!
# The Game Editor originally had just a brush size slider.
bad_game_editor_ui = '''<div style="display:flex;gap:10px;">
              <label style="flex:1;font-family:'Roboto Mono',monospace;font-size:0.75rem;font-weight:800;">BRUSH
                <input type="range" id="brushSize" min="1" max="15" value="3" style="width:100%;margin-top:0.4rem;">
              </label>
              <label style="flex:1;font-family:'Roboto Mono',monospace;font-size:0.75rem;font-weight:800;">WARNA
                <input type="color" id="drawColor" value="#ffffff" style="width:100%;height:24px;border:none;padding:0;cursor:pointer;margin-top:0.2rem;border-radius:4px;">
              </label>
              <label style="flex:1;font-family:'Roboto Mono',monospace;font-size:0.75rem;font-weight:800;">BG CANVAS
                <input type="color" id="canvasColor" value="#000000" style="width:100%;height:24px;border:none;padding:0;cursor:pointer;margin-top:0.2rem;border-radius:4px;">
              </label>
            </div>'''
good_game_editor_ui = '''<label style="display:block;margin-top:10px;font-weight:800;font-size:0.8rem;color:#182336;">Ukuran Brush <input type="range" id="gameBrushSize" min="1" max="7" value="2" style="width:100%;margin-top:0.4rem;"></label>'''
content = content.replace(bad_game_editor_ui, good_game_editor_ui)

# And fix the Game Editor JS to use gameBrushSize
# Oh wait, did the Game Editor JS use rushSize?
# In Game Editor JS: ctx.lineWidth = document.getElementById('brushSize').value?
# Let's check ctx.lineWidth in the whole file later if needed.
content = content.replace("getElementById('brushSize')", "getElementById('tftBrushSize')")
content = content.replace("brushSize", "gameBrushSize") # if any
# We'll replace all TFT input IDs manually now:
content = content.replace('id="brushSize"', 'id="tftBrushSize"')
content = content.replace('id="drawColor"', 'id="tftDrawColor"')
content = content.replace('id="canvasColor"', 'id="tftCanvasColor"')

# Fix TFT JS references
content = content.replace("getElementById('tftDrawColor')", "getElementById('tftDrawColor')") # already matched by wildcard above
content = content.replace("getElementById('tftCanvasColor')", "getElementById('tftCanvasColor')")
content = content.replace("getElementById('canvasColor')", "getElementById('tftCanvasColor')")
content = content.replace("getElementById('drawColor')", "getElementById('tftDrawColor')")

with open('web_serial_server.js', 'w', encoding='utf-8') as f:
    f.write(content)
print("Done fixing HTML and IDs")
