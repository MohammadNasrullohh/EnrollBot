const fs = require('fs');
const { Client } = require('ssh2');

const conn = new Client();
conn.on('ready', () => {
  const cmd = "sed -i 's/streamAudioToWS(ws, ttsFile, \"0.40\")/streamAudioToWS(ws, ttsFile, \"0.70\")/g' /root/owibot/vps_server.js && sed -i 's/loudnorm=I=-20:TP=-4.0:LRA=8/loudnorm=I=-16:TP=-3.0:LRA=7/g' /root/owibot/vps_server.js && pm2 restart owi";
  
  conn.exec(cmd, (err, stream) => {
    if (err) throw err;
    stream.on('close', () => conn.end());
    stream.on('data', (d) => process.stdout.write(d));
  });
}).connect({ host: '212.2.253.247', port: 22, username: 'root', password: 'cAh2TrVUlG' });
