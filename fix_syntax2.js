const fs = require('fs');
let code = fs.readFileSync('web_serial_server_patched.js', 'utf8');

code = code.replace(/function sendChatText[\s\S]*?async function sendReminderText/, 
`function sendChatText(text) {
  const clean = sanitizeOledText(text).slice(0, 200);
  logEvent(\`chat "\${clean}"\`);
  if (owiSocket) owiSocket.send("CMD:T:" + clean);
}

async function sendReminderText`);

code = code.replace(/async function sendReminderText[\s\S]*?async function sendReminderSchedule/, 
`async function sendReminderText(text) {
  const clean = String(text || "").replace(/[^\\x20-\\x7E]/g, "").trim().slice(0, 32) || "enroll lagi ya deck";
  logEvent(\`reminder "\${clean}"\`);
  if (owiSocket) owiSocket.send("CMD:S:" + clean);
}

async function sendReminderSchedule`);

code = code.replace(/async function sendReminderSchedule[\s\S]*?function logEvent/, 
`async function sendReminderSchedule(time, text) {
  const safeTime = /^([01]\\d|2[0-3]):[0-5]\\d$/.test(String(time || "")) ? String(time) : "07:30";
  const clean = String(text || "").replace(/[^\\x20-\\x7E]/g, "").trim().slice(0, 32) || "enroll lagi ya deck";
  logEvent(\`schedule \${safeTime} "\${clean}"\`);
  if (owiSocket) owiSocket.send("CMD:C:" + safeTime + clean);
}

function logEvent`);

fs.writeFileSync('web_serial_server_patched.js', code);
console.log('Fixed syntax 2');
