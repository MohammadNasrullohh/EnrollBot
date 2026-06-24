const fs = require('fs');

const srcFile = 'src/wifi_max_stream.cpp';
const targetFile = 'src/main_esp32_tft.cpp';

const srcContent = fs.readFileSync(srcFile, 'utf8');
let targetContent = fs.readFileSync(targetFile, 'utf8');

const matchStart = srcContent.indexOf('void ringReset() {');
const matchEnd = srcContent.indexOf('void setup() {');

if (matchStart !== -1 && matchEnd !== -1) {
  const missingCode = srcContent.substring(matchStart, matchEnd);
  
  targetContent = targetContent.replace('void setup() {', missingCode + '\nvoid setup() {');
  fs.writeFileSync(targetFile, targetContent);
  console.log('Restored missing code!');
} else {
  console.log('Could not find start or end match in src file.');
}
