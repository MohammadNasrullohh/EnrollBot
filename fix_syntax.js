const fs = require('fs');
let code = fs.readFileSync('web_serial_server_patched.js', 'utf8');

code = code.replace(/await sendPacket\([^;]+;\n/g, 'if(owiSocket) owiSocket.send("CMD:GENERIC:" + clean);\n');

// specifically for T:, S: and C:
code = code.replace(/function sendChatText\(text\) {\n  const clean = sanitizeOledText\(text\)\.slice\(0, 200\);\n  logEvent\(`chat "\${clean}"`\);\n  if\(owiSocket\) owiSocket\.send\("CMD:GENERIC:" \+ clean\);\n}/, 
`function sendChatText(text) {
  const clean = sanitizeOledText(text).slice(0, 200);
  logEvent(\`chat "\${clean}"\`);
  if(owiSocket) owiSocket.send("CMD:T:" + clean);
}`);

code = code.replace(/async function sendReminderText\(text\) {[\s\S]*?if\(owiSocket\) owiSocket\.send\("CMD:GENERIC:" \+ clean\);\n}/, 
`async function sendReminderText(text) {
  const clean = String(text || "").replace(/[^\\x20-\\x7E]/g, "").trim().slice(0, 32) || "enroll lagi ya deck";
  logEvent(\`reminder "\${clean}"\`);
  if(owiSocket) owiSocket.send("CMD:S:" + clean);
}`);

code = code.replace(/async function sendReminderSchedule\(time, text\) {[\s\S]*?if\(owiSocket\) owiSocket\.send\("CMD:GENERIC:" \+ clean\);\n}/, 
`async function sendReminderSchedule(time, text) {
  const safeTime = /^([01]\\d|2[0-3]):[0-5]\\d$/.test(String(time || "")) ? String(time) : "07:30";
  const clean = String(text || "").replace(/[^\\x20-\\x7E]/g, "").trim().slice(0, 32) || "enroll lagi ya deck";
  logEvent(\`schedule \${safeTime} "\${clean}"\`);
  if(owiSocket) owiSocket.send("CMD:C:" + safeTime + clean);
}`);

fs.writeFileSync('web_serial_server_patched.js', code);
console.log('Fixed syntax');
