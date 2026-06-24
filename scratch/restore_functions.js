const fs = require('fs');

const wifi = fs.readFileSync('src/wifi_max_stream.cpp', 'utf8');
const tft = fs.readFileSync('src/main_esp32_tft.cpp', 'utf8');

// Get updateMPU from wifi
let startMPU = wifi.indexOf('void updateMPU() {');
let endMPU = wifi.indexOf('void updateDHT() {', startMPU);
let updateMPU = wifi.substring(startMPU, endMPU);

// Get updateDFPlayer from wifi
let startDF = wifi.indexOf('void updateDFPlayer() {');
let endDF = wifi.indexOf('void updateTouch() {', startDF);
let updateDF = wifi.substring(startDF, endDF);

// Combine them
let newFunctions = updateDF + '\n\n' + updateMPU + '\n\n';

// Insert before void loop() {
let loopIdx = tft.indexOf('void loop() {');
if (loopIdx > -1) {
    let cleanTft = tft.substring(0, loopIdx) + newFunctions + tft.substring(loopIdx);
    fs.writeFileSync('src/main_esp32_tft.cpp', cleanTft);
    console.log('Successfully inserted updateMPU and updateDFPlayer!');
} else {
    console.log('Failed to find loop()');
}
