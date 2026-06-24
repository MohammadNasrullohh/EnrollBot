import re

with open('web_serial_server.js', 'r', encoding='utf-8') as f:
    content = f.read()

bad1 = "if (safeAction === \"PLAY\") payload = DFP:PLAY:;"
good1 = 'if (safeAction === "PLAY") payload = DFP:PLAY:;'

bad2 = "else if (safeAction === \"VOL\") payload = DFP:VOL:;"
good2 = 'else if (safeAction === "VOL") payload = DFP:VOL:;'

content = content.replace(bad1, good1).replace(bad2, good2)

with open('web_serial_server.js', 'w', encoding='utf-8') as f:
    f.write(content)
