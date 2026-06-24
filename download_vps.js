const fs = require('fs');
const { Client } = require('ssh2');
const conn = new Client();
conn.on('ready', () => {
  conn.sftp((err, sftp) => {
    if (err) throw err;
    sftp.fastGet('/root/owibot/vps_server.js', 'vps_server_local.js', (err) => {
      if (err) throw err;
      console.log('Downloaded');
      conn.end();
    });
  });
}).connect({ host: '212.2.253.247', port: 22, username: 'root', password: 'cAh2TrVUlG' });
