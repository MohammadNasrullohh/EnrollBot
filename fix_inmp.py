import re

with open('src/gembot.cpp', 'r', encoding='utf-8') as f:
    cpp_content = f.read()

cpp_content = cpp_content.replace('initMicI2S();', '// initMicI2S();')

with open('src/gembot.cpp', 'w', encoding='utf-8') as f:
    f.write(cpp_content)

print("INMP disabled.")
