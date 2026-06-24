import re

with open('src/gembot.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

bad_drw = '''      } else if (text.startsWith("CMD:T:")) {'''

good_drw = '''      } else if (text.startsWith("CMD:DRW:")) {
         if (!isDrawMode) return;
         String data = text.substring(8);
         int start = 0;
         while (start < data.length()) {
            int end = data.indexOf('|', start);
            if (end == -1) end = data.length();
            String cmd = data.substring(start, end);
            
            int p1 = cmd.indexOf(',');
            int p2 = cmd.indexOf(',', p1 + 1);
            int p3 = cmd.indexOf(',', p2 + 1);
            int p4 = cmd.indexOf(',', p3 + 1);
            int p5 = cmd.indexOf(',', p4 + 1);
            
            if (p1 > 0 && p2 > 0 && p3 > 0 && p4 > 0 && p5 > 0) {
                int x1 = cmd.substring(0, p1).toInt();
                int y1 = cmd.substring(p1 + 1, p2).toInt();
                int x2 = cmd.substring(p2 + 1, p3).toInt();
                int y2 = cmd.substring(p3 + 1, p4).toInt();
                uint16_t c = cmd.substring(p4 + 1, p5).toInt();
                int s = cmd.substring(p5 + 1).toInt();
                
                if (s <= 1) {
                    spr.drawLine(x1, y1, x2, y2, c);
                } else {
                    spr.fillCircle(x2, y2, s/2, c);
                    int dx = x2 - x1;
                    int dy = y2 - y1;
                    int steps = max(abs(dx), abs(dy)) / (s/2 + 1);
                    if (steps > 0) {
                        for (int i = 0; i <= steps; i++) {
                            int ix = x1 + dx * i / steps;
                            int iy = y1 + dy * i / steps;
                            spr.fillCircle(ix, iy, s/2, c);
                        }
                    }
                }
            }
            start = end + 1;
         }
         spr.pushSprite(0, 0);
      } else if (text.startsWith("CMD:T:")) {'''

content = content.replace(bad_drw, good_drw)

with open('src/gembot.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
