import re

with open('web_serial_server.js', 'r', encoding='utf-8') as f:
    content = f.read()

bad = '''          } else {
            await sendReminderSchedule(data.time, data.text);
  if (req.method === "POST" && req.url === "/frame") {'''

good = '''          } else {
            await sendReminderSchedule(data.time, data.text);
            res.end("Reminder jam tersimpan");
          }
        } else {
          await sendReminderText(text);
          res.end("Reminder teks tersimpan");
        }
      } catch (err) {
        res.writeHead(500);
        res.end(err.message);
      }
    });
    return;
  }
  if (req.method === "POST" && req.url === "/frame") {'''

content = content.replace(bad, good)

with open('web_serial_server.js', 'w', encoding='utf-8') as f:
    f.write(content)
