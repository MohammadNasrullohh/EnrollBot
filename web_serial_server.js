const http = require("http");
const dgram = require("dgram");
const net = require("net");
const { spawn } = require("child_process");
const ffmpegPath = require("ffmpeg-static");

const PORT = 3000;
const SERIAL_PORT = process.env.SERIAL_PORT || "COM4";
const BAUD = 115200;

let serial = null;
let serialJustOpened = false;
const logs = [];

let latestTelemetry = {};
const udpServer = dgram.createSocket("udp4");
udpServer.on("message", (msg, rinfo) => {
  try {
    latestTelemetry = JSON.parse(msg.toString());
    latestTelemetry.lastUpdate = Date.now();
    latestTelemetry.ip = rinfo.address;
    if (latestTelemetry.req_lovestory === 1 && !isStreamingAudio) {
      streamAudio(latestTelemetry.ip, "0.06", "lovestory.mp3");
    }
  } catch(e){}
});
udpServer.bind(7788);

let isStreamingAudio = false;

async function streamAudio(ip, volume = "0.06", mp3Path = "lovestory.mp3") {
  if (isStreamingAudio) return;
  if (!ip) return;
  isStreamingAudio = true;
  logEvent(`stream audio ${mp3Path} ke ${ip}:7777 vol ${volume}`);

  try {
    const port = 7777;
    const sampleRate = 16000;
    const bytesPerSecond = sampleRate * 2;
    const chunkSize = 1024;

    const socket = net.createConnection({ host: ip, port });
    socket.setNoDelay(true);
    await new Promise((resolve, reject) => {
      socket.once("connect", resolve);
      socket.once("error", reject);
    });

    const ffmpeg = spawn(ffmpegPath, [
      "-hide_banner",
      "-loglevel", "error",
      "-i", mp3Path,
      "-f", "s16le",
      "-acodec", "pcm_s16le",
      "-ac", "1",
      "-ar", String(sampleRate),
      "-filter:a", `highpass=f=150,lowpass=f=7500,volume=${volume}`,
      "pipe:1",
    ], { stdio: ["ignore", "pipe", "pipe"] });

    let sent = 0;
    let started = Date.now();
    const leadBytes = 8192;

    for await (const chunk of ffmpeg.stdout) {
      for (let offset = 0; offset < chunk.length; offset += chunkSize) {
        const slice = chunk.subarray(offset, Math.min(offset + chunkSize, chunk.length));
        await new Promise((resolve, reject) => socket.write(slice, (err) => err ? reject(err) : resolve()));
        sent += slice.length;

        if (sent > leadBytes) {
          const targetMs = ((sent - leadBytes) / bytesPerSecond) * 1000;
          const elapsedMs = Date.now() - started;
          const waitMs = targetMs - elapsedMs;
          if (waitMs > 1) await sleep(Math.min(waitMs, 24));
        } else {
          started = Date.now();
        }
      }
    }

    await sleep(500);
    socket.end();
  } catch (err) {
    logEvent(`stream audio err: ${err.message}`);
  } finally {
    isStreamingAudio = false;
    logEvent("stream audio selesai");
  }
}

