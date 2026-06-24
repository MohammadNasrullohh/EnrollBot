const fs = require('fs');
let src = fs.readFileSync('src/main_esp32_tft.cpp', 'utf8');

// The file is a mess of duplicated functions.
let lines = src.split('\n');
let cleanLines = [];
let seenFunctions = new Set();
let inFunction = false;
let currentFunc = '';
let bracketCount = 0;

for (let i = 0; i < lines.length; i++) {
    let line = lines[i];
    
    if (!inFunction) {
        // Match return type, optional pointers/references, then function name
        let m = line.match(/^(?:void|int|bool|uint8_t|uint16_t|uint32_t|size_t|float|double|char|String)\s+([A-Za-z0-9_]+)\s*\(/);
        if (m) {
            currentFunc = m[1];
            if (seenFunctions.has(currentFunc)) {
                inFunction = true;
                bracketCount = (line.match(/\{/g) || []).length - (line.match(/\}/g) || []).length;
                if (bracketCount <= 0 && line.includes('}')) {
                    inFunction = false; // single line function
                }
                continue; // skip this duplicate function
            } else {
                seenFunctions.add(currentFunc);
                cleanLines.push(line);
            }
        } else {
            cleanLines.push(line);
        }
    } else {
        // We are skipping a duplicate function
        bracketCount += (line.match(/\{/g) || []).length;
        bracketCount -= (line.match(/\}/g) || []).length;
        if (bracketCount <= 0) {
            inFunction = false;
        }
    }
}

fs.writeFileSync('src/main_esp32_tft.cpp', cleanLines.join('\n'));
console.log('Cleaned file written to main_esp32_tft.cpp. Unique functions:', seenFunctions.size);
