const fs = require('fs');
let web = fs.readFileSync('web_serial_server.js', 'utf8');

// I need to add M30 (Pusing) and M31 (Nakal)
// The HTML for the expression buttons looks like this:
// <button class="exprBtn" data-cmd="M29"><span class="big">●︵●</span><small>Nangis</small></button></div></div>
// I will insert M30 and M31 right before `</div></div>` for the expressions pane.

const newButtons = '<button class="exprBtn" data-cmd="M30"><span class="big">@_@</span><small>Pusing</small></button><button class="exprBtn" data-cmd="M31"><span class="big">●_−</span><small>Nakal</small></button>';

// I will just replace the end of that specific list.
web = web.replace('<button class="exprBtn" data-cmd="M29"><span class="big">●︵●</span><small>Nangis</small></button></div></div>', '<button class="exprBtn" data-cmd="M29"><span class="big">●︵●</span><small>Nangis</small></button>' + newButtons + '</div></div>');

fs.writeFileSync('web_serial_server.js', web);
console.log('Patched HTML with new expressions!');
