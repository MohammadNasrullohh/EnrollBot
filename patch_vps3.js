const fs = require('fs');
const { Client } = require('ssh2');

const conn = new Client();
conn.on('ready', () => {
  console.log('Client :: ready');
  
  const cmd = "sed -i 's/highpass=f=140,lowpass=f=7000,loudnorm=I=-18:TP=-2.5/highpass=f=250,lowpass=f=6000,loudnorm=I=-20:TP=-4.0/g' /root/owibot/vps_server.js && sed -i 's/streamAudioToWS(ws, ttsFile, volume)/streamAudioToWS(ws, ttsFile, \"0.40\")/g' /root/owibot/vps_server.js && pm2 restart owi";
  
  conn.exec(cmd, (err, stream) => {
    if (err) throw err;
    stream.on('close', (code, signal) => {
      console.log('Stream close');
      conn.end();
    }).on('data', (data) => {
      process.stdout.write(data);
    }).stderr.on('data', (data) => {
      process.stderr.write(data);
    });
  });
}).connect({ host: '212.2.253.247', port: 22, username: 'root', password: 'cAh2TrVUlG' });
