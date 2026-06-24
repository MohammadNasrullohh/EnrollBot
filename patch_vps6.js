const fs = require('fs');
const { Client } = require('ssh2');

const conn = new Client();
conn.on('ready', () => {
  const cmd = "sed -i 's/streamAudioToWS(ws, ttsFile, \"0.85\")/streamAudioToWS(ws, ttsFile, \"0.65\")/g' /root/owibot/vps_server.js && sed -i 's/highpass=f=250/highpass=f=350/g' /root/owibot/vps_server.js && pm2 restart owi";
  
  conn.exec(cmd, (err, stream) => {
    if (err) throw err;
    stream.on('close', () => conn.end());
    stream.on('data', (d) => process.stdout.write(d));
  });
}).connect({ host: '212.2.253.247', port: 22, username: 'root', password: 'cAh2TrVUlG' });