function logEvent(message) {
  const line = `${new Date().toLocaleTimeString()} ${message}`;
  logs.push(line);
  while (logs.length > 80) logs.shift();
  console.log(line);
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function closeSerial() {
  return new Promise((resolve) => {
    if (!serial) return resolve();
    const port = serial;
    serial = null;
    if (!port.isOpen) return resolve();
    port.close(() => resolve());
  });
}

async function openSerial() {
  if (serial && serial.isOpen && serial.writable) return serial;
  logEvent(`open ${SERIAL_PORT}`);
  const { SerialPort } = await import("serialport");
  serial = new SerialPort({ path: SERIAL_PORT, baudRate: BAUD, autoOpen: false, rtscts: false });
  serial.on("error", () => {
    logEvent("serial error");
    serial = null;
  });
  await new Promise((resolve, reject) => {
    serial.open((err) => (err ? reject(err) : resolve()));
  });
  await new Promise((resolve) => {
    serial.set({ dtr: false, rts: false }, () => resolve());
  });
  serialJustOpened = true;
  logEvent("serial opened");
  return serial;
}

function writeChunk(port, chunk) {
  return new Promise((resolve, reject) => {
    if (!port || !port.isOpen || !port.writable) {
      reject(new Error("Serial belum siap, coba klik lagi."));
      return;
    }
    port.write(chunk, (err) => {
      if (err) return reject(err);
      port.drain((drainErr) => (drainErr ? reject(drainErr) : resolve()));
    });
  });
}

async function sendPacket(packet, options = {}) {
  const chunkSize = options.chunkSize || 64;
  const chunkDelay = options.chunkDelay ?? 1;
  const openDelay = options.openDelay ?? 80;
  logEvent(`send ${packet.length} bytes`);
  let port = await openSerial();
  if (serialJustOpened) {
    serialJustOpened = false;
    await sleep(openDelay);
  }
  for (let i = 0; i < packet.length; i += chunkSize) {
    if (!port || !port.isOpen || !port.writable) {
      await closeSerial();
      await sleep(120);
      port = await openSerial();
    }
    await writeChunk(port, packet.subarray(i, i + chunkSize));
    if (chunkDelay > 0) await sleep(chunkDelay);
  }
}

function bytesToHex(buffer) {
  let out = "";
  for (const b of buffer) out += b.toString(16).padStart(2, "0");
  return out;
}

async function sendToSerial(buffer) {
  logEvent(`frame start ${buffer.length} bytes`);
  const packet = Buffer.from("H" + bytesToHex(buffer) + "\n", "ascii");
  try {
    await sendPacket(packet, { chunkSize: 128, chunkDelay: 1, openDelay: 180 });
  } catch (err) {
    await closeSerial();
    await sleep(250);
    await sendPacket(packet, { chunkSize: 128, chunkDelay: 1, openDelay: 180 });
  } finally {
    await sleep(20);
    await closeSerial();
    logEvent("serial closed");
  }
}

async function sendCommand(command) {
  const allowed = new Set(["C", "M", "R", "T", "G", "F", "P", "O", "D", "E", "L", "1", "2"]);
  if (!allowed.has(command)) throw new Error("Command tidak valid");
  logEvent(`cmd ${command}`);
  try {
    await sendPacket(Buffer.from(command + "\n"), { chunkSize: 8, chunkDelay: 0, openDelay: 40 });
  } finally {
    await sleep(10);
    await closeSerial();
    logEvent("serial closed");
  }
}

async function sendReminderText(text) {
  const clean = String(text || "").replace(/[^\x20-\x7E]/g, "").trim().slice(0, 32) || "enroll lagi ya deck";
  logEvent(`reminder "${clean}"`);
  try {
    await sendPacket(Buffer.from("S" + clean + "\n", "ascii"), { chunkSize: 34, chunkDelay: 0, openDelay: 40 });
  } finally {
    await sleep(10);
    await closeSerial();
    logEvent("serial closed");
  }
}

async function sendReminderSchedule(time, text) {
  const safeTime = /^([01]\d|2[0-3]):[0-5]\d$/.test(String(time || "")) ? String(time) : "07:30";
  const clean = String(text || "").replace(/[^\x20-\x7E]/g, "").trim().slice(0, 32) || "enroll lagi ya deck";
  const payload = `${safeTime}|${clean}`;
  logEvent(`reminder ${payload}`);
  try {
    await sendPacket(Buffer.from("S" + payload + "\n", "ascii"), { chunkSize: 40, chunkDelay: 0, openDelay: 40 });
  } finally {
    await sleep(10);
    await closeSerial();
    logEvent("serial closed");
  }
}

async function sendReminderSchedules(reminders) {
  const items = Array.isArray(reminders) ? reminders.slice(0, 5) : [];
  const payloadItems = items.map((item) => {
    const safeTime = /^([01]\d|2[0-3]):[0-5]\d$/.test(String(item.time || "")) ? String(item.time) : "07:30";
    const clean = String(item.text || "").replace(/[^\x20-\x7E]/g, "").trim().slice(0, 32) || "enroll lagi ya deck";
    return `${safeTime}|${clean}`;
  });
  if (payloadItems.length === 0) payloadItems.push("07:30|enroll lagi ya deck");
  const payload = `A:${payloadItems.join(";")}`;
  logEvent(`reminders ${payloadItems.length}`);
  try {
    await sendPacket(Buffer.from("S" + payload + "\n", "ascii"), { chunkSize: 96, chunkDelay: 0, openDelay: 40 });
  } finally {
    await sleep(10);
    await closeSerial();
    logEvent("serial closed");
  }
}

function pageHtml() {
  return `<!doctype html>
<html lang="id">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Owi Bot</title>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Inter:wght@700;900&family=Roboto+Mono:wght@500;700&display=swap');
    :root {
      --bg: #f5f5f5;
      --text: #000000;
      --border: 3px solid #000;
      --shadow: 4px 4px 0 #000;
      --hover-shadow: 2px 2px 0 #000;
      --accent: #ff0000;
      --accent-alt: #0000ff;
    }
    * { box-sizing: border-box; }
    html { scroll-behavior: smooth; }
    body {
      margin: 0; background: var(--bg); color: var(--text);
      font-family: 'Inter', sans-serif; text-transform: uppercase;
      overflow-x: hidden;
    }
    h1, h2, h3 { font-weight: 900; margin: 0 0 1rem; letter-spacing: -1px; }
    p { line-height: 1.5; font-family: 'Roboto Mono', monospace; font-weight: 500; margin: 0 0 1.5rem; text-transform: none; }
    a { color: var(--text); text-decoration: none; border-bottom: 3px solid transparent; transition: 0.2s; }
    a:hover { border-bottom: 3px solid #000; }

    /* Ticker */
    .ticker {
      border-bottom: var(--border); padding: 10px 0; background: #fff;
      font-family: 'Roboto Mono', monospace; font-weight: 700; overflow: hidden; white-space: nowrap;
      display: flex;
    }
    .ticker span { padding-left: 100%; animation: marq 20s linear infinite; }
    @keyframes marq { to { transform: translateX(-100%); } }

    /* Header */
    header { border-bottom: var(--border); background: var(--bg); position: sticky; top: 0; z-index: 50; }
    .nav { max-width: 1200px; margin: 0 auto; padding: 1rem 2rem; display: flex; justify-content: space-between; align-items: center; }
    .brand { font-size: 2.5rem; font-weight: 900; background: var(--accent); color: #fff; padding: 0 10px; border: var(--border); box-shadow: var(--shadow); }
    .nav-links { display: flex; gap: 2rem; font-family: 'Roboto Mono', monospace; font-weight: 700; }
    .nav-links a { padding: 0.5rem 1rem; border: var(--border); background: #fff; box-shadow: var(--shadow); transition: 0.1s; border-bottom: var(--border); }
    .nav-links a:hover { transform: translate(2px, 2px); box-shadow: var(--hover-shadow); background: var(--accent-alt); color: #fff; }
    .nav-links a#navControl { background: var(--accent); color: #fff; }
    
    /* Forms & Buttons */
    button, input, textarea {
      font-family: 'Roboto Mono', monospace; font-weight: 700; text-transform: uppercase;
      border-radius: 0; outline: none; border: var(--border); color: #000;
    }
    button {
      background: #fff; padding: 1rem 2rem; cursor: pointer;
      box-shadow: var(--shadow); transition: transform 0.1s, box-shadow 0.1s;
    }
    button:hover { transform: translate(2px, 2px); box-shadow: var(--hover-shadow); }
    button:active { transform: translate(4px, 4px); box-shadow: 0 0 0 #000; }
    button.primary { background: var(--accent); color: #fff; }

    input, textarea {
      width: 100%; padding: 1rem; background: #fff;
      box-shadow: var(--shadow); margin-bottom: 1.5rem;
    }
    input:focus, textarea:focus { background: #e0e0e0; }

    /* Layout */
    main { max-width: 1200px; margin: 0 auto; padding: 4rem 2rem; }
    .hero { display: grid; grid-template-columns: 1.2fr 1fr; gap: 4rem; align-items: center; margin-bottom: 6rem; }
    .hero-text h1 { font-size: 5rem; line-height: 0.9; margin-bottom: 2rem; }
    .eyebrow { font-family: 'Roboto Mono', monospace; font-weight: 700; background: #000; color: #fff; padding: 5px 15px; display: inline-block; margin-bottom: 1rem; border: var(--border); box-shadow: var(--shadow); }
    .actions { display: flex; gap: 1.5rem; flex-wrap: wrap; margin-top: 2rem; }

    /* Device Preview */
    .device-container { display: flex; justify-content: center; perspective: none; }
    .device {
      border: var(--border); background: #fff; padding: 2rem;
      box-shadow: 10px 10px 0 #000; width: 100%; max-width: 400px; aspect-ratio: 1;
      display: grid; place-items: center;
    }
    .oled {
      width: 100%; aspect-ratio: 2/1; background: #000; border: var(--border);
      position: relative; overflow: hidden;
    }
    .face { width: 100%; height: 100%; position: absolute; }
    .eye { position: absolute; top: 20%; width: 15%; height: 35%; background: var(--accent); }
    .eye.left { left: 20%; } .eye.right { right: 20%; }
    .mouth { position: absolute; bottom: 20%; left: 35%; width: 30%; height: 10%; background: var(--accent); }

    /* Sections */
    .section { margin-bottom: 6rem; }
    .section-head { text-align: left; max-width: 800px; margin-bottom: 3rem; }
    .section-head h2 { font-size: 3.5rem; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 2rem; }
    .panel { border: var(--border); background: #fff; padding: 2rem; box-shadow: var(--shadow); }

    /* Auth */
    .auth-container { max-width: 500px; margin: 0 auto; }
    .auth-tabs { display: flex; gap: 1rem; margin-bottom: 2rem; }
    .auth-tabs button { flex: 1; box-shadow: none; transform: none; background: transparent; border: 3px solid transparent; border-bottom: var(--border); }
    .auth-tabs button.active { background: #000; color: #fff; border: var(--border); }
    
    .status-msg { margin-top: 1rem; font-family: 'Roboto Mono', monospace; font-weight: 700; padding: 1rem; border: var(--border); display: none; }
    .status-msg.show { display: block; }
    .danger { background: var(--accent); color: #fff; }
    .success { background: #00ff00; color: #000; }
    .hidden { display: none !important; }

    footer { text-align: center; padding: 3rem; border-top: var(--border); font-family: 'Roboto Mono', monospace; font-weight: 700; background: #fff; }

    @media (max-width: 768px) {
      .hero { grid-template-columns: 1fr; gap: 2rem; }
      .hero-text h1 { font-size: 3.5rem; }
      .nav { flex-direction: column; gap: 1.5rem; }
      .nav-links { flex-wrap: wrap; justify-content: center; }
    }
  </style>
</head>
<body>
  <div class="ticker"><span>OWI BOT • BRUTALIST EDITION • NO BLURS • NO GRADIENTS • JUST PURE PIXELS AND HARD EDGES •</span></div>
  
  <header>
    <div class="nav">
      <div class="brand">OWI BOT</div>
      <nav class="nav-links">
        <a href="#features">FITUR</a>
        <a id="navLogin" href="#login">LOGIN</a>
        <a id="navControl" class="hidden" href="/control">CONTROL PANEL</a>
      </nav>
    </div>
  </header>

  <main>
    <section class="hero" id="top">
      <div class="hero-text">
        <span class="eyebrow">OWI GENERATION 1</span>
        <h1>SMALL OLED.<br>BIG MOOD.</h1>
        <p>Owi Bot is a tiny desk companion that reacts to its environment. Built with raw, unapologetic brutalist aesthetics. No soft corners.</p>
        <div class="actions">
          <a href="#features"><button class="primary">EXPLORE</button></a>
          <a href="#login"><button>LOGIN</button></a>
        </div>
      </div>
      <div class="device-container">
        <div class="device">
          <div class="oled">
            <div class="face">
              <div class="eye left"></div>
              <div class="eye right"></div>
              <div class="mouth"></div>
            </div>
          </div>
        </div>
      </div>
    </section>

    <section class="section" id="features">
      <div class="section-head">
        <h2>MADE TO FEEL ALIVE</h2>
        <p>Raw sensors. Direct feedback. High contrast interactions.</p>
      </div>
      <div class="grid">
        <div class="panel">
          <h3>SENSOR AWARE</h3>
          <p>Touch and motion sensors directly dictate Owi's expression. Zero latency, pure response.</p>
        </div>
        <div class="panel">
          <h3>PERSONAL LOOKS</h3>
          <p>Upload raw images. Destroy them into pure 1-bit high-contrast arrays. Feed them to Owi.</p>
        </div>
        <div class="panel">
          <h3>PRIVATE CONTROL</h3>
          <p>Locked down interface. Authenticate to gain raw access to the control mechanisms.</p>
        </div>
      </div>
    </section>

    <section class="section" id="login">
      <div class="auth-container panel">
        <div class="auth-tabs">
          <button id="loginTab" class="active">LOGIN</button>
          <button id="registerTab">REGISTER</button>
        </div>
        <div>
          <input id="authName" placeholder="USERNAME" type="text" autocomplete="off">
          <input id="authPass" placeholder="PASSWORD" type="password">
          <button id="authSubmit" class="primary" style="width: 100%;">ENTER</button>
          <button id="logoutBtn" style="display:none; width: 100%; margin-top: 1.5rem;">LOGOUT</button>
          <div id="authStatus" class="status-msg"></div>
        </div>
      </div>
    </section>
  </main>

  <footer>
    OWI BOT COMPANION WEB &copy; 2026. BRUTALIST EDITION.
  </footer>

  <script>
    let authMode = 'login';
    let currentUser = localStorage.getItem('owi_current_user') || '';
    
    function getUsers() { try { return JSON.parse(localStorage.getItem('owi_users') || '{}') } catch { return {} } }
    function saveUsers(users) { localStorage.setItem('owi_users', JSON.stringify(users)) }
    function setAuthStatus(text, bad) {
      const el = document.getElementById('authStatus');
      el.textContent = text;
      el.className = 'status-msg show ' + (bad ? 'danger' : 'success');
    }
    
    function updateAuthUi() {
      const logged = !!currentUser;
      document.getElementById('loginTab').classList.toggle('active', authMode === 'login');
      document.getElementById('registerTab').classList.toggle('active', authMode === 'register');
      document.getElementById('authSubmit').textContent = authMode === 'login' ? 'ENTER' : 'CREATE ACCOUNT';
      document.getElementById('authSubmit').style.display = logged ? 'none' : 'block';
      document.getElementById('authName').style.display = logged ? 'none' : 'block';
      document.getElementById('authPass').style.display = logged ? 'none' : 'block';
      document.getElementById('logoutBtn').style.display = logged ? 'block' : 'none';
      document.getElementById('navControl').classList.toggle('hidden', !logged);
      document.getElementById('navLogin').classList.toggle('hidden', logged);
      
      const st = document.getElementById('authStatus');
      if (logged) setAuthStatus('ACCESS GRANTED: ' + currentUser, false);
      else st.classList.remove('show');
    }
    
    document.getElementById('loginTab').onclick = () => { authMode = 'login'; updateAuthUi(); };
    document.getElementById('registerTab').onclick = () => { authMode = 'register'; updateAuthUi(); };
    
    document.getElementById('authSubmit').onclick = () => {
      const name = document.getElementById('authName').value.trim();
      const pass = document.getElementById('authPass').value;
      if (name.length < 3 || pass.length < 4) {
        setAuthStatus('MIN 3 CHAR USER, MIN 4 CHAR PASS.', true);
        return;
      }
      const users = getUsers();
      if (authMode === 'register') {
        if (users[name]) { setAuthStatus('USER EXISTS.', true); return; }
        users[name] = { pass };
        saveUsers(users);
      } else {
        if (!users[name] || users[name].pass !== pass) {
          setAuthStatus('INVALID CREDENTIALS.', true);
          return;
        }
      }
      currentUser = name;
      localStorage.setItem('owi_current_user', name);
      updateAuthUi();
      location.href = '/control';
    };
    
    document.getElementById('logoutBtn').onclick = () => {
      currentUser = '';
      localStorage.removeItem('owi_current_user');
      updateAuthUi();
    };
    
    updateAuthUi();
  </script>
</body>
</html>`;
}

function controlPageHtml() {
  return `<!doctype html>
<html lang="id">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Owi Bot Control</title>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Inter:wght@700;900&family=Roboto+Mono:wght@500;700&display=swap');
    :root {
      --bg: #f5f5f5;
      --text: #000;
      --border: 3px solid #000;
      --shadow: 4px 4px 0 #000;
      --hover-shadow: 2px 2px 0 #000;
      --accent: #ff0000;
      --accent-alt: #0000ff;
      --success: #00ff00;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    html { scroll-behavior: smooth; }
    body {
      background: var(--bg); color: var(--text);
      font-family: 'Inter', sans-serif; text-transform: uppercase;
      overflow-x: hidden; min-height: 100vh;
    }
    h2, h3 { font-weight: 900; letter-spacing: -1px; }
    p { font-family: 'Roboto Mono', monospace; text-transform: none; font-weight: 500; margin: 0; }

    .top-bar {
      display: flex; justify-content: space-between; align-items: center;
      padding: 1rem 2rem; border-bottom: var(--border); background: #fff;
      position: sticky; top: 0; z-index: 50;
    }
    .brand { font-size: 1.8rem; font-weight: 900; background: var(--accent); color: #fff; padding: 4px 12px; border: var(--border); box-shadow: var(--shadow); display: inline-block; }
    .sub-brand { font-size: 0.7rem; background: #000; color: #fff; padding: 2px 8px; display: inline-block; margin-left: 0.5rem; vertical-align: middle; }

    button, input {
      font-family: 'Roboto Mono', monospace; font-size: 0.85rem; font-weight: 700;
      outline: none; border: var(--border); text-transform: uppercase; border-radius: 0;
    }
    button {
      padding: 0.7rem 1.2rem; cursor: pointer; background: #fff; color: #000;
      box-shadow: var(--shadow); transition: transform 0.1s, box-shadow 0.1s;
    }
    button:hover { transform: translate(2px, 2px); box-shadow: var(--hover-shadow); }
    button:active { transform: translate(4px, 4px); box-shadow: 0 0 0 #000; }
    button.primary { background: var(--accent); color: #fff; }
    button.blue { background: var(--accent-alt); color: #fff; }
    button.sm { padding: 0.5rem 0.8rem; font-size: 0.75rem; }

    input[type="text"], input[type="time"] {
      width: 100%; padding: 0.7rem; background: #fff; color: #000; box-shadow: var(--shadow);
    }
    input:focus { background: #e0e0e0; }

    main { max-width: 1100px; margin: 0 auto; padding: 1.5rem; }
    .row { display: flex; gap: 0.8rem; flex-wrap: wrap; align-items: center; }
    .panel { border: var(--border); background: #fff; padding: 1.5rem; box-shadow: var(--shadow); }

    .hero-dash {
      display: grid; grid-template-columns: 300px 1fr; gap: 1.5rem; margin-bottom: 1.5rem;
    }
    @media (max-width: 800px) { .hero-dash { grid-template-columns: 1fr; } }

    .face-box {
      border: var(--border); background: #000; padding: 1.5rem;
      box-shadow: 8px 8px 0 #000; display: grid; place-items: center;
    }
    .oled {
      width: 100%; aspect-ratio: 2/1; background: #000; border: 2px solid #333;
      position: relative; overflow: hidden;
    }
    .face { width: 100%; height: 100%; position: absolute; transition: transform 0.15s ease-out; }
    .eye { position: absolute; top: 20%; width: 15%; height: 35%; background: var(--accent); transition: all 0.2s; }
    .eye.left { left: 20%; } .eye.right { right: 20%; }
    .mouth { position: absolute; bottom: 20%; left: 35%; width: 30%; height: 10%; background: var(--accent); transition: all 0.2s; }
    @keyframes breathe { 0%,100%{transform:scale(1)} 50%{transform:scale(1.02)} }
    .face-box .oled { animation: breathe 3s ease-in-out infinite; }
    .face-label { margin-top: 0.8rem; text-align: center; font-family: 'Roboto Mono', monospace; font-weight: 700; color: #555; font-size: 0.75rem; }
    .ip-label { margin-top: 0.3rem; text-align: center; font-family: 'Roboto Mono', monospace; font-weight: 700; font-size: 0.7rem; color: var(--accent); }

    .ctrl-stack { display: flex; flex-direction: column; gap: 1rem; justify-content: space-between; }
    .ctrl-stack h2 { font-size: 1.6rem; margin-bottom: 0.3rem; }

    .badge {
      background: #000; color: #fff; padding: 6px 10px;
      font-family: 'Roboto Mono', monospace; font-weight: 700;
      font-size: 0.75rem; border: 2px solid #000; transition: box-shadow 0.2s;
    }
    .badge.ok { box-shadow: 2px 2px 0 var(--success); }
    .badge.err { box-shadow: 2px 2px 0 var(--accent); }
    .badge.active { background: var(--accent); }

    .gesture-row { display: flex; gap: 6px; flex-wrap: wrap; min-height: 28px; }
    .gesture-badge {
      background: #eee; color: #999; padding: 3px 8px;
      font-family: 'Roboto Mono', monospace; font-weight: 700;
      font-size: 0.65rem; border: 2px solid #ccc; transition: all 0.15s;
    }
    .gesture-badge.on { background: #000; color: #fff; border-color: #000; }

    .status-bar {
      font-family: 'Roboto Mono', monospace; font-weight: 700; font-size: 0.75rem;
      padding: 0.5rem 0.8rem; background: #000; color: var(--success); border: var(--border);
    }
    .status-bar.err { color: var(--accent); }

    .sensor-grid {
      display: grid; grid-template-columns: repeat(4, 1fr); gap: 1rem; margin-bottom: 1.5rem;
    }
    @media (max-width: 800px) { .sensor-grid { grid-template-columns: repeat(2, 1fr); } }
    .sensor-card {
      border: var(--border); background: #fff; padding: 1rem;
      box-shadow: var(--shadow); text-align: center;
    }
    .sensor-card .label { font-family: 'Roboto Mono', monospace; font-weight: 700; font-size: 0.65rem; color: #666; margin-bottom: 0.3rem; }
    .sensor-card .value { font-family: 'Inter', sans-serif; font-weight: 900; font-size: 2rem; line-height: 1; letter-spacing: -2px; }
    .sensor-card .unit { font-family: 'Roboto Mono', monospace; font-weight: 700; font-size: 0.7rem; color: #999; }

    .tools-grid {
      display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 1.5rem;
    }
    .tools-grid h3 { font-size: 1.1rem; margin-bottom: 0.8rem; border-bottom: var(--border); padding-bottom: 0.5rem; }

    .reminderRow { display: grid; grid-template-columns: 100px 1fr auto; gap: 0.5rem; margin-bottom: 0.5rem; }
    .reminderRow input { margin: 0; box-shadow: 2px 2px 0 #000; font-size: 0.8rem; padding: 0.5rem; }

    .pingpong-card { text-align: center; }
    .score-display { display: flex; justify-content: center; align-items: center; gap: 1.5rem; margin: 1rem 0; }
    .score-num { font-family: 'Inter', sans-serif; font-weight: 900; font-size: 3.5rem; line-height: 1; letter-spacing: -3px; }
    .score-vs { font-family: 'Roboto Mono', monospace; font-weight: 700; font-size: 0.8rem; color: #999; }
    .score-label { font-family: 'Roboto Mono', monospace; font-weight: 700; font-size: 0.65rem; color: #666; }
  </style>
</head>
<body>
  <div class="top-bar">
    <div>
      <span class="brand">OWI BOT</span>
      <span class="sub-brand">CONTROL PANEL</span>
    </div>
    <div class="row">
      <a href="/"><button class="sm">PUBLIC WEB</button></a>
      <button id="logoutBtn" class="sm primary">LOGOUT</button>
    </div>
  </div>

  <main>
    <section class="hero-dash">
      <div class="face-box">
        <div class="oled">
          <div class="face">
            <div class="eye left"></div>
            <div class="eye right"></div>
            <div class="mouth"></div>
          </div>
        </div>
        <div class="face-label" id="faceLabel">MENUNGGU KONEKSI...</div>
        <div class="ip-label" id="ipLabel">IP: --</div>
      </div>

      <div class="panel ctrl-stack">
        <div>
          <h2>LIVE STATUS</h2>
          <div class="row" style="margin-bottom:0.8rem;">
            <span id="badgeMpu" class="badge">MPU: --</span>
            <span id="badgeInmp" class="badge">INMP: --</span>
            <span id="badgeMax" class="badge">MAX: --</span>
          </div>
          <div id="gestureRow" class="gesture-row">
            <span class="gesture-badge" data-g="touch">TOUCH</span>
            <span class="gesture-badge" data-g="nod">NOD</span>
            <span class="gesture-badge" data-g="headShake">GELENG</span>
            <span class="gesture-badge" data-g="surprised">KAGET</span>
            <span class="gesture-badge" data-g="curious">CURIOUS</span>
            <span class="gesture-badge" data-g="angry">ANGRY</span>
            <span class="gesture-badge" data-g="laugh">LAUGH</span>
            <span class="gesture-badge" data-g="sleep">SLEEP</span>
            <span class="gesture-badge" data-g="dizzy">PUSING</span>
            <span class="gesture-badge" data-g="sad">SEDIH</span>
            <span class="gesture-badge" data-g="love">LOVE</span>
            <span class="gesture-badge" data-g="cry">CRY</span>
            <span class="gesture-badge" data-g="pant">PANAS</span>
          </div>
        </div>
        <div>
          <div class="row" style="margin-bottom:0.8rem;">
            <span style="font-size:0.75rem;font-weight:700;font-family:'Roboto Mono',monospace;color:#999;">POSISI OWI SAAT INI:</span>
            <span id="menuStateLabel" class="badge" style="background:#000;color:var(--success);">--</span>
          </div>
          <div class="row" style="margin-bottom:0.8rem;">
            <button class="primary" data-cmd="P">TAP (NEXT)</button>
            <button class="primary" data-cmd="O">HOLD (OK)</button>
            <button data-cmd="E">PET</button>
            <button id="btnLoveStory" class="blue">&#9835; LOVE STORY</button>
            <button id="btnMbg" class="blue">&#9835; MBG</button>
          </div>
          <div class="row" style="margin-bottom:0.8rem;">
            <span style="font-size:0.7rem;font-weight:700;font-family:'Roboto Mono',monospace;">VOL MUSIK:</span>
            <input type="range" id="volLoveStory" min="1" max="100" value="50" style="width:100px;">
          </div>
          <div id="status" class="status-bar">SYSTEM READY</div>
        </div>
      </div>
    </section>

    <section class="sensor-grid">
      <div class="sensor-card" style="border-color:var(--accent);">
        <div class="label">EKSPRESI</div>
        <div class="value" id="valExpr" style="font-size:1.2rem;letter-spacing:0;">--</div>
        <div class="unit" id="valSpeech" style="color:var(--accent);font-size:0.85rem;">...</div>
      </div>
      <div class="sensor-card">
        <div class="label">SUHU</div>
        <div class="value" id="valTemp">--</div>
        <div class="unit">&deg;C</div>
      </div>
      <div class="sensor-card">
        <div class="label">KELEMBABAN</div>
        <div class="value" id="valHum">--</div>
        <div class="unit">%RH</div>
      </div>
      <div class="sensor-card">
        <div class="label">GUNCANGAN</div>
        <div class="value" id="valShake">0</div>
        <div class="unit">METER</div>
      </div>
    </section>

    <section class="tools-grid">
      <div class="panel">
        <h3>REMINDERS</h3>
        <div id="reminderList"></div>
        <div class="row" style="margin-top:0.8rem;">
          <button id="addReminder" class="sm">+ TAMBAH</button>
          <button id="sendReminder" class="sm primary">SYNC</button>
          <button id="sendReminderText" class="sm">KIRIM TEKS</button>
        </div>
      </div>
      <div class="panel pingpong-card">
        <h3>PINGPONG</h3>
        <div class="score-display">
          <div><div class="score-label">KAMU</div><div class="score-num" id="scoreP">0</div></div>
          <div class="score-vs">VS</div>
          <div><div class="score-label">AI</div><div class="score-num" id="scoreA">0</div></div>
        </div>
        <div class="row" style="justify-content:center;">
          <button class="blue" data-cmd="G">&#127955; MULAI GAME</button>
          <button class="sm" data-cmd="C">KEMBALI</button>
        </div>
      </div>
      <div class="panel" style="grid-column:1/-1;">
        <h3>&#127908; SPEECH RECOGNITION (INMP441)</h3>
        <div class="row" style="margin-bottom:0.8rem;">
          <button id="startSpeech" class="primary">MULAI DENGAR</button>
          <button id="stopSpeech" class="sm">STOP</button>
          <span id="speechStatus" style="font-family:'Roboto Mono',monospace;font-weight:700;font-size:0.75rem;color:#999;">IDLE</span>
        </div>
        <div id="speechLive" style="font-family:'Roboto Mono',monospace;font-weight:700;font-size:1.1rem;min-height:2rem;padding:0.8rem;border:var(--border);background:#000;color:var(--success);margin-bottom:0.8rem;text-transform:none;">...</div>
        <div id="speechLog" style="font-family:'Roboto Mono',monospace;font-size:0.75rem;max-height:150px;overflow-y:auto;padding:0.5rem;border:var(--border);background:#f9f9f9;text-transform:none;color:#333;"></div>
      </div>
    </section>
  </main>

  <script>
    if(!localStorage.getItem('owi_current_user')) location.href='/#login';
    const st=document.getElementById('status');
    const reminderList=document.getElementById('reminderList');
    function setStatus(t,bad){st.textContent=t;st.className=bad?'status-bar err':'status-bar';}

    function addReminderRow(time,text){
      time=time||'07:30';text=text||'enroll lagi ya deck';
      if(reminderList.children.length>=5){setStatus('MAX 5 REMINDERS.',true);return;}
      const row=document.createElement('div');row.className='reminderRow';
      row.innerHTML='<input class="reminderTime" type="time" value="'+time+'"><input class="reminderText" maxlength="32" value="'+text.replace(/"/g,'&quot;')+'"><button type="button" class="sm" style="padding:0.5rem">X</button>';
      row.querySelector('button').onclick=()=>{if(reminderList.children.length>1)row.remove();};
      reminderList.appendChild(row);
    }
    function collectReminders(){
      return Array.from(reminderList.querySelectorAll('.reminderRow')).slice(0,5).map(r=>({
        time:r.querySelector('.reminderTime').value,
        text:r.querySelector('.reminderText').value
      }));
    }
    document.getElementById('addReminder').onclick=()=>addReminderRow('12:00','enroll lagi ya deck');
    document.getElementById('sendReminder').onclick=async()=>{
      try{const r=await fetch('/reminder',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({reminders:collectReminders()})});setStatus(await r.text());}
      catch(e){setStatus(e.message,true);}
    };
    document.getElementById('sendReminderText').onclick=async()=>{
      try{const list=collectReminders();const r=await fetch('/reminder',{method:'POST',headers:{'Content-Type':'text/plain'},body:(list[0]&&list[0].text)||'enroll lagi ya deck'});setStatus(await r.text());}
      catch(e){setStatus(e.message,true);}
    };
    addReminderRow();

    document.getElementById('btnLoveStory').onclick = async () => {
      try {
        const vol = document.getElementById('volLoveStory').value;
        const r = await fetch('/play_audio', { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({ volume: (vol/100).toFixed(2), file: 'lovestory.mp3' }) });
        setStatus(await r.text());
      } catch(e) { setStatus(e.message, true); }
    };
    document.getElementById('btnMbg').onclick = async () => {
      try {
        const vol = document.getElementById('volLoveStory').value;
        const r = await fetch('/play_audio', { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({ volume: (vol/100).toFixed(2), file: 'mbg.mp3' }) });
        setStatus(await r.text());
      } catch(e) { setStatus(e.message, true); }
    };

    document.querySelectorAll('[data-cmd]').forEach(btn=>btn.onclick=async()=>{
      try{
        let r = await fetch('/cmd/'+btn.dataset.cmd,{method:'POST'});
        setStatus(await r.text());
      }catch(e){setStatus(e.message,true);}
    });
    document.getElementById('logoutBtn').onclick=()=>{localStorage.removeItem('owi_current_user');location.href='/';};

    async function refreshSensors(){
      try{
        const r=await fetch('/api/sensors');const s=await r.json();
        if(!s.lastUpdate)return;
        const bMpu=document.getElementById('badgeMpu');
        bMpu.textContent='MPU: '+(s.mpu==1?'OK':'ERR');
        bMpu.className='badge '+(s.mpu==1?'ok':'err');
        const bInmp=document.getElementById('badgeInmp');
        const inmpPct=s.inmp||0;
        bInmp.textContent='INMP: '+inmpPct+'%';
        bInmp.className='badge '+(inmpPct>0?'ok':'');
        const bMax=document.getElementById('badgeMax');
        bMax.textContent=(s.max==1?'🔊 MAX: PLAY':'🔈 MAX: IDLE');
        bMax.className='badge '+(s.max==1?'active':'');

        const gMap={touch:s.touch,nod:s.nod,headShake:s.headShake,surprised:s.surprised,curious:s.curious,angry:s.angry,laugh:s.laugh,sleep:s.sleep,dizzy:s.dizzy,sad:s.sad,love:s.love,cry:s.cry,pant:s.pant};
        document.querySelectorAll('.gesture-badge').forEach(el=>{el.classList.toggle('on',!!gMap[el.dataset.g]);});

        const temp=s.temp;
        document.getElementById('valTemp').textContent=(temp&&temp>-90)?temp.toFixed(1):'--';
        document.getElementById('valHum').textContent=(s.hum&&s.hum>=0)?s.hum.toFixed(0):'--';
        document.getElementById('valShake').textContent=Number(s.shakeMeter||0).toFixed(1);

        // Expression
        if(s.expr) {
          document.getElementById('valExpr').textContent=s.expr;
          document.getElementById('faceLabel').textContent=s.expr;
        }

        const stateMap = ["WAJAH NORMAL", "MENU UTAMA", "GAMES PINGPONG", "SENSOR SUHU", "REMINDER ALARM"];
        if(s.state !== undefined && s.state >= 0 && s.state < stateMap.length) {
          document.getElementById('menuStateLabel').textContent = stateMap[s.state];
        }

        if(s.scoreP!==undefined)document.getElementById('scoreP').textContent=s.scoreP;
        if(s.scoreA!==undefined)document.getElementById('scoreA').textContent=s.scoreA;

        const faceEl=document.querySelector('.face');
        if(faceEl)faceEl.style.transform='translate('+(s.tiltX*40||0)+'px, '+(s.tiltY*30||0)+'px)';
        if(s.ip)document.getElementById('ipLabel').textContent='IP: '+s.ip;
        setStatus('TILT X:'+Number(s.tiltX||0).toFixed(2)+' Y:'+Number(s.tiltY||0).toFixed(2)+' | SHAKE:'+Number(s.shakeMeter||0).toFixed(2));
      }catch(e){}
    }
    setInterval(refreshSensors,250);

    // ─── SPEECH RECOGNITION (Web Speech API - id-ID) ───
    let recognition = null;
    let isListening = false;
    const speechLive = document.getElementById('speechLive');
    const speechLog = document.getElementById('speechLog');
    const speechStatus = document.getElementById('speechStatus');

    function initSpeech() {
      const SR = window.SpeechRecognition || window.webkitSpeechRecognition;
      if (!SR) { speechStatus.textContent = 'TIDAK DIDUKUNG'; return null; }
      const r = new SR();
      r.lang = 'id-ID';
      r.continuous = true;
      r.interimResults = true;
      r.maxAlternatives = 1;
      r.onstart = () => { isListening = true; speechStatus.textContent = 'MENDENGAR...'; speechStatus.style.color = 'var(--accent)'; };
      r.onend = () => { if (isListening) { try { r.start(); } catch(e){} } else { speechStatus.textContent = 'IDLE'; speechStatus.style.color = '#999'; } };
      r.onerror = (e) => { if (e.error !== 'no-speech' && e.error !== 'aborted') { speechStatus.textContent = 'ERR: ' + e.error; } };
      r.onresult = (e) => {
        let interim = '';
        for (let i = e.resultIndex; i < e.results.length; i++) {
          const t = e.results[i][0].transcript;
          if (e.results[i].isFinal) {
            const ts = new Date().toLocaleTimeString('id-ID',{hour:'2-digit',minute:'2-digit',second:'2-digit'});
            const line = document.createElement('div');
            line.textContent = '[' + ts + '] ' + t;
            speechLog.prepend(line);
            speechLive.textContent = t;
            speechLive.style.color = 'var(--success)';
            // Send to Owi as reminder text
            fetch('/reminder',{method:'POST',headers:{'Content-Type':'text/plain'},body:t.slice(0,32)}).catch(()=>{});
          } else {
            interim += t;
          }
        }
        if (interim) { speechLive.textContent = interim; speechLive.style.color = '#ffff00'; }
      };
      return r;
    }

    document.getElementById('startSpeech').onclick = () => {
      if (!recognition) recognition = initSpeech();
      if (!recognition) return;
      isListening = true;
      try { recognition.start(); } catch(e) {}
    };
    document.getElementById('stopSpeech').onclick = () => {
      isListening = false;
      if (recognition) try { recognition.stop(); } catch(e) {}
      speechStatus.textContent = 'IDLE'; speechStatus.style.color = '#999';
      speechLive.textContent = '...';
    };
  </script>
</body>
</html>`;
}


const server = http.createServer((req, res) => {
  if (req.method === "GET" && req.url === "/") {
    res.writeHead(200, { "Content-Type": "text/html; charset=utf-8" });
    res.end(pageHtml());
    return;
  }
  if (req.method === "GET" && req.url === "/control") {
    res.writeHead(200, { "Content-Type": "text/html; charset=utf-8" });
    res.end(controlPageHtml());
    return;
  }
  if (req.method === "GET" && req.url === "/logs") {
    res.writeHead(200, { "Content-Type": "text/plain; charset=utf-8", "Cache-Control": "no-store" });
    res.end(logs.join("\\n"));
    return;
  }
  if (req.method === "GET" && req.url === "/api/sensors") {
    res.writeHead(200, { "Content-Type": "application/json", "Cache-Control": "no-store" });
    res.end(JSON.stringify(latestTelemetry));
    return;
  }
  if (req.method === "POST" && req.url === "/clear") {
    sendCommand("C").then(() => res.end("Balik ke wajah")).catch((err) => {res.writeHead(500);res.end(err.message);});
    return;
  }
  if (req.method === "POST" && req.url === "/play_audio") {
    let vol = "0.50";
    let file = "lovestory.mp3";
    const chunks = [];
    req.on("data", (chunk) => chunks.push(chunk));
    req.on("end", () => {
      try {
        const data = JSON.parse(Buffer.concat(chunks).toString() || "{}");
        if (data.volume) vol = data.volume;
        if (data.file) file = data.file;
      } catch(e) {}
      
      if (!latestTelemetry.ip) {
        res.writeHead(400); res.end("IP belum diketahui");
        return;
      }
      if (isStreamingAudio) {
        res.writeHead(400); res.end("Sedang stream");
        return;
      }
      streamAudio(latestTelemetry.ip, vol, file);
      res.end(`Memutar ${file}`);
    });
    return;
  }
  if (req.method === "POST" && req.url.startsWith("/cmd/")) {
    const cmd = decodeURIComponent(req.url.slice("/cmd/".length)).slice(0, 1);
    sendCommand(cmd).then(() => res.end("Command " + cmd + " terkirim")).catch((err) => {res.writeHead(500);res.end(err.message);});
    return;
  }
  if (req.method === "POST" && req.url === "/reminder") {
    const chunks = [];
    req.on("data", (chunk) => chunks.push(chunk));
    req.on("end", async () => {
      const text = Buffer.concat(chunks).toString("utf8");
      try {
        if ((req.headers["content-type"] || "").includes("application/json")) {
          const data = JSON.parse(text || "{}");
          if (Array.isArray(data.reminders)) {
            await sendReminderSchedules(data.reminders);
            res.end("Semua reminder tersimpan");
          } else {
            await sendReminderSchedule(data.time, data.text);
            res.end("Reminder jam tersimpan");
          }
        } else {
          await sendReminderText(text);
          res.end("Reminder teks tersimpan");
        }
      } catch (err) {
        res.writeHead(500);
        res.end(err.message);
      }
    });
    return;
  }
  if (req.method === "POST" && req.url === "/frame") {
    const chunks = [];
    req.on("data", (chunk) => chunks.push(chunk));
    req.on("end", async () => {
      const body = Buffer.concat(chunks);
      if (body.length !== 1024) {res.writeHead(400);res.end("Frame harus 1024 byte");return}
      try {await sendToSerial(body);res.end("Terkirim ke OLED")} catch (err) {res.writeHead(500);res.end(err.message)}
    });
    return;
  }
  res.writeHead(404);res.end("Not found");
});

server.listen(PORT, () => {
  console.log("Web: http://localhost:" + PORT);
  console.log("Serial: " + SERIAL_PORT + " @ " + BAUD);
});
