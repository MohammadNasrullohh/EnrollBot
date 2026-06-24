import re
with open('web_serial_server.js', 'r', encoding='utf-8') as f:
    content = f.read()

content = content.replace("const c=drawCanvas", "const c=tftCanvas")

with open('web_serial_server.js', 'w', encoding='utf-8') as f:
    f.write(content)
print("Done")
