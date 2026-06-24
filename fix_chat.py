import re
import os

files_to_patch = ['web_serial_server.js', 'web_serial_server_new.js', 'backend_mod1.js', 'backend_only.js', 'vps_server.js']

old_prompt_regex = re.compile(r'const OWI_SYSTEM_PROMPT = ".*?";')
new_prompt = 'const OWI_SYSTEM_PROMPT = "Kamu adalah Owi, robot desktop peliharaan cerdas berbasis ESP32. Bicaralah dengan bahasa Indonesia yang natural, asyik, dan to-the-point layaknya teman cowok santai. JANGAN ALAY, JANGAN CRINGE. DILARANG KERAS memanggil dengan kata \'besti\', \'bosku\', atau \'bro\'. Cukup panggil \'Bos\' atau \'kamu\'. Jawab sangat singkat (maksimal 2 kalimat pendek) agar tidak ngelag. Fakta penting: Jika ditanya siapa Eca, jawab bahwa Eca adalah orang paling plenger.";'

for fname in files_to_patch:
    if os.path.exists(fname):
        with open(fname, 'r', encoding='utf-8') as f:
            content = f.read()
            content = old_prompt_regex.sub(new_prompt, content)
            
            content = content.replace("clampVolume(data.voiceVolume, 0.24)", "clampVolume(data.voiceVolume, 0.85)")
            content = content.replace("voiceVolume: chatVoiceVol ? (Number(chatVoiceVol.value) / 100).toFixed(2) : '0.24'", "voiceVolume: chatVoiceVol ? (Number(chatVoiceVol.value) / 100).toFixed(2) : '0.85'")
            content = content.replace("volume = '0.30'", "volume = '0.85'")
            content = content.replace('volume = "0.45"', 'volume = "0.85"')
            content = content.replace("fallback = 0.45", "fallback = 0.85")
            
            content = content.replace("const MAX_CHUNK_SIZE = 1024;", "const MAX_CHUNK_SIZE = 2048;")
            content = content.replace("await sleep(durationMs * 0.8);", "await sleep(durationMs * 0.95);")

        with open(fname, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Patched {fname}")

