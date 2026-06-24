import re
with open('web_serial_server.js', 'r', encoding='utf-8') as f:
    content = f.read()

content = content.replace('const AI_MAX_TOKENS = Number(process.env.AI_MAX_TOKENS || 256);', 'const AI_MAX_TOKENS = Number(process.env.AI_MAX_TOKENS || 1024);')
content = content.replace('if (cleaned.length > 200) cleaned = cleaned.substring(0, 197) + \"...\";', 'if (cleaned.length > 1000) cleaned = cleaned.substring(0, 997) + \"...\";')

lines = content.split('\n')
new_lines = []
skip = False
for i, line in enumerate(lines):
    if line.startswith('function clampVolume(value, fallback = 0.22) {'):
        skip = True
    if skip and line.startswith('}'):
        skip = False
        continue
    if not skip:
        new_lines.append(line)

content = '\n'.join(new_lines)

content = content.replace('return Math.max(0.04, Math.min(0.85, parsed));', 'return Math.max(0.04, Math.min(0.99, parsed));')

with open('web_serial_server.js', 'w', encoding='utf-8') as f:
    f.write(content)
print("Fixed!")
