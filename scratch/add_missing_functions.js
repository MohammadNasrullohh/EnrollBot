const fs = require('fs');

const wifi = fs.readFileSync('src/wifi_max_stream.cpp', 'utf8');
const tft = fs.readFileSync('src/main_esp32_tft.cpp', 'utf8');

function extract(startStr, endStr) {
    let s = wifi.indexOf(startStr);
    if (s === -1) return '';
    let e = wifi.indexOf(endStr, s);
    if (e === -1) return wifi.substring(s);
    return wifi.substring(s, e);
}

let codeToAdd = '';
codeToAdd += extract('void setVoiceState(', 'void beginVoiceCapture(') + '\n';
// processDfPlayerCommand is just a small switch statement, but let's grab dfPause, dfPlayTrack too
// Actually, dfPause and dfPlayTrack might be inside processDfPlayerCommand or separate.
// Let's just grab the whole block of DFPlayer helpers.
codeToAdd += extract('void processDfPlayerCommand(', 'void setupWiFi(') + '\n';
codeToAdd += extract('void updateTouch()', 'void updateMPU()') + '\n';
codeToAdd += extract('void updateDHT()', 'void sendTelemetry()') + '\n';

// wait, dfPause and dfPlayTrack might be before processDfPlayerCommand.
codeToAdd += extract('void dfPause()', 'void dfResume()') + '\n';
codeToAdd += extract('void dfPlayTrack(', 'void processDfPlayerCommand(') + '\n';

let loopIdx = tft.indexOf('void loop() {');
if (loopIdx > -1) {
    let cleanTft = tft.substring(0, loopIdx) + codeToAdd + '\n' + tft.substring(loopIdx);
    fs.writeFileSync('src/main_esp32_tft.cpp', cleanTft);
    console.log('Successfully added missing functions!');
}
