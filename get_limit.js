const { Client } = require('ssh2');
const conn = new Client();
conn.on('ready', () => {
  conn.exec('grep -ni "limit" /root/owibot/vps_server.js', (err, stream) => {
    if (err) throw err;
    stream.on('close', () => conn.end());
    stream.on('data', (d) => process.stdout.write(d));
  });
}).connect({ host: '212.2.253.247', port: 22, username: 'root', password: 'cAh2TrVUlG' });
