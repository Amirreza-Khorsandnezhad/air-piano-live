// Bridge: Wokwi serial (RFC2217, port 4000) <-> web page (WebSocket), served from http://localhost:8787
const http = require('http'), fs = require('fs'), net = require('net'), path = require('path');
const { spawn } = require('child_process');
const { WebSocketServer } = require('ws');
const PORT = 8787, WOKWI_PORT = 4000;

const server = http.createServer((req, res) => {
  fs.readFile(path.join(__dirname, 'public', 'index.html'), (e, d) => {
    if (e) { res.writeHead(500); return res.end('public/index.html missing'); }
    res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' }); res.end(d);
  });
});
const wss = new WebSocketServer({ server });
let sock = null, wokwi = false, buf = '', cli = null;
const bc = o => { const s = JSON.stringify(o); wss.clients.forEach(c => c.readyState === 1 && c.send(s)); };

function telnetStrip(b) { // drop telnet/RFC2217 negotiation bytes
  const o = [];
  for (let i = 0; i < b.length; i++) {
    if (b[i] === 255) {
      const c = b[i + 1];
      if (c >= 251 && c <= 254) i += 2;
      else if (c === 250) { while (i < b.length && !(b[i] === 255 && b[i + 1] === 240)) i++; i++; }
      else i++;
    } else o.push(b[i]);
  }
  return Buffer.from(o).toString('utf8');
}
function connect() {
  const s = net.connect(WOKWI_PORT, '127.0.0.1');
  s.on('connect', () => { sock = s; wokwi = true; console.log('Wokwi serial connected'); bc({ type: 'status', wokwi: true }); });
  s.on('data', chunk => {
    buf += telnetStrip(chunk);
    let i;
    while ((i = buf.indexOf('\n')) >= 0) {
      const line = buf.slice(0, i).replace(/\r/g, ''); buf = buf.slice(i + 1);
      if (line) bc({ type: 'line', text: line });
    }
  });
  s.on('error', () => {});
  s.on('close', () => {
    if (sock === s) sock = null;
    if (wokwi) { wokwi = false; console.log('Wokwi serial lost'); bc({ type: 'status', wokwi: false }); }
    setTimeout(connect, 1000);
  });
}
function runCli() { // optional: start a headless simulation with wokwi-cli (needs WOKWI_CLI_TOKEN)
  if (cli) return;
  console.log('Starting wokwi-cli...');
  cli = spawn('wokwi-cli', ['--timeout', '3600000', '.'], { cwd: __dirname, shell: true, stdio: 'inherit' });
  cli.on('exit', () => { cli = null; });
  cli.on('error', e => { console.log('wokwi-cli failed:', e.message); cli = null; });
}
wss.on('connection', ws => {
  ws.send(JSON.stringify({ type: 'status', wokwi }));
  ws.on('message', m => {
    const t = m.toString().trim();
    if (t === 'RUN') return runCli();
    if (sock) sock.write(t + '\n');
  });
});
connect();
server.listen(PORT, () => console.log('Open http://localhost:' + PORT));
