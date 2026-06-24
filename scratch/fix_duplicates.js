const fs = require('fs');
let src = fs.readFileSync('src/main_esp32_tft.cpp', 'utf8');

let idx1 = src.indexOf('void ringReset() {');
let idx2 = src.indexOf('void ringReset() {', idx1 + 1);

if (idx2 > idx1) {
    // We need to find the end of the second block.
    // The second block ends after the second webSocketEvent function.
    let wsEvent = src.indexOf('void webSocketEvent(', idx2);
    // Find the end of webSocketEvent (which has a huge switch statement)
    // Actually, the next function after the second webSocketEvent is 'void setup() {'
    let setupIdx = src.indexOf('\nvoid setup() {', wsEvent);
    
    if (setupIdx > -1) {
        let cleanSrc = src.substring(0, idx2) + src.substring(setupIdx);
        fs.writeFileSync('src/main_esp32_tft.cpp', cleanSrc);
        console.log('Successfully removed duplicate block from idx ' + idx2 + ' to ' + setupIdx);
    } else {
        console.log('Could not find setup()');
    }
} else {
    console.log('Could not find second ringReset');
}
