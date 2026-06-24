const fs = require('fs');

const srcWifi = fs.readFileSync('src/wifi_max_stream.cpp', 'utf8');
const srcTft = fs.readFileSync('src/main_esp32_tft.cpp', 'utf8');

// Extract updateMPU from wifi_max_stream.cpp
const mpuRegex = /(void updateMPU\(\) \{[\s\S]+?\n\})/;
const match = srcWifi.match(mpuRegex);

if (match) {
    let updateMpuCode = match[1];
    
    // Find the place to insert in main_esp32_tft.cpp.
    // Insert right before `void updateDFPlayer()` which is around line 3900.
    if (!srcTft.includes('void updateMPU() {')) {
        let newTft = srcTft.replace('void updateDFPlayer() {', updateMpuCode + '\n\nvoid updateDFPlayer() {');
        fs.writeFileSync('src/main_esp32_tft.cpp', newTft);
        console.log('Successfully inserted updateMPU()!');
    } else {
        console.log('updateMPU() already exists in TFT file.');
    }
} else {
    console.log('Could not find updateMPU in wifi_max_stream.cpp');
}
