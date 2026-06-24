const { Client } = require('ssh2');
const fs = require('fs');
const path = require('path');

const conn = new Client();

const config = {
  host: '212.2.253.247',
  port: 22,
  username: 'root',
  password: 'cAh2TrVUlG'
};

const commands = [
  'apt-get update && apt-get install -y curl ffmpeg',
  'curl -fsSL https://deb.nodesource.com/setup_20.x | bash -',
  'apt-get install -y nodejs',
  'npm install -g pm2',
  'mkdir -p /root/owibot',
  'cd /root/owibot && npm init -y || true',
  'cd /root/owibot && npm install ws dotenv @google/genai serialport ffmpeg-static'
];

conn.on('ready', () => {
  console.log('Client :: ready');
  
  let i = 0;
  function nextCmd() {
    if (i >= commands.length) {
      uploadFiles();
      return;
    }
    const cmd = commands[i++];
    console.log('Executing:', cmd);
    conn.exec(cmd, (err, stream) => {
      if (err) throw err;
      stream.on('close', (code, signal) => {
        console.log('Stream :: close :: code: ' + code + ', signal: ' + signal);
        nextCmd();
      }).on('data', (data) => {
        // console.log('STDOUT: ' + data);
      }).stderr.on('data', (data) => {
        // console.error('STDERR: ' + data);
      });
    });
  }
  
  nextCmd();

  function uploadFiles() {
    conn.sftp((err, sftp) => {
      if (err) throw err;
      
      const files = ['vps_server.js', '.env'];
      let uploaded = 0;
      
      files.forEach(file => {
        const localPath = path.join(__dirname, file);
        if (!fs.existsSync(localPath)) {
          uploaded++;
          return;
        }
        sftp.fastPut(localPath, '/root/owibot/' + file, (err) => {
          if (err) throw err;
          console.log('Uploaded', file);
          uploaded++;
          if (uploaded === files.length) {
            startApp();
          }
        });
      });
    });
  }
  
  function startApp() {
    conn.exec('cd /root/owibot && pm2 start vps_server.js --name owi', (err, stream) => {
      if (err) throw err;
      stream.on('close', () => {
        console.log('App started!');
        conn.end();
      });
    });
  }
}).connect(config);
