import re

with open('web_serial_server.js', 'r', encoding='utf-8') as f:
    js_content = f.read()

# Increase packet size and lead time in streamAudioToWS
js_content = re.sub(r'const packetBytes = 1024;', 'const packetBytes = 2048;', js_content)
js_content = re.sub(r'const leadMs = 260;', 'const leadMs = 450;', js_content)

with open('web_serial_server.js', 'w', encoding='utf-8') as f:
    f.write(js_content)

with open('src/gembot.cpp', 'r', encoding='utf-8') as f:
    cpp_content = f.read()

# Increase prebuffering in ESP32
cpp_content = re.sub(r'const size_t TTS_PREBUFFER_BYTES = 6144;', 'const size_t TTS_PREBUFFER_BYTES = 16384;', cpp_content)
cpp_content = re.sub(r'const unsigned long TTS_PREBUFFER_MAX_MS = 650UL;', 'const unsigned long TTS_PREBUFFER_MAX_MS = 1200UL;', cpp_content)

# Fix mouth level
cpp_content = re.sub(r'playedLevel = constrain\(\(\(float\)levelSum / samples\) / 9000\.0f, 0\.0f, 1\.25f\);', 'playedLevel = constrain(((float)levelSum / samples) / 3000.0f, 0.0f, 1.5f);', cpp_content)
cpp_content = re.sub(r'ttsMouthLevel = ttsMouthLevel \* 0\.52f \+ playedLevel \* 0\.48f;', 'ttsMouthLevel = ttsMouthLevel * 0.40f + playedLevel * 0.60f;', cpp_content)

with open('src/gembot.cpp', 'w', encoding='utf-8') as f:
    f.write(cpp_content)

print("Patch applied.")
