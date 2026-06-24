const fs = require('fs');
let src = fs.readFileSync('src/main_esp32_tft.cpp', 'utf8');

// The file is a mess of duplicated functions.
// We will split the file by "void " and keep track of function names.
let lines = src.split('\n');
let cleanLines = [];
let seenFunctions = new Set();
let inFunction = false;
let currentFunc = '';
let bracketCount = 0;

for (let i = 0; i < lines.length; i++) {
    let line = lines[i];
    
    if (!inFunction) {
        let m = line.match(/^void\s+([A-Za-z0-9_]+)\s*\(/);
        if (m) {
            currentFunc = m[1];
            if (seenFunctions.has(currentFunc)) {
                inFunction = true;
                bracketCount = (line.match(/\{/g) || []).length - (line.match(/\}/g) || []).length;
                if (bracketCount === 0 && line.includes('}')) {
                    inFunction = false; // single line function
                }
                continue; // skip this duplicate function
            } else {
                seenFunctions.add(currentFunc);
                cleanLines.push(line);
                // We also need to track brackets here if we want to know when it ends? No need if it's the first time.
                // Wait, if it's the first time, we just keep the lines normally until the next duplicate.
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

fs.writeFileSync('src/main_esp32_tft_cleaned.cpp', cleanLines.join('\n'));
console.log('Cleaned file written to main_esp32_tft_cleaned.cpp. Unique functions:', seenFunctions.size);
