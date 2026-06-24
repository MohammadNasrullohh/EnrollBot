const fs = require('fs');
const { Client } = require('ssh2');

const conn = new Client();
conn.on('ready', () => {
  console.log('Client :: ready');
  
  const cmd = "sed -i 's/maxOutputTokens: 220/maxOutputTokens: 1024/g' /root/owibot/vps_server.js && sed -i 's/Jawab ringkas tetapi selalu selesaikan kalimat. Jika perlu penjelasan panjang, gunakan 3 sampai 5 poin pendek./Jawab senatural mungkin, boleh panjang jika diperlukan untuk menjelaskan, tetapi pastikan kalimat tidak terputus di tengah jalan./g' /root/owibot/vps_server.js && sed -i 's/highpass=f=95,lowpass=f=7200,loudnorm=I=-16:TP=-1.5/highpass=f=140,lowpass=f=7000,loudnorm=I=-18:TP=-2.5/g' /root/owibot/vps_server.js && pm2 restart owi";
  
  conn.exec(cmd, (err, stream) => {
    if (err) throw err;
    stream.on('close', (code, signal) => {
      console.log('Stream :: close :: code: ' + code + ', signal: ' + signal);
      conn.end();
    }).on('data', (data) => {
      console.log('STDOUT: ' + data);
    }).stderr.on('data', (data) => {
      console.log('STDERR: ' + data);
    });
  });
}).connect({
  host: '212.2.253.247',
  port: 22,
  username: 'root',
  password: 'cAh2TrVUlG'
});
