import re

with open('web_serial_server.js', 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Game Editor Canvas: Make sure ID is "gameCanvas"
# We changed it to gameCanvas already, let's verify.
# Change the JS reference in the Game Editor block from drawCanvas to gameCanvas
content = content.replace("document.getElementById('drawCanvas')", "document.getElementById('gameCanvas')")

# 2. TFT Draw Canvas: Make sure ID is "tftCanvas"
# The HTML was changed to tftCanvas. Let's fix the JS block.
content = content.replace("const c=drawCanvas,ctx=c.getContext('2d'", "const c=tftCanvas,ctx=c.getContext('2d'")

with open('web_serial_server.js', 'w', encoding='utf-8') as f:
    f.write(content)
print("Robust fix applied")
