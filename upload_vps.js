const fs = require('fs');
const { Client } = require('ssh2');
const conn = new Client();
conn.on('ready', () => {
  conn.sftp((err, sftp) => {
    if (err) throw err;
    sftp.fastPut('vps_server_local.js', '/root/owibot/vps_server.js', (err) => {
      if (err) throw err;
      console.log('Uploaded');
      conn.exec('pm2 restart owi', (err, stream) => {
         if (err) throw err;
         stream.on('data', d => console.log(d.toString()));
         stream.on('close', () => conn.end());
      });
    });
  });
}).connect({ host: '212.2.253.247', port: 22, username: 'root', password: 'cAh2TrVUlG' });
