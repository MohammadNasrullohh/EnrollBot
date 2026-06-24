const { Client } = require('ssh2');
const conn = new Client();
conn.on('ready', () => {
  const cmd = "sed -i 's/const packetBytes = 1024;/const packetBytes = 2048;/g' /root/owibot/vps_server.js && sed -i 's/const leadMs = 260;/const leadMs = 450;/g' /root/owibot/vps_server.js && pm2 restart owi";
  conn.exec(cmd, (err, stream) => {
    if (err) throw err;
    stream.on('close', (code, signal) => {
      conn.end();
    }).on('data', (data) => {
      console.log(data.toString());
    }).stderr.on('data', (data) => {
      console.error(data.toString());
    });
  });
}).connect({
  host: '212.2.253.247',
  port: 22,
  username: 'root',
  password: 'cAh2TrVUlG'
});
