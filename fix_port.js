const { Client } = require('ssh2');

const conn = new Client();

const config = {
  host: '212.2.253.247',
  port: 22,
  username: 'root',
  password: 'cAh2TrVUlG'
};

conn.on('ready', () => {
  console.log('Client :: ready');
  conn.exec('sed -i "s/const PORT = 3000;/const PORT = 3001;/" /root/owibot/vps_server.js && pm2 restart owi', (err, stream) => {
    if (err) throw err;
    stream.on('close', () => {
      console.log('Port changed and PM2 restarted');
      conn.end();
    }).on('data', (data) => {
      console.log('STDOUT: ' + data);
    }).stderr.on('data', (data) => {
      console.log('STDERR: ' + data);
    });
  });
}).connect(config);
