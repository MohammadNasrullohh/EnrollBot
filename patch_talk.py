import re

with open('src/gembot.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

old_logic = '''        default: // Normal
            break;
    }'''

new_logic = '''        default: // Normal
            break;
    }

    // Chatbot speaking override
    if (isChatActive) {
        float talkCycle = sin(now * 0.035f); 
        targetMouthScaleY = 0.8f + (talkCycle * 1.5f); 
        if (targetMouthScaleY < 0.2f) targetMouthScaleY = 0.2f;
        targetMouthScaleX = 0.8f + (cos(now * 0.02f) * 0.4f);
        
        targetEyeScaleY = 0.85f + (sin(now * 0.015f) * 0.1f); 
        exprBobY = sin(now * 0.02f) * 4.0f; 
    }'''

content = content.replace(old_logic, new_logic)

with open('src/gembot.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print("Talk override applied.")
