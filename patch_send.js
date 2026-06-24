const fs = require('fs');
let web = fs.readFileSync('web_serial_server.js', 'utf8');

const sendUdpLogic = `
  if (latestTelemetry && latestTelemetry.ip) {
    const dgram = require('dgram');
    const client = dgram.createSocket('udp4');
    client.send(Buffer.from('CMD:' + command), 7789, latestTelemetry.ip, (err) => {
      client.close();
      if (err) logEvent('UDP send err: ' + err);
      else logEvent('UDP sent CMD:' + command + ' to ' + latestTelemetry.ip);
    });
  } else {
    logEvent('No IP to send UDP command: ' + command);
  }
`;

// In patch_server.js, I replaced requireOwiSocket().send("CMD:" + command) with empty string.
// Let's find function sendCommand(command) and replace its body!
const fnStart = web.indexOf('function sendCommand(command) {');
if (fnStart !== -1) {
  const nextFn = web.indexOf('function sendChatText', fnStart);
  if (nextFn !== -1) {
    const newFn = `function sendCommand(command) {
${sendUdpLogic}
}
`;
    web = web.substring(0, fnStart) + newFn + web.substring(nextFn);
  }
}

fs.writeFileSync('web_serial_server.js', web);
console.log('Patched sendCommand with UDP!');
