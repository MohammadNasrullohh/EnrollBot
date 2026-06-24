const { Client } = require('ssh2');
const conn = new Client();
conn.on('ready', () => {
  conn.exec('tail -n 100 /root/.pm2/logs/owi-out.log /root/.pm2/logs/owi-error.log', (err, stream) => {
    if (err) throw err;
    stream.on('data', (d) => process.stdout.write(d));
    stream.on('close', () => conn.end());
  });
}).connect({ host: '212.2.253.247', port: 22, username: 'root', password: 'cAh2TrVUlG' });
